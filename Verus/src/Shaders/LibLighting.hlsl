// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

float3 SSSHueToColor(float hue)
{
	const float3 red = float3(1, 0.006, 0.056); // Color 4 blood.
	const float3 green = float3(0.008, 1, 0.001); // Color for plants.
	const float x2 = hue * 2.0;
	const float2 alpha = saturate(float2(x2 - 1.0, 1.0 - x2));
	return lerp(lerp(1.0, green, alpha.x), red, alpha.y);
}

float3 ComputeF0(float3 albedo, float metallic)
{
	return lerp(0.04, albedo, metallic);
}

float3 ComputeRoughnessDependentFresnel(float3 f0, float roughness)
{
	return max(1.0 - roughness, f0) - f0;
}

float RoughnessToMip(float roughness, float maxMipLevel)
{
	// Linearize the exponential function of mips.
	const float firstMipShare = 1.0 / (exp2(maxMipLevel));
	return 1.0 - firstMipShare * exp2((1.0 - roughness) * maxMipLevel);
}

float ComputeCavity(float3 albedo, float roughness)
{
	// With great roughness comes great responsibility:
	return saturate(8.0 * max(dot(albedo, 1.0 / 3.0), 1.0 - roughness));
}

// Aka D:
float MicrofacetDistribution(float alphaRoughnessSq, float nDosH)
{
	const float f = (nDosH * nDosH) * (alphaRoughnessSq - 1.0) + 1.0;
	return alphaRoughnessSq / (f * f);
}

// Aka V:
float MaskingShadowingFunction(float alphaRoughnessSq, float nDosL, float nDosV)
{
	const float2 nDosL_nDosV = float2(nDosL, nDosV);
	const float2 nDosLSq_nDosVSq = nDosL_nDosV * nDosL_nDosV;
	const float2 ggxl_ggxv = nDosL_nDosV.yx * sqrt(alphaRoughnessSq + (1.0 - alphaRoughnessSq) * nDosLSq_nDosVSq);
	const float ggx = ggxl_ggxv.x + ggxl_ggxv.y;
	if (ggx > 0.0)
		return 0.5 / ggx;
	return 0.0;
}

void VerusLit(float3 normal, float3 dirToLight, float3 dirToEye, float3 tangent,
	float3 albedo, float3 sssColor,
	float roughness, float metallic, float roughDiffuse, float wrapDiffuse, float anisoSpec,
	out float3 punctualDiff, out float3 punctualSpec)
{
	const float alphaRoughness = roughness * roughness;
	const float alphaRoughnessSq = alphaRoughness * alphaRoughness;

	const float3 hv = normalize(dirToLight + dirToEye);
	const float nDotL = dot(normal, dirToLight);
	const float nDosL = saturate(nDotL);
	const float nWrapL = saturate((nDotL + wrapDiffuse) / (1.0 + wrapDiffuse));
	const float nDosV = saturate(dot(normal, dirToEye));
	const float nDosH = saturate(dot(normal, hv));
	const float vDosH = saturate(dot(dirToEye, hv));
	const float aDotH = SinAcos(dot(tangent, hv));
	// Poor man's Oren–Nayar model:
	const float roughFactor = roughness * roughDiffuse;
	const float2 pow5_nRoughL = pow(float2(1.0 - vDosH, nWrapL), float2(5.0, saturate((1.0 + _SINGULARITY_FIX) - roughFactor)));
	const float nRoughL = pow5_nRoughL.y * (1.0 - 0.5 * roughFactor);
	// Subsurface scattering:
	const float3 sss = lerp(1.0, sssColor, saturate(-nDotL + 0.25));
	// Anisotropic specular highlights:
	const float ash = 1.0 - anisoSpec * 0.44; // Drop to sqrt(1/pi).
	// Fake cavity map from albedo:
	const float cavity = ComputeCavity(albedo, roughness);

	const float3 f0 = ComputeF0(albedo, metallic);
	const float3 f1 = ComputeRoughnessDependentFresnel(f0, alphaRoughness); // alphaRoughness looks better.
	const float3 fresnel = FresnelSchlick(f0, f1, pow5_nRoughL.x);

	punctualDiff = nRoughL * (1.0 - fresnel) * (1.0 - metallic) * sss;
	punctualSpec = nDosL * fresnel * ash * cavity *
		MicrofacetDistribution(alphaRoughnessSq, lerp(nDosH, aDotH, anisoSpec)) *
		MaskingShadowingFunction(alphaRoughnessSq, nDosL, nDosV);
}

