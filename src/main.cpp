#define NOMINMAX

#include <cmath>

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>

#include "common.hpp"
#include "math.hpp"
#include "gfx.hpp"
#include "rdr.hpp"
#include "path_helpers.hpp"
#include "texture.hpp"
#include "model.hpp"
#include "rig.hpp"
#include "anim.hpp"
#include "input.hpp"
#include "camera.hpp"
#include "imgui_impl.hpp"
#include "sh.hpp"
#include "light.hpp"
#include <imgui.h>

namespace dx = DirectX;
using dx::XMFLOAT4;

#include <assimp/Importer.hpp>
#include "assimp_loader.hpp"

class cSDLInit {
public:
	cSDLInit() {
		Uint32 initFlags = SDL_INIT_VIDEO;
		SDL_Init(initFlags);
	}

	~cSDLInit() {
		SDL_Quit();
	}
};

class cSDLWindow {
	moveable_ptr<SDL_Window> win;
	vec2i mWindowSize;
public:
	cSDLWindow(cstr title, int x, int y, int w, int h, Uint32 flags) {
		init(title, x, y, w, h, flags);
	}
	cSDLWindow(cstr title, int w, int h, Uint32 flags) {
		init(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
	}
	~cSDLWindow() {
		deinit();
	}

	cSDLWindow(cSDLWindow&& o) : win(std::move(o.win)) { }
	cSDLWindow(cSDLWindow& o) = delete;

	HWND get_handle() const {
		HWND hwnd = 0;

		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if (SDL_GetWindowWMInfo(win, &info)) {
			hwnd = info.info.win.window;
		}

		return hwnd;
	}

	vec2i get_window_size() const { return mWindowSize; }

private:
	void init(cstr title, int x, int y, int w, int h, Uint32 flags) {
		win = SDL_CreateWindow(title.p, x, y, w, h, flags);
		mWindowSize = { w, h };
	}
	void deinit() {
		if (!win) return;

		SDL_DestroyWindow(win);
		win.reset();
	}
};


struct sGlobals {
	GlobalSingleton<cSDLWindow> win;
	GlobalSingleton<cGfx> gfx;
	GlobalSingleton<cShaderStorage> shaderStorage;
	GlobalSingleton<cTextureStorage> textureStorage;
	GlobalSingleton<cInputMgr> input;
	GlobalSingleton<cCamera> camera;
	GlobalSingleton<cConstBufStorage> cbufStorage;
	GlobalSingleton<cSamplerStates> samplerStates;
	GlobalSingleton<cBlendStates> blendStates;
	GlobalSingleton<cRasterizerStates> rasterizeStates;
	GlobalSingleton<cDepthStencilStates> depthStates;
	GlobalSingleton<cImgui> imgui;
	GlobalSingleton<cLightMgr> lightMgr;
	GlobalSingleton<cPathManager> pathManager;
};

sGlobals globals;

cGfx& get_gfx() { return globals.gfx.get(); }
cShaderStorage& cShaderStorage::get() { return globals.shaderStorage.get(); }
cInputMgr& get_input_mgr() { return globals.input.get(); }
cCamera& get_camera() { return globals.camera.get(); }
vec2i get_window_size() { return globals.win.get().get_window_size(); }
cConstBufStorage& cConstBufStorage::get() { return globals.cbufStorage.get(); }
cSamplerStates& cSamplerStates::get() { return globals.samplerStates.get(); }
cBlendStates& cBlendStates::get() { return globals.blendStates.get(); }
cRasterizerStates& cRasterizerStates::get() { return globals.rasterizeStates.get(); }
cDepthStencilStates& cDepthStencilStates::get() { return globals.depthStates.get(); }
cImgui& cImgui::get() { return globals.imgui.get(); }
cTextureStorage& cTextureStorage::get() { return globals.textureStorage.get(); }
cLightMgr& cLightMgr::get() { return globals.lightMgr.get(); }
cPathManager& cPathManager::get() { return globals.pathManager.get(); }

class cGnomon {
	struct sVtx {
		float mPos[3];
		float mClr[4];
	};

	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	com_ptr<ID3D11InputLayout> mpIL;
	cVertexBuffer mVtxBuf;
	int mState = 0;
public:

