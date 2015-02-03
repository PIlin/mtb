#include "shader.hlsli"

cbuffer Mesh : register(b0) {
	float4x4 g_world;
};

cbuffer Camera : register(b1) {
	float4x4 g_viewProj;
	float4x4 g_view;
	float4x4 g_proj;
};

void main(sVSSimple vin, out sPSSimple vout) 
{
	float4 pos = float4(vin.pos.xyz, 1);
	//float3 wpos = pos.xyz;
	float4 wpos = mul(pos, g_world);
	
	//float4 cpos = float4(wpos, 1.0);

	//float4 cpos = mul(float4(wpos, 1.0), g_viewProj);
	float4 cpos = mul(wpos, g_viewProj);

	vout.cpos = cpos;
	vout.clr = vin.clr;
}
