#include "math.hpp"
#include "camera.hpp"
#include "common.hpp"
#include "input.hpp"

#include <SDL_keycode.h>

namespace dx = DirectX;

void cCamera::sView::calc_view(dx::XMVECTOR const& pos, dx::XMVECTOR const& tgt, dx::XMVECTOR const& up) {
	mView = dx::XMMatrixLookAtRH(pos, tgt, up);
	mPos = pos;
}

void cCamera::sView::calc_proj(float fovY, float aspect, float nearZ, float farZ) {
	mProj = dx::XMMatrixPerspectiveFovRH(fovY, aspect, nearZ, farZ);
}

void cCamera::sView::calc_viewProj() {
	mViewProj = mView * mProj;
}

void cCamera::set_default() {
	//mPos = dx::XMVectorSet(1.0f, 2.0f, 3.0f, 1.0f);
	mPos = dx::XMVectorSet(0.5f, 0.1f, 1.0f, 1.0f);
	mTgt = dx::XMVectorSet(0.0f, 0.1f, 0.0f, 1.0f);
	mUp = dx::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);


	mFovY = DEG2RAD(45.0f);
	//mFovY = dx::XM_PIDIV2;
	mAspect = 640.0f / 480.0f;
	mNearZ = 0.001f;
	mFarZ = 1000.0f;

	recalc();
}

void cCamera::recalc() {
	mView.calc_view(mPos, mTgt, mUp);
	mView.calc_proj(mFovY, mAspect, mNearZ, mFarZ);
	mView.calc_viewProj();
}

extern vec2i get_window_size();
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

void cTrackball::apply(cCamera& cam, DirectX::XMVECTOR dir) {
	dir = dx::XMVector3Rotate(dir, mQuat);
	cam.mPos = dx::XMVectorAdd(cam.mTgt, dir);

	cam.mUp = dx::XMVector3Rotate(dx::g_XMIdentityR1, mQuat);
}

void cTrackball::set_home() {
	mQuat = dx::XMQuaternionIdentity();
	mSpin = dx::XMQuaternionIdentity();
}

void cTrackballCam::update(cCamera& cam) {
	auto& input = get_input_mgr();
	if (input.kbtn_pressed(SDL_SCANCODE_F1)) {
		mCatchInput = !mCatchInput;
	}
	if (!mCatchInput && !input.kbtn_state(SDL_SCANCODE_SPACE)) {
		return;
	}
	bool updated = false;
	updated = updated || update_trackball(cam);
	updated = updated || update_distance(cam);
	updated = updated || update_translation(cam);
	if (updated) {
		cam.recalc();
	}
}

void cTrackballCam::init(cCamera& cam) {
	auto dir = dx::XMVectorSubtract(cam.mPos, cam.mTgt);
	auto angle = dx::XMVector3AngleBetweenVectors(dir, dx::g_XMIdentityR2);
	auto a = dx::XMVectorGetX(angle);
	tb.mQuat = dx::XMQuaternionRotationAxis(cam.mUp, a);
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
