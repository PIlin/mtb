#pragma once

#include "common.hpp"

#include <chrono>
#include <memory>

class cFrameTimer {
public:
	using frame_clock = std::chrono::steady_clock;

private:
	struct sDbgCounters;

	frame_clock::duration mIdealFrameDur = {};
	frame_clock::duration mNextFrameDur = {};
	frame_clock::time_point mFrameBeginTime = {};
	frame_clock::duration mRealFrameTime = {};
	float mFrameTimeMul = 1.0f;
	float mFrameTimeSec = 0.0f;
	
	winhandle_ptr mpWaitableTimer;
	bool mIsWaitableTimerSet = false;
	bool mUseWaitableTimer = true;
	bool mUseVsync = false;

	int mIdealFrameRate = 0;

	std::unique_ptr<sDbgCounters> mpDbg;

public:
	static cFrameTimer& get();
	const frame_clock::duration& get_frame_time() const { return mRealFrameTime; }
	float get_framt_time_sec() const { return mFrameTimeSec; }
	float get_frame_time_mul() const { return mFrameTimeMul; }

	cFrameTimer();
	~cFrameTimer();

	void set_ideal_framerate(int rate);
	bool use_vsync() const { return mUseVsync; }

	void init_loop();
	void sleep();
	void frame_flip();

	void dbg_ui();

private:
	void set_waitable_timer();
	void update_frame_time_mul();
};
