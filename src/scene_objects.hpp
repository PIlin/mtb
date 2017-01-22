#include <memory>

class cScene;

class cSceneMgr {
	std::unique_ptr<cScene> mpScene;
public:

	static cSceneMgr& get();
	
	cSceneMgr();
	~cSceneMgr();

	void disp();
};
