#include <memory>

#include "math.hpp"
#include "common.hpp"
#include "dbg_ui.hpp"
#include "rdr/rdr.hpp"
#include "res/path_helpers.hpp"
#include "rdr/texture.hpp"
#include "rdr/model.hpp"
#include "rdr/camera.hpp"
#include "rdr/sh.hpp"
#include "rdr/light.hpp"
#include "scene/scene_components.hpp"

#include <entt/entt.hpp>

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wunused-local-typedef")
CLANG_DIAG_IGNORE("-Wunused-private-field")
CLANG_DIAG_IGNORE("-Wexceptions")
#include <cereal/archives/json.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
CLANG_DIAG_POP


#include <fstream>

#define ARC(x) do { auto nvp = (x); try { arc(nvp); } catch (cereal::Exception& ex) { dbg_msg("%s: %s\n", nvp.name, ex.what()); } } while(0)
#define ARCD(x, def) \
	do { \
		auto&& nvp = (x); \
		try { arc(nvp); } \
		catch (cereal::Exception& ex) { \
			nvp.value = (def); \
			dbg_msg("%s: %s\n", nvp.name, ex.what()); \
		} \
	} while(0)

template <class T>
static bool save_to_json(T& object, const fs::path& filepath) {
	std::ofstream ofs(filepath, std::ios::binary);
	if (!ofs.is_open()) { return false; }

	cereal::JSONOutputArchive arc(ofs);
	arc(object);

	return true;
}

template <class T>
static bool load_from_json(T& object, const fs::path& filepath) {
	std::ifstream ifs(filepath, std::ios::binary);
	if (!ifs.is_open()) { return false; }

	cereal::JSONInputArchive arc(ifs);
	arc(object);

	return true;
}

//template <class Archive, typename NVP>
//void ARCD(Archive& arc, NVP&& nvp, typename NVP::Value const& defVal) try {
//	arc(std::forward(nvp));
//} catch (cereal::Exception& ex) {
//	nvp.value = defVal;
//	dbg_msg("%s: %s\n", nvp.name, ex.what());
//}

// todo: serialziation of XMVECTOR doesn't work with clang-cl
//template <class Archive>
//void save(Archive& arc, DirectX::XMVECTOR const& m) {
//	DirectX::XMVECTORF32 tmp;
//	tmp.v = m;
//	arc(tmp.f);
//}
//
//template <class Archive>
//void load(Archive& arc, DirectX::XMVECTOR& m) {
//	DirectX::XMVECTORF32 tmp;
//	arc(tmp.f);
//	m = tmp.v;
//}


void load(cereal::JSONInputArchive& arc, fs::path& p) {
	std::string str;
	arc(str);
	p = fs::path(str);
}

void save(cereal::JSONOutputArchive& arc, fs::path const& p) {
	std::string str = p.u8string();
	arc(str);
}

// TODO: why cereal cannot find fs::path save/load functions?
struct sPathWrapper {
	fs::path& p;
};

template<class Archive>
void load(Archive& arc, sPathWrapper& p) {
	load(arc, p.p);
}

template<class Archive>
void save(Archive& arc, sPathWrapper const& p) {
	save(arc, p.p);
}
#define FILEPATH_NVP(p) ::cereal::make_nvp(#p, sPathWrapper{p})


template<class Archive>
void serialize(Archive& arc, vec4& v)
{
	arc(v[0], v[1], v[2], v[3]);
}


template <class Archive, class T>
void serialize(Archive& arc, tvec2<T>& vec2) {
	arc(vec2.x, vec2.y);
}

template <class Archive>
void serialize(Archive& arc, sXform& xform)
{
	vec4 pos, quat, scl;
	if (Archive::is_saving::value)
	{
		DirectX::XMStoreFloat4(&pos.mVal, xform.mPos);
		DirectX::XMStoreFloat4(&quat.mVal, xform.mQuat);
		DirectX::XMStoreFloat4(&scl.mVal, xform.mScale);
	}
	ARC(CEREAL_NVP(pos));
	ARC(CEREAL_NVP(quat));
	ARC(CEREAL_NVP(scl));
	if (Archive::is_loading::value)
	{
		xform.mPos = DirectX::XMLoadFloat4(&pos.mVal);
		xform.mQuat = DirectX::XMLoadFloat4(&quat.mVal);
		xform.mScale = DirectX::XMLoadFloat4(&scl.mVal);
	}
}

