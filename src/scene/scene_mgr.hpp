#include <memory>

class cScene;
class cSceneEditor;
class cUpdateQueue;

class cSceneMgr {
	std::unique_ptr<cUpdateQueue> mpUpdateQueue;
	std::unique_ptr<cScene> mpScene;
	std::unique_ptr<cSceneEditor> mpSceneEditor;
public:

	static cSceneMgr& get();
	
	cSceneMgr();
	~cSceneMgr();

	void update();

	cUpdateQueue& get_update_queue() { return *mpUpdateQueue.get(); }
};
