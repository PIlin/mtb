#include <string>
#include <memory>

#include "common.hpp"
#include "math.hpp"
#include "anim.hpp"
#include "rig.hpp"
#include "assimp_loader.hpp"
#include "json_helpers.hpp"

#include <assimp/scene.h>

using nJsonHelpers::Document;
using nJsonHelpers::Value;
using nJsonHelpers::Size;

class cAnimJsonLoaderImpl {
	cAnimationData& mData;
public:
	cAnimJsonLoaderImpl(cAnimationData& data) : mData(data) {}
	bool operator()(Value const& doc) {
		CHECK_SCHEMA(doc.IsObject(), "doc is not an object\n");
		CHECK_SCHEMA(doc.HasMember("name"), "no animation name\n");
		auto& n = doc["name"];
		std::string animName(n.GetString(), n.GetStringLength());

		CHECK_SCHEMA(doc.HasMember("lastFrame"), "no animation name\n");
		float lastFrame = (float)doc["lastFrame"].GetDouble();

		CHECK_SCHEMA(doc.HasMember("channels"), "no channels\n");
		auto& channels = doc["channels"];
		CHECK_SCHEMA(channels.IsArray(), "channels is not an array\n");

		Size channelsNum = channels.Size();
		auto pChannels = std::make_unique<cChannel[]>(channelsNum);
		for (Size i = 0; i < channelsNum; ++i) {
			auto& c = channels[i];
			if (!load_channel(c, pChannels[i])) {
				return false;
			}
		}

		mData.mpChannels = pChannels.release();
		mData.mChannelsNum = channelsNum;
		mData.mLastFrame = lastFrame;
		mData.mName = std::move(animName);

		return true;
	}
private:
	bool load_channel(Value const& doc, cChannel& ch) {
		CHECK_SCHEMA(doc.IsObject(), "channel is not an object\n");
		
		CHECK_SCHEMA(doc.HasMember("name"), "channel has no name\n");
		auto& n = doc["name"];
		std::string name(n.GetString(), n.GetStringLength());
		
		CHECK_SCHEMA(doc.HasMember("subName"), "channel has no subname\n");
		auto& sn = doc["subName"];
		std::string subName(sn.GetString(), sn.GetStringLength());
		
		CHECK_SCHEMA(doc.HasMember("type"), "channel has no type\n");
		int type = doc["type"].GetInt();

		CHECK_SCHEMA(doc.HasMember("rord"), "channel has no rord\n");
		uint16_t rord = doc["rord"].GetInt();

		CHECK_SCHEMA(doc.HasMember("expr"), "channel has no expr\n");
		uint16_t expr = (uint16_t)doc["expr"].GetInt();

		CHECK_SCHEMA(doc.HasMember("size"), "channel has no size\n");
		Size size = doc["size"].GetInt();

		CHECK_SCHEMA(doc.HasMember("comp"), "channel has no comp\n");
		auto& comp = doc["comp"];
		CHECK_SCHEMA(comp.IsArray(), "comp is not an array\n");
		CHECK_SCHEMA(comp.Size() >= size, "comp size mismatch\n");

		auto pKfrNum = std::make_unique<int[]>(size);
		auto pComp = std::make_unique<sKeyframe*[]>(size);

		int totalKeyframes = 0;
		for (Size i = 0; i < size; ++i) {
			auto& kfrs = comp[i];
			CHECK_SCHEMA(kfrs.IsArray(), "keyframes is not an array\n");
			int count = (int)kfrs.Size();
			CHECK_SCHEMA(count > 0, "channel has 0 keyframes\n");
			pKfrNum[i] = count;
			totalKeyframes += count;
		}

		auto pKfr = std::make_unique<sKeyframe[]>(totalKeyframes);
		sKeyframe* p = pKfr.get();
		for (Size i = 0; i < size; ++i) {
			pComp[i] = p;
			p += pKfrNum[i];
		}

		for (Size i = 0; i < size; ++i) {
			p = pComp[i];
			auto& kfrs = comp[i];
			for (int j = 0; j < pKfrNum[i]; ++j) {
				auto& k = kfrs[j];
				CHECK_SCHEMA(k.IsArray(), "keyframe is not an array\n");
				CHECK_SCHEMA(k.Size() == 4, "invalid keyframe\n");

				p->frame = (float)k[0u].GetDouble();
				p->value = (float)k[1].GetDouble();
				p->inSlope = (float)k[2].GetDouble();
				p->outSlope = (float)k[3].GetDouble();
				p++;
			}
		}

		ch.mpKeyframesNum = pKfrNum.release();
		ch.mpComponents = pComp.release();
		pKfr.release();
		ch.mComponentsNum = size;
		ch.mExpr = (expr < cChannel::E_EXPR_LAST) ? 
			(cChannel::eExpressionType)expr : cChannel::E_EXPR_CONSTANT;
		ch.mRotOrd = (rord < cChannel::E_ROT_LAST) ?
			(cChannel::eRotOrder)rord : cChannel::E_ROT_XYZ;
		ch.mName = std::move(name);
		ch.mSubname = std::move(subName);

		return true;
	}
};

