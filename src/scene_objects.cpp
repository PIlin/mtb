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

struct sSceneEditCtx
{
	cCamera::sView camView;
};

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


class cModelComp {
	cModel mModel;
public:
	cModelComp() = default;
	cModelComp(cModelComp&& other) : mModel(std::move(other.mModel)) {}
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

	return res;
}



class cLightning {
	entt::entity mEntity;
public:
	bool init(entt::registry& reg) {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("lightning");

		ConstModelDataPtr pMdlData;
		ConstModelMaterialPtr pMtl;
		res = res && nResLoader::find_or_load(root / "lightning.geo", *&pMdlData);
		res = res && nResLoader::find_or_load(root / "lightning.mtl", pMdlData, *&pMtl);

		entt::entity en = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(en);
		cModelComp& mdl = reg.emplace<cModelComp>(en);

		res = res && mdl.init(std::move(pMdlData), std::move(pMtl));

		mEntity = en;

		//mModel.mWmtx = DirectX::XMMatrixScaling(0.3f, 0.3f, 0.3f);

		return res;
	}
};



class cRigComp {
	cRig mRig;
public:
	cRigComp() = default;
	cRigComp(cRigComp&& other) : mRig(std::move(other.mRig)) {}
	cRigComp& operator=(cRigComp&&) = default;

	void init(ConstRigDataPtr pRigData) {
		mRig.init(std::move(pRigData));
	}

	void update_rig_mtx(const sPositionComp& pos) {
		mRig.calc_local();
		mRig.calc_world(pos.wmtx);
	}

	void upload_skin(cRdrContext const& ctx) const { mRig.upload_skin(ctx); }

	cRig& get() { return mRig; }
};


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
			//auto view = mRegistry.view<cModelComp>();
			//view.each([](cModelComp& mdl) {
			//	mdl.dbg_ui();
			//});

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
			mdl.disp(ctx, pos.wmtx);
		});
	}

	void disp_skinned(cRdrContext const& ctx) const {
		auto view = mRegistry.view<sPositionComp, cModelComp, cRigComp>();
		view.each([&ctx](const sPositionComp& pos, const cModelComp& mdl, cRigComp& rig) {
			rig.update_rig_mtx(pos); // todo: move to separate step
			rig.upload_skin(ctx);
			mdl.disp(ctx, pos.wmtx);
		});
	}
};


class cAnimationComp {
	ConstAnimationListPtr mpAnimList;
	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;
	cstr mId;

public:

	cAnimationComp(ConstAnimationListPtr pAnimList, cstr id, float speed = 1.0f)
		: mpAnimList(pAnimList)
		, mId(id)
		, mSpeed(speed)
	{}

	void update(cRig& rig) {
		assert(mpAnimList);

		int32_t animCount = mpAnimList->get_count();
		if (animCount > 0) {
			auto& anim = (*mpAnimList)[mCurAnim];
			float lastFrame = anim.get_last_frame();

			anim.eval(rig, mFrame);
			mFrame += mSpeed;
			if (mFrame > lastFrame)
				mFrame = 0.0f;

			char buf[64];
			::sprintf_s(buf, "anim %s", mId.p);
			ImGui::Begin(buf);
			ImGui::LabelText("name", "%s", anim.get_name().p);
			ImGui::SliderInt("curAnim", &mCurAnim, 0, animCount - 1);
			ImGui::SliderFloat("frame", &mFrame, 0.0f, lastFrame);
			ImGui::SliderFloat("speed", &mSpeed, 0.0f, 3.0f);
			ImGui::End();
		}
	}
};

class cAnimationSys {
	entt::registry& mRegistry;
	cUpdateSubscriberScope mAnimUpdate;
public:
	cAnimationSys(entt::registry& reg) : mRegistry(reg) {}

	void register_anim_update() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneAnimUpdate, tUpdateFunc(std::bind(&cAnimationSys::update_anim, this)), mAnimUpdate);
	}

	void update_anim() {
		auto view = mRegistry.view<cAnimationComp, cRigComp>();
		view.each([](cAnimationComp& anim, cRigComp& rig) {
			anim.update(rig.get());
		});
	}
};



