#pragma once

#define NOMINMAX
#include <d3d11.h>

#include "rdr_buf.hpp"
#include "math.hpp"

class cConstBufStorage;

struct sSimpleVtx {
	float mPos[3];
	float mClr[4];
};

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


class cRdrContext {
	ID3D11DeviceContext* mpCtx = nullptr;
	cConstBufStorage& mConstBufStorage;

public:
	static cRdrContext create_imm();

	cRdrContext(ID3D11DeviceContext* pCtx, cConstBufStorage& cbufs)
		: mpCtx(pCtx)
		, mConstBufStorage(cbufs)
	{}

	ID3D11DeviceContext* get_ctx() const { return mpCtx; }
	cConstBufStorage& get_cbufs() const { return mConstBufStorage; }
};