// todo: why DirectX::XMFLOAT4X4 doesn't work directly?
struct sMatrixWrapper {
	using MtxArray = std::array<float, 16>;
	MtxArray& arr;

	sMatrixWrapper(DirectX::XMFLOAT4X4& mtx)
		: arr(reinterpret_cast<MtxArray&>(*&mtx.m[0][0]))
	{}

	template<class Archive>
	void serialize(Archive& arc) {
		arc(arr);
	}
};
//template<class Archive>
//void serialize(Archive& arc, DirectX::XMFLOAT4X4& mtx)
//{
//	using MtxArray = std::array<float, 16>;
//	MtxArray& arr = reinterpret_cast<MtxArray&>(*&mtx.m[0][0]);
//	arc(arr);
//}

template <class Archive>
void sTestMtlCBuf::serialize(Archive& arc) {
	ARC(CEREAL_NVP(fresnel));
	ARC(CEREAL_NVP(shin));
	ARCD(CEREAL_NVP(nmap0Power), 1.0f);
	ARCD(CEREAL_NVP(nmap1Power), 1.0f);
}

template <class Archive>
void sGroupMaterial::serialize(Archive& arc) {
	ARC(CEREAL_NVP(texBaseName));
	ARC(CEREAL_NVP(texNmap0Name));
	ARC(CEREAL_NVP(texNmap1Name));
	ARC(CEREAL_NVP(texMaskName));
	ARC(CEREAL_NVP(vsProg));
	ARC(CEREAL_NVP(psProg));
	ARCD(CEREAL_NVP(twosided), false);
	ARC(CEREAL_NVP(params));
}

template <size_t size>
static int build_grp_name_tag(char(&buf)[size], cstr name) {
	return ::sprintf_s(buf, "grp:%s", name.p);
}

template <class Archive>
void serialize(Archive& arc, cModelMaterial& m) {
	char buf[64];
	auto grpNum = m.mpMdlData->mGrpNum;
	for (uint32_t i = 0; i < grpNum; ++i) {
		sGroupMaterial& mtl = m.mpGrpMtl[i];
		build_grp_name_tag(buf, m.get_grp_name(i));
		ARC(cereal::make_nvp(buf, mtl));
	}
}

bool cModelMaterial::deserialize(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}

bool cModelMaterial::serialize(const fs::path& filepath) const {
	return save_to_json(*this, filepath);
}


template <class Archive>
void cCamera::serialize(Archive& arc) {
	// todo: serialziation of XMVECTOR doesn't work with clang-cl
	vec4 mPos;
	vec4 mTgt;
	vec4 mUp;
	if (Archive::is_saving::value)
	{
		DirectX::XMStoreFloat4(&mPos.mVal, this->mPos);
		DirectX::XMStoreFloat4(&mTgt.mVal, this->mTgt);
		DirectX::XMStoreFloat4(&mUp.mVal, this->mUp);
	}
	ARC(CEREAL_NVP(mPos));
	ARC(CEREAL_NVP(mTgt));
	ARC(CEREAL_NVP(mUp));
	if (Archive::is_loading::value)
	{
		this->mPos = DirectX::XMLoadFloat4(&mPos.mVal);
		this->mTgt = DirectX::XMLoadFloat4(&mTgt.mVal);
		this->mUp = DirectX::XMLoadFloat4(&mUp.mVal);
	}

	ARC(CEREAL_NVP(mFovY));
	ARC(CEREAL_NVP(mAspect));
	ARC(CEREAL_NVP(mNearZ));
	ARC(CEREAL_NVP(mFarZ));
}

bool cCamera::load(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}

