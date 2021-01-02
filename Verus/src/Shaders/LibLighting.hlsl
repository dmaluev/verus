// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

float SinAcos(float x)
{
	return sqrt(saturate(1.f - x * x));
}

float EnergyNorm(float gloss)
{
	return (gloss + 8.f) * (1.f / (8.f * _PI));
}

float4 VerusLit(float3 dirToLight, float3 normal, float3 dirToEye, float gloss,
	float2 lamScaleBias, float4 aniso = 0.f, float eyeFresnel = 0.f)
{
	float4 ret = float4(1, 0, 0, 1);
	const float3 hv = normalize(dirToEye + dirToLight);
	const float nDotLRaw = dot(normal, dirToLight);
	const float nDotL = nDotLRaw * lamScaleBias.x + lamScaleBias.y;
	const float nDotE = dot(normal, dirToEye);
	const float nDotH = dot(normal, hv);
	const float aDotH = SinAcos(dot(aniso.xyz, hv));

	const float4 spec = pow(saturate(float4(nDotH, aDotH, 1.f - nDotL, 1.f - nDotE)), float4(gloss, gloss * 8.f, 5, 5));

	const float maxSpec = max(spec.x, spec.y * aniso.w);
	const float specContrast = 1.f + gloss * (0.5f / 4096.f);

	ret.x = saturate(nDotLRaw * -4.f); // For subsurface scattering effect.
	ret.y = saturate(nDotL);
	ret.z = saturate(nDotL * 8.f) * saturate((maxSpec - 0.5f) * specContrast + 0.5f) * EnergyNorm(gloss);
	ret.w = lerp(spec.z, spec.w, eyeFresnel); // For fresnel effect.

	return ret;
}

float ComputePointLightIntensity(float distSq, float radiusSq, float invRadiusSq)
{
	const float scaleFivePercent = 19.f * invRadiusSq;
	const float invSquareLaw = 1.f / max(0.1f, distSq * scaleFivePercent);
	const float x = max(0.f, radiusSq - distSq); // Zero at radius.
	const float limit = x * x * invRadiusSq * invRadiusSq;
	return min(invSquareLaw, limit);
}

float ComputeSpotLightConeIntensity(float3 dirToLight, float3 lightDir, float coneOut, float invConeDelta)
{
	const float dp = dot(-dirToLight, lightDir);
	return saturate((dp - coneOut) * invConeDelta);
}
