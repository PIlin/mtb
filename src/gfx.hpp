#include <d3d11.h>
#include <vector>
#include <memory>

class cSDLWindow;

class cGfx : noncopyable {
	struct sDev : noncopyable {
		com_ptr<IDXGISwapChain> mpSwapChain;
		com_ptr<ID3D11Device> mpDev;
		com_ptr<ID3D11DeviceContext> mpImmCtx;

		sDev() {}
		sDev(DXGI_SWAP_CHAIN_DESC& sd, UINT flags);
		sDev& operator=(sDev&& o) {
			release();
			mpSwapChain = std::move(o.mpSwapChain);
			mpDev = std::move(o.mpDev);
			mpImmCtx = std::move(o.mpImmCtx);
			return *this;
		}
		~sDev() {
			release();
		}
		void release() {
			if (mpSwapChain) {
				mpSwapChain->SetFullscreenState(FALSE, nullptr);
				mpSwapChain.reset();
			}
			mpImmCtx.reset();
			mpDev.reset();
		}
	};
	struct sRTView : noncopyable {
		com_ptr<ID3D11RenderTargetView> mpRTV;

		sRTView() {}
		sRTView(sDev& dev);
		sRTView& operator=(sRTView&& o) {
			mpRTV = std::move(o.mpRTV);
			return *this;
		}
	};

	struct sDepthStencilBuffer : noncopyable {
		com_ptr<ID3D11Texture2D> mpTex;
		com_ptr<ID3D11DepthStencilView> mpDSView;

		sDepthStencilBuffer() {}
		sDepthStencilBuffer(sDev& dev, uint32_t w, uint32_t h, DXGI_SAMPLE_DESC const& sampleDesc);
		sDepthStencilBuffer& operator=(sDepthStencilBuffer&& o) {
			mpTex = std::move(o.mpTex);
			mpDSView = std::move(o.mpDSView);
			return *this;
		}
	};

	sDev mDev;
	sRTView mRTV;
	sDepthStencilBuffer mDS;

	bool mbInFrame = false;
public:
	cGfx(HWND hwnd);

	void begin_frame();
	void end_frame();

	void on_window_size_changed(uint32_t w, uint32_t h);

	ID3D11Device* get_dev() { return mDev.mpDev; }
	ID3D11DeviceContext* get_ctx() { return mDev.mpImmCtx; }

private:
	void set_viewport(uint32_t w, uint32_t h);
};

class cShaderBytecode : noncopyable {
	std::unique_ptr<char[]> mCode;
	size_t                  mSize;
public:
	cShaderBytecode() : mCode(), mSize(0) {}
	cShaderBytecode(std::unique_ptr<char[]> code, size_t size) : mCode(std::move(code)), mSize(size) {}

	cShaderBytecode(cShaderBytecode&& o) : mCode(std::move(o.mCode)), mSize(o.mSize) {}
	cShaderBytecode& operator=(cShaderBytecode&& o) {
		mCode = std::move(o.mCode);
		mSize = o.mSize;
		return *this;
	}

	char const* get_code() const { return mCode.get(); }
	size_t get_size() const { return mSize; }
};

class cShader  : noncopyable  {
	com_ptr<ID3D11DeviceChild> mpShader;
	cShaderBytecode            mCode;
public:
	cShader(ID3D11DeviceChild* pShader, cShaderBytecode&& code) : mpShader(pShader), mCode(std::move(code)) {}
	cShader(cShader&& o) : mpShader(std::move(o.mpShader)), mCode(std::move(o.mCode)) {}
	cShader& operator=(cShader&& o) {
		mpShader = std::move(o.mpShader);
		mCode = std::move(o.mCode);
		return *this;
	}

	ID3D11VertexShader* asVS() { return static_cast<ID3D11VertexShader*>(mpShader.p); }
	ID3D11PixelShader* asPS() { return static_cast<ID3D11PixelShader*>(mpShader.p); }

	cShaderBytecode const& get_code() const { return mCode; }
};


class cShaderStorage {
	class cStorage;
	cStorage* mpImpl;
public:
	static cShaderStorage& get();

	cShaderStorage();
	~cShaderStorage();

	cShader* load_VS(cstr filepath);
	cShader* load_PS(cstr filepath);

	cShader* create_VS(cstr code, cstr name);
	cShader* create_PS(cstr code, cstr name);
};


cGfx& get_gfx();
vec2i get_window_size();
