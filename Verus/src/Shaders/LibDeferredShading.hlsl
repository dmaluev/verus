// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

struct DS_FSO
{
	float4 target0 : SV_Target0; // {Albedo.rgb, SSSHue}.
	float4 target1 : SV_Target1; // {Normal.xy, Emission, MotionBlur}.
	float4 target2 : SV_Target2; // {Occlusion, Roughness, Metallic, WrapDiffuse}.
	float4 target3 : SV_Target3; // {Tangent.xy, AnisoSpec, RoughDiffuse}.
};

struct DS_ACC_FSO
{
	float4 target0 : SV_Target0; // Ambient.
	float4 target1 : SV_Target1; // Diffuse.
	float4 target2 : SV_Target2; // Specular.
};

// See: http://aras-p.info/texts/CompactNormalStorage.html#method04spheremap
float2 EncodeNormal(float3 n)
{
	const float rf = rsqrt(abs(8.0 * n.z + 8.0));
	return n.xy * rf + 0.5;
}

float3 DecodeNormal(float2 enc)
{
	const float2 fenc = enc * 4.0 - 2.0;
	const float f = dot(fenc, fenc);
	const float2 f2 = 1.0 - f * float2(0.25, 0.5);
	const float g = sqrt(abs(f2.x));
	return float3(fenc * g, f2.y);
}

void DS_Reset(out DS_FSO so)
{
	so.target0 = 0.0;
	so.target1 = 0.0;
	so.target2 = 0.0;
	so.target3 = 0.0;
}

void DS_Reset(out DS_ACC_FSO so)
{
	so.target0 = 0.0;
	so.target1 = 0.0;
	so.target2 = 0.0;
}

// <GBuffer0>
void DS_SetAlbedo(inout DS_FSO so, float3 albedo)
{
	so.target0.rgb = albedo;
}

void DS_SetSSSHue(inout DS_FSO so, float sssHue)
{
	so.target0.a = sssHue;
}
// </GBuffer0>

// <GBuffer1>
float3 DS_GetNormal(float4 gBuffer)
{
	return DecodeNormal(gBuffer.rg);
}
void DS_SetNormal(inout DS_FSO so, float3 normal)
{
	so.target1.rg = EncodeNormal(normal);
}

float DS_GetEmission(float4 gBuffer)
{
	return exp2(gBuffer.b * 15.0) - 1.0;
}
void DS_SetEmission(inout DS_FSO so, float emission)
{
	so.target1.b = saturate(log2(1.0 + emission) * (1.0 / 15.0));
}

void DS_SetMotionBlur(inout DS_FSO so, float motionBlur)
{
	so.target1.a = motionBlur;
}
// </GBuffer1>

// <GBuffer2>
void DS_SetOcclusion(inout DS_FSO so, float occlusion)
{
	so.target2.r = occlusion;
}

void DS_SetRoughness(inout DS_FSO so, float roughness)
{
	so.target2.g = roughness;
}

void DS_SetMetallic(inout DS_FSO so, float metallic)
{
	so.target2.b = metallic;
}

void DS_SetWrapDiffuse(inout DS_FSO so, float wrapDiffuse)
{
	so.target2.a = wrapDiffuse;
}
// </GBuffer2>

// <GBuffer3>
float3 DS_GetTangent(float4 gBuffer)
{
	return DecodeNormal(gBuffer.rg);
}
void DS_SetTangent(inout DS_FSO so, float3 tangent)
{
	so.target3.rg = EncodeNormal(tangent);
}

void DS_SetAnisoSpec(inout DS_FSO so, float anisoSpec)
{
	so.target3.b = anisoSpec;
}

void DS_SetRoughDiffuse(inout DS_FSO so, float roughDiffuse)
{
	so.target3.a = roughDiffuse;
}
// </GBuffer3>

// <Depth>
float3 DS_GetPosition(float4 gBuffer, matrix matInvProj, float2 ndcPos)
{
	float4 pos = mul(float4(ndcPos, gBuffer.r, 1), matInvProj);
	pos /= pos.w;
	return pos.xyz;
}
// </Depth>
