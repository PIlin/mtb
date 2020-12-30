#pragma once
#include "update_queue.hpp"
#include <entt/fwd.hpp>

class cSceneCompMetaReg;

class cAnimationSys {
	entt::registry& mRegistry;
	cUpdateSubscriberScope mAnimUpdate;
public:
	cAnimationSys(entt::registry& reg) : mRegistry(reg) {}

	void register_update(cUpdateQueue& queue);
	void register_update(cUpdateGraph& graph);
	void update_anim();

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);
};