class cOwl {
	entt::entity mEntity;
public:

	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "owl";
		const fs::path root = cPathManager::build_data_path("owl");

		ConstModelDataPtr pMdlData;
		ConstModelMaterialPtr pMtl;
		ConstRigDataPtr pRigData;
		res = res && nResLoader::find_or_load(root / "def.geo", *&pMdlData);
		res = res && nResLoader::find_or_load(root / "def.mtl", pMdlData, *&pMtl);

		res = res && nResLoader::find_or_load(root / "def.rig", *&pRigData);

		ConstAnimationListPtr pAnimList;
		res = res && nResLoader::find_or_load(root, "def.alist", *pRigData, *&pAnimList);

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, std::move(pAnimList), id);

		res = res && mdl.init(std::move(pMdlData), std::move(pMtl));
		rig.init(std::move(pRigData));

		const float scl = 0.01f;
		pos.wmtx = dx::XMMatrixScaling(scl, scl, scl);
		pos.wmtx *= dx::XMMatrixTranslation(1.0f, 0.0f, 0.0f);

		return res;
	}
};

class cJumpingSphere {
	entt::entity mEntity;
public:
	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "jumping_sphere";
		const fs::path root = cPathManager::build_data_path("jumping_sphere");

		ConstModelDataPtr pMdlData;
		ConstModelMaterialPtr pMtl;
		ConstRigDataPtr pRigData;
		res = res && nResLoader::find_or_load(root / "def.geo", *&pMdlData);
		res = res && nResLoader::find_or_load(root / "def.mtl", pMdlData, *&pMtl, true);
		res = res && nResLoader::find_or_load(root / "def.rig", *&pRigData);

		ConstAnimationListPtr pAnimList;
		res = res && nResLoader::find_or_load(root, "def.alist", *pRigData, *&pAnimList);

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, std::move(pAnimList), id);

		res = res && mdl.init(std::move(pMdlData), std::move(pMtl));
		rig.init(std::move(pRigData));

		const float scl = 1.0f;
		pos.wmtx = dx::XMMatrixScaling(scl, scl, scl);
		pos.wmtx *= dx::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

		return res;
	}
};


class cUnrealPuppet {
	entt::entity mEntity;
public:

	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "unreal_puppet";
		const fs::path root = cPathManager::build_data_path("unreal_puppet");

		ConstModelDataPtr pMdlData;
		ConstRigDataPtr pRigData;
		ConstModelMaterialPtr pMtl;
		res = res && nResLoader::find_or_load_unreal(root / "SideScrollerSkeletalMesh.FBX", *&pMdlData, *&pRigData);
		res = res && nResLoader::find_or_load(root / "def.mtl", pMdlData, *&pMtl, true);

		ConstAnimationListPtr pAnimList;
		res = res && nResLoader::find_or_load_unreal(root / "SideScrollerWalk.FBX", *pRigData, *&pAnimList);

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		float speed = 1.0f / 60.0f;
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, std::move(pAnimList), id, speed);

		res = res && mdl.init(std::move(pMdlData), std::move(pMtl));
		rig.init(std::move(pRigData));

		const float scl = 0.01f;
		pos.wmtx = dx::XMMatrixScaling(scl, scl, scl);
		pos.wmtx *= dx::XMMatrixRotationX(DEG2RAD(-90.0f));
		pos.wmtx *= dx::XMMatrixTranslation(3.0f, 0.0f, 0.0f);

		return res;
	}
};

////

