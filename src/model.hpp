#include <memory>
#include <string>

struct sModelVtx;
class cShader;
class cAssimpLoader;

struct sGroup {
	uint32_t mVtxOffset;
	uint32_t mIdxOffset;
	uint32_t mIdxCount;
	uint32_t mPolyType;
};

class cModelData : noncopyable {
public:
	uint32_t mGrpNum;
	std::unique_ptr<sGroup[]> mpGroups;
	std::unique_ptr<std::string[]> mpGrpNames;

	cVertexBuffer mVtx;
	cIndexBuffer mIdx;

public:
	cModelData() {}
	cModelData(cModelData&& o) : 
		mpGroups(std::move(o.mpGroups)),
		mpGrpNames(std::move(o.mpGrpNames)),
		mVtx(std::move(o.mVtx)),
		mIdx(std::move(o.mIdx))
	{}
	cModelData& operator=(cModelData&& o) {
		mpGroups = std::move(o.mpGroups);
		mpGrpNames = std::move(o.mpGrpNames);
		mVtx = std::move(o.mVtx);
		mIdx = std::move(o.mIdx);
		return *this;
	}

	//cModelData& operator=(cModelData&) = delete;

	bool load(const fs::path& filepath);
	void unload();

	bool load_assimp(const fs::path& filepath);
	bool load_assimp(cAssimpLoader& loader);
	bool load_hou_geo(const fs::path& filepath);
};


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
	void apply(ID3D11DeviceContext* pCtx) const;
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

	void apply(ID3D11DeviceContext* pCtx);
};

class cModelMaterial {
public:
	cModelData const* mpMdlData = nullptr;
	std::unique_ptr<sGroupMaterial[]> mpGrpMtl;
	std::unique_ptr<sGroupMtlRes[]> mpGrpRes;

	fs::path mFilepath;

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

	void apply(ID3D11DeviceContext* pCtx, int grp);

	bool load(ID3D11Device* pDev, cModelData const& mdlData, 
		const fs::path& filepath, bool isSkinnedByDef = false);
	bool save(const fs::path& filepath = fs::path());

	cstr get_grp_name(uint32_t i) const { return mpMdlData->mpGrpNames[i].c_str(); }

protected:
	bool serialize(const fs::path& filepath);
	bool deserialize(const fs::path& filepath);
};

class cModel {
	cModelData const* mpData = nullptr;
	cModelMaterial* mpMtl = nullptr;

	com_ptr<ID3D11InputLayout> mpIL;

public:
	DirectX::XMMATRIX mWmtx;
	
	cModel() {}
	~cModel() {}

	bool init(cModelData const& mdlData, cModelMaterial& mtl);
	void deinit();

	void disp();

	void dbg_ui();
};
