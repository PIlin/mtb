#pragma once

#include "rdr/camera.hpp"
#include "rdr/rdr_queue.hpp"
#include "update_queue.hpp"

class cRdrContext;

class cCameraManager : public iRdrJob {
	cCamera mCamera;
	cTrackballCam mTrackballCam;
	cUpdateSubscriberScope mCameraUpdate;

public:
	void init(cUpdateQueue& queue);

	void update_cam();

	virtual void disp_job(cRdrContext const& rdrCtx) const override;

	void upload_cam(cRdrContext const& rdrCtx) const;

	const cCamera& get_cam() const { return mCamera; }
};
