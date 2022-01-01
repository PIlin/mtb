#pragma once

//#include <cassert>

#pragma warning(push)
#pragma warning(disable: 4267)
#include <microprofile.h>
#pragma warning(pop)

struct ImDrawList;

class cProfiler {
public:
	cProfiler();
	~cProfiler();

	void init_ui();

	void draw();
	void flip();

	static cProfiler& get();

	void draw_text(int x, int y, uint32_t color, const char* pText, uint32_t numCharacters);
	void draw_box(int x, int y, int x1, int y1, uint32_t color, MicroProfileBoxType boxType);
	void draw_line_2d(uint32_t nVertices, float* pVertices, uint32_t color);

private:
	void update_input();

private:

	std::unique_ptr<ImDrawList> mpDrawList;
	bool mNoInput = false;
};
