#include "common.hpp"
#include "math.hpp"
#include "rdr/gfx.hpp"
#include "rdr/rdr.hpp"
#include "model_sys.hpp"
#include "path_helpers.hpp"
#include "position_component.hpp"
#include "rig_component.hpp"
#include "scene_components.hpp"
#include "scene_editor.hpp"

#include "rdr/model.hpp"
#include "resource_load.hpp"
#include "imgui.hpp"

#include <entt/entt.hpp>

namespace dx = DirectX;


class cModelComp {
	cModel mModel;
public:
	DirectX::XMMATRIX lmtx = dx::XMMatrixIdentity();

	cModelComp() = default;
	cModelComp(cModelComp&& other)
		: mModel(std::move(other.mModel))
		, lmtx(other.lmtx)
	{}
	cModelComp& operator=(cModelComp&&) = default;

	bool init(ConstModelDataPtr pModelData, ConstModelMaterialPtr pMtl) {
		static_assert(std::is_move_constructible_v<cModelComp>, "The managed type must be at least move constructible");
		static_assert(std::is_move_assignable_v<cModelComp>, "The managed type must be at least move assignable");

		return mModel.init(pModelData, pMtl);
	}

	void disp(cRdrContext const& rdrCtx, const DirectX::XMMATRIX& wmtx) const {
		mModel.disp(rdrCtx, wmtx);
	}

	void dbg_ui(sSceneEditCtx& ctx) {
		ImguiEditTransform(&lmtx);
		mModel.dbg_ui();
	}
};

/////////////////////////

void cModelDispSys::register_update(cUpdateQueue& queue) {
	if (!mDispUpdate) {
		queue.add(eUpdatePriority::Begin, MAKE_UPDATE_FUNC_THIS(cModelDispSys::update_begin), mUpdateBegin);
		queue.add(eUpdatePriority::SceneDisp, MAKE_UPDATE_FUNC_THIS(cModelDispSys::disp), mDispUpdate);
		queue.add(eUpdatePriority::End, MAKE_UPDATE_FUNC_THIS(cModelDispSys::update_end), mUpdateEnd);
	}
}

void cModelDispSys::register_update(cUpdateGraph& graph) {
	assert(!mDispUpdate);
	sUpdateDepRes rdrJobProlModel = graph.register_res("rdr_prologue_job_mdl");
	sUpdateDepRes rdrJobProlAny = graph.register_res("rdr_prologue_job_*");
	sUpdateDepRes compAny = graph.register_res("components*");
	sUpdateDepRes rig = graph.register_res("rig");
	sUpdateDepRes rdrJobModel = graph.register_res("rdr_job_mdl");
	sUpdateDepRes rdrJobAny = graph.register_res("rdr_job_*");

	graph.add(sUpdateDepDesc{ {}, {rdrJobProlModel, rdrJobProlAny} }, 
		MAKE_UPDATE_FUNC_THIS(cModelDispSys::update_begin), mUpdateBegin);
	graph.add(sUpdateDepDesc{ {compAny, rig}, {rdrJobModel, rdrJobAny} },
		MAKE_UPDATE_FUNC_THIS(cModelDispSys::disp), mDispUpdate);
	graph.add(sUpdateDepDesc{ {rdrJobProlAny, rdrJobAny}, {} },
		MAKE_UPDATE_FUNC_THIS(cModelDispSys::update_end), mUpdateEnd);
}

void cModelDispSys::update_begin() {
	cRdrQueueMgr::get().add_model_prologue_job(mPrologueJob);
}

void cModelDispSys::disp() {
	if (!mRegistry.view<cModelComp>().empty()) {
		cRdrQueueMgr::get().add_model_job(*this);
	}
}

void cModelDispSys::update_end() {
	cRdrQueueMgr::get().exec_model_jobs();
}

void cModelDispSys::disp_job(cRdrContext const& ctx) const {
	disp_solid(ctx);
	disp_skinned(ctx);
}

void cModelDispSys::sPrologueJob::disp_job(cRdrContext const& rdrCtx) const {
	auto pCtx = rdrCtx.get_ctx();
	get_gfx().apply_default_rt_vp(pCtx);
	cRasterizerStates::get().set_def(pCtx);
	cDepthStencilStates::get().set_def(pCtx);
}


void cModelDispSys::disp_solid(cRdrContext const& ctx) const {
	auto view = mRegistry.view<sPositionComp, cModelComp>();
	view.each([&ctx](const sPositionComp& pos, const cModelComp& mdl) {
		dx::XMMATRIX mtx = mdl.lmtx * pos.wmtx;
		mdl.disp(ctx, mtx);
	});
}

void cModelDispSys::disp_skinned(cRdrContext const& ctx) const {
	auto view = mRegistry.view<sPositionComp, cModelComp, cRigComp>();
	view.each([&ctx](const sPositionComp& pos, const cModelComp& mdl, cRigComp& rig) {
		dx::XMMATRIX mtx = mdl.lmtx * pos.wmtx;
		rig.update_rig_mtx(mtx); // todo: move to separate step
		rig.upload_skin(ctx);
		mdl.disp(ctx, mtx);
	});
}

