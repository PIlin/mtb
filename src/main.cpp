#define NOMINMAX

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>

#include "common.hpp"
#include "math.hpp"
#include "gfx.hpp"
#include "rdr.hpp"
#include "path_helpers.hpp"
#include "input.hpp"
#include "camera.hpp"
#include "texture.hpp"
#include "imgui_impl.hpp"
#include "sh.hpp"
#include "light.hpp"
#include "scene_objects.hpp"
#include <imgui.h>

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
	GlobalSingleton<cSceneMgr> sceneMgr;
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
cSceneMgr& cSceneMgr::get() { return globals.sceneMgr.get(); }

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
	cSceneMgr::get().disp();

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
	auto scene = globals.sceneMgr.ctor_scoped();

	trackballCam.init(get_camera());

	auto& l = cConstBufStorage::get().mLightCBuf; 
	::memset(&l.mData, 0, sizeof(l.mData));

	loop();

	return 0;
}