class cAnimAssimpLoaderImpl {
	cAnimationData& mData;
public:
	cAnimAssimpLoaderImpl(cAnimationData& data) : mData(data) {}
	bool operator()(aiAnimation const& anim) {
		std::string animName(anim.mName.C_Str(), anim.mName.length);

		float lastFrame = (float)anim.mDuration;

		

		uint32_t channelsNum = anim.mNumChannels * 3;
		auto pChannels = std::make_unique<cChannel[]>(channelsNum);
		for (Size i = 0; i < anim.mNumChannels; ++i) {
			auto& node = *anim.mChannels[i];

			if (!load_channel(node, 's', pChannels[i * 3 + 0])) { return false; }
			if (!load_channel(node, 'r', pChannels[i * 3 + 1])) { return false; }
			if (!load_channel(node, 't', pChannels[i * 3 + 2])) { return false; }
		}

		mData.mpChannels = pChannels.release();
		mData.mChannelsNum = channelsNum;
		mData.mLastFrame = lastFrame;
		mData.mName = std::move(animName);

		return true;
	}
private:
	bool load_channel(aiNodeAnim const& node, char chType, cChannel& ch) {
		auto& n = node.mNodeName;
		std::string name(n.C_Str(), n.length);

		char buf[2] = { chType, 0 };
		std::string subName(buf, 2);

		cChannel::eChannelType type = cChannel::E_CH_COMMON;
		cChannel::eExpressionType expr = cChannel::E_EXPR_LINEAR;
		cChannel::eRotOrder rord = cChannel::E_ROT_XYZ;
		int compNum = 3;
		int kfrNum = 0;
		switch (chType) {
		case 's':
			kfrNum = node.mNumScalingKeys;
			break;
		case 'r':
			type = cChannel::E_CH_QUATERNION;
			expr = cChannel::E_EXPR_QLINEAR;
			compNum = 4;
			kfrNum = node.mNumRotationKeys;
			break;
		case 't':
			kfrNum = node.mNumPositionKeys;
			break;
		}

		int totalKeyframes = compNum * kfrNum;

		auto pKfrNum = std::make_unique<int[]>(totalKeyframes);
		auto pComp = std::make_unique<sKeyframe*[]>(totalKeyframes);

		for (int i = 0; i < compNum; ++i) {
			pKfrNum[i] = kfrNum;
		}

		auto pKfr = std::make_unique<sKeyframe[]>(totalKeyframes);
		sKeyframe* p = pKfr.get();
		for (int i = 0; i < compNum; ++i) {
			pComp[i] = p;
			p += pKfrNum[i];
		}

		switch (chType) {
		case 's':
			load_keys(node.mScalingKeys, kfrNum, compNum, pComp.get());
			break;
		case 'r':
			load_keys(node.mRotationKeys, kfrNum, compNum, pComp.get());
			break;
		case 't':
			load_keys(node.mPositionKeys, kfrNum, compNum, pComp.get());
			break;
		}
		
		ch.mpKeyframesNum = pKfrNum.release();
		ch.mpComponents = pComp.release();
		pKfr.release();
		ch.mComponentsNum = compNum;
		ch.mType = type;
		ch.mExpr = expr;
		ch.mRotOrd = rord;
		ch.mName = std::move(name);
		ch.mSubname = std::move(subName);

		return true;
	}

	template <typename T>
	void load_keys(T const* pKeys, int kfrNum, int compNum, sKeyframe** pComp) {
		for (int i = 0; i < kfrNum; ++i) {
			auto const& k = pKeys[i];

			for (int j = 0; j < compNum; ++j) {
				auto& p = pComp[j][i];
				p.frame = (float)k.mTime;
				p.value = get_value(k, j);
				p.inSlope = 0.0f;
				p.outSlope = 0.0f;
			}
		}
	}