void cModelDispSys::register_to_editor(cSceneCompMetaReg& metaRegistry) {
	metaRegistry.register_component<cModelComp>("Model");
	metaRegistry.register_component<cRigComp>("Rig");

	metaRegistry.register_param<sModelCompParams>("Model");
	metaRegistry.register_param<sRiggedModelCompParams>("Rigged Model");
	metaRegistry.register_param<sFbxRiggedModelParams>("Fbx Model");
}

/////////////////////////

bool sModelCompParams::create(entt::registry& reg, entt::entity en) const {
	ConstModelDataPtr pMdlData;
	ConstModelMaterialPtr pMtl;

	bool res = true;
	res = res && nResLoader::find_or_load(cPathManager::build_data_path(modelPath), *&pMdlData);
	res = res && nResLoader::find_or_load(cPathManager::build_data_path(materialPath), pMdlData, *&pMtl);

	if (!res) return res;

	sPositionComp& pos = reg.get_or_emplace<sPositionComp>(en);
	cModelComp& mdl = reg.emplace<cModelComp>(en);
	res = res && mdl.init(std::move(pMdlData), std::move(pMtl));
	mdl.lmtx = dx::XMLoadFloat4x4A(&localXform);

	return res;
}


bool sModelCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	reg.remove<cModelComp>(en);
	return create(reg, en);
}

bool sModelCompParams::remove_component(entt::registry& reg, entt::entity en) const {
	return reg.remove<cModelComp>(en) > 0;
}

bool sModelCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = false;
	changed |= ImguiInputTextPath("Model", modelPath);
	changed |= ImguiInputTextPath("Material", materialPath);
	changed |= ImguiGizmoEditTransform(&localXform, ctx.camView, true);
	return changed;
}

sModelCompParams sModelCompParams::init_ui() {
	return sModelCompParams();
}

/////////////////////////


bool sRiggedModelCompParams::create(entt::registry& reg, entt::entity en) const {
	if (Base::create(reg, en)) {
		return create_rig(reg, en);
	}
	return false;
}

bool sRiggedModelCompParams::create_rig(entt::registry& reg, entt::entity en) const {
	ConstRigDataPtr pRigData;
	if (!nResLoader::find_or_load(cPathManager::build_data_path(rigPath), *&pRigData))
		return false;
	cRigComp& rig = reg.emplace<cRigComp>(en);
	rig.init(std::move(pRigData));
	return true;
}


bool sRiggedModelCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	reg.remove<cRigComp>(en);
	if (Base::edit_component(reg, en)) {
		return create_rig(reg, en);
	}
	return false;
}

bool sRiggedModelCompParams::remove_component(entt::registry& reg, entt::entity en) const {
	bool removed = reg.remove<cRigComp>(en);
	bool removedBase = Base::remove_component(reg, en);
	return removed || removedBase;
}


bool sRiggedModelCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = Base::dbg_ui(ctx);
	changed |= ImguiInputTextPath("Rig", rigPath);
	return changed;
}

sRiggedModelCompParams sRiggedModelCompParams::init_ui() {
	return sRiggedModelCompParams();
}

/////////////////////////

sFbxRiggedModelParams::sFbxRiggedModelParams() {
	const float scl = 0.01f;
	dx::XMMATRIX mtx = dx::XMMatrixScaling(scl, scl, scl);
	mtx *= dx::XMMatrixRotationX(DEG2RAD(-90.0f));
	dx::XMStoreFloat4x4A(&localXform, mtx);
}

bool sFbxRiggedModelParams::create(entt::registry& reg, entt::entity en) const {
	ConstModelDataPtr pMdlData;
	ConstModelMaterialPtr pMtl;
	ConstRigDataPtr pRigData;

	bool res = true;
	res = res && nResLoader::find_or_load_unreal(cPathManager::build_data_path(modelPath), *&pMdlData, *&pRigData);
	res = res && nResLoader::find_or_load(cPathManager::build_data_path(materialPath), pMdlData, *&pMtl, true);

	if (!res) return res;

	sPositionComp& pos = reg.get_or_emplace<sPositionComp>(en);
	cModelComp& mdl = reg.emplace<cModelComp>(en);
	cRigComp& rig = reg.emplace<cRigComp>(en);
	res = res && mdl.init(std::move(pMdlData), std::move(pMtl));
	mdl.lmtx = dx::XMLoadFloat4x4A(&localXform);
	rig.init(std::move(pRigData));

	return res;
}

bool sFbxRiggedModelParams::edit_component(entt::registry& reg, entt::entity en) const {
	remove_component(reg, en);
	return create(reg, en);
}

bool sFbxRiggedModelParams::remove_component(entt::registry& reg, entt::entity en) const {
	size_t removed = 0;
	removed += reg.remove<cModelComp>(en);
	removed += reg.remove<cRigComp>(en);
	return removed > 0;
}

bool sFbxRiggedModelParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = false;
	changed |= ImguiInputTextPath("Model", modelPath);
	changed |= ImguiInputTextPath("Material", materialPath);
	changed |= ImguiGizmoEditTransform(&localXform, ctx.camView, true);
	return changed;
}

sFbxRiggedModelParams sFbxRiggedModelParams::init_ui() {
	return sFbxRiggedModelParams();
}

