#include "common.hpp"
#include "math.hpp"
#include "sh.hpp"
#include "path_helpers.hpp"
#include "light.hpp"
#include "rdr.hpp"
#include "gfx.hpp"
#include "imgui.hpp"

namespace dx = DirectX;

static fs::path get_light_settings_path() {
	return cPathManager::build_settings_path("light.json");
}

// See "Stupid Spherical Harmonics (SH) Tricks" by Peter-Pike Sloan, Appendix A10
void pack_sh_param(sSHCoef::sSHChan const& chan, DirectX::XMVECTOR sh[7], int idx) {
	static const float spi = (float)sqrt(DirectX::XM_PI);
	const float C0 = 1.0f / (2.0f * spi);
	const float C1 = sqrtf(3.0f) / (3.0f * spi);
	const float C2 = sqrtf(15.0f) / (8.0f  * spi);
	const float C3 = sqrtf(5.0f) / (16.0f * spi);
	const float C4 = 0.5f * C2;

	dx::XMVECTOR mul1 = dx::XMVectorSet(-C1, -C1, C1, C0);
	dx::XMVECTOR mul2 = dx::XMVectorSet(C2, -C2, 3.0f * C3, -C2);

	dx::XMVECTOR v = dx::XMVectorSet(chan[3], chan[1], chan[2], chan[0]);
	v = dx::XMVectorMultiply(v, mul1);
	v.m128_f32[3] -= C3 * chan[6];
	sh[idx] = v;

	v = dx::XMVectorSet(chan[4], chan[5], chan[6], chan[7]);
	v = dx::XMVectorMultiply(v, mul2);
	sh[idx + 3] = v;

	sh[6].m128_f32[idx] = C4*chan[8];
}

void pack_sh_param(sSHCoef const& coef, DirectX::XMVECTOR sh[7]) {
	pack_sh_param(coef.mR, sh, 0);
	pack_sh_param(coef.mG, sh, 1);
	pack_sh_param(coef.mB, sh, 2);
	sh[6].m128_f32[3] = 1.0f;
}



cLightMgr::cLightMgr() {
	if (!load(get_light_settings_path())) {
		set_default();
	}
}

void cLightMgr::set_default() {
	::memset(&mPointLights, 0, 
		LENGTHOF_ARRAY(mPointLights) * sizeof(mPointLights[0]));

	//l.mData.clr[0] = dx::XMVectorScale(l.mData.clr[0], 1.0f);
	
	mPointLights[0].pos = { { 0, 0, 1, 1 } };
	mPointLights[0].clr = { { 1, 1, 1, 1 } };
	mPointLights[0].enabled = true;

	mPointLights[1].pos = { { 0, 1, -1, 1 } };
	mPointLights[1].clr = { { 1, 1, 1, 1 } };
	mPointLights[1].enabled = true;

	mPointLights[2].pos = { { 0, 3, 0, 1 } };
	mPointLights[2].clr = { { 1, 1, 1, 1 } };
	mPointLights[2].enabled = true;

	// blossom
	//mSH.mR = { { 1.4311553239822388f, -1.1200312376022339f, -0.48424786329269409f, 0.27141338586807251f, -0.2891039252281189f, 0.5796363353729248f, -0.36794754862785339f, -0.16639560461044312f, -0.78260189294815063f } };
	//mSH.mG = { { 1.4695711135864258f, -1.2203395366668701f, -0.4916711151599884f, 0.28614127635955811f, -0.31375649571418762f, 0.58923381567001343f, -0.38483285903930664f, -0.1729036271572113f, -0.82542788982391357f } };
	//mSH.mB = { { 1.541804313659668f, -1.4233723878860474f, -0.52123433351516724f, 0.29771363735198975f, -0.35702097415924072f, 0.60223019123077393f, -0.44343027472496033f, -0.19318416714668274f, -0.94708257913589478f } };

	// blossom flip z
	mSH.mR = { { 1.4311553239822388f, -1.1200312376022339f, 0.48424786329269409f, 0.27141338586807251f, -0.2891039252281189f, -0.5796363353729248f, -0.36794754862785339f, 0.16639560461044312f, -0.78260189294815063f } };
	mSH.mG = { { 1.4695711135864258f, -1.2203395366668701f, 0.4916711151599884f, 0.28614127635955811f, -0.31375649571418762f, -0.58923381567001343f, -0.38483285903930664f, 0.1729036271572113f, -0.82542788982391357f } };
	mSH.mB = { { 1.541804313659668f, -1.4233723878860474f, 0.52123433351516724f, 0.29771363735198975f, -0.35702097415924072f, -0.60223019123077393f, -0.44343027472496033f, 0.19318416714668274f, -0.94708257913589478f } };

	// subway
	//mSH.mR = { { 2.9015493392944336f, -0.97448551654815674f, -0.035802226513624191f, 0.5218314528465271f, -0.56667429208755493f, -0.15583968162536621f, 0.5977063775062561f, 0.37468424439430237f, 0.82314270734786987f } };
	//mSH.mG = { { 2.691716194152832f, -0.94804292917251587f, -0.073127821087837219f, 0.52174961566925049f, -0.55379670858383179f, -0.11602098494768143f, 0.57575201988220215f, 0.34524986147880554f, 0.79096674919128418f } };
	//mSH.mB = { { 2.4294383525848389f, -0.92562246322631836f, -0.11760011315345764f, 0.51144671440124512f, -0.54375362396240234f, -0.069152571260929108f, 0.54059600830078125f, 0.20570482313632965f, 0.75606906414031982f } };

}

void cLightMgr::update() {
	dbg_ui();

	auto& lcbuf = cConstBufStorage::get().mLightCBuf;
	::memset(&lcbuf.mData, 0, sizeof(lcbuf.mData));

	for (int i = 0, j = 0; i < LENGTHOF_ARRAY(mPointLights); ++i) {
		if (j >= sLightCBuf::MAX_LIGHTS) { break; }

		auto const& l = mPointLights[i];
		if (l.enabled) {
			::memcpy(&lcbuf.mData.pos[j], &l.pos, sizeof(l.pos));
			::memcpy(&lcbuf.mData.clr[j], &l.clr, sizeof(l.clr));
			lcbuf.mData.isEnabled[j] = true;
			++j;
		}
	}

	pack_sh_param(mSH, lcbuf.mData.sh);

	auto* pCtx = get_gfx().get_ctx();
	lcbuf.update(pCtx);
	lcbuf.set_PS(pCtx);
}

void cLightMgr::dbg_ui() {
	ImGui::Begin("light");
	char buf[64];
	for (int i = 0; i < LENGTHOF_ARRAY(mPointLights); ++i) {
		auto& pl = mPointLights[i];

		::sprintf_s(buf, "point #%d", i);
		if (ImGui::CollapsingHeader(buf)) {
			ImGui::PushID(buf);

			ImGui::SliderFloat3("pos", reinterpret_cast<float*>(&pl.pos), -5.0f, 5.0f);
			ImguiSlideFloat3_1("clr", reinterpret_cast<float*>(&pl.clr), 0.0f, 1.0f);
			ImGui::Checkbox("enabled", &pl.enabled);

			ImGui::PopID();
		}
	}

	if (ImGui::Button("Save")) {
		save(get_light_settings_path());
	}

	ImGui::End();
}