bool cCamera::save(const fs::path& filepath) {
	return save_to_json(*this, filepath);
	return false;
}


template <class Archive>
void sWindowSettings::serialize(Archive& arc) {
	ARC(CEREAL_NVP(mSize));
}

bool sWindowSettings::load(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}

bool sWindowSettings::save(const fs::path& filepath) {
	return save_to_json(*this, filepath);
}


template <class Archive>
void sSHCoef::sSHChan::serialize(Archive& arc) {
	ARC(CEREAL_NVP(mSH));
}

template <class Archive>
void sSHCoef::serialize(Archive& arc) {
	ARC(CEREAL_NVP(mR));
	ARC(CEREAL_NVP(mG));
	ARC(CEREAL_NVP(mB));
}

template <class Archive>
void sLightPoint::serialize(Archive& arc) {
	vec4 def = { { 1.0f, 1.0f, 1.0f, 1.0f } };
	ARCD(CEREAL_NVP(pos), def);
	ARCD(CEREAL_NVP(clr), def);
	ARCD(CEREAL_NVP(enabled), true);
}

template <class Archive>
void cLightMgr::serialize(Archive& arc) {
	ARC(CEREAL_NVP(mPointLights));
	ARC(CEREAL_NVP(mSH));
}


bool cLightMgr::load(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}


bool cLightMgr::save(const fs::path& filepath) {
	return save_to_json(*this, filepath);
}


template <class Archive>
void sPositionCompParams::serialize(Archive& arc) {
	ARC(CEREAL_NVP(xform));
}


template <class Archive>
void sMoveCompParams::serialize(Archive& arc) {
	ARC(CEREAL_NVP(pos));
	ARC(CEREAL_NVP(rad));
}


template <class Archive>
void sModelCompParams::serialize(Archive& arc) {
	ARC(FILEPATH_NVP(modelPath));
	ARC(FILEPATH_NVP(materialPath));
	sMatrixWrapper xform(localXform);
	ARC(CEREAL_NVP(xform));
}

template <class Archive>
void sRiggedModelCompParams::serialize(Archive& arc) {
	Base::serialize(arc);
	ARC(FILEPATH_NVP(rigPath));
}

template <class Archive>
void sFbxRiggedModelParams::serialize(Archive& arc) {
	ARC(FILEPATH_NVP(modelPath));
	ARC(FILEPATH_NVP(materialPath));
	sMatrixWrapper xform(localXform);
	ARC(CEREAL_NVP(xform));
}

template <class Archive>
void sAnimationCompParams::serialize(Archive& arc) {
	ARC(FILEPATH_NVP(animRootPath));
	ARC(FILEPATH_NVP(animListName));
	ARC(CEREAL_NVP(curAnim));
	ARC(CEREAL_NVP(speed));
}



template <typename Params>
template <class Archive>
void sParamList<Params>::serialize(Archive& arc) {
	ARC(CEREAL_NVP(paramList));
	ARC(CEREAL_NVP(entityList));
}


template <class Archive>
void sSceneSnapshot::serialize(Archive& arc) {
	ARC(CEREAL_NVP(entityIds));
	ARC(CEREAL_NVP(paramsOrder));

	invoke_for_params([&](entt::id_type t, auto* pList) { 
		std::string name = "id_" + std::to_string(t);
		ARC(cereal::make_nvp(name, *pList));
	});
}

bool sSceneSnapshot::load(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}
bool sSceneSnapshot::save(const fs::path& filepath) {
	return save_to_json(*this, filepath);
}



template <class Archive>
void sDbgTools::serialize(Archive& arc) {
	ARC(CEREAL_NVP(scene_editor));
	ARC(CEREAL_NVP(update_queue));
	ARC(CEREAL_NVP(light_mgr));
	ARC(CEREAL_NVP(imgui_demo));
}

bool sDbgTools::load(const fs::path& filepath) {
	return load_from_json(*this, filepath);
}

bool sDbgTools::save(const fs::path& filepath) {
	return save_to_json(*this, filepath);
}

