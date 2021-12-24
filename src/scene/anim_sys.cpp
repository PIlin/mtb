#include "common.hpp"
#include "anim_sys.hpp"
#include "rig_component.hpp"

#include "math.hpp"

#include "scene_editor.hpp"

#include "scene_components.hpp"
#include "res/path_helpers.hpp"
#include "res/resource_load.hpp"

#include "rdr/anim.hpp"
#include "imgui.hpp"

#include <entt/entt.hpp>


class cAnimationComp {
	ConstAnimationListPtr mpAnimList;
	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;

public:

	cAnimationComp(ConstAnimationListPtr pAnimList, int curAnim = 0, float speed = 1.0f)
		: mpAnimList(pAnimList)
		, mSpeed(speed)
		, mCurAnim(curAnim)
	{}

	void update(cRig& rig) {
		assert(mpAnimList);

		const int32_t animCount = mpAnimList->get_count();
		if (animCount > 0 && mCurAnim < animCount) {
			auto& anim = (*mpAnimList)[mCurAnim];
			float lastFrame = anim.get_last_frame();

			anim.eval(rig, mFrame);
			mFrame += mSpeed;
			if (mFrame >= lastFrame)
				mFrame = 0.0f;
		}
	}

	void dbg_ui(sSceneEditCtx& ctx) {
		const int32_t animCount = mpAnimList->get_count();
		if (animCount > 0 && mCurAnim < animCount) {
			auto& anim = (*mpAnimList)[mCurAnim];
			float lastFrame = anim.get_last_frame();

			ImGui::LabelText("name", "%s", anim.get_name().p);
			ImGui::SliderInt("curAnim", &mCurAnim, 0, animCount - 1);
			ImGui::SliderFloat("frame", &mFrame, 0.0f, lastFrame);
			ImGui::SliderFloat("speed", &mSpeed, 0.0f, 3.0f);
		}
	}
};

void cAnimationSys::register_update(cUpdateQueue& queue) {
	queue.add(eUpdatePriority::SceneAnimUpdate, MAKE_UPDATE_FUNC_THIS(cAnimationSys::update_anim), mAnimUpdate);
}

void cAnimationSys::register_update(cUpdateGraph& graph) {
	sUpdateDepRes anim = graph.register_res("components", "anim");
	sUpdateDepRes rig = graph.register_res("components", "rig");
	sUpdateDepRes compAny = graph.register_res("components", nullptr);

	graph.add(sUpdateDepDesc{ {compAny}, {anim, rig} },
		MAKE_UPDATE_FUNC_THIS(cAnimationSys::update_anim), mAnimUpdate);
}

void cAnimationSys::update_anim() {
	auto view = mRegistry.view<cAnimationComp, cRigComp>();
	view.each([](cAnimationComp& anim, cRigComp& rig) {
		anim.update(rig.get());
	});
}

void cAnimationSys::register_to_editor(cSceneCompMetaReg& metaRegistry) {
	metaRegistry.register_component<cAnimationComp>("Animation");
	metaRegistry.register_param<sAnimationCompParams>("Animation");
}

bool sAnimationCompParams::create(entt::registry& reg, entt::entity en) const {
	cRigComp const* pRig = reg.try_get<cRigComp>(en);
	if (!pRig) return false;

	cRigData const* pRigData = pRig->get().get_rig_data();
	if (!pRigData) return false;

	ConstAnimationListPtr pAnimList;
	if (!nResLoader::find_or_load(cPathManager::build_data_path(animRootPath), animListName, *pRigData, *&pAnimList))
		return false;

	cAnimationComp& anim = reg.emplace<cAnimationComp>(en, std::move(pAnimList), curAnim, speed);
	return true;
}

bool sAnimationCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	remove_component(reg, en);
	return create(reg, en);
}

bool sAnimationCompParams::remove_component(entt::registry& reg, entt::entity en) const {
	return reg.remove<cAnimationComp>(en) > 0;
}

bool sAnimationCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = false;
	changed |= ImguiInputTextPath("Anim root", animRootPath);
	changed |= ImguiInputTextPath("Anim list", animListName);

	cRigData tmpRig;
	ConstAnimationListPtr pAnimList;
	if (nResLoader::find_or_load(cPathManager::build_data_path(animRootPath), animListName, tmpRig, *&pAnimList)) {
		const int32_t animCount = pAnimList->get_count();
		changed |= ImGui::SliderInt("curAnim", &curAnim, 0, animCount - 1);
		if (animCount > 0 && curAnim < animCount) {
			auto& anim = (*pAnimList)[curAnim];
			float lastFrame = anim.get_last_frame();

			ImGui::LabelText("name", "%s", anim.get_name().p);
			changed |= ImGui::SliderFloat("speed", &speed, 0.0f, 3.0f);
		}
	}
	return changed;
}

sAnimationCompParams sAnimationCompParams::init_ui() {
	return sAnimationCompParams();
}







