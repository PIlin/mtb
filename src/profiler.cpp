#define MICROPROFILE_IMPL 1
#define MICROPROFILEUI_IMPL 1
#define MICROPROFILE_TEXT_WIDTH 7
#define MICROPROFILE_TEXT_HEIGHT 12
//#define MICROPROFILE_GPU_TIMERS_D3D11 1
#define _CRT_SECURE_NO_WARNINGS

#include "profiler.hpp"
#include "common.hpp"
#include "input.hpp"

//#include <microprofiledraw.h>
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#include <microprofileui.h>
#pragma warning(pop)

#include <imgui.h>
#include "imgui_impl.hpp"
#include <array>

#include <SDL_keycode.h>

cProfiler::cProfiler() {
	MicroProfileWebServerStart();
	MicroProfileSetEnableAllGroups(true);
}

cProfiler::~cProfiler() {
	MicroProfileWebServerStop();
}

void cProfiler::init_ui() {
	mpDrawList = std::make_unique<ImDrawList>(ImGui::GetDrawListSharedData());
}

void cProfiler::draw() {
	{
		cInputMgr& input = get_input_mgr();
		if (input.kbtn_released(SDL_SCANCODE_F4)) {
			MicroProfileToggleDisplayMode();
		}
		MicroProfileMousePosition(input.mMousePos.x, input.mMousePos.y, 0);
		MicroProfileMouseButton(input.mbtn_state(cInputMgr::EMBLEFT), input.mbtn_state(cInputMgr::EMBRIGHT));
	}

	if (mpDrawList) {
		const ImGuiIO& io = ImGui::GetIO();
		mpDrawList->_ResetForNewFrame();
		mpDrawList->PushTextureID(io.Fonts->TexID);
		mpDrawList->PushClipRectFullScreen();
		
		MicroProfileDraw((uint32_t)io.DisplaySize.x, (uint32_t)io.DisplaySize.y);
		
		ImDrawData drawData;
		drawData.Valid = true;

		std::array<ImDrawList*, 1> pDrawLists = { mpDrawList.get()/*, mpDrawListText.get()*/ };
		drawData.CmdLists = &pDrawLists.front();
		drawData.CmdListsCount = (int)pDrawLists.size();
		drawData.DisplayPos = ImVec2(0.0f, 0.0f);
		drawData.DisplaySize = io.DisplaySize;
		drawData.FramebufferScale = io.DisplayFramebufferScale;
		drawData.TotalVtxCount = drawData.TotalIdxCount = 0;
		for (ImDrawList* pList : pDrawLists) {
			drawData.TotalVtxCount += pList->VtxBuffer.Size;
			drawData.TotalIdxCount += pList->IdxBuffer.Size;
		}
		if (drawData.TotalVtxCount > 0) {
			cImgui::get().render_callback(&drawData);
		}
	}
}

void cProfiler::flip() {
	MicroProfileFlip();
}

void cProfiler::draw_text(int x, int y, uint32_t color, const char* pText, uint32_t numCharacters) {
	assert(mpDrawList);
	mpDrawList->AddText(ImVec2((float)x, (float)y), color, pText, pText + numCharacters);
}
void cProfiler::draw_box(int x, int y, int x1, int y1, uint32_t color, MicroProfileBoxType boxType) {
	assert(mpDrawList);

	float rounding = 0.0f;
	switch (boxType) {
	case MicroProfileBoxTypeBar: rounding = 1.0f; break;
	case MicroProfileBoxTypeFlat: break;
	}

	mpDrawList->AddRectFilled(ImVec2((float)x, (float)y), ImVec2((float)x1, (float)y1), color, rounding, ImDrawCornerFlags_All);
}
void cProfiler::draw_line_2d(uint32_t nVertices, float* pVertices, uint32_t color) {
	assert(mpDrawList);
	for (uint32_t i = 0; i < nVertices; i += 4) {
		ImVec2 a(pVertices[i + 0], pVertices[i + 1]);
		ImVec2 b(pVertices[i + 2], pVertices[i + 3]);
		mpDrawList->AddLine(a, b, color);
	}
}


void MicroProfileDrawText(int nX, int nY, uint32_t nColor, const char* pText, uint32_t nNumCharacters) {
	cProfiler::get().draw_text(nX, nY, nColor, pText, nNumCharacters);
}

void MicroProfileDrawBox(int nX, int nY, int nX1, int nY1, uint32_t nColor, MicroProfileBoxType boxType/*= MicroProfileBoxTypeFlat*/) {
	cProfiler::get().draw_box(nX, nY, nX1, nY1, nColor, boxType);
}

void MicroProfileDrawLine2D(uint32_t nVertices, float* pVertices, uint32_t nColor) {
	cProfiler::get().draw_line_2d(nVertices, pVertices, nColor);
}
