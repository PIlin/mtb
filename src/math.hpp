#include <DirectXMath.h>

inline constexpr const float DEG2RAD(const float deg) {
	return deg * DirectX::XM_PI / 180.f;
}

inline constexpr const float RAD2DEG(const float rad) {
	return rad * 180.f / DirectX::XM_PI;
}


struct vec3 {
	float x, y, z;
};
struct vec4 {
	DirectX::XMFLOAT4 mVal;

	float operator[](int idx) const {
		return reinterpret_cast<float const*>(&mVal)[idx];
	}
	float& operator[](int idx) {
		return reinterpret_cast<float*>(&mVal)[idx];
	}
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

	tvec2& operator+(T s) {
		x += s;
		y += s;
		return *this;
	}
	tvec2& operator-(T s) {
		x -= s;
		y -= s;
		return *this;
	}
	tvec2& operator*(T s) {
		x *= s;
		y *= s;
		return *this;
	}
	tvec2& operator/(T s) {
		x /= s;
		y /= s;
		return *this;
	}
	
	friend tvec2 operator+(tvec2 const& a, tvec2 const& b) {
		tvec2 res(a);
		res.x += b.x;
		res.y += b.y;
		return res;
	}
	friend tvec2 operator-(tvec2 const& a, tvec2 const& b) {
		tvec2 res(a);
		res.x -= b.x;
		res.y -= b.y;
		return res;
	}
	friend tvec2 operator*(tvec2 const& a, tvec2 const& b) {
		tvec2 res(a);
		res.x *= b.x;
		res.y *= b.y;
		return res;
	}
	friend tvec2 operator/(tvec2 const& a, tvec2 const& b) {
		tvec2 res(a);
		res.x /= b.x;
		res.y /= b.y;
		return res;
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

namespace nMtx {
extern const DirectX::XMMATRIX g_Identity;
}


struct sXform {
	DirectX::XMVECTOR mPos;
	DirectX::XMVECTOR mQuat;
	DirectX::XMVECTOR mScale;

	void XM_CALLCONV init(DirectX::FXMMATRIX mtx);
	DirectX::XMMATRIX XM_CALLCONV build_mtx() const;
};

