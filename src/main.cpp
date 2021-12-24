#include "common.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>
CLANG_DIAG_POP

#include "profiler.hpp"
#include "math.hpp"
#include "rdr/gfx.hpp"
#include "rdr/rdr.hpp"
#include "input.hpp"
#include "rdr/camera.hpp"
#include "rdr/texture.hpp"
#include "imgui_impl.hpp"
#include "scene/scene_mgr.hpp"
#include <imgui.h>
#include "rdr/rdr_queue.hpp"
#include "res/path_helpers.hpp"
#include "res/resource_mgr.hpp"
#include "dbg_ui.hpp"
#include "rdr/dbg_draw.hpp"

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
	sWindowSettings mWindowSettings;
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

	vec2i get_window_size() const { return mWindowSettings.mSize; }

	void process_window_event(const SDL_WindowEvent& ev) {
		switch ((SDL_WindowEventID)ev.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED: {
				const int32_t w = ev.data1;
				const int32_t h = ev.data2;

				mWindowSettings.mSize = { w, h };

				get_gfx().on_window_size_changed(w, h);
			}
			break;
			default: break;
		}
	}

private:
	static fs::path get_window_settings_path() {
		return cPathManager::build_settings_path("window.json");
	}

	void init(cstr title, int x, int y, int w, int h, Uint32 flags) {
		mWindowSettings.mSize = { w, h };

		mWindowSettings.load(get_window_settings_path());

		win = SDL_CreateWindow(title.p, x, y, mWindowSettings.mSize.x, mWindowSettings.mSize.y, flags);
	}
	void deinit() {
		if (!win) return;

		SDL_DestroyWindow(win);
		win.reset();

		mWindowSettings.save(get_window_settings_path());
	}
};


struct sGlobals {
	GlobalSingleton<cProfiler> profiler;
	GlobalSingleton<cSDLWindow> win;
	GlobalSingleton<cGfx> gfx;
	GlobalSingleton<cShaderStorage> shaderStorage;
	GlobalSingleton<cTextureStorage> textureStorage;
	GlobalSingleton<cInputMgr> input;
	GlobalSingleton<cConstBufStorage> cbufStorage;
	GlobalSingleton<cSamplerStates> samplerStates;
	GlobalSingleton<cBlendStates> blendStates;
	GlobalSingleton<cRasterizerStates> rasterizeStates;
	GlobalSingleton<cDepthStencilStates> depthStates;
	GlobalSingleton<cImgui> imgui;
	GlobalSingleton<cPathManager> pathManager;
	GlobalSingleton<cSceneMgr> sceneMgr;
	GlobalSingleton<cRdrQueueMgr> rdrQueueMgr;
	GlobalSingleton<cResourceMgr> resourceMgr;
	GlobalSingleton<cDbgToolsMgr> dbgToolsMgr;
	GlobalSingleton<cDbgDrawMgr> dbgDrawMgr;
};

sGlobals globals;

cProfiler& cProfiler::get() { return globals.profiler.get(); }
cGfx& get_gfx() { return globals.gfx.get(); }
cShaderStorage& cShaderStorage::get() { return globals.shaderStorage.get(); }
cInputMgr& get_input_mgr() { return globals.input.get(); }
vec2i get_window_size() { return globals.win.get().get_window_size(); }
cConstBufStorage& cConstBufStorage::get_global() { return globals.cbufStorage.get(); }
cSamplerStates& cSamplerStates::get() { return globals.samplerStates.get(); }
cBlendStates& cBlendStates::get() { return globals.blendStates.get(); }
cRasterizerStates& cRasterizerStates::get() { return globals.rasterizeStates.get(); }
cDepthStencilStates& cDepthStencilStates::get() { return globals.depthStates.get(); }
cImgui& cImgui::get() { return globals.imgui.get(); }
cTextureStorage& cTextureStorage::get() { return globals.textureStorage.get(); }
cPathManager& cPathManager::get() { return globals.pathManager.get(); }
cSceneMgr& cSceneMgr::get() { return globals.sceneMgr.get(); }
cRdrQueueMgr& cRdrQueueMgr::get() { return globals.rdrQueueMgr.get(); }
cResourceMgr& cResourceMgr::get() { return globals.resourceMgr.get(); }
cDbgToolsMgr& cDbgToolsMgr::get() { return globals.dbgToolsMgr.get(); }
cDbgDrawMgr& cDbgDrawMgr::get() { return globals.dbgDrawMgr.get(); }

void do_frame() {
	{
		MICROPROFILE_SCOPEI("main", "do_frame", -1);
		auto& gfx = get_gfx();
		gfx.begin_frame();
		cImgui::get().update();

		cSceneMgr::get().update();

		cDbgToolsMgr::get().update();
		cDbgDrawMgr::get().disp();
		cImgui::get().disp();
		cProfiler::get().draw();
		gfx.end_frame();
	}
}

bool poll_events(cInputMgr& inputMgr) {
	SDL_Event ev;
	bool quit = false;
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
		case SDL_MOUSEWHEEL:
			inputMgr.on_mouse_wheel(ev.wheel);
			break;
		case SDL_TEXTINPUT:
			inputMgr.on_text_input(ev.text);
			break;
		case SDL_WINDOWEVENT:
			globals.win.get().process_window_event(ev.window);
			break;
		}
	}
	return quit;
}

void loop() {
	bool quit = false;
	
	auto& inputMgr = get_input_mgr();
	while (!quit) {
		MICROPROFILE_SCOPEI("frame", "frame", -1);
		Uint32 ticks = SDL_GetTicks();
		{
			MICROPROFILE_SCOPEI("main", "input", -1);
			inputMgr.preupdate();
			quit = poll_events(inputMgr);
			inputMgr.update();
		}

		if (!quit) {
			do_frame();
		}

		Uint32 now = SDL_GetTicks();
		Uint32 spent = now - ticks;
		if (spent < 16) {
			MICROPROFILE_SCOPEI("main", "sleep", -1);
			SDL_Delay(16 - spent);
		}
		cProfiler::get().flip();
	}
}

int main(int argc, char* argv[]) {
	cSDLInit sdl;
	auto profiler = globals.profiler.ctor_scoped();
	auto pathManager = globals.pathManager.ctor_scoped();
	auto dbgUi = globals.dbgToolsMgr.ctor_scoped();
	auto win = globals.win.ctor_scoped("TestBed - SPACE + mouse to control camera", 1200, 900, SDL_WINDOW_RESIZABLE);
	auto input = globals.input.ctor_scoped();
	auto gfx = globals.gfx.ctor_scoped(globals.win.get().get_handle());
	auto resourceMgr = globals.resourceMgr.ctor_scoped();
	auto ss = globals.shaderStorage.ctor_scoped();
	auto ts = globals.textureStorage.ctor_scoped(get_gfx().get_dev());
	auto cbuf = globals.cbufStorage.ctor_scoped(get_gfx().get_dev());
	auto smps = globals.samplerStates.ctor_scoped(get_gfx().get_dev());
	auto blnds = globals.blendStates.ctor_scoped(get_gfx().get_dev());
	auto rsst = globals.rasterizeStates.ctor_scoped(get_gfx().get_dev());
	auto dpts = globals.depthStates.ctor_scoped(get_gfx().get_dev());
	auto rdrQueueMgr = globals.rdrQueueMgr.ctor_scoped(get_gfx());
	auto imgui = globals.imgui.ctor_scoped(get_gfx());
	auto dbgDraw = globals.dbgDrawMgr.ctor_scoped(get_gfx());
	auto scene = globals.sceneMgr.ctor_scoped();

	loop();

	return 0;
}
