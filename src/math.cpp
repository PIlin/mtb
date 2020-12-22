#include "math.hpp"

namespace dx = DirectX;

namespace nMtx {
const DirectX::XMMATRIX g_Identity = DirectX::XMMatrixIdentity();
}

// Hermite cubic spline
// Result = (2 * t^3 - 3 * t^2 + 1) * Position0 +
//          (t^3 - 2 * t^2 + t) * Tangent0 +
//          (-2 * t^3 + 3 * t^2) * Position1 +
//          (t^3 - t^2) * Tangent1

float hermite(float pos0, float tan0, float pos1, float tan1, float t) {
	float tt = t*t;
	float ttt = tt*t;
	float tt3 = tt*3.0f;
	float tt2 = tt + tt;
	float ttt2 = tt2 * t;
	float a = ttt2 - tt3 + 1.0f;
	float b = ttt - tt2 + t;
	float c = -ttt2 + tt3;
	float d = ttt - tt;
	return a * pos0 + b * tan0 + c * pos1 + d * tan1;
}

DirectX::XMVECTOR XM_CALLCONV hermite(
	DirectX::FXMVECTOR pos0, DirectX::FXMVECTOR tan0,
	DirectX::FXMVECTOR pos1, DirectX::GXMVECTOR tan1,
	DirectX::HXMVECTOR t
	) {
	dx::XMVECTOR tt = dx::XMVectorMultiply(t, t);
	dx::XMVECTOR ttt = dx::XMVectorMultiply(tt, t);
	dx::XMVECTOR ttt2 = dx::XMVectorScale(ttt, 2.0f);
	dx::XMVECTOR tt3 = dx::XMVectorScale(tt, 3.0f);
	dx::XMVECTOR tt2 = dx::XMVectorScale(tt, 2.0f);

	dx::XMVECTOR a = dx::XMVectorSubtract(ttt2, tt3);
	dx::XMVECTOR c = dx::XMVectorNegate(a);
	a = dx::XMVectorAdd(a, dx::g_XMOne);

	dx::XMVECTOR b = dx::XMVectorSubtract(ttt, tt2);
	b = dx::XMVectorAdd(b, t);

	dx::XMVECTOR d = dx::XMVectorSubtract(ttt, tt);

	dx::XMVECTOR res = dx::XMVectorMultiply(a, pos0);
	res = dx::XMVectorMultiplyAdd(b, tan0, res);
	res = dx::XMVectorMultiplyAdd(c, pos1, res);
	return dx::XMVectorMultiplyAdd(d, tan1, res);
}

DirectX::XMVECTOR XM_CALLCONV euler_xyz_to_quat(DirectX::FXMVECTOR xyz) {
	assert(false && "doesn't work");

	dx::XMVECTOR qx = dx::XMQuaternionRotationNormal(dx::g_XMIdentityR0, dx::XMVectorGetByIndex(xyz, 0));
	dx::XMVECTOR qy = dx::XMQuaternionRotationNormal(dx::g_XMIdentityR1, dx::XMVectorGetByIndex(xyz, 1));
	dx::XMVECTOR qz = dx::XMQuaternionRotationNormal(dx::g_XMIdentityR2, dx::XMVectorGetByIndex(xyz, 2));

	dx::XMVECTOR res = dx::XMQuaternionMultiply(qx, qy);
	return dx::XMQuaternionMultiply(res, qz);
}



DirectX::XMVECTOR XM_CALLCONV euler_zyx_to_quat(DirectX::XMFLOAT3 euler) {
	// from "Technical Concepts Orientation, Rotation, Velocity and Acceleration, and the SRM"
	// https://www.sedris.org/wg8home/Documents/WG80485.pdf
	// 3.4.9 Euler angle convention to quaternion

	vec4 tmp = vec4::from_float3(euler, 0.0f);
	dx::XMVECTOR angles = dx::XMLoadFloat4(&tmp.mVal);
	angles = dx::XMVectorScale(angles, 0.5f);
	dx::XMVECTOR vs, vc;
	dx::XMVectorSinCos(&vs, &vc, angles);

	dx::XMFLOAT4 s, c;
	dx::XMStoreFloat4(&s, vs);
	dx::XMStoreFloat4(&c, vc);

	float e0 = (c.z * c.y * c.x) + (s.z * s.y * s.x);
	float e1 = (c.z * c.y * s.x) - (s.z * s.y * c.x);
	float e2 = (c.z * s.y * c.x) + (s.z * c.y * s.x);
	float e3 = (s.z * c.y * c.x) - (c.z * s.y * s.x);

	dx::XMVECTOR quat = dx::XMVectorSet(e1, e2, e3, e0);
	return quat;
}

DirectX::XMFLOAT3 XM_CALLCONV quat_to_euler_zyx(DirectX::FXMVECTOR quat) {
	dx::XMFLOAT4 q;
	dx::XMStoreFloat4(&q, quat);
	dx::XMFLOAT3 euler;

	// from "Technical Concepts Orientation, Rotation, Velocity and Acceleration, and the SRM"
	// https://www.sedris.org/wg8home/Documents/WG80485.pdf
	// 3.4.10 Quaternion to Euler angle convention
	float e0 = q.w;
	float e1 = q.x;
	float e2 = q.y;
	float e3 = q.z;

	float pri = 2.0f * (e1 * e3 - e0 * e2);
	if ((pri >= 1.0f) || (pri <= -1.0f)) {
		euler.x = 0.0f;
		euler.y = -copysignf(dx::XM_PIDIV2, pri);
		float z = atan2f((e1 * e2 - e0 * e3), (e1 * e3 + e0 * e2));
		euler.z = copysignf(z, pri);
	}
	else {
		euler.x = atan2f((e2 * e3 + e0 * e1), 0.5f - (e1 * e1 + e2 * e2));
		euler.y = asinf(-pri);
		euler.z = atan2f((e1 * e2 + e0 * e3), 0.5f - (e2 * e2 + e3 * e3));
	}

	return euler;
}

void XM_CALLCONV sXform::init(DirectX::FXMMATRIX mtx) {
	// dx::XMMatrixDecompose(&mScale, &mQuat, &mPos, mtx) does, probably, too much.
	mPos = mtx.r[3];
	mQuat = dx::XMQuaternionRotationMatrix(mtx);
	mScale = dx::g_XMOne3;
}

DirectX::XMMATRIX XM_CALLCONV sXform::build_mtx() const {
	// Simplified dx::XMMatrixAffineTransformation()
	dx::XMMATRIX scale = dx::XMMatrixScalingFromVector(mScale);
	dx::XMMATRIX rot = dx::XMMatrixRotationQuaternion(mQuat);
	dx::XMMATRIX res = dx::XMMatrixMultiply(scale, rot);
	//dx::XMMATRIX res = scale;
	res.r[3] = mPos;
	return res;
}
