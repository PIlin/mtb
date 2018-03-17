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
		mModel.disp(ctx);
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
		mModel.disp(ctx);
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
		mModel.mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mModel.mWmtx *= dx::XMMatrixTranslation(1.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
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
		mModel.mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mModel.mWmtx *= dx::XMMatrixTranslation(2.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
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
		mModel.mWmtx = dx::XMMatrixScaling(scl, scl, scl);
		mModel.mWmtx *= dx::XMMatrixRotationX(DEG2RAD(-90.0f));
		mModel.mWmtx *= dx::XMMatrixTranslation(3.0f, 0.0f, 0.0f);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
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

////
#include <Psapi.h>
namespace Cry {
	template<class T>
	inline void ZeroStruct(T& t)
	{
		memset(&t, 0, sizeof(t));
	}

	struct SProcessMemInfo
	{
		uint64_t PageFaultCount;
		uint64_t PeakWorkingSetSize;
		uint64_t WorkingSetSize;
		uint64_t QuotaPeakPagedPoolUsage;
		uint64_t QuotaPagedPoolUsage;
		uint64_t QuotaPeakNonPagedPoolUsage;
		uint64_t QuotaNonPagedPoolUsage;
		uint64_t PagefileUsage;
		uint64_t PeakPagefileUsage;

		uint64_t TotalPhysicalMemory;
		int64_t  FreePhysicalMemory;

		uint64_t TotalVideoMemory;
		int64_t  FreeVideoMemory;
	};

	bool GetProcessMemInfo(SProcessMemInfo& minfo)
	{
		ZeroStruct(minfo);
		MEMORYSTATUSEX mem;
		mem.dwLength = sizeof(mem);
		GlobalMemoryStatusEx(&mem);

		minfo.TotalPhysicalMemory = mem.ullTotalPhys;
		minfo.FreePhysicalMemory = mem.ullAvailPhys;

		//////////////////////////////////////////////////////////////////////////
		typedef BOOL(WINAPI * GetProcessMemoryInfoProc)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);

		PROCESS_MEMORY_COUNTERS pc;
		ZeroStruct(pc);
		pc.cb = sizeof(pc);
		static HMODULE hPSAPI = LoadLibraryA("psapi.dll");
		if (hPSAPI)
		{
			static GetProcessMemoryInfoProc pGetProcessMemoryInfo = (GetProcessMemoryInfoProc)GetProcAddress(hPSAPI, "GetProcessMemoryInfo");
			if (pGetProcessMemoryInfo)
			{
				if (pGetProcessMemoryInfo(GetCurrentProcess(), &pc, sizeof(pc)))
				{
					minfo.PageFaultCount = pc.PageFaultCount;
					minfo.PeakWorkingSetSize = pc.PeakWorkingSetSize;
					minfo.WorkingSetSize = pc.WorkingSetSize;
					minfo.QuotaPeakPagedPoolUsage = pc.QuotaPeakPagedPoolUsage;
					minfo.QuotaPagedPoolUsage = pc.QuotaPagedPoolUsage;
					minfo.QuotaPeakNonPagedPoolUsage = pc.QuotaPeakNonPagedPoolUsage;
					minfo.QuotaNonPagedPoolUsage = pc.QuotaNonPagedPoolUsage;
					minfo.PagefileUsage = pc.PagefileUsage;
					minfo.PeakPagefileUsage = pc.PeakPagefileUsage;

					return true;
				}
			}
		}
		return false;
	}

}

class cTexAllocationTest {
	using tTexPtr = com_ptr<ID3D11Texture2D>;
	using tViewPtr = com_ptr<ID3D11ShaderResourceView>;

	struct sValue
	{
		tTexPtr tex;
		tViewPtr view;
		size_t size;
	};

	cUpdateSubscriberScope mUpdate;

	std::deque<sValue> mTextures;
	int mW = 1024;
	int mH = 1024;

	sValue create() {
		ID3D11Device* pDev = get_gfx().get_dev();

		size_t size = mW * mH * 4;

		

		auto desc = D3D11_TEXTURE2D_DESC();
		desc.Width = (UINT)mW;
		desc.Height = (UINT)mH;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		//desc.Usage = D3D11_USAGE_DYNAMIC;
		//desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA* pInitData = nullptr;

		std::unique_ptr<char[]> pData(new char[size]);
#if 1
		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = pData.get();
		initData.SysMemPitch = desc.Width * 4;
		initData.SysMemSlicePitch = 0;
		pInitData = &initData;
#endif

		tTexPtr pTex;
		HRESULT hr = pDev->CreateTexture2D(&desc, pInitData, pTex.pp());
		if (!SUCCEEDED(hr))
			return sValue{ tTexPtr(), tViewPtr(), 0 };

		auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC();
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		tViewPtr pView;
		hr = pDev->CreateShaderResourceView(pTex, &srvDesc, pView.pp());
		if (!SUCCEEDED(hr))
			return sValue{ tTexPtr(), tViewPtr(), 0 };


		return sValue{ std::move(pTex), std::move(pView), size };
	}

	float MB(uint64_t b) { return b / float(1024 * 1024); }
	float MB(int64_t b) { return b / float(1024 * 1024); }

	void update() {
		ImGui::Begin("tex allocation test");

		int texturesCount = (int)mTextures.size();

		ImGui::SliderInt("W", &mW, 1, 4096);
		ImGui::SliderInt("H", &mH, 1, 4096);

		ImGui::SliderInt("Textures Count", &texturesCount, 0, 100);

		while (texturesCount < mTextures.size()) {
			mTextures.pop_front();
		}
		while (texturesCount > mTextures.size()) {
			mTextures.emplace_back(create());
		}

		size_t sizeSum = 0;
		for (const auto& val : mTextures) {
			sizeSum += val.size;
		}

		ImGui::LabelText("Mem Estimation", "%.3f", MB(sizeSum));

		Cry::SProcessMemInfo memInfo;
		if (Cry::GetProcessMemInfo(memInfo)) {
			ImGui::LabelText("PageFaults", "%zu", memInfo.PageFaultCount);
			ImGui::LabelText("PagefileUsage", "%.3f ; %.3f", MB(memInfo.PagefileUsage), MB(memInfo.PeakPagefileUsage));
			ImGui::LabelText("WorkingSet", "%.3f ; %.3f", MB(memInfo.WorkingSetSize), MB(memInfo.PeakWorkingSetSize));

			ImGui::LabelText("DIFF", "%.3f", MB(int64_t(memInfo.PagefileUsage) - int64_t(sizeSum)));
		}

		ImGui::End();
	}


	
public:

	void init() {
		cSceneMgr::get().get_update_queue().add(eUpdatePriority::SceneDisp, tUpdateFunc(std::bind(&cTexAllocationTest::update, this)), mUpdate);
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
	cTexAllocationTest texAllocationTest;
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
