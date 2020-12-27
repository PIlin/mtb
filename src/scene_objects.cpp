#include "common.hpp"
#include "scene_objects.hpp"
#include "math.hpp"
#include "path_helpers.hpp"
#include "gfx.hpp"
#include "rdr.hpp"
#include "texture.hpp"
#include "model.hpp"
#include "rig.hpp"
#include "anim.hpp"
#include "update_queue.hpp"
#include "camera.hpp"
#include "sh.hpp"
#include "light.hpp"
#include "rdr_queue.hpp"
#include "resource_load.hpp"
#include "scene_components.hpp"
#include "imgui.hpp"

#include "scene/anim_sys.hpp"
#include "scene/camera_mgr.hpp"
#include "scene/scene.hpp"
#include "scene/scene_editor.hpp"

#include <imgui.h>

CLANG_DIAG_PUSH
CLANG_DIAG_IGNORE("-Wpragma-pack")
#include <assimp/Importer.hpp>
CLANG_DIAG_POP
#include "assimp_loader.hpp"

#include <entt/entt.hpp>

#include <deque>

namespace dx = DirectX;
using dx::XMFLOAT4;


class cGnomon : public iRdrJob {
	struct sVtx {
		float mPos[3];
		float mClr[4];
	};

	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	com_ptr<ID3D11InputLayout> mpIL;
	cVertexBuffer mVtxBuf;

	cUpdateSubscriberScope mDispUpdate;
public:

	cGnomon() = default;
	~cGnomon() { deinit(); }

	void disp() {
		cRdrQueueMgr::get().add_model_job(*this);
	}

	virtual void disp_job(cRdrContext const& rdrCtx) const override {
		if (!(mpVS && mpVS)) return;

		auto pCtx = rdrCtx.get_ctx();
		auto& cbuf = rdrCtx.get_cbufs().mMeshCBuf;

		//cbuf.mData.wmtx = dx::XMMatrixIdentity();
		cbuf.mData.wmtx = dx::XMMatrixTranslation(0, 0, 0);
		cbuf.update(pCtx);
		cbuf.set_VS(pCtx);

		mVtxBuf.set(pCtx, 0, 0);
		pCtx->IASetInputLayout(mpIL);
		pCtx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		pCtx->VSSetShader(mpVS->asVS(), nullptr, 0);
		pCtx->PSSetShader(mpPS->asPS(), nullptr, 0);
		pCtx->Draw(6, 0);
	}

	void init() {
		auto& ss = cShaderStorage::get();
		mpVS = ss.load_VS("simple.vs.cso");
		if (!mpVS) return;
		mpPS = ss.load_PS("simple.ps.cso");
		if (!mpPS) return;

		D3D11_INPUT_ELEMENT_DESC vdsc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sVtx, mPos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(sVtx, mClr), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		auto pDev = get_gfx().get_dev();
		auto& code = mpVS->get_code();
		HRESULT hr = pDev->CreateInputLayout(vdsc, LENGTHOF_ARRAY(vdsc), code.get_code(), code.get_size(), mpIL.pp());
		if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

		sVtx vtx[6] = {
			{ { 0.0f, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f, 1.0f } },
			{ { 1.0f, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f, 1.0f } },
			{ { 0.0f, 0.0f, 0.0f }, { 0.0, 1.0f, 0.0f, 1.0f } },
			{ { 0.0f, 1.0f, 0.0f }, { 0.0, 1.0f, 0.0f, 1.0f } },
			{ { 0.0f, 0.0f, 0.0f }, { 0.0, 0.0f, 1.0f, 1.0f } },
			{ { 0.0f, 0.0f, 1.0f }, { 0.0, 0.0f, 1.0f, 1.0f } },
		};

		mVtxBuf.init(pDev, vtx, LENGTHOF_ARRAY(vtx), sizeof(vtx[0]));

		cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneDisp, tUpdateFunc(std::bind(&cGnomon::disp, this)), mDispUpdate);
	}

	void deinit() {
		mDispUpdate.reset();
		mVtxBuf.deinit();
		mpIL.reset();
	}
};


struct sPositionComp {
	DirectX::XMMATRIX wmtx;

	sPositionComp()
		: wmtx(DirectX::XMMatrixIdentity())
	{}

