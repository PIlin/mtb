#include "common.hpp"
#include "math.hpp"
#include "res/path_helpers.hpp"
#include "rdr/rdr.hpp"
#include "camera_mgr.hpp"


void cCameraManager::init(cUpdateQueue& queue) {
	mTrackballCam.init(mCamera);
	queue.add(eUpdatePriority::Camera, MAKE_UPDATE_FUNC_THIS(cCameraManager::update_cam), mCameraUpdate);
}

void cCameraManager::init(cUpdateGraph& graph) {
	mTrackballCam.init(mCamera);

	sUpdateDepRes rdrJobProlCam = graph.register_res("rdr_prologue_job", "rdr_prologue_job_cam");
	sUpdateDepRes camera = graph.register_res("global", "camera");

	graph.add(sUpdateDepDesc{ {}, {rdrJobProlCam, camera} },
		MAKE_UPDATE_FUNC_THIS(cCameraManager::update_cam), mCameraUpdate);
}

void cCameraManager::update_cam() {
	mTrackballCam.update(mCamera);
	cRdrQueueMgr::get().add_model_prologue_job(*this);
}

void cCameraManager::disp_job(cRdrContext const& rdrCtx) const {
	upload_cam(rdrCtx);
}

void cCameraManager::upload_cam(cRdrContext const& rdrCtx) const {
	auto pCtx = rdrCtx.get_ctx();
	auto& camCBuf = rdrCtx.get_cbufs().mCameraCBuf;
	camCBuf.mData.viewProj = mCamera.mView.mViewProj;
	camCBuf.mData.view = mCamera.mView.mView;
	camCBuf.mData.proj = mCamera.mView.mProj;
	camCBuf.mData.camPos = mCamera.mView.mPos;
	camCBuf.update(pCtx);
	camCBuf.set_VS(pCtx);
	camCBuf.set_PS(pCtx);
}
