#pragma once

#include <memory>
#include <string>
#include <vector>

class cScene;
class cSceneEditor;
class cUpdateQueue;

class cSceneMgr {
	using ScenesVector = std::vector<std::unique_ptr<cScene>>;
	std::vector<std::unique_ptr<cScene>> mScenes;
	std::unique_ptr<cSceneEditor> mpSceneEditor;
	size_t mCurrentScene = 0;
public:

	static cSceneMgr& get();
	
	cSceneMgr();
	~cSceneMgr();

	void update();

private:
	void set_current(size_t idx);
	void load_scene(const std::string& name);
	void dbg_ui();
};
