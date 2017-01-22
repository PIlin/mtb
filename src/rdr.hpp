#define NOMINMAX
#include <d3d11.h>


struct sModelVtx {
	vec3 pos;
	vec3 nrm;
	vec2f uv;
	vec4 tgt;
	vec3 bitgt;
	vec2f uv1;
	vec3 clr;
	vec4i jidx;
	vec4 jwgt;
};

struct sCameraCBuf {
	DirectX::XMMATRIX viewProj;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMVECTOR camPos;
};

struct sMeshCBuf {
	DirectX::XMMATRIX wmtx;
};

struct sImguiCameraCBuf {
	DirectX::XMMATRIX proj;
};

struct sLightCBuf {
	struct Bool {
		uint32_t val;
		uint32_t pad[3];

		Bool() = default;
		Bool(bool v) : val(v) {}
		operator bool() { return !!val; }
	};
	enum {MAX_LIGHTS = 8};

	DirectX::XMVECTOR pos[MAX_LIGHTS];
	DirectX::XMVECTOR clr[MAX_LIGHTS];
	DirectX::XMVECTOR dir[MAX_LIGHTS];
	Bool isEnabled[MAX_LIGHTS];

	DirectX::XMVECTOR sh[7];
};

struct sTestMtlCBuf {
	float fresnel[3];
	float shin;
	float nmap0Power;
	float nmap1Power;
	
	float pad[2];

	template <class Archive>
	void serialize(Archive& arc);
};

struct sSkinCBuf {
	enum { MAX_SKIN_MTX = 64 };
	DirectX::XMMATRIX skin[MAX_SKIN_MTX];
};

class cBufferBase : noncopyable {
protected:
	com_ptr<ID3D11Buffer> mpBuf;
public:
	class cMapHandle : noncopyable {
		ID3D11DeviceContext* mpCtx;
		ID3D11Buffer* mpBuf;
		D3D11_MAPPED_SUBRESOURCE mMSR;
	public:
		cMapHandle(ID3D11DeviceContext* pCtx, ID3D11Buffer* pBuf);
		~cMapHandle();
		cMapHandle(cMapHandle&& o) : mpCtx(o.mpCtx), mpBuf(o.mpBuf), mMSR(o.mMSR) {
			o.mMSR.pData = nullptr;
		}
		cMapHandle& operator=(cMapHandle&& o) {
			mpCtx = o.mpCtx;
			mpBuf = o.mpBuf;
			mMSR = o.mMSR;
			o.mMSR.pData = nullptr;
			return *this;
		}
		void* data() const { return mMSR.pData; }
		bool is_mapped() const { return !!data(); }
	};

	cBufferBase() = default;
	~cBufferBase() { deinit(); }
	cBufferBase(cBufferBase&& o) : mpBuf(std::move(o.mpBuf)) {}
	cBufferBase& operator=(cBufferBase&& o) {
		mpBuf = std::move(o.mpBuf);
		return *this;
	}

	void deinit() {
		mpBuf.reset();
	}

	cMapHandle map(ID3D11DeviceContext* pCtx);

protected:
	void init_immutable(ID3D11Device* pDev,
		void const* pData, uint32_t size,
		D3D11_BIND_FLAG bind);

	void init_write_only(ID3D11Device* pDev, 
		uint32_t size, D3D11_BIND_FLAG bind);
};

class cConstBufferBase : public cBufferBase {
public:
	void set_VS(ID3D11DeviceContext* pCtx, UINT slot) {
		ID3D11Buffer* bufs[1] = { mpBuf };
		pCtx->VSSetConstantBuffers(slot, 1, bufs);
	}

	void set_PS(ID3D11DeviceContext* pCtx, UINT slot) {
		ID3D11Buffer* bufs[1] = { mpBuf };
		pCtx->PSSetConstantBuffers(slot, 1, bufs);
	}
protected:
	void init(ID3D11Device* pDev, size_t size);
	void update(ID3D11DeviceContext* pCtx, void const* pData, size_t size);
};

template <typename T>
class cConstBuffer : public cConstBufferBase {
public:
	T mData;

	void init(ID3D11Device* pDev) {
		cConstBufferBase::init(pDev, sizeof(T));
	}

	void update(ID3D11DeviceContext* pCtx) {
		cConstBufferBase::update(pCtx, &mData, sizeof(T));
	}
};

template <typename T, int slot>
class cConstBufferSlotted : public cConstBuffer<T> {
public: 
	void set_VS(ID3D11DeviceContext* pCtx) {
		cConstBuffer<T>::set_VS(pCtx, slot);
	}
	void set_PS(ID3D11DeviceContext* pCtx) {
		cConstBuffer<T>::set_PS(pCtx, slot);
	}
};

class cVertexBuffer : public cBufferBase {
	uint32_t mVtxSize = 0;
	uint32_t mVtxCount = 0;
public:
	cVertexBuffer() = default;
	cVertexBuffer(cVertexBuffer&& o) : cBufferBase(std::move(o)), mVtxSize(o.mVtxSize) {}
	cVertexBuffer& operator=(cVertexBuffer&& o) {
		mVtxSize = o.mVtxSize;
		cBufferBase::operator=(std::move(o));
		return *this;
	}

