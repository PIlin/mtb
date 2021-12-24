#pragma once

struct ID3D11Device;
struct ID3D11Resource;
struct ID3D11ShaderResourceView;
struct ID3D11DeviceContext;

namespace std::filesystem { class path; }
namespace fs = std::filesystem;

class cTexture : noncopyable {
	moveable_ptr<ID3D11Resource> mpTex;
	moveable_ptr<ID3D11ShaderResourceView> mpView;
public:

	cTexture() {}
	cTexture(cTexture&& o) : mpTex(std::move(o.mpTex)), mpView(std::move(o.mpView)) {}
	cTexture(ID3D11Resource* pTex, ID3D11ShaderResourceView* pView) : mpTex(pTex), mpView(pView) {}
	~cTexture() { unload(); }
	cTexture& operator=(cTexture&& o) {
		mpTex = std::move(o.mpTex); 
		mpView = std::move(o.mpView);
		return *this;
	}

	bool create2d1_rgba_u8(ID3D11Device* pDev, void* data, uint32_t w, uint32_t h);
	bool create2d1_rgba_f32(ID3D11Device* pDev, void* data, uint32_t w, uint32_t h);
	bool load(ID3D11Device* pDev, const fs::path& filepath);
	void unload();

	bool is_ready() const { return !!mpView; }

	void set_PS(ID3D11DeviceContext* pCtx, uint32_t slot);

	static void set_null_PS(ID3D11DeviceContext* pCtx, uint32_t slot);
};


class cTextureStorage : noncopyable {
	cTexture mDefNmap;
	cTexture mDefWhite;

	class cStorage;
	cStorage* mpImpl;
public:
	static cTextureStorage& get();

	cTextureStorage(ID3D11Device* pDev);
	~cTextureStorage();

	cTexture* load(ID3D11Device* pDev, const fs::path& filepath);

	cTexture* get_def_nmap() { return &mDefNmap; }
	cTexture* get_def_white() { return &mDefWhite; }

protected:
	void init_def_nmap(ID3D11Device* pDev);
	void init_def_white(ID3D11Device* pDev);
};
