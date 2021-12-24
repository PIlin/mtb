#include "common.hpp"
#include "dbg_draw.hpp"
#include "gfx.hpp"
#include "cbufs.hpp"

namespace dx = DirectX;

cDbgDrawMgr::cDbgDrawMgr(cGfx& gfx) {
	auto& ss = cShaderStorage::get();
	mpVS = ss.load_VS("simple.vs.cso");
	if (!mpVS) return;
	mpPS = ss.load_PS("simple.ps.cso");
	if (!mpPS) return;

	D3D11_INPUT_ELEMENT_DESC vdsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sSimpleVtx, mPos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(sSimpleVtx, mClr), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	auto pDev = get_gfx().get_dev();
	auto& code = mpVS->get_code();
	HRESULT hr = pDev->CreateInputLayout(vdsc, LENGTHOF_ARRAY(vdsc), code.get_code(), code.get_size(), mpIL.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

	mVtxBuf.init_write_only(pDev, 2048, sizeof(sSimpleVtx));
}

void cDbgDrawMgr::disp() {
	auto& gfx = get_gfx();
	auto pDev = gfx.get_dev();
	auto pCtx = gfx.get_imm_ctx();

	if (!(mpVS && mpPS))
		return;

	gfx.apply_default_rt_vp(pCtx);
	
	nRdrHelpers::upload_cam_cbuf(cRdrContext(pCtx, cConstBufStorage::get_global()), mCamView);

	{
		auto& cbuf = cConstBufStorage::get_global().mMeshCBuf;
		cbuf.mData.wmtx = dx::XMMatrixTranslation(0, 0, 0);
		cbuf.update(pCtx);
		cbuf.set_VS(pCtx);
	}


	cRasterizerStates::get().set_def(pCtx);
	cBlendStates::get().set_imgui(pCtx);
	cDepthStencilStates::get().set_def(pCtx);

	pCtx->IASetInputLayout(mpIL);
	pCtx->VSSetShader(mpVS->asVS(), nullptr, 0);
	pCtx->PSSetShader(mpPS->asPS(), nullptr, 0);

	disp_lines(gfx);
	
	flush();
}

void cDbgDrawMgr::disp_lines(cGfx& gfx) {
	if (mLineVertices.empty()) return;

	auto pDev = gfx.get_dev();
	auto pCtx = gfx.get_imm_ctx();

	if (mVtxBuf.get_vtx_count() < mLineVertices.size()) {
		dbg_msg("cDbgDrawMgr: reallocate vertex buffer, needs %zu, current size %u vetices\n",
			mLineVertices.size(), mVtxBuf.get_vtx_count());
		mVtxBuf = cVertexBuffer();
		mVtxBuf.init_write_only(pDev, (uint32_t)mLineVertices.capacity(), (uint32_t)sizeof(sSimpleVtx));
	}

	{
		auto vm = mVtxBuf.map(pCtx);
		if (!vm.is_mapped())
			return;

		sSimpleVtx* pVtx = reinterpret_cast<sSimpleVtx*>(vm.data());
		memcpy(pVtx, mLineVertices.data(), mLineVertices.size() * sizeof(pVtx[0]));
	}
	
	mVtxBuf.set(pCtx, 0, 0);
	pCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	pCtx->Draw((uint32_t)mLineVertices.size(), 0);
}

void cDbgDrawMgr::flush() {
	mLineVertices.clear();
}

void cDbgDrawMgr::set_camera(const cCamera& cam) {
	mCamView = cam.mView;
}

////

void cDbgDrawMgr::draw_line(const vec3& a, const vec3& b) {
	sSimpleVtx& va = mLineVertices.emplace_back();
	sSimpleVtx& vb = mLineVertices.emplace_back();

	memcpy(va.mPos, &a, sizeof(vec3));
	memcpy(vb.mPos, &b, sizeof(vec3));

	float white[4] = { 1, 1, 1, 1 };

	memcpy(va.mClr, white, sizeof(white));
	memcpy(vb.mClr, white, sizeof(white));
}
