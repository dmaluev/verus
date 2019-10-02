struct DS_FSO
{
	float4 target0 : SV_Target0; // {color, spec}
	float4 target1 : SV_Target1; // {depth}
	float4 target2 : SV_Target2; // {normal, emission, motion}
	float4 target3 : SV_Target3; // {lam, metal, gloss}
};
struct DS_FSO_DEPTH
{
	float4 color : SV_Target0;
};
#ifdef DEF_DEPTH
#	define _DS_FSO DS_FSO_DEPTH
#else
#	define _DS_FSO DS_FSO
#endif

struct DS_ACC_FSO
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
};

// See: http://aras-p.info/texts/CompactNormalStorage.html#method04spheremap
float2 EncodeNormal(float3 n)
{
	const float f = sqrt(abs(8.0*n.z + 8.0));
	return n.xy / f + 0.5;
}

float3 DecodeNormal(float2 enc)
{
	const float2 fenc = enc * 4.0 - 2.0;
	const float f = dot(fenc, fenc);
	const float2 f2 = 1.0 - f * float2(0.25, 0.5);
	const float g = sqrt(abs(f2.x));
	return float3(fenc * g, f2.y);
}

float GetEmissionScale()
{
	return 4.0;
}

float3 ApplyEmission(float3 color, float emission)
{
	return color * (1.0 + emission * GetEmissionScale());
}

void DS_Reset(out DS_FSO so)
{
	so.target0 = 0.0;
	so.target1 = 0.0;
	so.target2 = 0.0;
	so.target3 = 0.0;
}

void DS_Test(out DS_FSO so, float3 normal, float spec, float gloss)
{
	so.target0 = float4(0.5, 0.5, 0.5, spec);
	so.target1 = 0.0;
	so.target2 = float4(EncodeNormal(normal), 0.5, 0.5);
	so.target3 = float4(1.0 / 8.0, 0.5, 0.5, gloss*(1.0 / 64.0));
}

void DS_SetAlbedo(inout DS_FSO so, float3 albedo)
{
	so.target0.rgb = albedo;
}

void DS_SetSpec(inout DS_FSO so, float spec)
{
	so.target0.a = spec;
}

void DS_SetDepth(inout DS_FSO so, float depth)
{
	so.target1 = depth;
}

float3 DS_GetNormal(float4 gbuffer)
{
	return DecodeNormal(gbuffer.rg);
}

void DS_SetNormal(inout DS_FSO so, float3 normal)
{
	so.target2.rg = EncodeNormal(normal);
}

float2 DS_GetEmission(float4 gbuffer)
{
	const float x2 = gbuffer.b*2.0;
	return float2(
		saturate(x2 - 1.0),
		saturate(1.0 - x2));
}

void DS_SetEmission(inout DS_FSO so, float emission, float skin)
{
	so.target2.b = (emission - skin)*0.5 + 0.5;
}

float2 DS_GetLamScaleBias(float4 gbuffer)
{
	return gbuffer.rg*float2(8, 4) - float2(0, 2); // {0 to 8, -2 to 2}.
}

void DS_SetLamScaleBias(inout DS_FSO so, float2 lamScaleBias, float4 aniso)
{
	so.target3.rg = lerp(lamScaleBias*float2(1.0 / 8.0, 0.25) + float2(0, 0.5), EncodeNormal(aniso.xyz), aniso.w);
}

float2 DS_GetMetallicity(float4 gbuffer)
{
	const float x2 = gbuffer.b*2.0;
	return float2(
		saturate(x2 - 1.0),
		saturate(1.0 - x2));
}

void DS_SetMetallicity(inout DS_FSO so, float metal, float hair)
{
	so.target3.b = (metal - hair)*0.5 + 0.5;
}

void DS_SetGloss(inout DS_FSO so, float gloss)
{
	so.target3.a = gloss * (1.0 / 64.0);
}

float3 DS_GetPosition(float4 gbuffer, matrix matInvProj, float2 posCS)
{
	float4 pos = mul(float4(posCS, gbuffer.r, 1), matInvProj);
	pos /= pos.w;
	return pos.xyz;
}

float3 DS_GetAnisoSpec(float4 gbuffer)
{
	return DecodeNormal(gbuffer.rg);
}