	void exec() {
		switch (mState) {
		case 0:
			state_init();
			mState++;
			break;
		case 2:
			state_deinit();
			break;
		}
	}
	void disp() {
		if (!(mpVS && mpVS)) return;

		auto pCtx = get_gfx().get_ctx();

		//mConstBuf.mData.wmtx = dx::XMMatrixIdentity();
		auto& cbuf = cConstBufStorage::get().mMeshCBuf;
		cbuf.mData.wmtx = dx::XMMatrixTranslation(0, 0, 0);
		cbuf.update(pCtx);
		cbuf.set_VS(pCtx);

		UINT pStride[] = { sizeof(sVtx) };
		UINT pOffset[] = { 0 };
		mVtxBuf.set(pCtx, 0, 0);
		pCtx->IASetInputLayout(mpIL);
		pCtx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		pCtx->VSSetShader(mpVS->asVS(), nullptr, 0);
		pCtx->PSSetShader(mpPS->asPS(), nullptr, 0);
		pCtx->Draw(6, 0);
	}

	void deinit() {
		mState = 2;
		exec();
	}
private:
	void state_init() {
		auto& ss = cShaderStorage::get();
		mpVS = ss.load_VS("simple.vs.cso");
		if (!mpVS) return;
		mpPS = ss.load_PS("simple.ps.cso");
		if (!mpPS) return;

		D3D11_INPUT_ELEMENT_DESC vdsc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sVtx, mPos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(sVtx, mClr), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		auto pDev = get_gfx().get_dev();
		auto& code = mpVS->get_code();
		HRESULT hr = pDev->CreateInputLayout(vdsc, LENGTHOF_ARRAY(vdsc), code.get_code(), code.get_size(), mpIL.pp());
		if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

		sVtx vtx[6] = {
			{ 0.0f, 0.0f, 0.0f, 1.0, 0.0f, 0.0f, 1.0f },
			{ 1.0f, 0.0f, 0.0f, 1.0, 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0, 1.0f, 0.0f, 1.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0, 1.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0, 0.0f, 1.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0, 0.0f, 1.0f, 1.0f },
		};

		mVtxBuf.init(pDev, vtx, LENGTHOF_ARRAY(vtx), sizeof(vtx[0]));
	}

	void state_deinit() {
		mVtxBuf.deinit();
		mpIL.reset();
	}
};


class cSolidModel {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;

public:
	void disp() {
		mModel.dbg_ui();
		mModel.disp();
	}
};

class cLightning : public cSolidModel {
public:
	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("owl");

		res = res && mMdlData.load(root / "lightning.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "lightning.mtl");
		res = res && mModel.init(mMdlData, mMtl);

		//mModel.mWmtx = DirectX::XMMatrixScaling(0.3f, 0.3f, 0.3f);

		return res;
	}
};

class cSkinnedModel {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;
	cRigData mRigData;
	cRig mRig;

	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;
public:
	void disp() {
		mRig.calc_local();
		mRig.calc_world();
		mRig.upload_skin(get_gfx().get_ctx());

		mModel.dbg_ui();
		mModel.disp();
	}
};

class cSkinnedAnimatedModel : public cSkinnedModel {
protected:
	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;

	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;
public:
	void disp() {
		int32_t animCount = mAnimList.get_count();
		if (animCount > 0) {
			auto& anim = mAnimList[mCurAnim];
			float lastFrame = anim.get_last_frame();

			anim.eval(mRig, mFrame);
			mFrame += mSpeed;
			if (mFrame > lastFrame)
				mFrame = 0.0f;

			ImGui::Begin("anim");
			ImGui::LabelText("name", "%s", anim.get_name());
			ImGui::SliderInt("curAnim", &mCurAnim, 0, animCount - 1);
			ImGui::SliderFloat("frame", &mFrame, 0.0f, lastFrame);
			ImGui::SliderFloat("speed", &mSpeed, 0.0f, 3.0f);
			ImGui::End();
		}
		cSkinnedModel::disp();
	}
};

class cOwl : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("owl");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root /"def.mtl");
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		float scl = 0.01f;
		mModel.mWmtx = DirectX::XMMatrixScaling(scl, scl, scl);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};

class cJumpingSphere : public cSkinnedAnimatedModel {
public:
	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("jumping_sphere");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl", true);
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};


