#pragma once
#include "update_queue.hpp"
#include <entt/fwd.hpp>

class cAnimationSys {
	entt::registry& mRegistry;
	cUpdateSubscriberScope mAnimUpdate;
public:
	cAnimationSys(entt::registry& reg) : mRegistry(reg) {}

	void register_update(cUpdateQueue& queue);
	void update_anim();
};
