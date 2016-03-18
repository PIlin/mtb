struct CTX {
	float4 cpos;
	float4 wpos;
	float3 wnrm;
	float3 wtgt;
	float3 wbitgt;
	float bitgtFlip;
	float2 uv;
	float2 uv1;
	float4 vclr;
	float3 base;
	float3 diff;
	float3 spec;
	float  alpha;
	float3 clr;
	float3 mask;
	float  face; // 1 = front, -1 = back
	float3 viewDir;
};

#include "shader.hlsli"
#include "light.hlsli"

float3 pack_nrm(float3 nrm) {
	return nrm * 0.5f + 0.5f;
}


CTX set_ctx(sPSModel pin) {
	CTX ctx;
	ctx.cpos = pin.cpos;
	ctx.wpos = pin.wpos;
	ctx.wnrm = normalize(pin.wnrm);
	ctx.bitgtFlip = pin.wtgt.w;
	ctx.wtgt = normalize(pin.wtgt.xyz);
	ctx.wbitgt = normalize(pin.wbitgt);
	ctx.uv   = pin.uv;
	ctx.uv1  = pin.uv1;
	ctx.vclr = pin.clr;

	ctx.base = float3(1, 1, 1);
	ctx.diff = float3(0, 0, 0);
	ctx.spec = float3(0, 0, 0);
	ctx.alpha = 1.0;
	ctx.clr = float3(0, 0, 0);
	ctx.mask = float3(1, 1, 1);

	ctx.viewDir = normalize(g_camPos - ctx.wpos).xyz;
	return ctx;
}

CTX set_ctx_predefined(CTX ctx, sPSPredefined predef) {
	ctx.face = predef.face ? 1.0 : -1.0;
	return ctx;
}


float3 fresnel(float3 f0, float HL) {
	return f0 + (1 - f0) * pow(1 - HL, 5);
}

float3 lambert_brdf() {
	//return float3(1, 1, 1) / PI;
	return float3(1, 1, 1);
}

float3 phong_brdf(float RV, float shin) {
	return pow(RV, shin);
}

float3 blinn_phong_brdf(float HN, float shin) {
	return pow(HN, shin);
}

float3 blinn_phong_nrm_brdf(float HN, float shin) {
	return ((shin + 2) / 8)* pow(HN, shin);
}

CTX shade(CTX ctx) {
	//LightParam lp;
	//lp.pos = float3(1, 0, 1);
	//lp.clr = float3(1, 1, 1) * 2;

	float shin = g_shin;
	float3 f0 = g_fresnel.xyz;

	for (int i = 0; i < 4; ++i) {
		if (!g_lightIsEnabled[i]) break;
		LightParam lp = get_light_param(i);
		LightInfo info = get_light_info_omni(ctx, lp);

		ctx.diff += lambert_brdf() * info.clrDiff * info.LN;
		//ctx.spec += blinn_phong_brdf(info.HN, shin) * fresnel(f0, info.HL) * info.clrSpec * info.LN;
		ctx.spec += blinn_phong_nrm_brdf(info.HN, shin) * fresnel(f0, info.HL) * info.clrSpec * info.LN;
	}

	ctx.diff += get_light_sh(ctx.wnrm);

	return ctx;
}


CTX noshade(CTX ctx) {
	ctx.diff = float3(1, 1, 1);
	return ctx;
}

float4 combine(CTX ctx) {
	ctx.clr = ctx.base * ctx.diff + ctx.spec;

	ctx.clr = ctx.clr * ctx.vclr.rgb;
	ctx.alpha *= ctx.vclr.a;
	
	ctx.clr = pow(abs(ctx.clr), 1.0 / g_gamma);

	return float4(ctx.clr, ctx.alpha);
}

CTX sample_base(CTX ctx) {
	ctx.base = g_meshDiffTex.Sample(g_meshDiffSmp, ctx.uv).rgb;
	ctx.base = pow(abs(ctx.base), g_gamma);
	//ctx.base = float3(1.0, 1.0, 1.0);
	return ctx;
}

CTX sample_mask_alphatest(CTX ctx) {
	ctx.mask = g_meshMaskTex.Sample(g_meshMaskSmp, ctx.uv).rgb;
	ctx.alpha = ctx.mask.g;
	return ctx;
}

CTX do_alphatest(CTX ctx, float tres) {
	if (ctx.alpha <= tres)
		discard;
	return ctx;
}

float3 unpack_nmap_xy(float2 xy, float pwr) {
	xy = xy * 2.0 - 1.0;
	xy *= pwr;
	float z = sqrt(1.0 - dot(xy, xy));
	return normalize(float3(xy, z));
}

// See: http://blog.selfshadow.com/publications/blending-in-detail/
float3 blend_nmap_whiteout(float3 n0, float3 n1) {
	return normalize(float3(n0.xy + n1.xy, n0.z*n1.z));
}

float3 blend_nmap_ff13(float3 n0, float3 n1) {
	return normalize(n0 * n1.z + float3(n1.xy, 0));
}

CTX apply_nmap(CTX ctx) 
{
	float3 nmap0 = g_meshNmap0Tex.Sample(g_meshNmap0Smp, ctx.uv).rgb;
	float3 nmap1 = g_meshNmap1Tex.Sample(g_meshNmap1Smp, ctx.uv1).rgb;

	float3 n0 = unpack_nmap_xy(nmap0.rg, g_nmap0Power);
	float3 n1 = unpack_nmap_xy(nmap1.rg, g_nmap1Power);
	//float3 n1 = float3(0, 0, 1);

	float3 tnrm = blend_nmap_whiteout(n0, n1);
	//float3 tnrm = blend_nmap_ff13(n0, n1);
	
	float3 wtgt = ctx.wtgt;
	float3 wnrm = ctx.wnrm;

#if 0 
	float3 wbitgt = normalize(cross(wnrm, wtgt));
	wbitgt *= ctx.bitgtFlip;
#else
	float3 wbitgt = ctx.wbitgt;
#endif
	
	float3x3 wtbn =  float3x3(wtgt, wbitgt, wnrm);
	wnrm = mul(tnrm, wtbn);
	
	ctx.wnrm = wnrm;

	return ctx;
}