	static float get_value(aiVectorKey const& k, int idx) {
		return (float)k.mValue[idx];
	}
	static float get_value(aiQuatKey const& k, int idx) {
		switch (idx) {
		case 0: return k.mValue.x;
		case 1: return k.mValue.y;
		case 2: return k.mValue.z;
		case 3: return k.mValue.w;
		}
		return 0.0f;
	}
};

class cAnimListJsonLoader {
	cstr mPath;
	cAnimationDataList& mList;
public:
	cAnimListJsonLoader(cAnimationDataList& list, cstr path) : mPath(path), mList(list) {}
	bool operator()(Value const& doc) {
		CHECK_SCHEMA(doc.IsArray(), "doc is not an array\n");
		Size count = doc.Size();
		
		char buf[256];
		auto pAdata = std::make_unique<cAnimationData[]>(count);
		std::unordered_map<std::string, int32_t> map;
		int anim = 0;

		for (Size i = 0; i < count; ++i) {
			auto& rec = doc[i];
			CHECK_SCHEMA(rec.HasMember("name"), "rec has no name\n");
			auto& n = rec["name"];
			CHECK_SCHEMA(rec.HasMember("fname"), "rec has no fname\n");
			auto& fn = rec["fname"];

			std::string fname(fn.GetString(), fn.GetStringLength());

			::sprintf_s(buf, "%s/%s", mPath.p, fname.c_str());
			if (pAdata[anim].load(buf)) {
				map[pAdata[anim].mName] = anim;
				anim++;
			}
		}

		mList.mpList = pAdata.release();
		mList.mCount = anim;
		mList.mMap = std::move(map);

		return true;
	}
};

cChannel::~cChannel() {
	delete[] mpKeyframesNum;
	if (mpComponents)
		delete[] mpComponents[0];
	delete[] mpComponents;
}



void cChannel::eval(DirectX::XMVECTOR& vec, float frame) const {
	sKeyframe const* pKfrA = nullptr;
	sKeyframe const* pKfrB = nullptr;

	DirectX::XMVECTOR a = vec;
	DirectX::XMVECTOR b = vec;
	DirectX::XMVECTOR left = DirectX::g_XMZero;
	DirectX::XMVECTOR right = DirectX::g_XMZero;
	DirectX::XMVECTOR t = DirectX::g_XMZero;

	bool interpolate = false;

	for (int i = 0; i < mComponentsNum && i < 4; ++i) {
		find_keyframe(i, frame, pKfrA, pKfrB);
		a.m128_f32[i] = pKfrA->value;
		b.m128_f32[i] = pKfrB->value;
		left.m128_f32[i] = pKfrA->outSlope;
		right.m128_f32[i] = pKfrB->inSlope;

		if (pKfrA != pKfrB) {
			interpolate = true; 

			float fa = pKfrA->frame;
			float fb = pKfrB->frame;

			float tt = (frame - fa) / (fb - fa);
			//t = clamp(t, 0.0f, 1.0f);
			
			t.m128_f32[i] = tt;
		}
	}

	if (!interpolate) {
		if (mType == cChannel::E_CH_EULER) {
			a = euler_xyz_to_quat(a);
		}
		vec = a;
		return;
	}

	switch (mType)
	{
	case cChannel::E_CH_EULER:
		t = DirectX::XMVectorSplatX(t);
		a = euler_xyz_to_quat(a);
		b = euler_xyz_to_quat(b);
		break;
	case cChannel::E_CH_QUATERNION:
		t = DirectX::XMVectorSplatX(t);
		break;
	}

	switch (mExpr) {
	case E_EXPR_CONSTANT:
		vec = a;
		break;
	case E_EXPR_LINEAR:
		vec = DirectX::XMVectorLerpV(a, b, t);
		break;
	case E_EXPR_CUBIC:
		vec = hermite(a, left, b, right, t);
		break;
	case E_EXPR_QLINEAR:
		vec = DirectX::XMQuaternionSlerpV(a, b, t);
		break;
	}

}

