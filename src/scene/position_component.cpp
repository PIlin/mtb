#include "common.hpp"
#include "math.hpp"
#include "position_component.hpp"
#include "scene_components.hpp"
#include "scene_editor.hpp"

#include "imgui.hpp"
#include <entt/entt.hpp>

sPositionComp::sPositionComp()
	: wmtx(DirectX::XMMatrixIdentity())
{}

sPositionComp::sPositionComp(DirectX::FXMMATRIX wmtx)
	: wmtx(wmtx)
{}

void sPositionComp::dbg_ui(sSceneEditCtx& ctx) {
	ImguiGizmoEditTransform(&wmtx, ctx.camView, true);
}

void sPositionComp::register_to_editor(cSceneCompMetaReg& metaRegistry) {
	metaRegistry.register_component<sPositionComp>("Position").openByDefault = false;
	metaRegistry.register_param<sPositionCompParams>("Position").openByDefault = true;
}



bool sPositionCompParams::create(entt::registry& reg, entt::entity en) const {
	reg.emplace<sPositionComp>(en, xform.build_mtx());
	return true;
}

bool sPositionCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	reg.emplace_or_replace<sPositionComp>(en, xform.build_mtx());
	return true;
}

bool sPositionCompParams::dbg_ui(sSceneEditCtx& ctx) {
	DirectX::XMMATRIX wmtx = xform.build_mtx();
	if (ImguiGizmoEditTransform(&wmtx, ctx.camView, true)) {
		xform.init_scaled(wmtx);
		return true;
	}
	return false;
}

sPositionCompParams sPositionCompParams::init_ui() {
	sXform xform;
	xform.init(nMtx::g_Identity);
	return sPositionCompParams{ xform };
}

