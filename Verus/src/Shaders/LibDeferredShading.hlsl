struct DS_FSO
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
	float4 target2 : SV_Target2;
	float4 target3 : SV_Target3;
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
	const float f = sqrt(8.0*n.z + 8.0);
	const float2 disk = n.xy / f * 2.0;
	return sqrt(abs(disk))*sign(disk)*0.5 + 0.5; // Warp to give center more details.
}

float3 DecodeNormal(float2 enc)
{
	const float2 warp = enc * 2.0 - 1.0;
	const float2 disk = warp * warp*sign(warp);
	const float2 fenc = disk * 2.0;
	const float f = dot(fenc, fenc);
	const float g = sqrt(1.0 - f * 0.25);
	return float3(fenc*g, 1.0 - f * 0.5);
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
	so.target0 = float4(0, 0, 0, 1);
	so.target1 = 0.0;
	so.target2 = 0.0;
	so.target3 = 0.0;
}

void DS_Test(inout DS_FSO so, float3 normal, float spec, float gloss)
{
	so.target0 = 0.5;
	so.target2 = float4(EncodeNormal(normal), 0.5, 0.5);
	so.target3 = float4(1.0 / 8.0, 0.5, spec, gloss / 64.0);
}

void DS_SetAlbedo(inout DS_FSO so, float3 albedo, float motionBlur = 1.0)
{
	so.target0 = float4(albedo, motionBlur);
}

void DS_SetDepth(inout DS_FSO so, float depth)
{
	so.target1 = depth;
}

float3 DS_GetNormal(float4 gbuffer)
{
	return DecodeNormal(gbuffer.xy);
}

void DS_SetNormal(inout DS_FSO so, float3 normal)
{
	so.target2.xy = EncodeNormal(normal);
}

float2 DS_GetEmission(float4 gbuffer)
{
	return float2(
		saturate(gbuffer.z*2.0 - 1.0),
		saturate(1.0 - gbuffer.z*2.0));
}

void DS_SetEmission(inout DS_FSO so, float emission, float skin)
{
	so.target2.z = (emission - skin)*0.5 + 0.5;
}

float2 DS_GetMetallicity(float4 gbuffer)
{
	return float2(
		saturate(gbuffer.w*2.0 - 1.0),
		saturate(1.0 - gbuffer.w*2.0));
}

void DS_SetMetallicity(inout DS_FSO so, float metal, float hair)
{
	so.target2.w = (metal - hair)*0.5 + 0.5;
}

float2 DS_GetLamScaleBias(float4 gbuffer)
{
	return gbuffer.xy*float2(8, 4) - float2(0, 2); // {0 to 8, -2 to 2}.
}

void DS_SetLamScaleBias(inout DS_FSO so, float2 lamScaleBias, float4 aniso)
{
	so.target3.xy = lerp(lamScaleBias*float2(1.0 / 8.0, 0.25) + float2(0, 0.5), EncodeNormal(aniso.xyz), aniso.w);
}

void DS_SetSpec(inout DS_FSO so, float spec)
{
	so.target3.z = spec;
}

void DS_SetGloss(inout DS_FSO so, float gloss)
{
	so.target3.w = gloss / 64.0;
}

float3 DS_GetPosition(float4 gbuffer, matrix matInvProj, float2 posCS)
{
	float4 pos = mul(float4(posCS, gbuffer.x, 1), matInvProj);
	pos /= pos.w;
	return pos.xyz;
}

float3 DS_GetAnisoSpec(float4 gbuffer)
{
	return DecodeNormal(gbuffer.xy);
}
