#include <string>
#include <memory>
#include <vector>

#include "math.hpp"
#include "common.hpp"
#include "rig.hpp"
#include "rdr.hpp"
#include "cbufs.hpp"
#include "res/path_helpers.hpp"
#include "res/assimp_loader.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <assimp/scene.h>
CLANG_DIAG_POP

#include "json_helpers.hpp"

using nJsonHelpers::Document;
using nJsonHelpers::Value;
using nJsonHelpers::Size;

class cJsonLoaderImpl {
	cRigData& mRigData;
public:
	cJsonLoaderImpl(cRigData& rig) : mRigData(rig) {}

	bool operator()(Value const& doc) {
		CHECK_SCHEMA(doc.IsObject(), "doc is not an object\n");
		CHECK_SCHEMA(doc.HasMember("joints"), "no joints in doc\n");
		CHECK_SCHEMA(doc.HasMember("mtx"), "no mtx in doc\n");
		CHECK_SCHEMA(doc.HasMember("imtx"), "no imtx in doc\n");
		auto& joints = doc["joints"];
		auto& mtx = doc["mtx"];
		auto& imtx = doc["imtx"];

		auto jointsNum = joints.Size();
		auto imtxNum = imtx.Size();
		CHECK_SCHEMA(mtx.Size() == jointsNum, "joints and mtx num differ\n");

		auto pJoints = std::make_unique<sJointData[]>(jointsNum);
		auto pMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum);
		auto pImtx = std::make_unique<DirectX::XMMATRIX[]>(imtxNum);
		auto pNames = std::make_unique<std::string[]>(jointsNum);

		for (auto pj = joints.Begin(); pj != joints.End(); ++pj) {
			auto const& j = *pj;
			CHECK_SCHEMA(j.IsObject(), "joint definition is not an object\n");

			CHECK_SCHEMA(j.HasMember("name"), "no joint's name\n");
			auto& jname = j["name"];
			CHECK_SCHEMA(j.HasMember("idx"), "no joint's idx\n");
			int idx = j["idx"].GetInt();
			CHECK_SCHEMA(j.HasMember("parIdx"), "no joint's parIdx\n");
			int parIdx = j["parIdx"].GetInt();
			CHECK_SCHEMA(j.HasMember("skinIdx"), "no joint's skinIdx\n");
			int skinIdx = j["skinIdx"].GetInt();
			
			auto name = jname.GetString();
			auto nameLen = jname.GetStringLength();

			pNames[idx] = std::string(name, nameLen);
			pJoints[idx] = { idx, parIdx, skinIdx };
		}

		auto readMtx = [](DirectX::XMMATRIX* pMtx, Value const& m) {
			CHECK_SCHEMA(m.IsArray(), "matrix is not an array\n");
			CHECK_SCHEMA(m.Size() == 16, "wrong matrix size\n");

			float tmp[16];
			for (int i = 0; i < 16; ++i) {
				tmp[i] = (float)m[i].GetDouble();
			}
			*pMtx = DirectX::XMMATRIX(tmp);
			return true;
		};

		for (Size i = 0; i < jointsNum; ++i) {
			auto const& m = mtx[i];
			if (!readMtx(&pMtx[i], m)) {
				return false;
			}
		}

		for (Size i = 0; i < imtxNum; ++i) {
			auto const& m = imtx[i];
			if (!readMtx(&pImtx[i], m)) {
				return false;
			}
		}

		mRigData.mJointsNum = jointsNum;
		mRigData.mIMtxNum = imtxNum;
		mRigData.mpJoints = std::move(pJoints);
		mRigData.mpLMtx = std::move(pMtx);
		mRigData.mpIMtx = std::move(pImtx);
		mRigData.mpNames = std::move(pNames);

		return true;
	}
};

bool cRigData::load(const fs::path& filepath) {
	if (filepath.extension() == ".rig") {
		return load_json(filepath);
	}

	dbg_msg("cRigData::load(): Unknown file extension in <%" PRI_FILE ">", filepath.c_str());
	return false;
}

bool cRigData::load_json(const fs::path& filepath) {
	cJsonLoaderImpl loader(*this);
	return nJsonHelpers::load_file(filepath, loader);
}

