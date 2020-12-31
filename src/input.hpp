#pragma once

#include "math.hpp"

#include <bitset>
#include <vector>

struct SDL_MouseMotionEvent;
struct SDL_MouseButtonEvent;
struct SDL_TextInputEvent;

class cInputMgr {
public:
	enum eMouseBtn {
		EMBLEFT,
		EMBRIGHT,
		EMBMIDDLE,

		EMBLAST
	};

	enum {
		KEYS_COUNT = 512
	};

	vec2i mMousePos = { 0, 0 };
	vec2i mMousePosPrev = { 0, 0 };
	vec2i mMousePosStart[EMBLAST];

	std::bitset<EMBLAST> mMouseBtn;
	std::bitset<EMBLAST> mMouseBtnPrev;

	std::bitset<KEYS_COUNT> mKeys;
	std::bitset<KEYS_COUNT> mKeysPrev;

	uint32_t mKMod;
	uint32_t mKModPrev;

	std::vector<uint32_t> mTextInputs;
	bool mTextinputEnabled = false;
public:

	cInputMgr();
		
	void on_mouse_button(SDL_MouseButtonEvent const& ev);
	void on_mouse_motion(SDL_MouseMotionEvent const& ev);
	void on_text_input(SDL_TextInputEvent const& ev);

	void preupdate();
	void update();

	bool mbtn_pressed(eMouseBtn btn) const { return mMouseBtn[btn] && !mMouseBtnPrev[btn]; }
	bool mbtn_released(eMouseBtn btn) const { return !mMouseBtn[btn] && mMouseBtnPrev[btn]; }
	bool mbtn_held(eMouseBtn btn) const { return mMouseBtn[btn] && mMouseBtnPrev[btn]; }
	bool mbtn_state(eMouseBtn btn) const { return mMouseBtn[btn]; }
	bool mbtn_state_prev(eMouseBtn btn) const { return mMouseBtnPrev[btn]; }

	// SDL_Scancode
	bool kbtn_pressed(int btn) const { return mKeys[btn] && !mKeysPrev[btn]; }
	bool kbtn_released(int btn) const { return !mKeys[btn] && mKeysPrev[btn]; }
	bool kbtn_held(int btn) const { return mKeys[btn] && mKeysPrev[btn]; }
	bool kbtn_state(int btn) const { return mKeys[btn]; }
	bool kbtn_state_prev(int btn) const { return mKeysPrev[btn]; }

	// SDL_Keymod
	bool kmod_pressed(uint32_t kmod) const { return (mKMod & kmod) && (~mKModPrev & kmod); }
	bool kmod_released(uint32_t kmod) const { return (~mKMod & kmod) && (mKModPrev & kmod); }
	bool kmod_held(uint32_t kmod) const { return (mKMod & kmod) && (mKModPrev & kmod); }
	bool kmod_state(uint32_t kmod) const { return !!(mKMod & kmod); }
	bool kmod_state_prev(uint32_t kmod) const { return !!(mKModPrev & kmod); }

	void enable_textinput(bool val) { mTextinputEnabled = val; }
	std::vector<uint32_t> const& get_textinput() const { return mTextInputs; }
};

cInputMgr& get_input_mgr();
