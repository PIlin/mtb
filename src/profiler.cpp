#define MICROPROFILE_IMPL 1
//#define MICROPROFILE_GPU_TIMERS_D3D11 1
#define _CRT_SECURE_NO_WARNINGS

#include "profiler.hpp"
#include "common.hpp"

#include <microprofiledraw.h>


cProfiler::cProfiler() {
	MicroProfileWebServerStart();
	MicroProfileSetEnableAllGroups(true);
}

cProfiler::~cProfiler() {
	MicroProfileWebServerStop();
}
void cProfiler::draw() {
	//MicroProfileDraw();
}

void cProfiler::flip() {
	MicroProfileFlip();
}