class cUnrealPuppet : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("unreal_puppet");
		{
			cAssimpLoader loader;
			res = res && loader.load_unreal_fbx(root / "SideScrollerSkeletalMesh.FBX");
			res = res && mMdlData.load_assimp(loader);
			res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl");
			res = res && mModel.init(mMdlData, mMtl);

			mRigData.load(loader);
			mRig.init(&mRigData);
		}
		{
			cAssimpLoader animLoader;
			//animLoader.load_unreal_fbx(cPathManager::build_data_path(OBJPATH "SideScrollerIdle.FBX"));
			animLoader.load_unreal_fbx(root / "SideScrollerWalk.FBX");
			mAnimDataList.load(animLoader);
			mAnimList.init(mAnimDataList, mRigData);

			mSpeed = 1.0f / 60.0f;
		}

		float scl = 0.01f;
		mModel.mWmtx = DirectX::XMMatrixScaling(scl, scl, scl);
		mModel.mWmtx *= DirectX::XMMatrixRotationX(DEG2RAD(-90.0f)); 

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};

cGnomon gnomon;
cLightning lightning;
cJumpingSphere sphere;
cOwl owl;
cUnrealPuppet upuppet;

cTrackballCam trackballCam;

void do_frame() {
	auto& gfx = get_gfx();
	gfx.begin_frame();
	cImgui::get().update();

	cRasterizerStates::get().set_def(get_gfx().get_ctx());
	cDepthStencilStates::get().set_def(get_gfx().get_ctx());

	auto& cam = get_camera();
	trackballCam.update(cam);
	auto& camCBuf = cConstBufStorage::get().mCameraCBuf;
	camCBuf.mData.viewProj = cam.mView.mViewProj;
	camCBuf.mData.view = cam.mView.mView;
	camCBuf.mData.proj = cam.mView.mProj;
	camCBuf.mData.camPos = cam.mView.mPos;
	camCBuf.update(gfx.get_ctx());
	camCBuf.set_VS(gfx.get_ctx());
	camCBuf.set_PS(gfx.get_ctx());

	cLightMgr::get().update();

	//lightning.disp();
	//sphere.disp();
	//owl.disp();
	upuppet.disp();

	gnomon.exec();
	gnomon.disp();

	cImgui::get().disp();
	gfx.end_frame();
}

void loop() {
	bool quit = false;
	SDL_Event ev;
	auto& inputMgr = get_input_mgr();
	while (!quit) {
		Uint32 ticks = SDL_GetTicks();
		inputMgr.preupdate();

		while (SDL_PollEvent(&ev)) {
			switch (ev.type) {
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEMOTION:
				inputMgr.on_mouse_motion(ev.motion);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				inputMgr.on_mouse_button(ev.button);
				break;
			case SDL_TEXTINPUT:
				inputMgr.on_text_input(ev.text);
				break;
			}
		}

		inputMgr.update();

		if (!quit) {
			do_frame();
		} else {
			gnomon.deinit();
		}

		Uint32 now = SDL_GetTicks();
		Uint32 spent = now - ticks;
		if (spent < 16) {
			SDL_Delay(16 - spent);
		}
	}
}





int main(int argc, char* argv[]) {
	cSDLInit sdl;
	auto pathManager = globals.pathManager.ctor_scoped();
	auto win = globals.win.ctor_scoped("TestBed - SPACE + mouse to control camera", 1200, 900, 0);
	auto input = globals.input.ctor_scoped();
	auto gfx = globals.gfx.ctor_scoped(globals.win.get().get_handle());
	auto ss = globals.shaderStorage.ctor_scoped();
	auto ts = globals.textureStorage.ctor_scoped(get_gfx().get_dev());
	auto cbuf = globals.cbufStorage.ctor_scoped(get_gfx().get_dev());
	auto smps = globals.samplerStates.ctor_scoped(get_gfx().get_dev());
	auto blnds = globals.blendStates.ctor_scoped(get_gfx().get_dev());
	auto rsst = globals.rasterizeStates.ctor_scoped(get_gfx().get_dev());
	auto dpts = globals.depthStates.ctor_scoped(get_gfx().get_dev());
	auto imgui = globals.imgui.ctor_scoped(get_gfx());
	auto lmgr = globals.lightMgr.ctor_scoped();
	auto cam = globals.camera.ctor_scoped();

	trackballCam.init(get_camera());

	//lightning.init();
	//sphere.init();
	//owl.init();
	upuppet.init();

	auto& l = cConstBufStorage::get().mLightCBuf; 
	::memset(&l.mData, 0, sizeof(l.mData));

	loop();

	return 0;
}
