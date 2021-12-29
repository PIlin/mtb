#pragma once
#include "update_queue.hpp"
#include "math.hpp"
#include <entt/fwd.hpp>

class cSceneCompMetaReg;
struct sPositionComp;

struct sMoveRequestComp {
	vec3 request;

	void update(sPositionComp& pos);
};

class cMoveSys {
	entt::registry& mRegistry;
	cUpdateSubscriberScope mMoveUpdate;
public:
	cMoveSys(entt::registry& reg) : mRegistry(reg) {}

	void register_update(cUpdateQueue& queue);
	void register_update(cUpdateGraph& graph);
	void update();

	static void register_to_editor(cSceneCompMetaReg& metaRegistry);
};