class cCameraManager : public iRdrJob {
	cCamera mCamera;
	cTrackballCam mTrackballCam;
	cUpdateSubscriberScope mCameraUpdate;

public:
	void init() {
		mTrackballCam.init(mCamera);
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::Camera, tUpdateFunc(std::bind(&cCameraManager::update_cam, this)), mCameraUpdate);
	}

	void update_cam() {
		mTrackballCam.update(mCamera);
		cRdrQueueMgr::get().add_model_prologue_job(*this);
	}

	virtual void disp_job(cRdrContext const& rdrCtx) const override {
		upload_cam(rdrCtx);
	}

	void upload_cam(cRdrContext const& rdrCtx) const {
		auto pCtx = rdrCtx.get_ctx();
		auto& camCBuf = rdrCtx.get_cbufs().mCameraCBuf;
		camCBuf.mData.viewProj = mCamera.mView.mViewProj;
		camCBuf.mData.view = mCamera.mView.mView;
		camCBuf.mData.proj = mCamera.mView.mProj;
		camCBuf.mData.camPos = mCamera.mView.mPos;
		camCBuf.update(pCtx);
		camCBuf.set_VS(pCtx);
		camCBuf.set_PS(pCtx);
	}

	const cCamera& get_cam() const { return mCamera; }
};

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



class cScene {
	friend class cSceneEditor;

	entt::registry registry;

	cGnomon gnomon;
	//cLightning lightning;
	//cJumpingSphere sphere;
	//cOwl owl;
	//cUnrealPuppet upuppet;
	cCameraManager cameraMgr;
	cLightMgrUpdate lightMgr;
	cModelRdrJobs modelRdrJobs;
	cAnimationSys animSys;

	sSceneSnapshot snapshot;

	
public:

	cScene() 
		: modelRdrJobs(registry)
		, animSys(registry)
	{
		gnomon.init();
		//lightning.init(registry);
		//sphere.init(registry);
		//owl.init(registry);
		//upuppet.init(registry);
		cameraMgr.init();
		lightMgr.init();
		modelRdrJobs.init();
		animSys.register_anim_update();

		

		load();
		create_from_snapshot();
	}

	~cScene() {
		save();
	}

	static fs::path get_scene_snapshot_path() { return cPathManager::build_data_path("scene.json"); }

	void create_from_snapshot() {
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

	void load() {
		if (snapshot.load(get_scene_snapshot_path())) {

		}
		else {
			fs::path root;
			root = fs::path("lightning");
			sXform xform;
			xform.init(nMtx::g_Identity);

			sParamList<sPositionCompParams>* pPosParams = new sParamList<sPositionCompParams>();
			sParamList<sModelCompParams>* pModelParams = new sParamList<sModelCompParams>();

			auto insertEntityParam = [](auto* pList, entt::entity en, auto&& param) {
				const uint32_t paramId = (uint32_t)pList->paramList.size();
				pList->paramList.emplace(paramId, std::forward<decltype(param)>(param));
				pList->entityList.emplace(en, paramId);
			};

			entt::entity lightning = registry.create();
			snapshot.entityIds.push_back(lightning);
			insertEntityParam(pPosParams, lightning, sPositionCompParams{ xform });
			insertEntityParam(pModelParams, lightning, sModelCompParams{ root / "lightning.geo", root / "lightning.mtl" });

			root = fs::path("owl");
			entt::entity owl = registry.create();
			snapshot.entityIds.push_back(owl);
			{
				const float scl = 0.01f;
				xform.mPos = dx::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
				xform.mQuat = dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				xform.mScale = dx::XMVectorSet(scl, scl, scl, 0.0f);
				insertEntityParam(pPosParams, owl, sPositionCompParams{ xform });
				insertEntityParam(pModelParams, owl, sModelCompParams{ root / "def.geo", root / "lightning.mtl" });
			}

			auto insertParam = [](sSceneSnapshot& snapshot, auto* pList) {
				entt::id_type t = entt::type_info<std::remove_pointer<decltype(pList)>::type::TParams>::id();
				snapshot.paramsOrder.push_back(t);
				snapshot.params.emplace(t, std::unique_ptr<iParamList>(pList));
			};
			
			insertParam(snapshot, pPosParams);
			insertParam(snapshot, pModelParams);
		}
	}

	void save() {
		snapshot.save(get_scene_snapshot_path());
	}
};

class cSceneEditor {
	struct iComponentReg {
		std::string name;
		entt::id_type id;
		int32_t order = 0;
		bool openByDefault = false;

		const char* ui_name() const { return name.c_str(); }
		virtual void ui(entt::registry& reg, entt::entity en, sSceneEditCtx& ctx) = 0;
		virtual ~iComponentReg() {}
	};

