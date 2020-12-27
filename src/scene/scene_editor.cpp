#include "common.hpp"
#include "math.hpp"
#include "path_helpers.hpp"
#include "rdr/rdr.hpp"
#include "scene_editor.hpp"
#include "scene.hpp"
#include "camera_mgr.hpp"

#include "scene_mgr.hpp"
#include "scene_components.hpp"

#include "imgui.hpp"

#include <entt/entt.hpp>


cSceneEditor::cSceneEditor() {

}

void cSceneEditor::init(cScene* pScene) {
	mpScene = pScene;
	mSelected = entt::null;
	cSceneMgr::get().get_update_queue().add(eUpdatePriority::Begin, tUpdateFunc(std::bind(&cSceneEditor::dbg_ui, this)), mDbgUpdate);
}

static const char* format_entity(std::string& buf, entt::entity en, const sSceneSnapshot& snapshot) {
	buf = std::to_string((uint32_t)entt::registry::entity(en));
	buf += ':';
	buf += std::to_string((uint32_t)entt::registry::version(en));

	if (snapshot.entityIds.find(en) == snapshot.entityIds.end()) {
		buf += " (dyn)";
	}

	return buf.c_str();
}

void cSceneEditor::dbg_ui() {
	if (!mpScene) return;

	sSceneEditCtx ctx;
	ctx.camView = mpScene->mpCameraMgr->get_cam().mView;

	ImGui::Begin("scene");
	show_entity_list(ctx);

	if (mSelected != entt::null) {
		ImGui::PushID((int)mSelected);
		show_entity_params(mSelected, ctx);
		show_entity_components(mSelected, ctx);
		ImGui::PopID();
	}

	ImGui::End();
}

void cSceneEditor::show_entity_list(sSceneEditCtx& ctx) {
	entt::registry& reg = mpScene->registry;
	sSceneSnapshot& snapshot = mpScene->snapshot;

	if (ImGui::Button("Create")) {
		mSelected = reg.create();
		snapshot.entityIds.insert(mSelected);
	}
	if (mSelected != entt::null) {
		ImGui::SameLine();
		if (ImGui::Button("Delete")) {
			reg.destroy(mSelected);
			snapshot.remove_entity(mSelected);
			mSelected = entt::null;
		}
	}

	std::vector<entt::entity> alive;
	int selectedIdx = -1;
	reg.each([&](entt::entity en) { if (en == mSelected) { selectedIdx = (int)alive.size(); } alive.push_back(en); });
	std::string buffer;
	auto getter = [&](int i, const char** szText) {
		if (i < alive.size()) {
			(*szText) = format_entity(buffer, alive[i], snapshot);
			return true;
		}
		return false;
	};
	using TGetter = decltype(getter);
	auto getterWrapper = [](void* g, int i, const char** szText) { return (*(TGetter*)g)(i, szText); };
	void* pGetter = &getter;
	if (ImGui::ListBox("entities", &selectedIdx, getterWrapper, pGetter, (int)alive.size(), 10)) {
		if (0 <= selectedIdx && selectedIdx < alive.size()) { mSelected = alive[selectedIdx]; }
		else { mSelected = entt::null; }
	}
}

