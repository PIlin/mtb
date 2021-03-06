#include "math.hpp"
#include "input.hpp"
#include "common.hpp"

#include <SDL_events.h>

static_assert(SDL_NUM_SCANCODES == cInputMgr::KEYS_COUNT, "SDL_NUM_SCANCODES != cInputMgr::KEYS_COUNT");

cInputMgr::cInputMgr() {
	mTextInputs.reserve(16);
}

void cInputMgr::on_mouse_motion(SDL_MouseMotionEvent const& ev) {
	mMousePos = { ev.x, ev.y };
}

static int sdl_mouse_btn_to_input(Uint8 btn) {
	switch (btn) {
	case SDL_BUTTON_LEFT: return cInputMgr::EMBLEFT;
	case SDL_BUTTON_RIGHT: return cInputMgr::EMBRIGHT;
	case SDL_BUTTON_MIDDLE: return cInputMgr::EMBMIDDLE;
	default: return -1;
	}
}

void cInputMgr::on_mouse_button(SDL_MouseButtonEvent const& ev) {
	int btn = sdl_mouse_btn_to_input(ev.button);
	if (btn < 0) return;

	mMouseBtn[btn] = (ev.state == SDL_PRESSED);

	//dbg_msg("%d %d %d\n", ev.type, ev.state, ev.clicks);
}

void cInputMgr::on_text_input(SDL_TextInputEvent const& ev) {
	auto res = SDL_iconv_utf8_ucs4(ev.text);
	if (res) {
		auto c = res;
		while (*c) {
			mTextInputs.push_back(*c);
			++c;
		}
	}
}

void cInputMgr::preupdate() {
	mMousePosPrev = mMousePos;
	mMouseBtnPrev = mMouseBtn;
	mKeysPrev = mKeys;
	mKModPrev = mKMod;

	if (mTextinputEnabled) {
		SDL_StartTextInput();
	} else {
		SDL_StopTextInput();
	}
	mTextInputs.clear();
}

void cInputMgr::update() {
	for (int i = 0; i < EMBLAST; ++i) {
		if (mbtn_pressed((eMouseBtn)i)) {
			mMousePosStart[i] = mMousePos;
		}
	}
		
	//if (mMouseBtn[EMBLEFT]) {
	//	dbg_msg("%d %d %d %d\n", mMousePos.x, mMousePos.y, 
	//		mMousePosStart[EMBLEFT].x, mMousePosStart[EMBLEFT].y);
	//}

	int numkeys = 0;
	auto keys = SDL_GetKeyboardState(&numkeys);
	int keysCount = std::min(numkeys, (int)mKeys.size());
	for (int i = 0; i < keysCount; ++i) {
		mKeys[i] = !!keys[i];
	}

	mKMod = SDL_GetModState();
}
