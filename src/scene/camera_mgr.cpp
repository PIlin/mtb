#include "common.hpp"
#include "math.hpp"
#include "res/path_helpers.hpp"
#include "rdr/rdr.hpp"
#include "rdr/dbg_draw.hpp"
#include "camera_mgr.hpp"
#include "dbg_ui.hpp"
#include "imgui.hpp"

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

	if (bool& isOpen = cDbgToolsMgr::tools().camera_mgr) {
		if (ImGui::Begin("Camera mgr", &isOpen)) {
			if (mCamera.dbg_edit()) {
				mTrackballCam.init(mCamera);
			}
		}
		ImGui::End();
	}

	cRdrQueueMgr::get().add_model_prologue_job(*this);
	cDbgDrawMgr::get().set_camera(mCamera);
}

void cCameraManager::disp_job(cRdrContext const& rdrCtx) const {
	upload_cam(rdrCtx);
}

void cCameraManager::upload_cam(cRdrContext const& rdrCtx) const {
	nRdrHelpers::upload_cam_cbuf(rdrCtx, mCamera.mView);
}
