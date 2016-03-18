#include <memory>

#include "math.hpp"
#include "common.hpp"
#include "texture.hpp"
#include "rdr.hpp"
#include "imgui_impl.hpp"
#include "gfx.hpp"
#include "input.hpp"

#include <imgui.h>

#include <SDL_keycode.h>

static char const* const g_vtxShader = 
"cbuffer Cam : register(b0) {"
"    row_major float4x4 g_MVP; "
"};"
"struct sVtx {"
"    float2 pos : POSITION;"
"    float2 uv  : TEXCOORD;"
"    float4 clr : COLOR;"
"};"
"struct sPix {"
"    float4 pos : SV_POSITION;"
"    float4 clr : COLOR0;"
"    float2 uv  : TEXCOORD;"
"};"
"sPix main(sVtx input) {"
"    sPix output;"
"    output.pos = mul(float4(input.pos.xy, 0.f, 1.f), g_MVP);"
"    output.clr = input.clr;"
"    output.uv  = input.uv;"
"    return output;"
"}";

static char const* const g_pixShader =
"struct sPix {"
"    float4 pos : SV_POSITION;"
"    float4 clr : COLOR0;"
"    float2 uv  : TEXCOORD;"
"};"
"sampler g_smp : register(s0);"
"Texture2D g_tex : register(t0);"
"float4 main(sPix input) : SV_Target {"
"    float4 out_col = input.clr * g_tex.Sample(g_smp, input.uv);"
"    return out_col;"
"}";


struct sImguiVtx {
	vec2f    pos;
	vec2f    uv;
	uint32_t clr;
};


