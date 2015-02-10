class cCamera {
public:
	struct sView {
		DirectX::XMMATRIX mView;
		DirectX::XMMATRIX mProj;
		DirectX::XMMATRIX mViewProj;
		DirectX::XMVECTOR mPos;

		void calc_view(DirectX::XMVECTOR const& pos, DirectX::XMVECTOR const& tgt, DirectX::XMVECTOR const& up);
		void calc_proj(float fovY, float aspect, float nearZ, float farZ);
		void calc_viewProj();
	};

	DirectX::XMVECTOR mPos;
	DirectX::XMVECTOR mTgt;
	DirectX::XMVECTOR mUp;

	sView mView;

	float mFovY;
	float mAspect;
	float mNearZ;
	float mFarZ;

public:
	cCamera() {
		set_default();
	}

	void set_default();
	void recalc();
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
	void apply(cCamera& cam);
	void apply(cCamera& cam, DirectX::XMVECTOR dir);
	void set_home();
};

class cTrackballCam {
	cTrackball tb;
	bool mCatchInput = false;
public:
	void init(cCamera& cam);
	void update(cCamera& cam);
protected:
	bool update_trackball(cCamera& cam);
	bool update_distance(cCamera& cam);
	bool update_translation(cCamera& cam);
};
