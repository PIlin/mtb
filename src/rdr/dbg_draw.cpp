#include "common.hpp"
#include "dbg_draw.hpp"
#include "gfx.hpp"
#include "cbufs.hpp"
#include "color.hpp"

namespace dx = DirectX;

cDbgDrawMgr::cDbgDrawMgr(cGfx& gfx) {
	auto& ss = cShaderStorage::get();
	mpVS = ss.load_VS("simple.vs.cso");
	if (!mpVS) return;
	mpPS = ss.load_PS("simple.ps.cso");
	if (!mpPS) return;

	D3D11_INPUT_ELEMENT_DESC vdsc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(sSimpleVtx, pos), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(sSimpleVtx, clr), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	auto pDev = get_gfx().get_dev();
	auto& code = mpVS->get_code();
	HRESULT hr = pDev->CreateInputLayout(vdsc, LENGTHOF_ARRAY(vdsc), code.get_code(), code.get_size(), mpIL.pp());
	if (!SUCCEEDED(hr)) throw sD3DException(hr, "CreateInputLayout failed");

	mVtxBuf.init_write_only(pDev, 2048, sizeof(sSimpleVtx));
}

void cDbgDrawMgr::disp() {
	disp_camera_grid();

	auto& gfx = get_gfx();
	auto pDev = gfx.get_dev();
	auto pCtx = gfx.get_imm_ctx();

	if (!(mpVS && mpPS))
		return;

	gfx.apply_default_rt_vp(pCtx);
	
	nRdrHelpers::upload_cam_cbuf(cRdrContext(pCtx, cConstBufStorage::get_global()), mCam.mView);

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
	mCam = cam;
}

////

void cDbgDrawMgr::draw_line(const vec3& a, const vec3& b) {
	draw_line(a, b, nColor::white);
}

void cDbgDrawMgr::draw_line(const vec3& a, const vec3& b, const vec4& clr) {
	draw_line(a, b, clr, clr);
}

void cDbgDrawMgr::draw_line(const vec3& a, const vec3& b, const vec4& clrA, const vec4& clrB) {
	sSimpleVtx& va = mLineVertices.emplace_back();
	sSimpleVtx& vb = mLineVertices.emplace_back();

	va.pos = a;
	vb.pos = b;
	va.clr = clrA;
	vb.clr = clrB;
}

void cDbgDrawMgr::draw_gnomon(const sXform& xform, float len) {
	draw_gnomon(xform.build_mtx(), len);
}

void XM_CALLCONV cDbgDrawMgr::draw_gnomon(DirectX::FXMMATRIX mtx, float len) {
	vec3 o = vec3::from_vector(dx::XMVector4Transform(dx::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), mtx));
	vec3 x = vec3::from_vector(dx::XMVector4Transform(dx::XMVectorSet(len, 0.0f, 0.0f, 1.0f), mtx));
	vec3 y = vec3::from_vector(dx::XMVector4Transform(dx::XMVectorSet(0.0f, len, 0.0f, 1.0f), mtx));
	vec3 z = vec3::from_vector(dx::XMVector4Transform(dx::XMVectorSet(0.0f, 0.0f, len, 1.0f), mtx));

	draw_line(o, x, nColor::red);
	draw_line(o, y, nColor::green);
	draw_line(o, z, nColor::blue);
}

void cDbgDrawMgr::disp_camera_grid() {
	dx::XMVECTOR xz = dx::XMPlaneFromPointNormal(dx::XMVectorZero(), dx::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
	dx::XMVECTOR root = dx::XMPlaneIntersectLine(xz, mCam.mPos, mCam.mTgt);
	if (dx::XMVector4IsNaN(root))
		return;

	//draw_gnomon(dx::XMMatrixTranslationFromVector(root), 0.3f);
	int count = 60;
	float s = 0.5f;

	float sMajor = s * 2;
	dx::XMVECTOR svec = dx::XMVectorSet(sMajor, sMajor, sMajor, 1.0f);
	root = dx::XMVectorDivide(root, svec);
	root = dx::XMVectorFloor(root);
	root = dx::XMVectorMultiply(root, svec);

	vec4 r;
	dx::XMStoreFloat4(&r.mVal, root);

	//draw_gnomon(dx::XMMatrixTranslationFromVector(root), 0.3f);

	
	vec2f o = { r.mVal.x, r.mVal.z };
	const vec2f step = { s, s };

	o = o - step * float(count / 2);

	const float y = 0.0f;
	const float cMajor = 0.75f;
	const vec4 clrMajor = { { cMajor, cMajor, cMajor, 0.8f } };
	const float cMinor = 0.75f;
	const vec4 clrMinor = { { cMinor, cMinor, cMinor, 0.3f } };
	auto v3 = [](const vec2f& v2, float y) { return vec3{ v2.x, y, v2.y }; };
	
	for (int i = 0; i <= count; ++i) {
		const vec4& clr = (i % 2) ? clrMinor : clrMajor;
		{
			vec2f oxz = vec2f(float(i), 0.0f) * step + o;
			vec2f exz = vec2f(float(i), float(count)) * step + o;
			draw_line(v3(oxz, y), v3(exz, y), clr);
		}
		{
			vec2f oxz = vec2f(0.0f, float(i)) * step + o;
			vec2f exz = vec2f(float(count), float(i)) * step + o;
			draw_line(v3(oxz, y), v3(exz, y), clr);
		}
	}
}