	sPositionComp(DirectX::FXMMATRIX wmtx)
		: wmtx(wmtx)
	{}

	void dbg_ui(sSceneEditCtx& ctx) {
		ImguiGizmoEditTransform(&wmtx, ctx.camView, true);
	}
};

bool sPositionCompParams::create(entt::registry& reg, entt::entity en) const {
	reg.emplace<sPositionComp>(en, xform.build_mtx());
	return true;
}

bool sPositionCompParams::edit_component(entt::registry& reg, entt::entity en) const {
	reg.emplace_or_replace<sPositionComp>(en, xform.build_mtx());
	return true;
}

bool sPositionCompParams::dbg_ui(sSceneEditCtx& ctx) {
	dx::XMMATRIX wmtx = xform.build_mtx();
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
	reg.remove_if_exists<cModelComp>(en);
	return create(reg, en);
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


class cRigComp {
	cRig mRig;
public:
	cRigComp() = default;
	cRigComp(cRigComp&& other) : mRig(std::move(other.mRig)) {}
	cRigComp& operator=(cRigComp&&) = default;

	void init(ConstRigDataPtr pRigData) {
		mRig.init(std::move(pRigData));
	}

	void XM_CALLCONV update_rig_mtx(dx::XMMATRIX wmtx) {
		mRig.calc_local();
		mRig.calc_world(wmtx);
	}

	void upload_skin(cRdrContext const& ctx) const { mRig.upload_skin(ctx); }

	void dbg_ui(sSceneEditCtx& ctx) {
	}

	cRig& get() { return mRig; }
	const cRig& get() const { return mRig; }
};

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
	reg.remove_if_exists<cRigComp>(en);
	if (Base::edit_component(reg, en)) {
		return create_rig(reg, en);
	}
	return false;
}

bool sRiggedModelCompParams::dbg_ui(sSceneEditCtx& ctx) {
	bool changed = Base::dbg_ui(ctx);
	changed |= ImguiInputTextPath("Rig", rigPath);
	return changed;
}

sRiggedModelCompParams sRiggedModelCompParams::init_ui() {
	return sRiggedModelCompParams();
}


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
	reg.remove_if_exists<cModelComp>(en);
	reg.remove_if_exists<cRigComp>(en);
	return create(reg, en);
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


class cModelDispSys : public iRdrJob {
	cUpdateSubscriberScope mDispUpdate;
	entt::registry& mRegistry;
public:

	cModelDispSys(entt::registry& reg) : mRegistry(reg) {}

	void register_disp_update() {
		if (!mDispUpdate) {
			cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneDisp, tUpdateFunc(std::bind(&cModelDispSys::disp, this)), mDispUpdate);
		}
	}

	void disp() {
		if (!mRegistry.empty<cModelComp>()) {
			cRdrQueueMgr::get().add_model_job(*this);
		}
	}

	virtual void disp_job(cRdrContext const& ctx) const override {
		disp_solid(ctx);
		disp_skinned(ctx);
	}

private:
	void disp_solid(cRdrContext const& ctx) const {
		auto view = mRegistry.view<sPositionComp, cModelComp>();
		view.each([&ctx](const sPositionComp& pos, const cModelComp& mdl) {
			dx::XMMATRIX mtx = mdl.lmtx * pos.wmtx;
			mdl.disp(ctx, mtx);
		});
	}

	void disp_skinned(cRdrContext const& ctx) const {
		auto view = mRegistry.view<sPositionComp, cModelComp, cRigComp>();
		view.each([&ctx](const sPositionComp& pos, const cModelComp& mdl, cRigComp& rig) {
			dx::XMMATRIX mtx = mdl.lmtx * pos.wmtx;
			rig.update_rig_mtx(mtx); // todo: move to separate step
			rig.upload_skin(ctx);
			mdl.disp(ctx, mtx);
		});
	}
};









////


////

class cLightMgrUpdate : public iRdrJob {
	cLightMgr mLightMgr;
	cUpdateSubscriberScope mLightUpdate;
public:
	void init() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::Light, tUpdateFunc(std::bind(&cLightMgrUpdate::update, this)), mLightUpdate);
	}

	void update() {
		mLightMgr.dbg_ui();
		cRdrQueueMgr::get().add_model_prologue_job(*this);
	}

	virtual void disp_job(cRdrContext const& rdrCtx) const override {
		mLightMgr.disp(rdrCtx);
	}
};

