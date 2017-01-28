#include <memory>

class cScene;
class cUpdateQueue;

class cSceneMgr {
	std::unique_ptr<cUpdateQueue> mpUpdateQueue;
	std::unique_ptr<cScene> mpScene;
public:

	static cSceneMgr& get();
	
	cSceneMgr();
	~cSceneMgr();

	void disp();

	cUpdateQueue& get_update_queue() { return *mpUpdateQueue.get(); }
};
