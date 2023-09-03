#include "math.hpp"
#include "common.hpp"
#include "res/path_helpers.hpp"
#include "camera.hpp"
#include "cbufs.hpp"
#include "rdr.hpp"
#include "imgui.hpp"

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <SDL_keycode.h>
CLANG_DIAG_POP

import input;

namespace dx = DirectX;

extern vec2i get_window_size();


void sCameraView::calc_view(dx::XMVECTOR const& pos, dx::XMVECTOR const& tgt, dx::XMVECTOR const& up) {
	mView = dx::XMMatrixLookAtRH(pos, tgt, up);
	mPos = pos;
}

void sCameraView::calc_proj(float fovY, float aspect, float nearZ, float farZ) {
	mProj = dx::XMMatrixPerspectiveFovRH(fovY, aspect, nearZ, farZ);
}

void sCameraView::calc_viewProj() {
	mViewProj = mView * mProj;
}

static fs::path get_camera_settings_path() {
	return cPathManager::build_settings_path("camera.json");
}

cCamera::cCamera() {
	if (load(get_camera_settings_path())) {
		set_aspect_from_window_size(get_window_size());
		recalc();
	} else {
		set_default();
	}
}

cCamera::~cCamera() {
	save(get_camera_settings_path());
}

void cCamera::set_default() {
	mPos = dx::XMVectorSet(1.0f, 2.0f, 3.0f, 1.0f);
	mTgt = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	mUp = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

	norm_up();

	mFovY = DEG2RAD(45.0f);
	//mFovY = dx::XM_PIDIV2;

	set_aspect_from_window_size(get_window_size());

	mNearZ = 0.01f;
	mFarZ = 300.0f;

	recalc();
}

void cCamera::recalc() {
	mView.calc_view(mPos, mTgt, mUp);
	mView.calc_proj(mFovY, mAspect, mNearZ, mFarZ);
	mView.calc_viewProj();
}

void cCamera::set_aspect_from_window_size(vec2f windowSize) {
	mAspect = windowSize.x / windowSize.y;
}

void cCamera::norm_up() {
	auto dir = dx::XMVectorSubtract(mPos, mTgt);
	auto x = dx::XMVector3Cross(mUp, dir);
	mUp = dx::XMVector3Cross(dir, x);
	mUp = dx::XMVector3Normalize(mUp);
}

bool cCamera::dbg_edit() {
	bool updated = false;
	updated = ImguiDragXmVector3("cam.mPos", mPos) || updated;
	updated = ImguiDragXmVector3("cam.mTgt", mTgt) || updated;
	updated = ImguiDragXmVector3("cam.mUp", mUp) || updated;

	if (updated) {
		norm_up();
	}

	updated = ImGui::SliderAngle("fovY", &mFovY, 1.0f, 180.0f) || updated;
	updated = ImGui::SliderFloat("aspect", &mAspect, 0.01f, 10.0f) || updated;
	updated = ImGui::SliderFloat("nearZ", &mNearZ, 0.0001f, 10000.0f, "%.5f", ImGuiSliderFlags_Logarithmic) || updated;
	updated = ImGui::SliderFloat("farZ", &mFarZ, 0.0001f, 10000.0f, "%.5f", ImGuiSliderFlags_Logarithmic) || updated;

	if (ImGui::Button("Set default")) {
		set_default();
		return true;
	}

	if (updated) {
		recalc();
	}
	return updated;
}

void cTrackball::update(vec2i pos, vec2i prev, float r) {
	vec2f fpos = pos;
	vec2f fprev = prev;
	vec2f size = get_window_size();

	fpos = ((fpos / size) - 0.5f) * 2.0f;
	fprev = ((fprev / size) - 0.5f) * 2.0f;

	fpos.y = -fpos.y;
	fprev.y = -fprev.y;

	update(fpos, fprev, r);
}

dx::XMVECTOR project_sphere_hyphsheet(vec2f p, float r) {
	float rr = r * r;
	float rr2 = rr * 0.5f;
	float sq = p.x*p.x + p.y*p.y;
	float z;
	bool onSp = false;
	if (sq <= rr2) {
		onSp = true;
		z = ::sqrtf(rr - sq);
	} else {
		z = rr2 / ::sqrtf(sq);
	}

	return dx::XMVectorSet(p.x, p.y, z, 0.0f);
}

void cTrackball::update(vec2f pos, vec2f prev, float r) {
	auto p0 = project_sphere_hyphsheet(prev, r);
	auto p1 = project_sphere_hyphsheet(pos, r);
	
	auto dir = dx::XMVectorSubtract(p0, p1);
	auto axis = dx::XMVector3Cross(p1, p0);

	float t = dx::XMVectorGetX(dx::XMVector4Length(dir)) / (2.0f * r);
	t = clamp(t, -1.0f, 1.0f);
	float angle = 2.0f * ::asinf(t);

	auto spin = dx::XMQuaternionRotationAxis(axis, angle);
	auto quat = dx::XMQuaternionMultiply(spin, mQuat);
	mQuat = dx::XMQuaternionNormalize(quat);
	mSpin = spin;
}

void cTrackball::apply(cCamera& cam, DirectX::XMVECTOR dir) const {
	dir = dx::XMVector3Rotate(dir, mQuat);
	cam.mPos = dx::XMVectorAdd(cam.mTgt, dir);

	cam.mUp = dx::XMVector3Rotate(dx::g_XMIdentityR1, mQuat);
}

