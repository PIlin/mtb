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
};

class cModelComp {
	cModel mModel;
public:
	cModelComp() = default;
	cModelComp(cModelComp&& other) : mModel(std::move(other.mModel)) {}
	cModelComp& operator=(cModelComp&&) = default;

	bool init(ConstModelDataPtr pModelData, cModelMaterial& mtl) {
		static_assert(std::is_move_constructible_v<cModelComp>, "The managed type must be at least move constructible");
		static_assert(std::is_move_assignable_v<cModelComp>, "The managed type must be at least move assignable");

		return mModel.init(pModelData, mtl);
	}

	void disp(cRdrContext const& rdrCtx, const DirectX::XMMATRIX& wmtx) const {
		mModel.disp(rdrCtx, wmtx);
	}

	void dbg_ui() {
		mModel.dbg_ui();
	}
};



class cSolidModelData {
protected:
	cModelMaterial mMtl;
public:
};


class cLightning : public cSolidModelData {
	entt::entity mEntity;
public:
	bool init(entt::registry& reg) {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("lightning");

		ConstModelDataPtr pMdlData;
		if (res)
		{
			pMdlData = nModelLoader::find_or_load(root / "lightning.geo");
			res = res && pMdlData;
		}
		res = res && mMtl.load(get_gfx().get_dev(), pMdlData, root / "lightning.mtl");

		entt::entity en = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(en);
		cModelComp& mdl = reg.emplace<cModelComp>(en);

		res = res && mdl.init(pMdlData, mMtl);

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

	void init(cRigData const* pRigData) {
		mRig.init(pRigData);
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
			auto view = mRegistry.view<cModelComp>();
			view.each([](cModelComp& mdl) {
				mdl.dbg_ui();
			});

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
	const cAnimationList* mpAnimList = nullptr;
	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;
	cstr mId;

public:

	cAnimationComp(const cAnimationList* pAnimList, cstr id, float speed = 1.0f)
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


class cSkinnedModelData {
protected:
	cModelMaterial mMtl;
	cRigData mRigData;
};


class cSkinnedAnimatedModelData : public cSkinnedModelData {
protected:
	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;
};

class cOwl : public cSkinnedAnimatedModelData {
	entt::entity mEntity;
public:

	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "owl";
		const fs::path root = cPathManager::build_data_path("owl");

		ConstModelDataPtr pMdlData;
		if (res)
		{
			pMdlData = nModelLoader::find_or_load(root / "def.geo");
			res = res && pMdlData;
		}
		res = res && mMtl.load(get_gfx().get_dev(), pMdlData, root / "def.mtl");

		res = res && mRigData.load(root / "def.rig");

		res = res && mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, &mAnimList, id);

		res = res && mdl.init(pMdlData, mMtl);
		rig.init(&mRigData);

		const float scl = 0.01f;
		pos.wmtx = dx::XMMatrixScaling(scl, scl, scl);
		pos.wmtx *= dx::XMMatrixTranslation(1.0f, 0.0f, 0.0f);

		return res;
	}
};

class cJumpingSphere : public cSkinnedAnimatedModelData {
	entt::entity mEntity;
public:
	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "jumping_sphere";
		const fs::path root = cPathManager::build_data_path("jumping_sphere");

		ConstModelDataPtr pMdlData;
		if (res)
		{
			pMdlData = nModelLoader::find_or_load(root / "def.geo");
			res = res && pMdlData;
		}
		res = res && mMtl.load(get_gfx().get_dev(), pMdlData, root / "def.mtl", true);

		res = res && mRigData.load(root / "def.rig");

		res = res && mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, &mAnimList, id);

		res = res && mdl.init(pMdlData, mMtl);
		rig.init(&mRigData);

		const float scl = 1.0f;
		pos.wmtx = dx::XMMatrixScaling(scl, scl, scl);
		pos.wmtx *= dx::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

		return res;
	}
};


class cUnrealPuppet : public cSkinnedAnimatedModelData {
	entt::entity mEntity;
public:

	bool init(entt::registry& reg) {
		bool res = true;

		cstr id = "unreal_puppet";
		const fs::path root = cPathManager::build_data_path("unreal_puppet");

		ConstModelDataPtr pMdlData;
		res = res && nModelLoader::find_or_load_unreal(root / "SideScrollerSkeletalMesh.FBX", *&pMdlData, &mRigData);
		res = res && mMtl.load(get_gfx().get_dev(), pMdlData, root / "def.mtl");

		{
			cAssimpLoader animLoader;
			//animLoader.load_unreal_fbx(cPathManager::build_data_path(OBJPATH "SideScrollerIdle.FBX"));
			animLoader.load_unreal_fbx(root / "SideScrollerWalk.FBX");
			mAnimDataList.load(animLoader);
			mAnimList.init(mAnimDataList, mRigData);
		}

		mEntity = reg.create();
		sPositionComp& pos = reg.emplace<sPositionComp>(mEntity);
		cModelComp& mdl = reg.emplace<cModelComp>(mEntity);
		cRigComp& rig = reg.emplace<cRigComp>(mEntity);
		float speed = 1.0f / 60.0f;
		cAnimationComp& anim = reg.emplace<cAnimationComp>(mEntity, &mAnimList, id, speed);

		res = res && mdl.init(pMdlData, mMtl);
		rig.init(&mRigData);

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
	entt::registry registry;

	cGnomon gnomon;
	cLightning lightning;
	cJumpingSphere sphere;
	cOwl owl;
	cUnrealPuppet upuppet;
	cCameraManager cameraMgr;
	cLightMgrUpdate lightMgr;
	cModelRdrJobs modelRdrJobs;
	cAnimationSys animSys;
public:

	cScene() 
		: modelRdrJobs(registry)
		, animSys(registry)
	{
		gnomon.init();
		lightning.init(registry);
		sphere.init(registry);
		owl.init(registry);
		upuppet.init(registry);
		cameraMgr.init();
		lightMgr.init();
		modelRdrJobs.init();
		animSys.register_anim_update();
	}
};



//////

cSceneMgr::cSceneMgr()
	: mpUpdateQueue(std::make_unique<cUpdateQueue>())
	, mpScene(std::make_unique<cScene>())
{}

cSceneMgr::~cSceneMgr() {}

void cSceneMgr::update() {
	mpUpdateQueue->begin_exec();
	mpUpdateQueue->advance_exec(eUpdatePriority::End);
	mpUpdateQueue->end_exec();
}