int cRigData::find_joint_idx(cstr name) const {
	for (int i = 0; i < mJointsNum; ++i) {
		if (0 == mpNames[i].compare(name)) {
			return i;
		}
	}
	return -1;
}


struct sNodeHie {
	aiNode const* mpNode;
	int32_t mJntIdx;
	int32_t mParentIdx;
	int32_t mBoneIdx;
	bool mRequired;
};

static void list_nodes(aiScene const* pScene, aiNode const* pNode, int32_t parentIdx, std::vector<sNodeHie>& nodeHie) {
	int32_t nodeIdx = (int32_t)nodeHie.size();
	nodeHie.emplace_back(sNodeHie{pNode, -1, parentIdx, -1, false});

	for (uint32_t i = 0; i < pNode->mNumChildren; ++i) {
		list_nodes(pScene, pNode->mChildren[i], nodeIdx, nodeHie);
	}
}

static void mark_required(std::vector<sNodeHie>& nodeHie, int idx) {
	auto& nh = nodeHie[idx];
	if (nh.mRequired) { return; }
	nh.mRequired = true;
	if (nh.mParentIdx >= 0) {
		mark_required(nodeHie, nh.mParentIdx);
	}
}

bool cRigData::load(cAssimpLoader& loader) {
	auto pScene = loader.get_scene();
	if (!pScene) { return false; }
	auto& bones = loader.get_bones_info();
	if (bones.size() == 0) { return false; }
	auto& bonesMap = loader.get_bones_map();

	std::vector<sNodeHie> nodeHie;
	list_nodes(pScene, pScene->mRootNode, -1, nodeHie);

	for (int i = 0; i < nodeHie.size(); ++i) {
		auto& nh = nodeHie[i];
		auto bit = bonesMap.find(nh.mpNode->mName.C_Str());
		if (bit != bonesMap.end()) {
			nh.mBoneIdx = bit->second;
			// TODO: would be nice to skip bones without any weights
			// Requires some remapping in model loader
			if (true || bones[nh.mBoneIdx].mpBone->mNumWeights > 0)
				mark_required(nodeHie, i);
		}
	}

	int32_t jointsNum = 0;
	for (auto& nh : nodeHie) {
		if (nh.mRequired) {
			nh.mJntIdx = jointsNum;
			++jointsNum;
		}
	}

	size_t imtxNum = bones.size();

	auto pJoints = std::make_unique<sJointData[]>(jointsNum);
	auto pMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum);
	auto pImtx = std::make_unique<DirectX::XMMATRIX[]>(imtxNum);
	auto pNames = std::make_unique<std::string[]>(jointsNum);

	int idx = 0;
	for (auto const& nh : nodeHie) {
		if (!nh.mRequired) { continue; }
		assert(idx < jointsNum);
		int parIdx = -1;
		if (nh.mParentIdx >= 0) {
			parIdx = nodeHie[nh.mParentIdx].mJntIdx;
		}
		pJoints[idx] = sJointData{ idx, parIdx, nh.mBoneIdx };
		//dbg_msg("%d %d %d %s\n", idx, parIdx, nh.mBoneIdx, nh.mpNode->mName.C_Str());
		if (pJoints[idx].skinIdx >= sSkinCBuf::MAX_SKIN_MTX)
		{
			dbg_msg("Too many joints");
			return false;
		}

		::memcpy(&pMtx[idx], &nh.mpNode->mTransformation, sizeof(pMtx[idx]));
		pMtx[idx] = DirectX::XMMatrixTranspose(pMtx[idx]);

		pNames[idx] = std::string(nh.mpNode->mName.C_Str(), nh.mpNode->mName.length);

		if (nh.mBoneIdx >= 0) {
			auto const& bone = bones[nh.mBoneIdx];
			auto& imtx = pImtx[nh.mBoneIdx];

			::memcpy(&imtx, &bone.mpBone->mOffsetMatrix, sizeof(imtx));
			imtx = DirectX::XMMatrixTranspose(imtx);
		}

		++idx;
	}

	mJointsNum = jointsNum;
	mIMtxNum = (int32_t)imtxNum;
	mpJoints = std::move(pJoints);
	mpLMtx = std::move(pMtx);
	mpIMtx = std::move(pImtx);
	mpNames = std::move(pNames);

	return true;
}


