#pragma once
#include "rdr_queue.hpp"
#include "update_queue.hpp"
#include <entt/fwd.hpp>

class cRdrContext;
class cSceneCompMetaReg;

class cModelDispSys : public iRdrJob {
	cUpdateSubscriberScope mDispUpdate;
	entt::registry& mRegistry;
public:

	cModelDispSys(entt::registry& reg) : mRegistry(reg) {}

	void register_disp_update();
	void disp();

	virtual void disp_job(cRdrContext const& ctx) const override;

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);

private:
	void disp_solid(cRdrContext const& ctx) const;
	void disp_skinned(cRdrContext const& ctx) const;
};





