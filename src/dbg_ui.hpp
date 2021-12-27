#pragma once
namespace std::filesystem { class path; }
namespace fs = std::filesystem;

struct sDbgTools {
	bool scene_editor = true;
	bool update_queue = true;
	bool light_mgr = true;
	bool camera_mgr = false;
	bool imgui_demo = false;

	bool save(const fs::path& filepath);
	bool load(const fs::path& filepath);

	template <class Archive>
	void serialize(Archive& arc);
};


class cDbgToolsMgr {
public:
	static cDbgToolsMgr& get();
	static sDbgTools& tools();

	cDbgToolsMgr();
	~cDbgToolsMgr();
	void update();
private:
	sDbgTools mToolFlags;
};
