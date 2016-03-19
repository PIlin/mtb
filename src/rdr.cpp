#include <d3d11.h>

#include "common.hpp"
#include "math.hpp"
#include "rdr.hpp"
#include "gfx.hpp"

cBufferBase::cMapHandle::cMapHandle(ID3D11DeviceContext* pCtx, ID3D11Buffer* pBuf) : mpCtx(pCtx), mpBuf(pBuf), mMSR() {
	HRESULT hr = pCtx->Map(pBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mMSR);
	if (SUCCEEDED(hr)) {
	}
}

cBufferBase::cMapHandle::~cMapHandle() {
	if (is_mapped()) {
		mpCtx->Unmap(mpBuf, 0);
		mMSR.pData = nullptr;
	}
}

cBufferBase::cMapHandle cBufferBase::map(ID3D11DeviceContext* pCtx) {
	return cMapHandle(pCtx, mpBuf);
}

void cBufferBase::init_immutable(ID3D11Device* pDev, void const* pData, uint32_t size, D3D11_BIND_FLAG bind) {
	auto desc = D3D11_BUFFER_DESC();
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = bind;

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = pData;
	initData.SysMemPitch = 0;
	initData.SysMemSlicePitch = 0;

	HRESULT hr = pDev->CreateBuffer(&desc, &initData, mpBuf.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateBuffer immutable failed");
}

void cBufferBase::init_write_only(ID3D11Device* pDev, uint32_t size, D3D11_BIND_FLAG bind) {
	auto desc = D3D11_BUFFER_DESC();
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = bind;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = pDev->CreateBuffer(&desc, nullptr, mpBuf.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateBuffer mutable write-only failed");
}

void cConstBufferBase::init(ID3D11Device* pDev, size_t size) {
	init_write_only(pDev, (uint32_t)size, D3D11_BIND_CONSTANT_BUFFER);
}

void cConstBufferBase::update(ID3D11DeviceContext* pCtx, void const* pData, size_t size) {
	auto m = map(pCtx);
	if (m.is_mapped()) {
		::memcpy(m.data(), pData, size);
	}
}

void cVertexBuffer::init(ID3D11Device* pDev, void const* pVtxData, uint32_t vtxCount, uint32_t vtxSize) {
	init_immutable(pDev, pVtxData, vtxCount * vtxSize, D3D11_BIND_VERTEX_BUFFER);
	mVtxSize = vtxSize;
	mVtxCount = vtxCount;
}

void cVertexBuffer::init_write_only(ID3D11Device* pDev, uint32_t vtxCount, uint32_t vtxSize) {
	cBufferBase::init_write_only(pDev, vtxCount * vtxSize, D3D11_BIND_VERTEX_BUFFER);
	mVtxSize = vtxSize;
	mVtxCount = vtxCount;
}

void cVertexBuffer::set(ID3D11DeviceContext* pCtx, uint32_t slot, uint32_t offset) const {
	pCtx->IASetVertexBuffers(slot, 1, &mpBuf.p, &mVtxSize, &offset);
}

void cIndexBuffer::init(ID3D11Device* pDev, void const* pIdxData, uint32_t idxCount, DXGI_FORMAT format) {
	uint32_t size = 0;
	switch (format) {
	case DXGI_FORMAT_R16_UINT: size = 2 * idxCount; break;
	case DXGI_FORMAT_R32_UINT: size = 4 * idxCount; break;
	}

	init_immutable(pDev, pIdxData, size, D3D11_BIND_INDEX_BUFFER);
	mFormat = format;
	mIdxCount = idxCount;
}

void cIndexBuffer::init_write_only(ID3D11Device* pDev, uint32_t idxCount, DXGI_FORMAT format) {
	uint32_t size = 0;
	switch (format) {
	case DXGI_FORMAT_R16_UINT: size = 2 * idxCount; break;
	case DXGI_FORMAT_R32_UINT: size = 4 * idxCount; break;
	}

	cBufferBase::init_write_only(pDev, size, D3D11_BIND_INDEX_BUFFER);
	mFormat = format;
	mIdxCount = idxCount;
}

void cIndexBuffer::set(ID3D11DeviceContext* pCtx, uint32_t offset) const {
	pCtx->IASetIndexBuffer(mpBuf, mFormat, offset);
}


cSamplerStates::cSamplerStates(ID3D11Device* pDev) {
	HRESULT hr;
	hr = pDev->CreateSamplerState(&linear_desc(), mpLinear.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateSamplerState linear failed");
}

D3D11_SAMPLER_DESC cSamplerStates::linear_desc() {
	auto desc = D3D11_SAMPLER_DESC();

	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	//desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	//desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0;
	//desc.MinLOD = 0;
	desc.MaxLOD = D3D11_FLOAT32_MAX;

	return desc;
}


cBlendStates::cBlendStates(ID3D11Device* pDev) {
	HRESULT hr;
	hr = pDev->CreateBlendState(&opaque_desc(), mpOpaque.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateBlendState opaque failed");
	hr = pDev->CreateBlendState(&imgui_desc(), mpImgui.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateBlendState imgui failed");
}

void cBlendStates::set(ID3D11DeviceContext* pCtx, ID3D11BlendState* pState) {
	pCtx->OMSetBlendState(pState, nullptr, 0xFFFFFFFF);
}

D3D11_BLEND_DESC cBlendStates::opaque_desc() {
	auto desc = D3D11_BLEND_DESC();
	desc.RenderTarget[0].BlendEnable = false;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}


D3D11_BLEND_DESC cBlendStates::imgui_desc() {
	auto desc = D3D11_BLEND_DESC();
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	return desc;
}


cRasterizerStates::cRasterizerStates(ID3D11Device* pDev) {
	HRESULT hr;
	hr = pDev->CreateRasterizerState(&default_desc(), mpDefault.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateRasterizerState default failed");

	hr = pDev->CreateRasterizerState(&twosided_desc(), mpTwosided.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateRasterizerState twosided failed");

	hr = pDev->CreateRasterizerState(&imgui_desc(), mpImgui.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateRasterizerState imgui failed");
}

void cRasterizerStates::set(ID3D11DeviceContext* pCtx, ID3D11RasterizerState* pState) {
	pCtx->RSSetState(pState);
}

D3D11_RASTERIZER_DESC cRasterizerStates::default_desc() {
	auto desc = D3D11_RASTERIZER_DESC();
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_BACK;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthClipEnable = TRUE;
	desc.ScissorEnable = FALSE;
	desc.MultisampleEnable = TRUE;
	desc.AntialiasedLineEnable = FALSE;
	return desc;
}

D3D11_RASTERIZER_DESC cRasterizerStates::twosided_desc() {
	auto desc = D3D11_RASTERIZER_DESC();
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_NONE;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthClipEnable = TRUE;
	desc.ScissorEnable = FALSE;
	desc.MultisampleEnable = TRUE;
	desc.AntialiasedLineEnable = FALSE;
	return desc;
}

D3D11_RASTERIZER_DESC cRasterizerStates::imgui_desc() {
	auto desc = D3D11_RASTERIZER_DESC();
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_NONE;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthClipEnable = TRUE;
	desc.ScissorEnable = TRUE;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;
	return desc;
}


cDepthStencilStates::cDepthStencilStates(ID3D11Device* pDev) {
	HRESULT hr;
	hr = pDev->CreateDepthStencilState(&default_desc(), mpDefault.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateDepthStencilState default failed");

	hr = pDev->CreateDepthStencilState(&imgui_desc(), mpImgui.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateDepthStencilState imgui failed");
}

void cDepthStencilStates::set(ID3D11DeviceContext* pCtx, ID3D11DepthStencilState* pState) {
	pCtx->OMSetDepthStencilState(pState, 0);
}

D3D11_DEPTH_STENCIL_DESC cDepthStencilStates::default_desc() {
	auto desc = D3D11_DEPTH_STENCIL_DESC();
	desc.DepthEnable = true;
	desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D11_COMPARISON_LESS;
	desc.StencilEnable = false;
	return desc;
}
D3D11_DEPTH_STENCIL_DESC cDepthStencilStates::imgui_desc() {
	auto desc = D3D11_DEPTH_STENCIL_DESC();
	desc.DepthEnable = false;
	desc.StencilEnable = false;
	return desc;
}

