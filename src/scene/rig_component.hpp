#pragma once

#include "math.hpp"
#include "rig.hpp"

struct sSceneEditCtx;

class cRigComp {
	cRig mRig;
public:

	cRigComp() = default;
	cRigComp(cRigComp&& other) : mRig(std::move(other.mRig)) {}
	cRigComp& operator=(cRigComp&&) = default;

	void init(ConstRigDataPtr pRigData) {
		mRig.init(std::move(pRigData));
	}

	void XM_CALLCONV update_rig_mtx(DirectX::XMMATRIX wmtx);

	void upload_skin(cRdrContext const& ctx) const;

	void dbg_ui(sSceneEditCtx& ctx) { }

	cRig& get() { return mRig; }
	const cRig& get() const { return mRig; }
};
