#include "common.hpp"
#include "math.hpp"
#include "gfx.hpp"
#include "frame_timer.hpp"

#include "res/path_helpers.hpp"
#include "microprofile.h"

#include <d3dcompiler.h>

#include <unordered_map>

cGfx::sDev::sDev(DXGI_SWAP_CHAIN_DESC& sd, UINT flags) {
	D3D_FEATURE_LEVEL lvl;

	auto hr = ::D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		flags,
		nullptr, 0,
		D3D11_SDK_VERSION,
		&sd,
		mpSwapChain.pp(),
		mpDev.pp(),
		&lvl,
		mpImmCtx.pp());
	if (!SUCCEEDED(hr))
		throw sD3DException(hr, "D3D11CreateDeviceAndSwapChain failed");
}

cGfx::sDepthStencilBuffer::sDepthStencilBuffer(sDev& dev, uint32_t w, uint32_t h, DXGI_SAMPLE_DESC const& sampleDesc) {
	auto desc = D3D11_TEXTURE2D_DESC();
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc = sampleDesc;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	HRESULT hr = dev.mpDev->CreateTexture2D(&desc, nullptr, mpTex.pp());
	if (!SUCCEEDED(hr))
		throw sD3DException(hr, "CreateTexture2D failed: depth stencil buffer");
	
	hr = dev.mpDev->CreateDepthStencilView(mpTex, nullptr, mpDSView.pp());
	if (!SUCCEEDED(hr))
		throw sD3DException(hr, "CreateDepthStencilView failed");
}

cGfx::sRTView::sRTView(sDev& dev) {
	ID3D11Texture2D* pBack = nullptr;
	HRESULT hr = dev.mpSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBack);
	if (!SUCCEEDED(hr))
		throw sD3DException(hr, "GetBuffer failed: back buffer");

	hr = dev.mpDev->CreateRenderTargetView(pBack, nullptr, mpRTV.pp());
	pBack->Release();

	if (!SUCCEEDED(hr))
		throw sD3DException(hr, "CreateRenderTargetView failed");
}

cGfx::cGfx(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	UINT w = rc.right - rc.left;
	UINT h = rc.bottom - rc.top;

	UINT devFlags = 0;
	devFlags |= D3D11_CREATE_DEVICE_DEBUG;

	auto sd = DXGI_SWAP_CHAIN_DESC();
	sd.BufferCount = 1;
	sd.BufferDesc.Width = w;
	sd.BufferDesc.Height = h;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 4;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	mDev = sDev(sd, devFlags);
	mRTV = sRTView(mDev);
	mDS = sDepthStencilBuffer(mDev, w, h, sd.SampleDesc);
	set_default_viewport(w, h);

	apply_default_rt_vp(mDev.mpImmCtx);
}

void cGfx::on_window_size_changed(uint32_t w, uint32_t h) {
	assert(!mbInFrame);

	mDev.mpImmCtx->OMSetRenderTargets(0, nullptr, nullptr);

	mRTV = sRTView();
	mDS = sDepthStencilBuffer();

	HRESULT hr = S_OK;
	{
		const UINT buffersCount = 0; // 0 to preserve buffers
		const DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; // unknown to preserve format
		const UINT flags = 0;
		hr = mDev.mpSwapChain->ResizeBuffers(buffersCount, w, h, format, flags);
		if (!SUCCEEDED(hr))
			throw sD3DException(hr, "IDXGISpawnChain::ResizeBuffers failed");
	}

	mRTV = sRTView(mDev);
	const DXGI_SAMPLE_DESC sd = { 4, 0 };
	mDS = sDepthStencilBuffer(mDev, w, h, sd);
	set_default_viewport(w, h);

	apply_default_rt_vp(mDev.mpImmCtx);
}

