#include "common.hpp"
#include "move_sys.hpp"
#include "math.hpp"

#include "scene_editor.hpp"

#include "position_component.hpp"
#include "scene_components.hpp"
#include "res/path_helpers.hpp"
#include "res/resource_load.hpp"

#include "imgui.hpp"

#include <entt/entt.hpp>

namespace dx = DirectX;

class cMoveComp {
	float mPos;
	float mRad;
	sXform mOrigXform;
public:

	cMoveComp(sMoveCompParams const& params, sPositionComp const& posComp)
		: mPos(params.pos)
		, mRad(params.rad)
	{
		mOrigXform.init_scaled(posComp.wmtx);
	}

	void update(sPositionComp& posComp) {
		const float rotTime = 5;
		mPos += (1.0f / 60.0f) / rotTime;
		float intPart;
		if (mPos > 1.0f) mPos = modf(mPos, &intPart);

		float angle = -dx::XM_PI * ((mPos * 2) - 1.0f);

		dx::XMMATRIX rot = dx::XMMatrixRotationY(angle);
		dx::XMVECTOR off = dx::XMVectorSet(mRad, 0.0f, mRad, 0.0f);
		off = dx::XMVector4Transform(off, rot);
		rot = rot * dx::XMMatrixTranslationFromVector(off);

		posComp.wmtx = mOrigXform.build_mtx() * rot;
	}

	void dbg_ui(sSceneEditCtx& ctx) {
		ImGui::SliderFloat("pos", &mPos, 0.0f, 1.0f);
		ImGui::SliderFloat("rad", &mRad, 0.0f, 10.0f);
	}
};

///////////////////////

void sMoveRequestComp::update(sPositionComp& pos) {
	auto mtx = dx::XMMatrixTranslation(request.x, request.y, request.z);
	pos.wmtx *= mtx;
}

///////////////////////


void cMoveSys::register_update(cUpdateQueue& queue) {
	queue.add(eUpdatePriority::SceneAnimUpdate, MAKE_UPDATE_FUNC_THIS(cMoveSys::update), mMoveUpdate);
}

void cMoveSys::register_update(cUpdateGraph& graph) {
	//sUpdateDepRes move = graph.register_res("move");
	sUpdateDepRes moveReq = graph.register_res("components", "moveReq");
	sUpdateDepRes pos = graph.register_res("components", "position");

	graph.add(sUpdateDepDesc{ {moveReq}, {pos} },
		MAKE_UPDATE_FUNC_THIS(cMoveSys::update), mMoveUpdate);
}

void cMoveSys::update() {
	{
		auto view = mRegistry.view<sPositionComp, cMoveComp>();
		view.each([](sPositionComp& pos, cMoveComp& move) {
			move.update(pos);
		});
	}

	{
		auto view = mRegistry.view<sPositionComp, sMoveRequestComp>();
		view.each([](sPositionComp& pos, sMoveRequestComp& move) {
			move.update(pos);
		});
		mRegistry.clear<sMoveRequestComp>();
	}
}

void cMoveSys::register_to_editor(cSceneCompMetaReg& metaRegistry) {
	metaRegistry.register_component<cMoveComp>("Move");
	metaRegistry.register_param<sMoveCompParams>("Move");
}

bool sMoveCompParams::create(entt::registry& reg, entt::entity en) const {
	sPositionComp const* pPos = reg.try_get<sPositionComp>(en);
	if (!pPos) return false;

	cMoveComp& move = reg.emplace<cMoveComp>(en, *this, *pPos);
	return true;
}

bool sMoveCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	remove_component(reg, en);
	return create(reg, en);
}

bool sMoveCompParams::remove_component(entt::registry& reg, entt::entity en) const {
	return reg.remove<cMoveComp>(en) > 0;
}

bool sMoveCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = false;
	changed |= ImGui::SliderFloat("pos", &pos, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("rad", &rad, 0.0f, 10.0f);
	return changed;
}

sMoveCompParams sMoveCompParams::init_ui() {
	return sMoveCompParams();
}
