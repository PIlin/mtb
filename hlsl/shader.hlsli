#pragma pack_matrix(row_major)

struct sVSSimple {
	float3 pos : POSITION;
	float4 clr : COLOR;
};

struct sPSSimple{
	float4 cpos : SV_POSITION;
	float4 clr : COLOR0;
};

struct sVSModel {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD;
	float4 tgt : TANGENT;
	float3 bitgt : BITANGENT;
	float2 uv1 : TEXCOORD1;
	float3 clr : COLOR;
	int4   jidx : BLENDINDEX;
	float4 jwgt : BLENDWEIGHT;
};

struct sPSModel {
	float4 cpos : SV_POSITION;
	float4 wpos : POSITION;
	float3 wnrm : NORMAL;
	float2 uv : TEXCOORD;
	float4 wtgt : TANGENT;
	float3 wbitgt : BITANGENT;
	float2 uv1 : TEXCOORD1;
	float4 clr : COLOR;
};

struct sPSPredefined {
	bool face : SV_IsFrontFace;
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

cbuffer TestMtl : register(b2) {
	float3 g_fresnel;
	float g_shin;
	float g_nmap0Power;
	float g_nmap1Power;

	float2 _pad;
};

#define MAX_LIGHTS 8
cbuffer Light : register(b3) {
	float3 g_lightPos[MAX_LIGHTS];
	float3 g_lightClr[MAX_LIGHTS];
	float3 g_lightDir[MAX_LIGHTS];
	bool   g_lightIsEnabled[MAX_LIGHTS];
	//float  attn[]

	float4 g_lightSH[7];
};

#define MAX_SKIN_MTX 64
cbuffer Skin : register(b4) {
	float4x4 g_skin[MAX_SKIN_MTX];
}

Texture2D    g_meshDiffTex : register(t0);
SamplerState g_meshDiffSmp : register(s0);
Texture2D    g_meshNmap0Tex : register(t1);
SamplerState g_meshNmap0Smp : register(s1);
Texture2D    g_meshNmap1Tex : register(t2);
SamplerState g_meshNmap1Smp : register(s2);
Texture2D    g_meshMaskTex : register(t3);
SamplerState g_meshMaskSmp : register(s3);

static const float g_gamma = 2.2;

#define PI 3.14159265
