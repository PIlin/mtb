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


#include <imgui.h>

#include <assimp/Importer.hpp>
#include "assimp_loader.hpp"

namespace dx = DirectX;
using dx::XMFLOAT4;

class cGnomon {
	struct sVtx {
		float mPos[3];
		float mClr[4];
	};

	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	com_ptr<ID3D11InputLayout> mpIL;
	cVertexBuffer mVtxBuf;
	int mState = 0;
public:

	cGnomon() = default;
	~cGnomon() { deinit(); }

	void disp() {
		if (!(mpVS && mpVS)) return;

		auto pCtx = get_gfx().get_ctx();

		//mConstBuf.mData.wmtx = dx::XMMatrixIdentity();
		auto& cbuf = cConstBufStorage::get().mMeshCBuf;
		cbuf.mData.wmtx = dx::XMMatrixTranslation(0, 0, 0);
		cbuf.update(pCtx);
		cbuf.set_VS(pCtx);

		UINT pStride[] = { sizeof(sVtx) };
		UINT pOffset[] = { 0 };
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
			{ 0.0f, 0.0f, 0.0f, 1.0, 0.0f, 0.0f, 1.0f },
			{ 1.0f, 0.0f, 0.0f, 1.0, 0.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0, 1.0f, 0.0f, 1.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0, 1.0f, 0.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f, 0.0, 0.0f, 1.0f, 1.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0, 0.0f, 1.0f, 1.0f },
		};

		mVtxBuf.init(pDev, vtx, LENGTHOF_ARRAY(vtx), sizeof(vtx[0]));
	}

	void deinit() {
		mVtxBuf.deinit();
		mpIL.reset();
	}
};



class cSolidModel {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;

public:
	void disp() {
		mModel.dbg_ui();
		mModel.disp();
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

		return res;
	}
};



class cSkinnedModel {
protected:
	cModel mModel;
	cModelData mMdlData;
	cModelMaterial mMtl;
	cRigData mRigData;
	cRig mRig;

	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;
public:
	void disp() {
		mRig.calc_local();
		mRig.calc_world();
		mRig.upload_skin(get_gfx().get_ctx());

		mModel.dbg_ui();
		mModel.disp();
	}
};

class cSkinnedAnimatedModel : public cSkinnedModel {
protected:
	cAnimationDataList mAnimDataList;
	cAnimationList mAnimList;

	float mFrame = 0.0f;
	float mSpeed = 1.0f;
	int mCurAnim = 0;
public:
	void disp() {
		int32_t animCount = mAnimList.get_count();
		if (animCount > 0) {
			auto& anim = mAnimList[mCurAnim];
			float lastFrame = anim.get_last_frame();

			anim.eval(mRig, mFrame);
			mFrame += mSpeed;
			if (mFrame > lastFrame)
				mFrame = 0.0f;

			ImGui::Begin("anim");
			ImGui::LabelText("name", "%s", anim.get_name());
			ImGui::SliderInt("curAnim", &mCurAnim, 0, animCount - 1);
			ImGui::SliderFloat("frame", &mFrame, 0.0f, lastFrame);
			ImGui::SliderFloat("speed", &mSpeed, 0.0f, 3.0f);
			ImGui::End();
		}
		cSkinnedModel::disp();
	}
};

class cOwl : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("owl");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl");
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		float scl = 0.01f;
		mModel.mWmtx = DirectX::XMMatrixScaling(scl, scl, scl);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};

class cJumpingSphere : public cSkinnedAnimatedModel {
public:
	bool init() {
		bool res = true;

		const fs::path root = cPathManager::build_data_path("jumping_sphere");

		res = res && mMdlData.load(root / "def.geo");
		res = res && mMtl.load(get_gfx().get_dev(), mMdlData, root / "def.mtl", true);
		res = res && mModel.init(mMdlData, mMtl);

		mRigData.load(root / "def.rig");
		mRig.init(&mRigData);

		mAnimDataList.load(root, "def.alist");
		mAnimList.init(mAnimDataList, mRigData);

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};


class cUnrealPuppet : public cSkinnedAnimatedModel {
public:

	bool init() {
		bool res = true;

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

		float scl = 0.01f;
		mModel.mWmtx = DirectX::XMMatrixScaling(scl, scl, scl);
		mModel.mWmtx *= DirectX::XMMatrixRotationX(DEG2RAD(-90.0f));

		auto pRootJnt = mRig.get_joint(0);
		if (pRootJnt) {
			pRootJnt->set_parent_mtx(&mModel.mWmtx);
		}

		return res;
	}
};



class cScene {
	cGnomon gnomon;
	cLightning lightning;
	cJumpingSphere sphere;
	cOwl owl;
	cUnrealPuppet upuppet;

public:
	cScene() {
		gnomon.init();
		//lightning.init();
		//sphere.init();
		//owl.init();
		upuppet.init();
	}

	void disp() {
		gnomon.disp();
		//lightning.disp();
		//sphere.disp();
		//owl.disp();
		upuppet.disp();
	}
};


cSceneMgr::cSceneMgr()
	: mpScene(std::make_unique<cScene>())
{}

cSceneMgr::~cSceneMgr() {}

void cSceneMgr::disp() {
	if (mpScene) { mpScene->disp(); }
}