////

class cModelRdrJobs : public iRdrJob {
	cUpdateSubscriberScope mUpdateBegin;
	cUpdateSubscriberScope mUpdateEnd;

	cModelDispSys mModelDispSys;
public:
	cModelRdrJobs(entt::registry& reg)
		: mModelDispSys(reg)
	{}

	void init() {
		mModelDispSys.register_disp_update();

		cSceneMgr::get().get_update_queue().add(eUpdatePriority::Begin, tUpdateFunc(std::bind(&cModelRdrJobs::update_begin, this)), mUpdateBegin);
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::End, tUpdateFunc(std::bind(&cModelRdrJobs::update_end, this)), mUpdateEnd);
	}

	void update_begin() {
		cRdrQueueMgr::get().add_model_prologue_job(*this);
	}
	
	void update_end() {
		cRdrQueueMgr::get().exec_model_jobs();
	}

	virtual void disp_job(cRdrContext const& rdrCtx) const override {
		auto pCtx = rdrCtx.get_ctx();
		get_gfx().apply_default_rt_vp(pCtx);
		cRasterizerStates::get().set_def(pCtx);
		cDepthStencilStates::get().set_def(pCtx);
	}
};



struct cScene::sSceneImpl {
	cGnomon gnomon;
	cLightMgrUpdate lightMgr;
	cModelRdrJobs modelRdrJobs;
	cAnimationSys animSys;



public:

	sSceneImpl(entt::registry& registry)
		: modelRdrJobs(registry)
		, animSys(registry)
	{
		gnomon.init();
		lightMgr.init();
		modelRdrJobs.init();
		animSys.register_update(cSceneMgr::get().get_update_queue());
	}
};

cScene::cScene()
	: mpCameraMgr(std::make_unique<cCameraManager>())
	, mpSceneImpl(std::make_unique<sSceneImpl>(registry))
{
	mpCameraMgr->init();
	load();
	create_from_snapshot();
}

cScene::~cScene() {
	save();
}

static fs::path get_scene_snapshot_path() { return cPathManager::build_data_path("scene.json"); }

void cScene::create_from_snapshot() {
	if (!registry.empty()) {
		registry.clear();
	}

	for (entt::entity en : snapshot.entityIds) {
		entt::entity realEn = registry.create(en);
		if (realEn != en) {
			dbg_break();
		}
	}

	snapshot.invoke_for_params([&](entt::id_type t, auto* pList) { pList->create(registry); });
}

