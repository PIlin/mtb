#pragma once

#include "update_queue.hpp"
#include <entt/fwd.hpp>

class cSceneCompMetaReg;

class cInputSys {
	entt::registry& mRegistry;
	cUpdateSubscriberScope mInputUpdate;

public:
	cInputSys(entt::registry& reg) : mRegistry(reg) {}

	void register_update(cUpdateQueue& queue);
	void register_update(cUpdateGraph& graph);
	void update_input();

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);
};