#pragma once

#include "math.hpp"

namespace std::filesystem { class path; }
namespace fs = std::filesystem;

struct sCameraView {
	DirectX::XMMATRIX mView;
	DirectX::XMMATRIX mProj;
	DirectX::XMMATRIX mViewProj;
	DirectX::XMVECTOR mPos;

	void calc_view(DirectX::XMVECTOR const& pos, DirectX::XMVECTOR const& tgt, DirectX::XMVECTOR const& up);
	void calc_proj(float fovY, float aspect, float nearZ, float farZ);
	void calc_viewProj();
};


class cCamera {
public:
	using sView = sCameraView;

	DirectX::XMVECTOR mPos;
	DirectX::XMVECTOR mTgt;
	DirectX::XMVECTOR mUp;

	sView mView;

	float mFovY;
	float mAspect;
	float mNearZ;
	float mFarZ;

public:
	cCamera();
	~cCamera();

	void set_default();
	void recalc();

	void set_aspect_from_window_size(vec2f windowSize);

	void norm_up();
	bool dbg_edit();
public:
	bool save(const fs::path& filepath);
	bool load(const fs::path& filepath);

	template <class Archive>
	void serialize(Archive& arc);
};

class cTrackball {
public:
	DirectX::XMVECTOR mQuat;
	DirectX::XMVECTOR mSpin;

	cTrackball() {
		set_home();
	}

	void update(vec2i pos, vec2i prev, float r = 0.8f);
	void update(vec2f pos, vec2f prev, float r = 0.8f);
	void apply(cCamera& cam, DirectX::XMVECTOR dir) const;
	void set_home();
};

class cTrackballCam {
	cTrackball tb;
	bool mCatchInput = false;
	vec2f mWindowSize;
public:
	void init(const cCamera& cam);
	void update(cCamera& cam);
protected:
	bool update_trackball(cCamera& cam);
	bool update_distance(cCamera& cam);
	bool update_translation(cCamera& cam);
};

struct sWindowSettings {
public:
	vec2i mSize;

	bool save(const fs::path& filepath);
	bool load(const fs::path& filepath);

	template <class Archive>
	void serialize(Archive& arc);
};

class cRdrContext;
namespace nRdrHelpers {
	void upload_cam_cbuf(cRdrContext const& rdrCtx, sCameraView const& camView);
}
