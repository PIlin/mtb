#include "frame_timer.hpp"
#include "profiler.hpp"
#include "imgui.hpp"
#include "dbg_ui.hpp"

#include <vector>

void cFrameTimer::init_loop() {
	std::chrono::duration<double> ft(1.0 / 60.0);
	mIdealFrameDur = std::chrono::duration_cast<frame_clock::duration>(ft);
	mNextFrameDur = mIdealFrameDur;
	mFrameBeginTime = frame_clock::now();
}

void cFrameTimer::sleep() {
	auto frameEndTime = frame_clock::now();
	auto spent = frameEndTime - mFrameBeginTime;
	if (spent < mNextFrameDur) {
		auto left = mNextFrameDur - spent;
		MICROPROFILE_SCOPEI("main", "sleep", -1);
		//std::this_thread::sleep_until(frameBeginTime + frameTarget);
		std::this_thread::sleep_until(frameEndTime + left);
	}
}

void cFrameTimer::frame_flip() {
	auto now = frame_clock::now();
	auto realFrameTime = now - mFrameBeginTime;
	auto overTime = realFrameTime - mNextFrameDur;
	mNextFrameDur = mIdealFrameDur - overTime;
	mFrameBeginTime = now;
	mRealFrameTime = realFrameTime;
	//dbg_msg("begin %zd, ft %zd, rt %zu, o %zu, \n",
	//	mFrameBeginTime.time_since_epoch(), mNextFrameDur.count(), realFrameTime.count(), overTime.count());
}

template <typename T>
struct sStatCounter {
	struct sStats {
		T min;
		T max;
		T avg;
	};

	std::vector<T> data;
	size_t maxSize;
	size_t pos = 0;

	sStatCounter(size_t maxSize) : maxSize(maxSize) {
		data.resize(maxSize);
	}

	void push(const T& val) {
		data[pos] = val;
		pos = (pos + 1) % maxSize;
	}

	sStats calc_stats() const {
		T min = data[0];
		T max = data[0];
		T sum = {};

		for (const T& v : data) {
			sum += v;
			min = std::min(min, v);
			max = std::max(max, v);
		}

		T avg = sum / maxSize;

		return sStats{ min, max, avg };
	}
};

struct cFrameTimer::sDbgCounters {
	//using dur = cFrameTimer::frame_clock::duration;
	using dur = float;
	static constexpr size_t maxCount = 128;

	sStatCounter<dur> frameTime = {maxCount};
	sStatCounter<dur> overTime = {maxCount};
	sStatCounter<dur> nextFrameDur = { maxCount };
};


void cFrameTimer::dbg_ui() {
	if (bool& enabled = cDbgToolsMgr::tools().frame_timer) {
		if (ImGui::Begin("Frame timer", &enabled)) {
			if (!mpDbg) {
				mpDbg = std::make_unique<sDbgCounters>();
			}

			using namespace std::chrono;
			using millisecf = duration<float, std::milli>;

			auto overTime = mIdealFrameDur - mNextFrameDur;

			float ft = duration_cast<millisecf>(mRealFrameTime).count();
			float ov = duration_cast<millisecf>(overTime).count();
			float nf = duration_cast<millisecf>(mNextFrameDur).count();

			mpDbg->frameTime.push(ft);
			mpDbg->overTime.push(ov);
			mpDbg->nextFrameDur.push(nf);

			auto ftStats = mpDbg->frameTime.calc_stats();
			auto ovStats = mpDbg->overTime.calc_stats();
			auto nfStats = mpDbg->nextFrameDur.calc_stats();

			ImGui::InputFloat("Frame Time", &ft);
			ImGui::InputFloat("Frame Time Min", &ftStats.min);
			ImGui::InputFloat("Frame Time Max", &ftStats.max);
			ImGui::InputFloat("Frame Time Avg", &ftStats.avg);
			ImGui::Separator();
			ImGui::InputFloat("Over Time", &ov);
			ImGui::InputFloat("Over Time Min", &ovStats.min);
			ImGui::InputFloat("Over Time Max", &ovStats.max);
			ImGui::InputFloat("Over Time Avg", &ovStats.avg);
			ImGui::Separator();
			ImGui::InputFloat("Next Frame", &nf);
			ImGui::InputFloat("Next Frame Min", &nfStats.min);
			ImGui::InputFloat("Next Frame Max", &nfStats.max);
			ImGui::InputFloat("Next Frame Avg", &nfStats.avg);

		}
		ImGui::End();
	}
}

cFrameTimer::cFrameTimer() = default;
cFrameTimer::~cFrameTimer() = default;
