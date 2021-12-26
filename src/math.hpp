#pragma once
#include <DirectXMath.h>

inline float DEG2RAD(const float deg) {
	return deg * DirectX::XM_PI / 180.f;
}

inline float RAD2DEG(const float rad) {
	return rad * 180.f / DirectX::XM_PI;
}


struct vec3 {
	float x, y, z;

	static vec3 XM_CALLCONV from_vector(DirectX::FXMVECTOR vec) {
		vec3 res;
		DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&res), vec);
		return res;
	}
};
struct vec4 {
	DirectX::XMFLOAT4 mVal;

	static vec4 from_float3(DirectX::XMFLOAT3 f3, float w) {
		return vec4{ { f3.x, f3.y, f3.z, w } };
	}

	float operator[](int idx) const {
		return reinterpret_cast<float const*>(&mVal)[idx];
	}
	float& operator[](int idx) {
		return reinterpret_cast<float*>(&mVal)[idx];
	}

	DirectX::XMFLOAT3 xyz() const { return DirectX::XMFLOAT3(mVal.x, mVal.y, mVal.z); }
};
struct vec4i {
	DirectX::XMINT4 mVal;

	int32_t operator[](int idx) const {
		return reinterpret_cast<int32_t const*>(&mVal)[idx];
	}
	int32_t& operator[](int idx) {
		return reinterpret_cast<int32_t*>(&mVal)[idx];
	}
};

template <typename T>
struct tvec2 {
	T x;
	T y;

	tvec2() {}
	tvec2(T x, T y) : x(x), y(y) {}
	tvec2(tvec2 const&) = default;
	tvec2& operator=(tvec2 const&) = default;

	template <typename U>
	operator tvec2<U>() {
		return{ static_cast<U>(x), static_cast<U>(y) };
	}

	bool operator==(tvec2 const& o) const {
		return (x == o.x) && (y == o.y);
	}
	bool operator!=(tvec2 const& o) const {
		return !(*this == o);
	}

	tvec2 operator+(T s) const {
		return tvec2(x + s, y + s);
	}
	tvec2 operator-(T s) const {
		return tvec2(x - s, y - s);
	}
	tvec2 operator*(T s) const {
		return tvec2(x * s, y * s);
	}
	tvec2 operator/(T s) const {
		return tvec2(x / s, y / s);
	}

	tvec2& operator+=(tvec2 const& v) {
		x += v.x;
		y += v.y;
		return *this;
	}
	tvec2& operator-=(tvec2 const& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}
	tvec2& operator*=(tvec2 const& v) {
		x *= v.x;
		y *= v.y;
		return *this;
	}
	tvec2& operator/=(tvec2 const& v) {
		x /= v.x;
		y /= v.y;
		return *this;
	}
	
	friend tvec2 operator+(tvec2 a, tvec2 const& b) {
		return a += b;
	}
	friend tvec2 operator-(tvec2 a, tvec2 const& b) {
		return a -= b;
	}
	friend tvec2 operator*(tvec2 a, tvec2 const& b) {
		return a *= b;
	}
	friend tvec2 operator/(tvec2 a, tvec2 const& b) {
		return a /= b;
	}
	
};

using vec2i = tvec2 < int32_t > ;
using vec2f = tvec2 < float >;

template <typename T>
T clamp(T x, T min, T max) {
	return std::max(min, std::min(x, max));
}

template <typename T>
T lerp(T a, T b, T t) {
	return (1.0f - t) * a + t * b;
}

float hermite(float pos0, float tan0, float pos1, float tan1, float t);

DirectX::XMVECTOR XM_CALLCONV hermite(
	DirectX::FXMVECTOR pos0, DirectX::FXMVECTOR tan0,
	DirectX::FXMVECTOR pos1, DirectX::GXMVECTOR tan1,
	DirectX::HXMVECTOR t
);

DirectX::XMVECTOR XM_CALLCONV euler_xyz_to_quat(DirectX::FXMVECTOR xyz);

DirectX::XMVECTOR XM_CALLCONV euler_zyx_to_quat(DirectX::XMFLOAT3 euler);
DirectX::XMFLOAT3 XM_CALLCONV quat_to_euler_zyx(DirectX::FXMVECTOR quat);

namespace nMtx {
extern const DirectX::XMMATRIX g_Identity;
extern const DirectX::XMFLOAT4X4A g_IdentityF;
}


struct sXform {
	DirectX::XMVECTOR mPos;
	DirectX::XMVECTOR mQuat;
	DirectX::XMVECTOR mScale;

	static sXform XM_CALLCONV identity();

	void XM_CALLCONV init(DirectX::FXMMATRIX mtx);
	void XM_CALLCONV init_scaled(DirectX::FXMMATRIX mtx);
	DirectX::XMMATRIX XM_CALLCONV build_mtx() const;
};
