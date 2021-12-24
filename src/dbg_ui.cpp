#include "math.hpp"
#include "dbg_ui.hpp"
#include "imgui.hpp"
#include "res/path_helpers.hpp"

sDbgTools& cDbgToolsMgr::tools() {
	return get().mToolFlags;
}

static fs::path get_dbg_ui_settings_path() {
	return cPathManager::build_settings_path("dbg_ui.json");
}

cDbgToolsMgr::cDbgToolsMgr() {
	mToolFlags.load(get_dbg_ui_settings_path());
}

cDbgToolsMgr::~cDbgToolsMgr() {
	mToolFlags.save(get_dbg_ui_settings_path());
}

void cDbgToolsMgr::update() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Tools")) {
			ImGui::MenuItem("Scene editor", "", &mToolFlags.scene_editor);
			ImGui::MenuItem("Update queue", "", &mToolFlags.update_queue);
			ImGui::MenuItem("Light", "", &mToolFlags.light_mgr);
			ImGui::MenuItem("ImGui demo", "", &mToolFlags.imgui_demo);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (tools().imgui_demo) {
		ImGui::ShowDemoWindow(&tools().imgui_demo);
	}
}