void cSceneEditor::show_entity_components(entt::entity en, sSceneEditCtx& ctx) {
	if (ImGui::TreeNode("Components")) {
		entt::registry& reg = mpScene->registry;

		std::vector<entt::id_type> componentTypes;
		reg.visit(en, [&](entt::id_type t) { componentTypes.push_back(t); });
		sort_components_by_order(componentTypes);

		for (auto t : componentTypes) {
			auto it = mComponentTypes.find(t);
			if (it != mComponentTypes.end()) {
				if (it->second->openByDefault)
					ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
				if (ImGui::TreeNode(it->second->ui_name())) {
					it->second->ui(reg, en, ctx);
					ImGui::TreePop();
				}
			}
			else {
				std::string node = "**Unknown component type " + std::to_string(uint32_t(t));
				if (ImGui::TreeNode(node.c_str())) {
					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}
}

static void ensure_entity_in_snapshot(entt::entity en, sSceneSnapshot& snapshot) {
	snapshot.entityIds.insert(en);
}

void cSceneEditor::show_entity_params(entt::entity en, sSceneEditCtx& ctx) {
	ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
	if (ImGui::TreeNode("Params")) {
		sSceneSnapshot& snapshot = mpScene->snapshot;
		entt::registry& reg = mpScene->registry;

		std::vector<entt::id_type> paramTypes;
		gather_params_in_snapshot(snapshot, en, paramTypes);

		entt::id_type typeToAdd;
		ImGui::SameLine();
		if (select_param_type_to_add(paramTypes, typeToAdd)) {
			nSceneEdit::iParamReg* pParam = mParamTypes[typeToAdd].get();
			ensure_entity_in_snapshot(en, snapshot);
			pParam->add(reg, snapshot, en);
			sort_components_by_order(snapshot.paramsOrder);
			paramTypes.clear();
			gather_params_in_snapshot(snapshot, en, paramTypes);
		}

		for (auto t : paramTypes) {
			auto it = mParamTypes.find(t);
			if (it != mParamTypes.end()) {
				if (it->second->openByDefault)
					ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
				if (ImGui::TreeNode(it->second->ui_name())) {
					it->second->ui(reg, snapshot, en, ctx);
					ImGui::TreePop();
				}
			}
			else {
				std::string node = "**Unknown param type " + std::to_string(uint32_t(t));
				if (ImGui::TreeNode(node.c_str())) {
					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}
}

void cSceneEditor::gather_params_in_snapshot(const sSceneSnapshot& snapshot, entt::entity en, std::vector<entt::id_type>& paramTypes) const {
	for (auto& paramListPair : snapshot.params) {
		if (paramListPair.second) {
			const iParamList::EntityList& el = paramListPair.second->entityList;
			if (el.find(en) != el.end())
				paramTypes.push_back(paramListPair.first);
		}
	}

	sort_params_by_order(paramTypes);
}

bool cSceneEditor::select_param_type_to_add(const std::vector<entt::id_type>& paramTypes, entt::id_type& typeToAdd) const {
	bool res = false;

	if (ImGui::SmallButton("New...")) {
		ImGui::OpenPopup("new_param_popup");
	}
	if (ImGui::BeginPopup("new_param_popup")) {
		std::vector<entt::id_type> newParamTypes;
		for (const auto& pt : mParamTypes) {
			if (std::find(paramTypes.begin(), paramTypes.end(), pt.first) == paramTypes.end()) {
				newParamTypes.push_back(pt.first);
			}
		}
		if (newParamTypes.empty()) {
			ImGui::TextDisabled("- no params to add -");
		}
		else {
			sort_params_by_order(newParamTypes);
			for (entt::id_type t : newParamTypes) {
				auto it = mParamTypes.find(t);
				if (it != mParamTypes.end()) {
					if (ImGui::Selectable(it->second.get()->ui_name())) {
						typeToAdd = t;
						res = true;
					}
				}
			}
		}
		ImGui::EndPopup();
	}
	return res;
}



template <typename TMap>
static void sort_components_by_order_impl(std::vector<entt::id_type>& comp, const TMap& map) {
	std::sort(comp.begin(), comp.end(), [&](entt::id_type a, entt::id_type b) {
		auto ait = map.find(a);
		auto bit = map.find(b);
		auto eit = map.end();
		if (ait != eit && bit != eit) {
			return ait->second->order < bit->second->order;
		}
		else if (ait != eit) {
			return true;
		}
		return false;
	});
}


void cSceneEditor::sort_components_by_order(std::vector<entt::id_type>& comp) const {
	sort_components_by_order_impl(comp, mComponentTypes);
}

void cSceneEditor::sort_params_by_order(std::vector<entt::id_type>& comp) const {
	sort_components_by_order_impl(comp, mParamTypes);
}

