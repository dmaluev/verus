// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

struct DS_FSO
{
	float4 target0 : SV_Target0; // {albedo, specMask}
	float4 target1 : SV_Target1; // {normal, emission|skinMask, motionBlurMask}
	float4 target2 : SV_Target2; // {lamScaleBias, metalMask|hairMask, gloss}
};

struct DS_ACC_FSO
{
	float4 target0 : SV_Target0;
	float4 target1 : SV_Target1;
};

// See: http://aras-p.info/texts/CompactNormalStorage.html#method04spheremap
float2 EncodeNormal(float3 n)
{
	const float rf = rsqrt(abs(8.f * n.z + 8.f));
	return n.xy * rf + 0.5f;
}

float3 DecodeNormal(float2 enc)
{
	const float2 fenc = enc * 4.f - 2.f;
	const float f = dot(fenc, fenc);
	const float2 f2 = 1.f - f * float2(0.25f, 0.5f);
	const float g = sqrt(abs(f2.x));
	return float3(fenc * g, f2.y);
}

void DS_Reset(out DS_FSO so)
{
	so.target0 = 0.f;
	so.target1 = 0.f;
	so.target2 = 0.f;
}

void DS_SolidColor(out DS_FSO so, float3 color)
{
	so.target0 = float4(color, 0.f);
	so.target1 = float4(EncodeNormal(float3(0, 0, 1)), 0.25f, 0);
	so.target2 = float4(1.f / 8.f, 0.5f, 0.5f, 0);
}

void DS_Test(out DS_FSO so, float3 normal, float specMask, float gloss)
{
	so.target0 = float4(0.5f, 0.5f, 0.5f, specMask);
	so.target1 = float4(EncodeNormal(normal), 0.25f, 0.5f);
	so.target2 = float4(1.f / 8.f, 0.5f, 0.5f, gloss * (1.f / 64.f));
}

void DS_SetAlbedo(inout DS_FSO so, float3 albedo)
{
	so.target0.rgb = albedo;
}

void DS_SetSpecMask(inout DS_FSO so, float specMask)
{
	so.target0.a = specMask;
}

float3 DS_GetNormal(float4 gbuffer)
{
	return DecodeNormal(gbuffer.rg);
}

void DS_SetNormal(inout DS_FSO so, float3 normal)
{
	so.target1.rg = EncodeNormal(normal);
}

float2 DS_GetEmission(float4 gbuffer)
{
	const float2 em_skin = saturate((gbuffer.b - 0.25f) * float2(1.f / 0.75f, -1.f / 0.25f));
	const float em = exp2(em_skin.x * 15.f) - 1.f;
	return float2(em, em_skin.y);
}

void DS_SetEmission(inout DS_FSO so, float emission, float skinMask)
{
	const float em = saturate(log2(1.f + emission) * (1.f / 15.f));
	so.target1.b = (3.f * em - skinMask) * 0.25f + 0.25f;
}

void DS_SetMotionBlurMask(inout DS_FSO so, float motionBlurMask)
{
	so.target1.a = motionBlurMask;
}

float2 DS_GetLamScaleBias(float4 gbuffer)
{
	return gbuffer.rg * float2(8, 4) - float2(0, 2); // {0 to 8, -2 to 2}.
}

void DS_SetLamScaleBias(inout DS_FSO so, float2 lamScaleBias, float4 aniso)
{
	so.target2.rg = lerp(lamScaleBias * float2(1.f / 8.f, 0.25f) + float2(0, 0.5f), EncodeNormal(aniso.xyz), aniso.w);
}

float2 DS_GetMetallicity(float4 gbuffer)
{
	const float x2 = gbuffer.b * 2.02f;
	return float2(
		saturate(x2 - 1.02f),
		saturate(1.f - x2));
}

void DS_SetMetallicity(inout DS_FSO so, float metalMask, float hairMask)
{
	so.target2.b = (metalMask - hairMask) * 0.5f + 0.5f;
}

void DS_SetGloss(inout DS_FSO so, float gloss)
{
	so.target2.a = gloss * (1.f / 64.f);
}

float3 DS_GetPosition(float4 gbuffer, matrix matInvProj, float2 ndcPos)
{
	float4 pos = mul(float4(ndcPos, gbuffer.r, 1), matInvProj);
	pos /= pos.w;
	return pos.xyz;
}

float3 DS_GetAnisoSpec(float4 gbuffer)
{
	return DecodeNormal(gbuffer.rg);
}
