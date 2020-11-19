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



class cSolidModel : public iRdrJob {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;

	DirectX::XMMATRIX mWmtx = dx::XMMatrixIdentity();

	cUpdateSubscriberScope mDispUpdate;

public:

	void register_disp_update() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneDisp, tUpdateFunc(std::bind(&cSolidModel::disp, this)), mDispUpdate);
	}

	void disp() {
		mModel.dbg_ui();

		cRdrQueueMgr::get().add_model_job(*this);
	}

	virtual void disp_job(cRdrContext const& ctx) const override {
		mModel.disp(ctx, mWmtx);
	}
};



class cLightning : public cSolidModel {
public:
	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("lightning");

		res = res && mMdlData.load(root / "lightning.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "lightning.mtl");
		res = res && mModel.init(mMdlData, mMtl);

		//mModel.mWmtx = DirectX::XMMatrixScaling(0.3f, 0.3f, 0.3f);

		if (res) {
			register_disp_update();
		}

		return res;
	}
};



class cSkinnedModel : public iRdrJob {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;
	cRigData mRigData;
	cRig mRig;

	DirectX::XMMATRIX mWmtx = dx::XMMatrixIdentity();

	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;

	cstr mId;

private:
	cUpdateSubscriberScope mDispUpdate;
	
public:

	void register_disp_update() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneDisp, tUpdateFunc(std::bind(&cSkinnedModel::disp, this)), mDispUpdate);
	}

	void disp() {
		mRig.calc_local();
		mRig.calc_world();

		mModel.dbg_ui();

		cRdrQueueMgr::get().add_model_job(*this);
	}

	virtual void disp_job(cRdrContext const& ctx) const override {
		mRig.upload_skin(ctx);
		mModel.disp(ctx, mWmtx);
	}
};

class cSkinnedAnimatedModel : public cSkinnedModel {
protected:
	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;

	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;

private:
	cUpdateSubscriberScope mAnimUpdate;

public:

	void register_anim_update() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::ScenePreDisp, tUpdateFunc(std::bind(&cSkinnedAnimatedModel::update_anim, this)), mAnimUpdate);
	}
	
	void update_anim() {
		int32_t animCount = mAnimList.get_count();
		if (animCount > 0) {
			auto& anim = mAnimList[mCurAnim];
			float lastFrame = anim.get_last_frame();

			anim.eval(mRig, mFrame);
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

class cOwl : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

		mId = "owl";
		const fs::path root = cPathManager::build_data_path("owl");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl");
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		const float scl = 0.01f;
		mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mWmtx *= dx::XMMatrixTranslation(1.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mWmtx);
		}

		if (res) {
			register_anim_update();
			register_disp_update();
		}

		return res;
	}
};

class cJumpingSphere : public cSkinnedAnimatedModel {
public:
	bool init() {
		bool res = true;

		mId = "jumping_sphere";
		const fs::path root = cPathManager::build_data_path("jumping_sphere");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl", true);
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		const float scl = 1.0f;
		mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mWmtx *= dx::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mWmtx);
		}

		if (res) {
			register_anim_update();
			register_disp_update();
		}

		return res;
	}
};


class cUnrealPuppet : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

		mId = "unreal_puppet";
		const fs::path root = cPathManager::build_data_path("unreal_puppet");
		{
			cAssimpLoader loader;
			res = res && loader.load_unreal_fbx(root / "SideScrollerSkeletalMesh.FBX");
			res = res && mMdlData.load_assimp(loader);
			res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl");
			res = res && mModel.init(mMdlData, mMtl);

			mRigData.load(loader);
			mRig.init(&mRigData);
		}
		{
			cAssimpLoader animLoader;
			//animLoader.load_unreal_fbx(cPathManager::build_data_path(OBJPATH "SideScrollerIdle.FBX"));
			animLoader.load_unreal_fbx(root / "SideScrollerWalk.FBX");
			mAnimDataList.load(animLoader);
			mAnimList.init(mAnimDataList, mRigData);

			mSpeed = 1.0f / 60.0f;
		}

		const float scl = 0.01f;
		mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mWmtx *= dx::XMMatrixRotationX(DEG2RAD(-90.0f));
		mWmtx *= dx::XMMatrixTranslation(3.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mWmtx);
		}

		if (res) {
			register_anim_update();
			register_disp_update();
		}

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
public:
	void init() {
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
	cGnomon gnomon;
	cLightning lightning;
	cJumpingSphere sphere;
	cOwl owl;
	cUnrealPuppet upuppet;
	cCameraManager cameraMgr;
	cLightMgrUpdate lightMgr;
	cModelRdrJobs modelRdrJobs;
public:

	cScene() {
		gnomon.init();
		lightning.init();
		sphere.init();
		owl.init();
		upuppet.init();
		cameraMgr.init();
		lightMgr.init();
		modelRdrJobs.init();
		//texAllocationTest.init();
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
