#pragma once

#include "camera.hpp"
#include "math.hpp"
#include "rdr.hpp"
#include <vector>

class cGfx;
class cShader;

class cDbgDrawMgr {
	sCameraView mCamView;
	std::vector<sSimpleVtx> mLineVertices;

	cVertexBuffer mVtxBuf;

	cShader* mpVS = nullptr;
	cShader* mpPS = nullptr;
	com_ptr<ID3D11InputLayout> mpIL;

public:
	static cDbgDrawMgr& get();

	cDbgDrawMgr(cGfx& gfx);

	void disp();

	void set_camera(const cCamera& cam);
	void draw_line(const vec3& a, const vec3& b);
	void draw_line(const vec3& a, const vec3& b, const vec4& clr);
	void draw_line(const vec3& a, const vec3& b, const vec4& clrA, const vec4& clrB);

private:
	void disp_lines(cGfx& gfx);
	void flush();
};