	template <typename T>
	struct sComponentReg : public iComponentReg {
		sComponentReg(const char* szName) {
			name = szName;
			id = entt::type_info<T>::id();
		}

		virtual void ui(entt::registry& reg, entt::entity en, sSceneEditCtx& ctx) override {
			if (T* p = reg.try_get<T>(en)) {
				p->dbg_ui(ctx);
			}
		}
	};

	cUpdateSubscriberScope mDbgUpdate;
	cScene* mpScene = nullptr;
	entt::entity mSelected = entt::null;
	std::map<entt::id_type, std::unique_ptr<iComponentReg>> mComponentTypes;

public:

	template <typename TComp>
	iComponentReg& register_component(const char* szName) {
		std::unique_ptr<iComponentReg> r = std::make_unique<sComponentReg<TComp>>(szName);
		entt::id_type id = r->id;
		r->order = (int32_t)mComponentTypes.size();
		return *(mComponentTypes[id] = std::move(r)).get();
	}

	cSceneEditor() {
		register_component<sPositionComp>("Position").openByDefault = true;
		register_component<cModelComp>("Model");
	}

	void init(cScene* pScene) {
		mpScene = pScene;
		mSelected = entt::null;
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::Begin, tUpdateFunc(std::bind(&cSceneEditor::dbg_ui, this)), mDbgUpdate);
	}

	static const char* format_entity(std::string& buf, entt::entity en) {
		buf = std::to_string((uint32_t)entt::registry::entity(en));
		buf += ':';
		buf += std::to_string((uint32_t)entt::registry::version(en));
		return buf.c_str();
	}

	void dbg_ui() {
		if (!mpScene) return;
		
		sSceneEditCtx ctx;
		ctx.camView = mpScene->cameraMgr.get_cam().mView;

		ImGui::Begin("scene");
		show_entity_list(ctx);
		
		if (mSelected != entt::null) show_entity_components(mSelected, ctx);

		ImGui::End();
	}

	void show_entity_list(sSceneEditCtx& ctx) {
		entt::registry& reg = mpScene->registry;

		std::vector<entt::entity> alive;
		int selectedIdx = -1;
		reg.each([&](entt::entity en) { if (en == mSelected) { selectedIdx = (int)alive.size(); } alive.push_back(en); });
		std::string buffer;
		auto getter = [&](int i, const char** szText) {
			if (i < alive.size()) {
				(*szText) = format_entity(buffer, alive[i]);
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

	void show_entity_components(entt::entity en, sSceneEditCtx& ctx) {
		entt::registry& reg = mpScene->registry;

		std::vector<entt::id_type> componentTypes;
		reg.visit(en, [&](entt::id_type t) { componentTypes.push_back(t); });
		sort_components_by_order(componentTypes);

		ImGui::PushID((int)en);
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
				std::string node = "**Unknown type " + std::to_string(uint32_t(t));
				if (ImGui::TreeNode(node.c_str())) {
					ImGui::TreePop();
				}
			}
		}
		ImGui::PopID();
	}

	void sort_components_by_order(std::vector<entt::id_type>& comp) const {
		std::sort(comp.begin(), comp.end(), [&](entt::id_type a, entt::id_type b) {
			auto ait = mComponentTypes.find(a);
			auto bit = mComponentTypes.find(b);
			auto eit = mComponentTypes.end();
			if (ait != eit && bit != eit) {
				return ait->second->order < bit->second->order;
			}
			else if (ait != eit) {
				return true;
			}
			return false;
		});
	}
};


//////

cSceneMgr::cSceneMgr()
	: mpUpdateQueue(std::make_unique<cUpdateQueue>())
	, mpScene(std::make_unique<cScene>())
	, mpSceneEditor(std::make_unique<cSceneEditor>())
{
	mpSceneEditor->init(mpScene.get());
}

cSceneMgr::~cSceneMgr() {}

void cSceneMgr::update() {
	mpUpdateQueue->begin_exec();
	mpUpdateQueue->advance_exec(eUpdatePriority::End);
	mpUpdateQueue->end_exec();
}