void cChannel::find_keyframe(int comp, float frame, sKeyframe const*& pKfrA, sKeyframe const*& pKfrB) const {
	int kfrNum = mpKeyframesNum[comp];
	sKeyframe const* pKeyframes = mpComponents[comp];
	auto pEnd = pKeyframes + kfrNum - 1;
	
	// Modified binary search to correctly find the frame inbetween keyframes or
	// to find frame outside of keyframes.

	auto first = pKeyframes;
	auto last = pEnd;
	while (last - first > 1) {
		auto mid = first + (last - first) / 2;
		if (mid->frame < frame) {
			first = mid;
		}
		else {
			last = mid;
		}
	}

	if (frame <= first->frame) {
		pKfrA = first;
		pKfrB = first;
	}
	else if (frame < last->frame) {
		pKfrA = first;
		pKfrB = last;
	}
	else {
		pKfrA = last;
		pKfrB = last;
	}
}

cAnimationData::~cAnimationData() {
	delete[] mpChannels;
}

bool cAnimationData::load(cstr filepath) {
	if (!filepath.ends_with(".anim")) {
		dbg_msg("Unknown animation file extension <%s>", filepath.p);
		return false;
	}

	cAnimJsonLoaderImpl loader(*this);
	return nJsonHelpers::load_file(filepath, loader);
}

bool cAnimationData::load(aiAnimation const& anim) {
	cAnimAssimpLoaderImpl loader(*this);
	return loader(anim);
}

cAnimation::~cAnimation() {
	delete[] mpLinks;
}

void cAnimation::init(cAnimationData const& animData, cRigData const& rigData) {
	auto pLinks = std::make_unique<sLink[]>(animData.mChannelsNum);

	int linksNum = 0;
	for (int i = 0; i < animData.mChannelsNum; ++i) {
		auto const& name = animData.mpChannels[i].mName;
		int idx = rigData.find_joint_idx(name.c_str());
		if (idx != -1) {
			pLinks[linksNum].chIdx = i;
			pLinks[linksNum].jntIdx = idx;
			linksNum++;
		}
	}

	mpAnimData = &animData;
	mpRigData = &rigData;
	mpLinks = pLinks.release();
	mLinksNum = linksNum;
}

void cAnimation::eval(cRig& rig, float frame) const {
	for (int i = 0; i < mLinksNum; ++i) {
		auto chIdx = mpLinks[i].chIdx;
		auto jntIdx = mpLinks[i].jntIdx;

		auto& ch = mpAnimData->mpChannels[chIdx];
		auto jnt = rig.get_joint(jntIdx);

		auto& xform = jnt->get_xform();

		if (ch.mSubname[0] == 't') {
			ch.eval(xform.mPos, frame);
		}
		else if (ch.mSubname[0] == 'r') {
			ch.eval(xform.mQuat, frame);
		}

		
	}
}


cAnimationDataList::~cAnimationDataList() {
	delete[] mpList;
}

bool cAnimationDataList::load(cstr path, cstr filename) {
	char buf[256];
	::sprintf_s(buf, "%s/%s", path.p, filename.p);
	cAnimListJsonLoader loader(*this, path);
	return nJsonHelpers::load_file(buf, loader);
}

bool cAnimationDataList::load(cAssimpLoader& loader) {
	auto pScene = loader.get_scene();
	if (!pScene) { return false; }

	if (!pScene->HasAnimations()) { return false; }

	uint32_t count = pScene->mNumAnimations;

	auto pAdata = std::make_unique<cAnimationData[]>(count);
	std::unordered_map<std::string, int32_t> map;
	int anim = 0;

	for (uint32_t i = 0; i < count; ++i) {
		auto const& pA = pScene->mAnimations[i];
		if (pAdata[anim].load(*pA)) {
			map[pAdata[anim].mName] = anim;
			anim++;
		}
	}

	mpList = pAdata.release();
	mCount = anim;
	mMap = std::move(map);

	return true;
}


cAnimationList::~cAnimationList() {
	delete[] mpList;
}

void cAnimationList::init(cAnimationDataList const& dataList, cRigData const& rigData) {
	int32_t count = dataList.get_count();
	if (count == 0) { return; }
	auto pList = std::make_unique<cAnimation[]>(count);

	for (int32_t i = 0; i < count; ++i) {
		auto& data = dataList[i];
		pList[i].init(data, rigData);
	}

	mpList = pList.release();
	mCount = count;
	mpDataList = &dataList;
}
