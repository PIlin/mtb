#pragma pack_matrix(row_major)

struct sVSSimple {
	float4 pos : POSITION;
	float4 clr : COLOR;
};

struct sPSSimple{
	float4 cpos : SV_POSITION;
	float4 clr : COLOR0;
};


struct sVSModelSolid {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD;
};

struct sPSModel {
	float4 cpos : SV_POSITION;
	float4 wpos : POSITION;
	float3 wnrm : NORMAL;
	float2 uv : TEXCOORD;
};


cbuffer Camera : register(b0) {
	float4x4 g_viewProj;
	float4x4 g_view;
	float4x4 g_proj;
	float4   g_camPos;
};

cbuffer Mesh : register(b1) {
	float4x4 g_world;
};

Texture2D g_meshDiffTex : register(t0);
SamplerState g_meshDiffSmp : register(s0);