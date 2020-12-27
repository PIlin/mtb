#pragma once

#include <memory>
#include <string>

#include "resource.hpp"

#include "cbufs.hpp"

struct sModelVtx;
class cShader;
class cAssimpLoader;
class cRdrContext;
class cTexture;

struct sGroup {
	uint32_t mVtxOffset;
	uint32_t mIdxOffset;
	uint32_t mIdxCount;
	uint32_t mPolyType;
};

class cModelData : public sResourceBase {
public:
	uint32_t mGrpNum;
	std::unique_ptr<sGroup[]> mpGroups;
	std::unique_ptr<std::string[]> mpGrpNames;

	cVertexBuffer mVtx;
	cIndexBuffer mIdx;

public:
	static ResourceTypeId type_id();

	cModelData() = default;

	bool load(const fs::path& filepath);
	void unload();

	bool load_assimp(const fs::path& filepath);
	bool load_assimp(cAssimpLoader& loader);
	bool load_hou_geo(const fs::path& filepath);
};

DEF_RES_PTR(cModelData, ModelDataPtr)


struct sGroupMaterial {
	sTestMtlCBuf params;
	std::string vsProg;
	std::string psProg;
	std::string texBaseName;
	std::string texNmap0Name;
	std::string texNmap1Name;
	std::string texMaskName;
	bool twosided;
public:
	void apply(cRdrContext const& rdrCtx) const;
	void set_default(bool isSkinned);

	template <class Archive>
	void serialize(Archive& arc);
};

struct sGroupMtlRes {
	cTexture* mpTexBase = nullptr;
	ID3D11SamplerState* mpSmpBase = nullptr;
	cTexture* mpTexNmap0 = nullptr;
	ID3D11SamplerState* mpSmpNmap0 = nullptr;
	cTexture* mpTexNmap1 = nullptr;
	ID3D11SamplerState* mpSmpNmap1 = nullptr;
	cTexture* mpTexMask = nullptr;
	ID3D11SamplerState* mpSmpMask = nullptr;
	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	ID3D11RasterizerState* mpRSState = nullptr;

	void apply(cRdrContext const& rdrCtx);
};

class cModelMaterial : public sResourceBase {
public:
	ConstModelDataPtr mpMdlData = nullptr;
	std::unique_ptr<sGroupMaterial[]> mpGrpMtl;
	std::unique_ptr<sGroupMtlRes[]> mpGrpRes;

	fs::path mFilepath;

	static ResourceTypeId type_id();

	cModelMaterial() {}
	cModelMaterial(cModelMaterial&& o) :
		mpMdlData(o.mpMdlData),
		mpGrpMtl(std::move(o.mpGrpMtl))
	{}
	cModelMaterial& operator=(cModelMaterial&& o) {
		mpMdlData = o.mpMdlData;
		mpGrpMtl = std::move(o.mpGrpMtl);
		return *this;
	}

	void apply(cRdrContext const& rdrCtx, int grp) const;

	bool load(ID3D11Device* pDev, const ConstModelDataPtr& pMdlData, 
		const fs::path& filepath, bool isSkinnedByDef = false);
	bool save(const fs::path& filepath = fs::path()) const;

	cstr get_grp_name(uint32_t i) const { return mpMdlData->mpGrpNames[i].c_str(); }

protected:
	bool serialize(const fs::path& filepath) const;
	bool deserialize(const fs::path& filepath);
};

DEF_RES_PTR(cModelMaterial, ModelMaterialPtr)

class cModel {
	ConstModelDataPtr mpData;
	ConstModelMaterialPtr mpMtl;

	com_ptr<ID3D11InputLayout> mpIL;

public:
	cModel() = default;
	cModel(cModel&&) = default;
	cModel& operator=(cModel&&) = default;
	~cModel() = default;

	bool init(ConstModelDataPtr& pMdlData, ConstModelMaterialPtr& pMtl);
	void deinit();

	void disp(cRdrContext const& rdrCtx, const DirectX::XMMATRIX& wmtx) const;

	void dbg_ui();
};


