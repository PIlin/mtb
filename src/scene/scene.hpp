#pragma once

#include "scene_components.hpp"

#include <entt/entt.hpp>

class cCameraManager;
class cSceneEditor;
class cUpdateQueue;
class cUpdateGraph;

class cScene {
	friend class cSceneEditor;

	struct sSceneImpl;

	std::unique_ptr<cUpdateGraph> mpUpdateGraph;
	std::unique_ptr<cUpdateQueue> mpUpdateQueue;

	entt::registry registry;
	std::unique_ptr<cCameraManager> mpCameraMgr;
	sSceneSnapshot snapshot;
	std::unique_ptr<sSceneImpl> mpSceneImpl;
	std::string mName;
public:

	explicit cScene(const std::string& name);
	~cScene();

	void create_from_snapshot();
	void load();
	void save();

	void update();

	const std::string& get_name() const { return mName; }

	cUpdateQueue& get_update_queue() const { return *mpUpdateQueue.get(); }
	cUpdateGraph& get_update_graph() const { return *mpUpdateGraph.get(); }
};
