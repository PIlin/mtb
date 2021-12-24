#include "common.hpp"
#include "math.hpp"

#include "scene.hpp"
#include "update_queue.hpp"

#include "rdr/rdr.hpp"
#include "rdr/rdr_queue.hpp"
#include "rdr/gfx.hpp"
#include "rdr/sh.hpp"
#include "rdr/light.hpp"

#include "model_sys.hpp"
#include "anim_sys.hpp"
#include "camera_mgr.hpp"
#include "move_sys.hpp"

namespace dx = DirectX;


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

	void init(cUpdateGraph& graph) {
		if (init()) {
			sUpdateDepRes rdrJobGnomon = graph.register_res("rdr_job", "rdr_job_gnomon");
			graph.add(sUpdateDepDesc{ {}, {rdrJobGnomon} },
				MAKE_UPDATE_FUNC_THIS(cGnomon::disp), mDispUpdate);
		}
	}

	void init(cUpdateQueue& queue) {
		if (init()) {
			queue.add(eUpdatePriority::SceneDisp, MAKE_UPDATE_FUNC_THIS(cGnomon::disp), mDispUpdate);
		}
	}

	void deinit() {
		mDispUpdate.reset();
		mVtxBuf.deinit();
		mpIL.reset();
	}

private:
	bool init() {
		auto& ss = cShaderStorage::get();
		mpVS = ss.load_VS("simple.vs.cso");
		if (!mpVS) return false;
		mpPS = ss.load_PS("simple.ps.cso");
		if (!mpPS) return false;

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
		return true;
	}
};


////

class cLightMgrUpdate : public iRdrJob {
	cLightMgr mLightMgr;
	cUpdateSubscriberScope mLightUpdate;
public:
	void init(cUpdateQueue& queue) {
		queue.add(eUpdatePriority::Light, MAKE_UPDATE_FUNC_THIS(cLightMgrUpdate::update), mLightUpdate);
	}

	void init(cUpdateGraph& graph) {
		sUpdateDepRes rdrJobProlLight = graph.register_res("rdr_prologue_job", "rdr_prologue_job_light");
		sUpdateDepRes light = graph.register_res("global", "light");
		graph.add(sUpdateDepDesc{ {}, {rdrJobProlLight, light} },
			MAKE_UPDATE_FUNC_THIS(cLightMgrUpdate::update), mLightUpdate);
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

struct cScene::sSceneImpl {
	cGnomon gnomon;
	cLightMgrUpdate lightMgr;
	cModelDispSys modelSys;
	cAnimationSys animSys;
	cMoveSys moveSys;

public:

	sSceneImpl(entt::registry& registry, cUpdateQueue& updateQueue)
		: modelSys(registry)
		, animSys(registry)
		, moveSys(registry)
	{
		gnomon.init(updateQueue);
		lightMgr.init(updateQueue);
		modelSys.register_update(updateQueue);
		animSys.register_update(updateQueue);
		moveSys.register_update(updateQueue);
	}

	sSceneImpl(entt::registry& registry, cUpdateGraph& graph)
		: modelSys(registry)
		, animSys(registry)
		, moveSys(registry)
	{
		gnomon.init(graph);
		lightMgr.init(graph);
		modelSys.register_update(graph);
		animSys.register_update(graph);
		moveSys.register_update(graph);
	}
};

cScene::cScene(const std::string& name)
#if USE_GRAPH_UPDATE
	: mpUpdateGraph(std::make_unique<cUpdateGraph>())
#else
	: mpUpdateQueue(std::make_unique<cUpdateQueue>())
#endif
	, mpCameraMgr(std::make_unique<cCameraManager>())
#if USE_GRAPH_UPDATE
	, mpSceneImpl(std::make_unique<sSceneImpl>(registry, *mpUpdateGraph))
#else
	, mpSceneImpl(std::make_unique<sSceneImpl>(registry, *mpUpdateQueue))
#endif
	, mName(name)
{
#if USE_GRAPH_UPDATE
	mpCameraMgr->init(*mpUpdateGraph);
#else
	mpCameraMgr->init(*mpUpdateQueue);
#endif
	load();
	create_from_snapshot();
}

cScene::~cScene() {
	save();
}

static fs::path get_scene_snapshot_path(const std::string& name) { return cPathManager::build_data_path(name); }

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
	if (snapshot.load(get_scene_snapshot_path(mName))) {

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
			entt::id_type t = entt::type_id<std::remove_pointer<decltype(pList)>::type::TParams>().hash();
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
	snapshot.save(get_scene_snapshot_path(mName));
}

void cScene::update() {
	MICROPROFILE_SCOPEI("main", "cScene::update()", -1);
#if USE_GRAPH_UPDATE
	mpUpdateGraph->exec();
#else
	mpUpdateQueue->begin_exec();
	mpUpdateQueue->advance_exec(eUpdatePriority::End);
	mpUpdateQueue->end_exec();
#endif
}
