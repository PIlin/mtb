#pragma once

#include <chrono>
#include <memory>

class cFrameTimer {
public:
	using frame_clock = std::chrono::steady_clock;

private:
	struct sDbgCounters;

	frame_clock::duration mIdealFrameDur;
	frame_clock::duration mNextFrameDur;
	frame_clock::time_point mFrameBeginTime;
	frame_clock::duration mRealFrameTime;

	std::unique_ptr<sDbgCounters> mpDbg;

public:
	static cFrameTimer& get();
	const frame_clock::duration& get_frame_time() { return mRealFrameTime; }

	cFrameTimer();
	~cFrameTimer();

	void init_loop();
	void sleep();
	void frame_flip();

	void dbg_ui();
};