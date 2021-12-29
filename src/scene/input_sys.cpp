#include "input_sys.hpp"
#include "scene_editor.hpp"
#include "scene_components.hpp"
#include "move_sys.hpp"

#include "input.hpp"
#include <SDL_scancode.h>

#include <entt/entt.hpp>

class cInputComp {
public:
	//cInputComp() = default;
};


void cInputSys::register_update(cUpdateQueue& queue) {
	//queue.add(eUpdatePriority::SceneAnimUpdate, MAKE_UPDATE_FUNC_THIS(cAnimationSys::update_anim), mAnimUpdate);
	assert(false);
}

void cInputSys::register_update(cUpdateGraph& graph) {
	sUpdateDepRes input = graph.register_res("components", "input");
	sUpdateDepRes moveReq = graph.register_res("components", "moveReq");

	graph.add(sUpdateDepDesc{ {input}, {moveReq} },
		MAKE_UPDATE_FUNC_THIS(cInputSys::update_input), mInputUpdate);
}

void cInputSys::update_input() {
	auto& input = get_input_mgr();

	if (input.is_locked())
		return;

	const bool isFwd = input.kbtn_state(SDL_SCANCODE_W);
	const bool isBack = input.kbtn_state(SDL_SCANCODE_S);
	const bool isLeft = input.kbtn_state(SDL_SCANCODE_A);
	const bool isRight = input.kbtn_state(SDL_SCANCODE_D);

	vec2f req {0, 0};
	if (isFwd) req.y = 1.0f;
	else if (isBack) req.y = -1.0f;
	if (isLeft) req.x = 1.0f;
	else if (isRight) req.x = -1.0f;

	if (req == vec2f{ 0, 0 })
		return;

	vec3 req3 = { req.x, 0.0f, req.y };

	auto view = mRegistry.view<cInputComp>();
	for (auto en : view) {
		//auto& comp = view.get<cInputComp>(en);
		mRegistry.emplace_or_replace<sMoveRequestComp>(en, req3);
	}
}

/*static*/ void cInputSys::register_to_editor(cSceneCompMetaReg& metaRegistry) {
	metaRegistry.register_component<cInputComp>("Input");
	metaRegistry.register_param<sInputCompParams>("Input");
}


///////////////////////////


bool sInputCompParams::create(entt::registry& reg, entt::entity en) const {
	reg.emplace<cInputComp>(en);
	return true;
}

bool sInputCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	remove_component(reg, en);
	return create(reg, en);
}

bool sInputCompParams::remove_component(entt::registry& reg, entt::entity en) const {
	return reg.remove<cInputComp>(en) > 0;
}

bool sInputCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = false;
	return changed;
}

sInputCompParams sInputCompParams::init_ui() {
	return sInputCompParams();
}