void cGfx::begin_frame() {
	MICROPROFILE_SCOPEI("main", "cGfx::begin_frame", -1);
	mbInFrame = true;
	float clrClr[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	mDev.mpImmCtx->ClearRenderTargetView(mRTV.mpRTV, clrClr);
	mDev.mpImmCtx->ClearDepthStencilView(mDS.mpDSView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void cGfx::end_frame() {
	MICROPROFILE_SCOPEI("main", "cGfx::end_frame", -1);
	const UINT interval = cFrameTimer::get().use_vsync()
		? 1
		: 0;

	mDev.mpSwapChain->Present(interval, 0);
	mbInFrame = false;
}

void cGfx::set_default_viewport(uint32_t w, uint32_t h) {
	auto vp = D3D11_VIEWPORT();
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = (FLOAT)w;
	vp.Height = (FLOAT)h;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	mDefaultViewport = vp;
}

void cGfx::apply_default_rt_vp(ID3D11DeviceContext* pCtx) {
	ID3D11RenderTargetView* pv = mRTV.mpRTV;
	ID3D11DepthStencilView* pdv = mDS.mpDSView;
	pCtx->OMSetRenderTargets(1, &pv, pdv);
	pCtx->RSSetViewports(1, &mDefaultViewport);
}


static cShaderBytecode load_shader_file(cstr filepath) {
	sInputFile file(cPathManager::build_shaders_path(fs::path(filepath.p)));
	if (!file.is_open()) { return cShaderBytecode(); }

	auto pdata = file.read_all();
	return cShaderBytecode(std::move(pdata), file.mSize);
}

static cShaderBytecode load_shader_cstr(cstr code, cstr profile) {
	size_t strSize = code.length();

	com_ptr<ID3DBlob> blob;
	com_ptr<ID3DBlob> err;
	HRESULT hr = ::D3DCompile(code, strSize, nullptr, nullptr, nullptr, "main", profile, 0, 0, blob.pp(), err.pp());
	if (FAILED(hr)) {
		if (err) {
			dbg_msg1((char*)err->GetBufferPointer());
		}
		return cShaderBytecode();
	}

	size_t size = blob->GetBufferSize();
	auto pdata = std::make_unique<char[]>(size);
	::memcpy(pdata.get(), blob->GetBufferPointer(), size);
	return cShaderBytecode(std::move(pdata), size);
}

class cShaderStorage::cStorage {
	std::vector<std::unique_ptr<cShader>> mShaders;
	std::unordered_map<std::string, cShader*> mNames;
public:
	enum class eShaderType {
		E_VERTEX_SHADER,
		E_PIXEL_SHADER,
	};

	cShader* create_VS(cShaderBytecode&& code, std::string&& name) {
		if (code.get_size() == 0) return nullptr;

		auto pDev = get_gfx().get_dev();
		ID3D11VertexShader* pVS;
		HRESULT hr = pDev->CreateVertexShader(code.get_code(), code.get_size(), nullptr, &pVS);
		if (!SUCCEEDED(hr)) throw sD3DException(hr, "cShaderStorage::cStorage::create_VS CreateVertexShader failed");

		return put_shader(pVS, std::move(code), std::move(name));
	}

	cShader* create_PS(cShaderBytecode&& code, std::string&& name) {
		if (code.get_size() == 0) return nullptr;

		auto pDev = get_gfx().get_dev();
		ID3D11PixelShader* pPS;
		HRESULT hr = pDev->CreatePixelShader(code.get_code(), code.get_size(), nullptr, &pPS);
		if (!SUCCEEDED(hr)) throw sD3DException(hr, "cShaderStorage::cStorage::create_PS CreatePixelShader failed");

		return put_shader(pPS, std::move(code), std::move(name));
	}

	cShader* put_shader(ID3D11DeviceChild* pShader, cShaderBytecode&& code, std::string&& name) {
		auto s = std::make_unique<cShader>(pShader, std::move(code));
		auto p = s.get();
		mShaders.push_back(std::move(s));
		mNames[std::move(name)] = p;
		return p;
	}


	cShader* find(std::string const& name) {
		auto it = mNames.find(name);
		if (it == mNames.end())
			return nullptr;
		return it->second;
	}
};

cShaderStorage::cShaderStorage() {
	mpImpl = new cStorage;
}

cShaderStorage::~cShaderStorage() {
	delete mpImpl;
}

cShader* cShaderStorage::load_VS(cstr filepath) {
	std::string name(filepath);
	cShader* s = mpImpl->find(name);
	if (s) return s;
	return mpImpl->create_VS(load_shader_file(filepath), std::move(name));
}

cShader* cShaderStorage::load_PS(cstr filepath) {
	std::string name(filepath);
	cShader* s = mpImpl->find(name);
	if (s) return s;
	return mpImpl->create_PS(load_shader_file(filepath), std::move(name));
}

cShader* cShaderStorage::create_VS(cstr code, cstr name) {
	std::string sname(name);
	cShader* s = mpImpl->find(sname);
	if (s) return s;
	return mpImpl->create_VS(load_shader_cstr(code, "vs_5_0"), std::move(sname));
}

cShader* cShaderStorage::create_PS(cstr code, cstr name) {
	std::string sname(name);
	cShader* s = mpImpl->find(sname);
	if (s) return s;
	return mpImpl->create_PS(load_shader_cstr(code, "ps_5_0"), std::move(sname));
}


