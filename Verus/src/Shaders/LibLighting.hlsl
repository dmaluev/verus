// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

float SinAcos(float x)
{
	return sqrt(saturate(1.0 - x * x));
}

float EnergyNorm(float gloss)
{
	return (gloss + 8.0) * (1.0 / (8.0 * _PI));
}

// Aka D:
float MicrofacetDistribution(float alphaRoughnessSq, float nDotH)
{
	const float f = (nDotH * nDotH) * (alphaRoughnessSq - 1.0) + 1.0;
	return alphaRoughnessSq / (f * f);
}

// Aka V:
float MaskingShadowingFunction(float alphaRoughnessSq, float nDotL, float nDotV)
{
	const float2 nDotL_nDotV = float2(nDotL, nDotV);
	const float2 nDotLSq_nDotVSq = nDotL_nDotV * nDotL_nDotV;
	const float2 ggxl_ggxv = nDotL_nDotV.yx * sqrt(alphaRoughnessSq + (1.0 - alphaRoughnessSq) * nDotLSq_nDotVSq);
	const float ggx = ggxl_ggxv.x + ggxl_ggxv.y;
	if (ggx > 0.0)
		return 0.5 / ggx;
	return 0.0;
}

void VerusLit2(float3 normal, float3 dirToLight, float3 dirToEye, float metallic, float roughness, float3 albedo, out float3 diff, out float3 spec)
{
	const float alphaRoughness = roughness * roughness;
	const float alphaRoughnessSq = alphaRoughness * alphaRoughness;
	const float3 hv = normalize(dirToLight + dirToEye);
	const float nDotL = saturate(dot(normal, dirToLight));
	const float nDotV = saturate(dot(normal, dirToEye));
	const float nDotH = saturate(dot(normal, hv));
	const float vDotH = saturate(dot(dirToEye, hv));

	const float3 diffuse = albedo * (1.0 - metallic);

	const float3 f0 = lerp(0.04, albedo, metallic);
	const float3 f = f0 + (1.0 - f0) * pow(saturate(1.0 - vDotH), 5.0);

	diff = nDotL * (1 - f) * diffuse;
	spec = nDotL * f * MicrofacetDistribution(alphaRoughnessSq, nDotH) * MaskingShadowingFunction(alphaRoughnessSq, nDotL, nDotV);

	diff = diff + spec;
}

float4 VerusLit(float3 dirToLight, float3 normal, float3 dirToEye, float gloss,
	float2 lamScaleBias, float4 aniso = 0.0, float eyeFresnel = 0.0)
{
	float4 ret = float4(1, 0, 0, 1);
	const float3 hv = normalize(dirToEye + dirToLight);
	const float nDotLRaw = dot(normal, dirToLight);
	const float nDotL = nDotLRaw * lamScaleBias.x + lamScaleBias.y;
	const float nDotE = dot(normal, dirToEye);
	const float nDotH = dot(normal, hv);
	const float aDotH = SinAcos(dot(aniso.xyz, hv));

	const float4 spec = pow(saturate(float4(nDotH, aDotH, 1.0 - nDotL, 1.0 - nDotE)), float4(gloss, gloss * 8.0, 5, 5));

	const float maxSpec = max(spec.x, spec.y * aniso.w);
	const float specContrast = 1.0 + gloss * (0.5 / 4096.0);

	ret.x = saturate(nDotLRaw * -4.0); // For subsurface scattering effect.
	ret.y = saturate(nDotL);
	ret.z = saturate(nDotL * 8.0) * saturate((maxSpec - 0.5) * specContrast + 0.5) * EnergyNorm(gloss);
	ret.w = lerp(spec.z, spec.w, eyeFresnel); // For fresnel effect.

	return ret;
}

float ComputePointLightIntensity(float distSq, float radiusSq, float invRadiusSq)
{
	const float scaleFivePercent = 19.0 * invRadiusSq;
	const float invSquareLaw = 1.0 / max(0.1, distSq * scaleFivePercent);
	const float x = max(0.0, radiusSq - distSq); // Zero at radius.
	const float limit = x * x * invRadiusSq * invRadiusSq;
	return min(invSquareLaw, limit);
}

float ComputeSpotLightConeIntensity(float3 dirToLight, float3 lightDir, float coneOut, float invConeDelta)
{
	const float dp = dot(-dirToLight, lightDir);
	return saturate((dp - coneOut) * invConeDelta);
}
