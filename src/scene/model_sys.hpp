#pragma once
#include "rdr/rdr_queue.hpp"
#include "update_queue.hpp"
#include <entt/fwd.hpp>

class cRdrContext;
class cSceneCompMetaReg;

class cModelDispSys : public iRdrJob {
	cUpdateSubscriberScope mUpdateBegin;
	cUpdateSubscriberScope mUpdateEnd;
	cUpdateSubscriberScope mDispUpdate;

	struct sPrologueJob : public iRdrJob {
		virtual void disp_job(cRdrContext const& ctx) const override;
	};

	entt::registry& mRegistry;
	sPrologueJob mPrologueJob;
public:

	cModelDispSys(entt::registry& reg) : mRegistry(reg) {}

	void register_update(cUpdateQueue& queue);
	void update_begin();
	void disp();
	void update_end();

	virtual void disp_job(cRdrContext const& ctx) const override;

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);

private:
	void disp_solid(cRdrContext const& ctx) const;
	void disp_skinned(cRdrContext const& ctx) const;
};





