#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "common.hpp"
#include "texture.hpp"
#include "res/path_helpers.hpp"

#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <locale>

bool cTexture::load(ID3D11Device* pDev, const fs::path& filepath) {
	unload();

	const bool isDDS = (filepath.extension() == ".dds");
	
	HRESULT hr;
	if (isDDS) {
		hr = DirectX::CreateDDSTextureFromFile(pDev, filepath.c_str(), mpTex.pp(), mpView.pp());
	}
	else {
		hr = DirectX::CreateWICTextureFromFile(pDev, filepath.c_str(), mpTex.pp(), mpView.pp());
	}

	return SUCCEEDED(hr);
}

bool cTexture::create2d1_rgba_u8(ID3D11Device* pDev, void* data, uint32_t w, uint32_t h) {
	unload();

	auto desc = D3D11_TEXTURE2D_DESC();
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D* pTex = nullptr;
	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = data;
	initData.SysMemPitch = desc.Width * 4;
	initData.SysMemSlicePitch = 0;
	HRESULT hr = pDev->CreateTexture2D(&desc, &initData, &pTex);
	if (!SUCCEEDED(hr))
		return false;
	mpTex.reset(pTex);

	auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC();
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	hr = pDev->CreateShaderResourceView(mpTex, &srvDesc, mpView.pp());

	return SUCCEEDED(hr);
}

bool cTexture::create2d1_rgba_f32(ID3D11Device* pDev, void* data, uint32_t w, uint32_t h) {
	unload();

	auto desc = D3D11_TEXTURE2D_DESC();
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ID3D11Texture2D* pTex = nullptr;
	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = data;
	initData.SysMemPitch = desc.Width * 4;
	initData.SysMemSlicePitch = 0;
	HRESULT hr = pDev->CreateTexture2D(&desc, &initData, &pTex);
	if (!SUCCEEDED(hr))
		return false;
	mpTex.reset(pTex);

	auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC();
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	hr = pDev->CreateShaderResourceView(mpTex, &srvDesc, mpView.pp());

	return SUCCEEDED(hr);
}

void cTexture::unload() {
	if (mpTex) {
		mpTex->Release();
		mpTex.reset(); 
	}
	if (mpView) {
		mpView->Release();
		mpView.reset();
	}
}

void cTexture::set_PS(ID3D11DeviceContext* pCtx, UINT slot) {
	ID3D11ShaderResourceView* views[1] = { mpView };
	pCtx->PSSetShaderResources(slot, 1, views);
}

void cTexture::set_null_PS(ID3D11DeviceContext* pCtx, UINT slot) {
	ID3D11ShaderResourceView* views[1] = { nullptr };
	pCtx->PSSetShaderResources(slot, 1, views);
}


class cTextureStorage::cStorage {
	std::vector<std::unique_ptr<cTexture>> mTextures;
	std::unordered_map<std::string, cTexture*> mNames;
public:
	cTexture* load(ID3D11Device* pDev, const fs::path& filepath) {
		std::string name = filepath.u8string();
		auto p = find(name);
		if (p) { return p; }

		auto pTex = std::make_unique<cTexture>();
		if (!pTex->load(pDev, filepath))
			return nullptr;

		p = pTex.get();
		mTextures.push_back(std::move(pTex));
		mNames[std::move(name)] = p;
		return p;
	}

	cTexture* find(std::string const& name) {
		auto it = mNames.find(name);
		if (it == mNames.end())
			return nullptr;
		return it->second;
	}
};


cTextureStorage::cTextureStorage(ID3D11Device* pDev) {
	mpImpl = new cStorage;
	init_def_nmap(pDev);
	init_def_white(pDev);
}

cTextureStorage::~cTextureStorage() {
	delete mpImpl;
}

void cTextureStorage::init_def_nmap(ID3D11Device* pDev) {
	//float rgba[4] = {0.5f, 0.5f, 1.0f, 1.0f};
	//mDefNmap.create2d1_rgba_f32(pDev, rgba, 1, 1);
	uint8_t rgba[4] = {128, 128, 255, 255};
	mDefNmap.create2d1_rgba_u8(pDev, rgba, 1, 1);
}

void cTextureStorage::init_def_white(ID3D11Device* pDev) {
	uint8_t rgba[4] = {255, 255, 255, 255};
	mDefWhite.create2d1_rgba_u8(pDev, rgba, 1, 1);
}


cTexture* cTextureStorage::load(ID3D11Device* pDev, const fs::path& filepath) {
	return mpImpl->load(pDev, filepath);
}