void VerusSimpleLit(float3 normal, float3 dirToLight, float3 dirToEye,
	float3 albedo,
	float roughness, float metallic,
	out float3 punctualDiff, out float3 punctualSpec)
{
	const float oneMinusRoughness = 1.0 - roughness;
	roughness = 1.0 - oneMinusRoughness * oneMinusRoughness;

	const float alphaRoughness = roughness * roughness;
	const float alphaRoughnessSq = alphaRoughness * alphaRoughness;

	const float3 hv = normalize(dirToLight + dirToEye);
	const float nDosL = saturate(dot(normal, dirToLight));
	const float nDosH = saturate(dot(normal, hv));
	const float vDosH = saturate(dot(dirToEye, hv));

	const float3 f0 = ComputeF0(albedo, metallic);
	const float3 f1 = ComputeRoughnessDependentFresnel(f0, roughness);
	const float3 fresnel = FresnelSchlick(f0, f1, pow(1.0 - vDosH, 5.0));

	punctualDiff = nDosL * (1.0 - fresnel) * (1.0 - metallic);
	punctualSpec = nDosL * fresnel * MicrofacetDistribution(alphaRoughnessSq, nDosH);
}

void VerusWaterLit(float3 normal, float3 dirToLight, float3 dirToEye,
	float3 albedo,
	float roughness, float wrapDiffuse,
	out float3 punctualDiff, out float3 punctualSpec, out float3 fresnel)
{
	const float alphaRoughness = roughness * roughness;
	const float alphaRoughnessSq = alphaRoughness * alphaRoughness;

	const float3 hv = normalize(dirToLight + dirToEye);
	const float nDotL = dot(normal, dirToLight);
	const float nDosL = saturate(nDotL);
	const float nWrapL = saturate((nDotL + wrapDiffuse) / (1.0 + wrapDiffuse));
	const float nDosV = saturate(dot(normal, dirToEye));
	const float nDosH = saturate(dot(normal, hv));

	const float3 f0 = 0.03;
	const float3 f1 = ComputeRoughnessDependentFresnel(f0, roughness);
	fresnel = FresnelSchlick(f0, f1, pow(1.0 - nDosV, 5.0));

	punctualDiff = nWrapL;
	punctualSpec = nDosL * MicrofacetDistribution(alphaRoughnessSq, nDosH);
}

float3 VerusIBL(float3 normal, float3 dirToEye,
	float3 albedo, float3 image,
	float roughness, float metallic,
	float2 scaleBias = float2(1, 0))
{
	const float nDosV = saturate(dot(normal, dirToEye));

	const float3 f0 = ComputeF0(albedo, metallic);
	const float3 f1 = ComputeRoughnessDependentFresnel(f0, roughness);
	const float3 fresnel = FresnelSchlick(f0, f1, pow(1.0 - nDosV, 5.0));

	return image * (fresnel * scaleBias.x + scaleBias.y);
}

float ComputePointLightIntensity(float distSq, float radiusSq, float invRadiusSq)
{
	// This light falloff is physically correct only for radius 10.
	const float invSquareLaw = 1.0 / (1.0 + 99.0 * distSq * invRadiusSq); // From 1 to 0.01 at radius.
	const float x = max(0.0, radiusSq - distSq); // Zero at radius.
	const float limit = x * x * invRadiusSq * invRadiusSq;
	return min(invSquareLaw, limit);
}

float ComputeSpotLightConeIntensity(float3 dirToLight, float3 lightDir, float coneOut, float invConeDelta)
{
	const float dp = dot(-dirToLight, lightDir);
	return saturate((dp - coneOut) * invConeDelta);
}

float ComputeLampRadius(float radius, float3 color)
{
	return radius * 10.0 / dot(color, 1.0 / 3.0);
}