void cTrackball::set_home() {
	mQuat = dx::XMQuaternionIdentity();
	mSpin = dx::XMQuaternionIdentity();
}

void cTrackballCam::update(cCamera& cam) {
	bool bNeedsUpdate = false;
	bool updated = false;

	const vec2f newSize = get_window_size();
	if (newSize != mWindowSize) {
		mWindowSize = newSize;
		cam.set_aspect_from_window_size(newSize);
		bNeedsUpdate = true;
		updated = true;
	}

	auto& input = get_input_mgr();

	if (!input.is_locked() || input.is_locked(eInputLock::Camera)) {
		if (input.kbtn_pressed(SDL_SCANCODE_F1)) {
			mCatchInput = !mCatchInput;
		}
		if (mCatchInput || input.kbtn_held(SDL_SCANCODE_SPACE)) {
			bNeedsUpdate = true;
		}

		if (bNeedsUpdate) {
			bNeedsUpdate = input.try_lock(eInputLock::Camera);
		}
		else {
			input.unlock(eInputLock::Camera);
		}

		if (bNeedsUpdate) {
			updated = updated || update_trackball(cam);
			updated = updated || update_distance(cam);
			updated = updated || update_translation(cam);
		}
	}

	if (updated) {
		cam.recalc();
	}
}

void cTrackballCam::init(const cCamera& cam) {
	dx::XMMATRIX mtx;
	auto dir = dx::XMVectorSubtract(cam.mPos, cam.mTgt);
	mtx.r[0] = dx::XMVector3Normalize(dx::XMVector3Cross(cam.mUp, dir));
	mtx.r[1] = dx::XMVector3Normalize(cam.mUp);
	mtx.r[2] = dx::XMVector3Normalize(dir);
	mtx.r[3] = dx::g_XMIdentityR3;
	tb.mQuat = dx::XMQuaternionRotationMatrix(mtx);

	mWindowSize = get_window_size();
}

bool cTrackballCam::update_trackball(cCamera& cam) {
	auto& input = get_input_mgr();
	//const auto btn = cInputMgr::EMBMIDDLE;
	const auto btn = cInputMgr::EMBLEFT;
	if (!input.mbtn_state(btn)) return false;
	auto now = input.mMousePos;
	auto prev = input.mMousePosPrev;
	if (now == prev) return false;

	tb.update(now, prev);

	auto dir = dx::XMVectorSubtract(cam.mPos, cam.mTgt);
	float dist = dx::XMVectorGetX(dx::XMVector4Length(dir));
	dir = dx::XMVectorScale(dx::g_XMIdentityR2, dist);

	tb.apply(cam, dir);
	return true;
}

bool cTrackballCam::update_distance(cCamera& cam) {
	auto& input = get_input_mgr();
	const auto btn = cInputMgr::EMBRIGHT;
	if (!input.mbtn_held(btn)) return false;

	auto pos = input.mMousePos;
	auto prev = input.mMousePosPrev;

	int dy = pos.y - prev.y;
	if (dy == 0) return false;
	float scale;
	float speed = 0.08f;
	if (dy < 0) {
		scale = 1.0f - ::log10f((float)clamp(-dy, 1, 10)) * speed;
	}
	else {
		scale = 1.0f + ::log10f((float)clamp(dy, 1, 10)) * speed;
	}

	auto dir = dx::XMVectorSubtract(cam.mPos, cam.mTgt);
	dir = dx::XMVectorScale(dir, scale);
	cam.mPos = dx::XMVectorAdd(dir, cam.mTgt);

	return true;
}

bool cTrackballCam::update_translation(cCamera& cam) {
	auto& input = get_input_mgr();
	const auto btn = cInputMgr::EMBMIDDLE;
	//const auto btn = cInputMgr::EMBLEFT;
	if (!input.mbtn_held(btn)) return false;

	auto pos = input.mMousePos;
	auto prev = input.mMousePosPrev;

	auto dt = pos - prev;
	if (dt == vec2i(0, 0)) return false;

	vec2f dtf = dt;
	dtf = dtf * 0.001f;

	auto cpos = cam.mPos;
	auto ctgt = cam.mTgt;
	auto cup = cam.mUp;
	auto cdir = dx::XMVectorSubtract(cpos, ctgt);
	auto side = dx::XMVector3Cross(cdir, cup); // reverse direction

	float len = dx::XMVectorGetX(dx::XMVector3LengthEst(cdir));

	auto move = dx::XMVectorSet(dtf.x, dtf.y * len, 0, 0);
	//move = dx::XMVectorScale(move, len);

	dx::XMMATRIX b(side, cup, cdir, dx::g_XMZero);
	move = dx::XMVector3Transform(move, b);

	cam.mPos = dx::XMVectorAdd(cpos, move);
	cam.mTgt = dx::XMVectorAdd(ctgt, move);

	return true;
}

namespace nRdrHelpers {
	void upload_cam_cbuf(cRdrContext const& rdrCtx, sCameraView const& camView) {
		auto pCtx = rdrCtx.get_ctx();
		auto& camCBuf = rdrCtx.get_cbufs().mCameraCBuf;
		camCBuf.mData.viewProj = camView.mViewProj;
		camCBuf.mData.view = camView.mView;
		camCBuf.mData.proj = camView.mProj;
		camCBuf.mData.camPos = camView.mPos;
		camCBuf.update(pCtx);
		camCBuf.set_VS(pCtx);
		camCBuf.set_PS(pCtx);
	}
}

