#include "shader.hlsli"

float4 main(sPSSimple pin) : SV_TARGET
{

	float4 clr = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float3 rgb = pin.clr.rgb;
	//rgb = rgb * 0.55 + 0.225;
	clr.rgb = rgb;
	clr.a = pin.clr.a;
	return clr;
}