	void init(ID3D11Device* pDev, void const* pVtxData, uint32_t vtxCount, uint32_t vtxSize);
	void init_write_only(ID3D11Device* pDev, uint32_t vtxCount, uint32_t vtxSize);

	void set(ID3D11DeviceContext* pCtx, uint32_t slot, uint32_t offset) const;

	uint32_t get_vtx_size() const { return mVtxSize; }
	uint32_t get_vtx_count() const { return mVtxCount; }
};

class cIndexBuffer : public cBufferBase {
	DXGI_FORMAT mFormat;
	uint32_t mIdxCount = 0;
public:
	cIndexBuffer() = default;
	cIndexBuffer(cIndexBuffer&& o) : cBufferBase(std::move(o)), mFormat(o.mFormat) {}
	cIndexBuffer& operator=(cIndexBuffer&& o) {
		mFormat = o.mFormat;
		cBufferBase::operator=(std::move(o));
		return *this;
	}

	void init(ID3D11Device* pDev, void const* pIdxData, uint32_t idxCount, DXGI_FORMAT format);
	void init_write_only(ID3D11Device* pDev, uint32_t idxCount, DXGI_FORMAT format);
	void set(ID3D11DeviceContext* pCtx, uint32_t offset) const;

	uint32_t get_idx_count() const { return mIdxCount; }
};

class cConstBufStorage {
public:
	cConstBufferSlotted<sCameraCBuf, 0> mCameraCBuf;
	cConstBufferSlotted<sMeshCBuf, 1> mMeshCBuf;
	cConstBufferSlotted<sImguiCameraCBuf, 0> mImguiCameraCBuf;
	cConstBufferSlotted<sTestMtlCBuf, 2> mTestMtlCBuf;
	cConstBufferSlotted<sLightCBuf, 3> mLightCBuf;
	cConstBufferSlotted<sSkinCBuf, 4> mSkinCBuf;

	cConstBufStorage(ID3D11Device* pDev) {
		mCameraCBuf.init(pDev);
		mMeshCBuf.init(pDev);
		mImguiCameraCBuf.init(pDev);
		mTestMtlCBuf.init(pDev);
		mLightCBuf.init(pDev);
		mSkinCBuf.init(pDev);
	}

	static cConstBufStorage& get();
};


class cSamplerStates {
	com_ptr<ID3D11SamplerState> mpLinear;
public:
	static cSamplerStates& get();

	cSamplerStates(ID3D11Device* pDev);

	ID3D11SamplerState* linear() const { return mpLinear; }

	static D3D11_SAMPLER_DESC linear_desc();
};

class cBlendStates : noncopyable {
	com_ptr<ID3D11BlendState> mpOpaque;
	com_ptr<ID3D11BlendState> mpImgui;
public:
	static cBlendStates& get();

	cBlendStates(ID3D11Device* pDev);

	ID3D11BlendState* opaque() const { return mpOpaque; }
	ID3D11BlendState* imgui() const { return mpImgui; }

	void set_opaque(ID3D11DeviceContext* pCtx) const { set(pCtx, opaque()); }
	void set_imgui(ID3D11DeviceContext* pCtx) const { set(pCtx, imgui()); }

	static void set(ID3D11DeviceContext* pCtx, ID3D11BlendState* pState);
	static D3D11_BLEND_DESC opaque_desc();
	static D3D11_BLEND_DESC imgui_desc();
};

class cRasterizerStates : noncopyable {
	com_ptr<ID3D11RasterizerState> mpDefault;
	com_ptr<ID3D11RasterizerState> mpTwosided;
	com_ptr<ID3D11RasterizerState> mpImgui;
public:
	static cRasterizerStates& get();

	cRasterizerStates(ID3D11Device* pDev);

	ID3D11RasterizerState* def() const { return mpDefault; }
	ID3D11RasterizerState* twosided() const { return mpTwosided; }
	ID3D11RasterizerState* imgui() const { return mpImgui; }

	void set_def(ID3D11DeviceContext* pCtx) const { set(pCtx, def()); }
	void set_twosided(ID3D11DeviceContext* pCtx) const { set(pCtx, twosided()); }
	void set_imgui(ID3D11DeviceContext* pCtx) const { set(pCtx, imgui()); }

	static void set(ID3D11DeviceContext* pCtx, ID3D11RasterizerState* pState);
	static D3D11_RASTERIZER_DESC default_desc();
	static D3D11_RASTERIZER_DESC twosided_desc();
	static D3D11_RASTERIZER_DESC imgui_desc();
};

class cDepthStencilStates : noncopyable {
	com_ptr<ID3D11DepthStencilState> mpDefault;
	com_ptr<ID3D11DepthStencilState> mpImgui;
public:
	static cDepthStencilStates& get();

	cDepthStencilStates(ID3D11Device* pDev);

	ID3D11DepthStencilState* def() const { return mpDefault; }
	ID3D11DepthStencilState* imgui() const { return mpImgui; }

	void set_def(ID3D11DeviceContext* pCtx) const { set(pCtx, def()); }
	void set_imgui(ID3D11DeviceContext* pCtx) const { set(pCtx, imgui()); }

	static void set(ID3D11DeviceContext* pCtx, ID3D11DepthStencilState* pState);
	static D3D11_DEPTH_STENCIL_DESC default_desc();
	static D3D11_DEPTH_STENCIL_DESC imgui_desc();
};
