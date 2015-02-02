#include "common.hpp"
#include "math.hpp"
#include "rdr.hpp"
#include "gfx.hpp"
#include "model.hpp"


#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>

#include <cassert>


static vec3 as_vec3(aiVector3D const& v) {
	return { { v.x, v.y, v.z } };
}

bool cModelData::load(cstr filepath) {
	Assimp::Importer importer;

	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
		aiPrimitiveType_LINE | aiPrimitiveType_POINT | aiPrimitiveType_POLYGON);

	aiScene const* pScene = importer.ReadFile(filepath, 
		/*aiProcess_FlipUVs |*/ /*aiProcess_FlipWindingOrder |*/ aiProcess_SortByPType);
	if (!pScene) {
		// todo: log importer.GetErrorString()
		return false;
	}

	int numVtx = 0;
	int numIdx = 0;
	int numGrp = pScene->mNumMeshes;

	for (int i = 0; i < numGrp; ++i) {
		aiMesh* pMesh = pScene->mMeshes[i];

		numVtx += pMesh->mNumVertices;

		int idx = pMesh->mNumFaces;
		numIdx += idx * 3;
	}

	auto pGroups = std::make_unique<sGroup[]>(numGrp);
	auto pVtx = std::make_unique<sModelVtx[]>(numVtx);
	auto pIdx = std::make_unique<uint16_t[]>(numIdx);
	auto pNames = std::make_unique<std::string[]>(numGrp);

	const uint32_t vtxSize = sizeof(sModelVtx);
	const DXGI_FORMAT idxFormat = DXGI_FORMAT_R16_UINT;

	auto pVtxItr = pVtx.get();
	auto pIdxItr = pIdx.get();
	auto pGrpItr = pGroups.get();
	auto pNamesItr = pNames.get();

	for (int i = 0; i < numGrp; ++i) {
		aiMesh* pMesh = pScene->mMeshes[i];

		auto pVtxGrpStart = pVtxItr;
		auto pIdxGrpStart = pIdxItr;

		const int meshVtx = pMesh->mNumVertices;
		aiVector3D const* pAIVtx = pMesh->mVertices;
		aiVector3D const* pAINrm = pMesh->mNormals;

		for (int vtx = 0; vtx < meshVtx; ++vtx) {
			pVtxItr->pos = as_vec3(*pAIVtx);
			pVtxItr->nrm = as_vec3(*pAINrm);

			++pVtxItr;
			++pAIVtx;
			++pAINrm;
		}

		const int meshFace = pMesh->mNumFaces;
		aiFace const* pFace = pMesh->mFaces;
		for (int face = 0; face < meshFace; ++face) {
			assert(pFace->mNumIndices == 3);
			pIdxItr[0] = pFace->mIndices[0];
			pIdxItr[1] = pFace->mIndices[1];
			pIdxItr[2] = pFace->mIndices[2];

			pIdxItr += 3;
			++pFace;
		}

		pGrpItr->mVtxOffset = static_cast<uint32_t>((pVtxGrpStart - pVtx.get()) * sizeof(pVtxGrpStart[0]));
		pGrpItr->mIdxCount = static_cast<uint32_t>((pIdxItr - pIdxGrpStart));
		pGrpItr->mIdxOffset = static_cast<uint32_t>((pIdxGrpStart - pIdx.get()) * sizeof(pIdxGrpStart[0]));
		pGrpItr->mPolyType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		*pNamesItr = pMesh->mName.C_Str();

		++pGrpItr;
		++pNamesItr;
	}

	auto pDev = get_gfx().get_dev();
	mVtx.init(pDev, pVtx.get(), numVtx, vtxSize);
	mIdx.init(pDev, pIdx.get(), numIdx, idxFormat);

	mGrpNum = numGrp;
	mpGroups = std::move(pGroups);
	mpGrpNames = std::move(pNames);

	return true;
}

void cModelData::unload() {
	mVtx.deinit();
	mIdx.deinit();
	mpGroups.release();
	mpGrpNames.release();
}


bool cModel::init(cModelData const& mdlData) {
	mpData = &mdlData;
		
	auto& ss = get_shader_storage();
	mpVS = ss.load_VS("simple.vs.cso");
	mpPS = ss.load_PS("simple.ps.cso");

	D3D11_INPUT_ELEMENT_DESC vdsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sModelVtx, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sModelVtx, nrm), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	auto pDev = get_gfx().get_dev();
	auto& code = mpVS->get_code();
	HRESULT hr = pDev->CreateInputLayout(vdsc, SIZEOF_ARRAY(vdsc), code.get_code(), code.get_size(), &mpIL);
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

	mConstBuf.init(pDev);

	return true;
}

void cModel::deinit() {
	mpData = nullptr;
}

void cModel::disp() {
	if (!mpData) return;

	auto pCtx = get_gfx().get_ctx();

	//mConstBuf.mData.wmtx = dx::XMMatrixIdentity();
	mConstBuf.mData.wmtx = DirectX::XMMatrixTranslation(0, 0, 0);
	mConstBuf.update(pCtx);
	mConstBuf.set_VS(pCtx, 0);

	//camCBuf.set_VS(pCtx, 1);

	pCtx->IASetInputLayout(mpIL);

	pCtx->VSSetShader(mpVS->asVS(), nullptr, 0);
	pCtx->PSSetShader(mpPS->asPS(), nullptr, 0);

	auto grpNum = mpData->mGrpNum;
	for (uint32_t i = 0; i < grpNum; ++i) {
		sGroup const& grp = mpData->mpGroups[i];

		mpData->mVtx.set(pCtx, 0, grp.mVtxOffset);
		mpData->mIdx.set(pCtx, grp.mIdxOffset);
		pCtx->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)grp.mPolyType);
		//pCtx->Draw(grp.mIdxCount, 0);
		pCtx->DrawIndexed(grp.mIdxCount, 0, 0);

		//break;
	}

}