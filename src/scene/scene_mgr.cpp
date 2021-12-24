#include "common.hpp"
#include "scene_mgr.hpp"
#include "math.hpp"
#include "rdr/rdr.hpp"
#include "update_queue.hpp"
#include "res/path_helpers.hpp"
#include "res/resource_load.hpp"
#include "scene_components.hpp"
#include "imgui.hpp"
#include "dbg_ui.hpp"

#include "scene/anim_sys.hpp"
#include "scene/camera_mgr.hpp"
#include "scene/model_sys.hpp"
#include "scene/move_sys.hpp"
#include "scene/position_component.hpp"
#include "scene/rig_component.hpp"
#include "scene/scene.hpp"
#include "scene/scene_editor.hpp"

#include <imgui.h>




//////

cSceneMgr::cSceneMgr()
	: mpSceneEditor(std::make_unique<cSceneEditor>())
{
	sPositionComp::register_to_editor(*mpSceneEditor);
	cModelDispSys::register_to_editor(*mpSceneEditor);
	cAnimationSys::register_to_editor(*mpSceneEditor);
	cMoveSys::register_to_editor(*mpSceneEditor);

	load_scene("def.scene");
}

cSceneMgr::~cSceneMgr() {}

void cSceneMgr::update() {
	if (mCurrentScene < mScenes.size()) {
		mScenes[mCurrentScene]->update();
	}
	
	dbg_ui();
}

void cSceneMgr::dbg_ui() {
	if (bool& isOpen = cDbgToolsMgr::tools().scene_editor) {
		if (ImGui::Begin("scene", &isOpen)) {
			if (ImGui::TreeNode("Scenes")) {
				if (ImGui::Button("Load")) {
					ImGui::OpenPopup("CrearScenePopUp");
				}
				if (mCurrentScene < mScenes.size()) {
					ImGui::SameLine();
					if (ImGui::Button("Unload")) {
						mScenes.erase(mScenes.begin() + mCurrentScene);
					}
				}
				if (ImGui::BeginPopup("CrearScenePopUp")) {
					char nameBuf[128] = { 0 };
					if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
						std::string name = nameBuf;
						load_scene(name);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}

				int current = (int)mCurrentScene;
				auto scenesListFunc = [](void* data, int idx, const char** outText) {
					const ScenesVector* pScenes = reinterpret_cast<ScenesVector*>(data);
					if (idx < pScenes->size()) {
						*outText = (*pScenes)[idx]->get_name().c_str();
						return true;
					}
					return false;
				};

				if (ImGui::ListBox("Scenes", &current, scenesListFunc, &mScenes, (int)mScenes.size())) {
					set_current(current);
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
}

void cSceneMgr::load_scene(const std::string& name) {
	mpSceneEditor->init(nullptr);
	mCurrentScene = mScenes.size();
	mScenes.emplace_back(std::make_unique<cScene>(name));
	set_current(mCurrentScene);
}

void cSceneMgr::set_current(size_t idx) {
	mCurrentScene = idx;
	if (mCurrentScene < mScenes.size()) {
		mpSceneEditor->init(mScenes[mCurrentScene].get());
	}
}