void cScene::load() {
	if (snapshot.load(get_scene_snapshot_path())) {

	}
	else {
		sParamList<sPositionCompParams>* pPosParams = new sParamList<sPositionCompParams>();
		sParamList<sModelCompParams>* pModelParams = new sParamList<sModelCompParams>();
		sParamList<sRiggedModelCompParams>* pRiggedModelParams = new sParamList<sRiggedModelCompParams>();
		sParamList<sFbxRiggedModelParams>* pFbxModelParams = new sParamList<sFbxRiggedModelParams>();
		sParamList<sAnimationCompParams>* pAnimParams = new sParamList<sAnimationCompParams>();

		auto insertEntityParam = [](auto* pList, entt::entity en, auto&& param) {
			const uint32_t paramId = (uint32_t)pList->paramList.size();
			pList->paramList.emplace(paramId, std::forward<decltype(param)>(param));
			pList->entityList.emplace(en, paramId);
		};

#if 1
		{
			entt::entity lightning = registry.create();
			snapshot.entityIds.insert(lightning);
			fs::path root = fs::path("lightning");
			sXform xform = sXform::identity();
			insertEntityParam(pPosParams, lightning, sPositionCompParams{ xform });
			insertEntityParam(pModelParams, lightning, sModelCompParams{ root / "lightning.geo", root / "lightning.mtl" });
		}

		{
			entt::entity owl = registry.create();
			snapshot.entityIds.insert(owl);
			fs::path root = fs::path("owl");
			sXform xform = sXform::identity();
			xform.mPos = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
			insertEntityParam(pPosParams, owl, sPositionCompParams{ xform });
			sRiggedModelCompParams rm;
			rm.modelPath = root / "def.geo";
			rm.materialPath = root / "def.mtl";
			rm.rigPath = root / "def.rig";
			xform = sXform::identity();
			const float scl = 0.01f;
			xform.mScale = dx::XMVectorSet(scl, scl, scl, 0.0f);
			dx::XMStoreFloat4x4A(&rm.localXform, xform.build_mtx());
			insertEntityParam(pRiggedModelParams, owl, std::move(rm));
			insertEntityParam(pAnimParams, owl, sAnimationCompParams{ root, "def.alist" });
		}
#endif

		{
			entt::entity jumpingSphere = registry.create();
			snapshot.entityIds.insert(jumpingSphere);
			fs::path root = fs::path("jumping_sphere");
			sXform xform = sXform::identity();
			xform.mPos = dx::XMVectorSet(2.0f, 0.0f, 0.0f, 1.0f);
			insertEntityParam(pPosParams, jumpingSphere, sPositionCompParams{ xform });
			sRiggedModelCompParams rm;
			rm.modelPath = root / "def.geo";
			rm.materialPath = root / "def.mtl";
			rm.rigPath = root / "def.rig";
			insertEntityParam(pRiggedModelParams, jumpingSphere, std::move(rm));
			insertEntityParam(pAnimParams, jumpingSphere, sAnimationCompParams{ root, "def.alist" });
		}

		{
			entt::entity unrealPuppet = registry.create();
			snapshot.entityIds.insert(unrealPuppet);
			fs::path root = fs::path("unreal_puppet");
			sXform xform = sXform::identity();
			xform.mPos = dx::XMVectorSet(3.0f, 0.0f, 0.0f, 1.0f);
			insertEntityParam(pPosParams, unrealPuppet, sPositionCompParams{ xform });
			sFbxRiggedModelParams rm;
			rm.modelPath = root / "SideScrollerSkeletalMesh.FBX";
			rm.materialPath = root / "def.mtl";
			insertEntityParam(pFbxModelParams, unrealPuppet, std::move(rm));
			insertEntityParam(pAnimParams, unrealPuppet, sAnimationCompParams{ root, "def.alist", 0, 1.0f / 60.0f });
		}

		auto insertParam = [](sSceneSnapshot& snapshot, auto* pList) {
			entt::id_type t = entt::type_info<std::remove_pointer<decltype(pList)>::type::TParams>::id();
			snapshot.paramsOrder.push_back(t);
			snapshot.params.emplace(t, std::unique_ptr<iParamList>(pList));
		};
			
		insertParam(snapshot, pPosParams);
		insertParam(snapshot, pModelParams);
		insertParam(snapshot, pRiggedModelParams);
		insertParam(snapshot, pFbxModelParams);
		insertParam(snapshot, pAnimParams);
	}
}

void cScene::save() {
	snapshot.save(get_scene_snapshot_path());
}




//////

cSceneMgr::cSceneMgr()
	: mpUpdateQueue(std::make_unique<cUpdateQueue>())
	, mpScene(std::make_unique<cScene>())
	, mpSceneEditor(std::make_unique<cSceneEditor>())
{
	mpSceneEditor->register_component<sPositionComp>("Position").openByDefault = false;
	mpSceneEditor->register_component<cModelComp>("Model");
	mpSceneEditor->register_component<cRigComp>("Rig");
	//mpSceneEditor->register_component<cAnimationComp>("Animation");

	mpSceneEditor->register_param<sPositionCompParams>("Position").openByDefault = true;
	mpSceneEditor->register_param<sModelCompParams>("Model");
	mpSceneEditor->register_param<sRiggedModelCompParams>("Rigged Model");
	mpSceneEditor->register_param<sFbxRiggedModelParams>("Fbx Model");
	//mpSceneEditor->register_param<sAnimationCompParams>("Animation");

	//mpScene->


	mpSceneEditor->init(mpScene.get());
}

cSceneMgr::~cSceneMgr() {}

void cSceneMgr::update() {
	mpUpdateQueue->begin_exec();
	mpUpdateQueue->advance_exec(eUpdatePriority::End);
	mpUpdateQueue->end_exec();
}
