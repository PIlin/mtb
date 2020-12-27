#pragma once

#include "scene_components.hpp"

#include <entt/entt.hpp>

class cCameraManager;
class cSceneEditor;

class cScene {
	friend class cSceneEditor;

	struct sSceneImpl;

	entt::registry registry;
	std::unique_ptr<cCameraManager> mpCameraMgr;
	sSceneSnapshot snapshot;
	std::unique_ptr<sSceneImpl> mpSceneImpl;

public:

	cScene();
	~cScene();

	void create_from_snapshot();
	void load();
	void save();
};