void cRig::init(ConstRigDataPtr pRigData) {
	if (!pRigData) { return; }
	
	const int jointsNum = pRigData->mJointsNum;
	auto pLMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum);
	auto pWMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum + 1);
	auto pJoints = std::make_unique<cJoint[]>(jointsNum);
	auto pXforms = std::make_unique<sXform[]>(jointsNum);

	::memcpy(pLMtx.get(), pRigData->mpLMtx.get(), sizeof(pRigData->mpLMtx[0]) * jointsNum);

	for (int i = 0; i < jointsNum; ++i) {
		auto const& jdata = pRigData->mpJoints[i];
		auto& jnt = pJoints[i];

		assert(jdata.idx == i);
		assert(jdata.parIdx < i);

		jnt.mpXform = &pXforms[i];
		jnt.mpLMtx = &pLMtx[i];
		jnt.mpWMtx = &pWMtx[i];
		
		jnt.mpXform->init(*jnt.mpLMtx);

		if (jdata.skinIdx >= 0) {
			jnt.mpIMtx = &pRigData->mpIMtx[jdata.skinIdx];
		}
		if (jdata.parIdx >= 0) {
			jnt.mpParentMtx = pJoints[jdata.parIdx].mpWMtx;
		}
		else {
			jnt.mpParentMtx = &nMtx::g_Identity;
		}
	}
	if (jointsNum > 0) {
		pJoints[0].set_parent_mtx(&pWMtx[jointsNum]);
	}
	
	mJointsNum = jointsNum;
	mpRigData = std::move(pRigData);
	mpJoints = std::move(pJoints);
	mpLMtx = std::move(pLMtx);
	mpWmtx = std::move(pWMtx);
	mpXforms = std::move(pXforms);

	calc_world(DirectX::XMMatrixIdentity());
}

void cRig::calc_local() {
	assert(mJointsNum > 0);
	assert(mpJoints);
	
	for (int i = 0; i < mJointsNum; ++i) {
		mpJoints[i].calc_local();
	}
}

void XM_CALLCONV cRig::calc_world(DirectX::XMMATRIX rootWMtx) {
	assert(mJointsNum > 0);
	assert(mpWmtx);
	assert(mpJoints);

	mpWmtx[mJointsNum] = rootWMtx;
	for (int i = 0; i < mJointsNum; ++i) {
		mpJoints[i].calc_world();
	}
}

void cRig::upload_skin(cRdrContext const& rdrCtx) const {
	auto& skinCBuf = rdrCtx.get_cbufs().mSkinCBuf;

	auto* pSkin = skinCBuf.mData.skin;

	for (int i = 0; i < mJointsNum; ++i) {
		auto pImtx = mpJoints[i].get_inv_mtx();
		if (!pImtx) { continue; }
		int skinIdx = mpRigData->mpJoints[i].skinIdx;
		assert(skinIdx < sSkinCBuf::MAX_SKIN_MTX);

		auto const& pWmtx = mpJoints[i].get_world_mtx();

		pSkin[skinIdx] = (*pImtx) * pWmtx;
	}

	auto pCtx = rdrCtx.get_ctx();
	skinCBuf.update(pCtx);
	skinCBuf.set_VS(pCtx);
}

cJoint* cRig::get_joint(int idx) const {
	if (!mpJoints) { return nullptr; }
	if (idx >= mJointsNum) { return nullptr; }
	return &mpJoints[idx];
}

cJoint* cRig::find_joint(cstr name) const {
	if (!mpJoints || !mpRigData) { return nullptr; }

	int idx = mpRigData->find_joint_idx(name);
	if (idx == -1)
		return nullptr;

	return &mpJoints[idx];
}

cRigData const* cRig::get_rig_data() const {
	return mpRigData.get();
}


void cJoint::calc_world() {
	(*mpWMtx) = (*mpLMtx) * (*mpParentMtx);
}

void cJoint::calc_local() {
	(*mpLMtx) = mpXform->build_mtx();
}

