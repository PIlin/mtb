#include "common.hpp"
#include "math.hpp"
#include "rig_component.hpp"

void XM_CALLCONV cRigComp::update_rig_mtx(DirectX::XMMATRIX wmtx) {
	mRig.calc_local();
	mRig.calc_world(wmtx);
}

void cRigComp::upload_skin(cRdrContext const& ctx) const { mRig.upload_skin(ctx); }

