#pragma once

//#include <cassert>

#pragma warning(push)
#pragma warning(disable: 4267)
#include <microprofile.h>
#pragma warning(pop)

class cProfiler {
public:
	cProfiler();
	~cProfiler();

	void update();
	void draw();
	void flip();

	static cProfiler& get();
};
