#include <string>
#include <memory>
#include <vector>

#include "math.hpp"
#include "common.hpp"
#include "rig.hpp"
#include "rdr.hpp"
#include "assimp_loader.hpp"

#include <assimp/scene.h>

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
		mRigData.mpJoints = pJoints.release();
		mRigData.mpLMtx = pMtx.release();
		mRigData.mpIMtx = pImtx.release();
		mRigData.mpNames = pNames.release();
		mRigData.mAllocatedArrays = true;

		return true;
	}
};

cRigData::~cRigData() {
	if (mAllocatedArrays) {
		delete[] mpJoints;
		delete[] mpLMtx;
		delete[] mpIMtx;
		delete[] mpNames;
	}
}

bool cRigData::load(cstr filepath) {
	if (filepath.ends_with(".rig")) {
		return load_json(filepath);
	}

	dbg_msg("cRigData::load(): Unknown file extension in <%s>", filepath.p);
	return false;
}

bool cRigData::load_json(cstr filepath) {
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
	mpJoints = pJoints.release();
	mpLMtx = pMtx.release();
	mpIMtx = pImtx.release();
	mpNames = pNames.release();
	mAllocatedArrays = true;

	return true;
}


void cRig::init(cRigData const* pRigData) {
	if (!pRigData) { return; }
	
	const int jointsNum = pRigData->mJointsNum;
	auto pLMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum);
	auto pWMtx = std::make_unique<DirectX::XMMATRIX[]>(jointsNum);
	auto pJoints = std::make_unique<cJoint[]>(jointsNum);
	auto pXforms = std::make_unique<sXform[]>(jointsNum);

	::memcpy(pLMtx.get(), pRigData->mpLMtx, sizeof(pRigData->mpLMtx[0]) * jointsNum);

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
			//jnt.mpParent = &pJoints[jdata.parIdx];
			jnt.mpParentMtx = pJoints[jdata.parIdx].mpWMtx;
		}
		else {
			jnt.mpParentMtx = &nMtx::g_Identity;
		}
	}
	
	mJointsNum = jointsNum;
	mpRigData = pRigData;
	mpJoints = pJoints.release();
	mpLMtx = pLMtx.release();
	mpWmtx = pWMtx.release();
	mpXforms = pXforms.release();

	calc_world();
}

void cRig::calc_local() {
	for (int i = 0; i < mJointsNum; ++i) {
		mpJoints[i].calc_local();
	}
}

void cRig::calc_world() {
	for (int i = 0; i < mJointsNum; ++i) {
		mpJoints[i].calc_world();
	}
}

void cRig::upload_skin(ID3D11DeviceContext* pCtx) {
	auto& skinCBuf = cConstBufStorage::get().mSkinCBuf;

	auto* pSkin = skinCBuf.mData.skin;

	for (int i = 0; i < mJointsNum; ++i) {
		auto pImtx = mpJoints[i].get_inv_mtx();
		if (!pImtx) { continue; }
		int skinIdx = mpRigData->mpJoints[i].skinIdx;

		auto const& pWmtx = mpJoints[i].get_world_mtx();

		pSkin[skinIdx] = (*pImtx) * pWmtx;
	}

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



void cJoint::calc_world() {
	(*mpWMtx) = (*mpLMtx) * (*mpParentMtx);
}

void cJoint::calc_local() {
	(*mpLMtx) = mpXform->build_mtx();
}