cImgui::cImgui(cGfx& gfx) {
	auto pDev = gfx.get_dev();
	vec2f winSize = get_window_size();

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(winSize.x, winSize.y);
	io.DeltaTime = 1.0f / 60.0f;
	io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_UP;
	io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
	io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
	io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
	io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
	io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
	io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
	io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
	io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
	io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
	io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;
	io.RenderDrawListsFn = render_callback_st;

	mVtx.init_write_only(pDev, 10000, sizeof(sImguiVtx));

	auto& ss = cShaderStorage::get();
	mpVS = ss.create_VS(g_vtxShader, "imgui_vs");
	mpPS = ss.create_PS(g_pixShader, "imgui_ps");

	D3D11_INPUT_ELEMENT_DESC vdsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(sImguiVtx, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(sImguiVtx, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(sImguiVtx, clr), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	auto& code = mpVS->get_code();
	HRESULT hr = pDev->CreateInputLayout(vdsc, LENGTHOF_ARRAY(vdsc), code.get_code(), code.get_size(), mpIL.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

	load_fonts();
}

void cImgui::load_fonts() {
	ImGuiIO& io = ImGui::GetIO();

	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	if (mFontTex.create2d1_rgba_u8(get_gfx().get_dev(), pixels, width, height)) {
		io.Fonts->TexID = (void*)&mFontTex;
	}
}

cImgui::~cImgui() {
	ImGui::Shutdown();
}

static ImVec2 as_ImVec2(vec2i v) {
	vec2f f = v;
	return ImVec2(f.x, f.y);
}

void cImgui::update() {
	ImGuiIO& io = ImGui::GetIO();

	auto& input = get_input_mgr();

	io.MousePos = as_ImVec2(input.mMousePos);
	for (int i = 0; i < cInputMgr::EMBLAST && i < LENGTHOF_ARRAY(io.MouseDown); ++i) {
		io.MouseDown[i] = input.mbtn_state((cInputMgr::eMouseBtn)i);
	}

	for (int i = 0; i < cInputMgr::KEYS_COUNT && i < LENGTHOF_ARRAY(io.KeysDown); ++i) {
		io.KeysDown[i] = input.kbtn_state(i);
	}
	io.KeyCtrl = input.kmod_state(KMOD_CTRL);
	io.KeyShift = input.kmod_state(KMOD_SHIFT);

	auto const& text = input.get_textinput();
	for (auto c : text) {
		auto imchar = (ImWchar)c;
		io.AddInputCharacter(imchar);
	}

	ImGui::NewFrame();

	input.enable_textinput(io.WantCaptureKeyboard);
}

void cImgui::disp() {
	ImGui::Render();
}

void cImgui::render_callback_st(ImDrawList** const draw_lists, int count) {
	get().render_callback(draw_lists, count);
}

static DirectX::XMMATRIX calc_imgui_ortho(ImGuiIO& io) {
	const float L = 0.0f;
	const float R = io.DisplaySize.x;
	const float B = io.DisplaySize.y;
	const float T = 0.0f;
	const float mvp[4][4] =
	{
		{ 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f / (T - B), 0.0f, 0.0f, },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
	};
	return DirectX::XMMATRIX(&mvp[0][0]);
	//return DirectX::XMMatrixOrthographicRH(io.DisplaySize.x, io.DisplaySize.y, 0.0f, 1.0f);
}

void cImgui::render_callback(ImDrawList** const drawLists, int count) {
	ImGuiIO& io = ImGui::GetIO();
	auto& gfx = get_gfx();
	auto pCtx = gfx.get_ctx();

	{
		auto vm = mVtx.map(pCtx);
		if (!vm.is_mapped())
			return;

		sImguiVtx* pVtx = reinterpret_cast<sImguiVtx*>(vm.data());
		for (int i = 0; i < count; ++i) {
			ImDrawList const* list = drawLists[i];
			ImDrawVert const* imvtx = &list->vtx_buffer[0];
			static_assert(sizeof(sImguiVtx) == sizeof(imvtx[0]), "Vertices sizes differ");

			size_t vtxCount = list->vtx_buffer.size();
			::memcpy(pVtx, imvtx, vtxCount * sizeof(pVtx[0]));
			pVtx += vtxCount;
		}
	}

	auto& cb = cConstBufStorage::get().mImguiCameraCBuf;
	cb.mData.proj = calc_imgui_ortho(io);
	cb.update(pCtx);
	cb.set_VS(pCtx);

	cRasterizerStates::get().set_imgui(pCtx);
	cBlendStates::get().set_imgui(pCtx);
	cDepthStencilStates::get().set_imgui(pCtx);

	pCtx->IASetInputLayout(mpIL);
	pCtx->VSSetShader(mpVS->asVS(), nullptr, 0);
	pCtx->PSSetShader(mpPS->asPS(), nullptr, 0);

	auto smp = cSamplerStates::get().linear();
	pCtx->PSSetSamplers(0, 1, &smp);

	pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mVtx.set(pCtx, 0, 0);

	int vtxOffset = 0;
	for (int i = 0; i < count; ++i) {
		ImDrawList const* list = drawLists[i];
		for (size_t cmd = 0; cmd < list->commands.size(); ++cmd) {
			ImDrawCmd const* pCmd = &list->commands[cmd];
			
			const D3D11_RECT rect = { 
				(LONG)pCmd->clip_rect.x, (LONG)pCmd->clip_rect.y,
				(LONG)pCmd->clip_rect.z, (LONG)pCmd->clip_rect.w };
			pCtx->RSSetScissorRects(1, &rect);

			cTexture* pTex = reinterpret_cast<cTexture*>(pCmd->texture_id);
			if (pTex) {
				pTex->set_PS(pCtx, 0);
			}

			pCtx->Draw(pCmd->vtx_count, vtxOffset);
			vtxOffset += pCmd->vtx_count;
		}
	}
}


bool ImguiSlideFloat3_1(char const* label, float v[3], float v_min, float v_max, const char* display_format = "%.3f", float power = 1.0f) {
	char keyBuf[64];
	::sprintf_s(keyBuf, "issng%s", label);
	ImGuiID id = ImGui::GetID(keyBuf);
	bool res;
	auto* pStorage = ImGui::GetStateStorage();
	auto& style = ImGui::GetStyle();
	bool isSingle = !!pStorage->GetInt(id);
	if (isSingle) {
		::sprintf_s(keyBuf, "##%s", label);
		res = ImGui::SliderFloat(keyBuf, v, v_min, v_max, display_format, power);
		if (res) {
			for (int i = 1; i < 3; ++i) {
				v[i] = v[0];
			}
		}
		ImGui::SameLine(0, (int)style.ItemInnerSpacing.x);
		ImGui::TextUnformatted(label);
	}
	else {
		res = ImGui::SliderFloat3(label, v, v_min, v_max, display_format, power);
	}

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))	{
		int v = !isSingle;
		pStorage->SetInt(id, v);
	}

	return res;
}

