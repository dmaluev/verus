float SinAcos(float x)
{
	return sqrt(saturate(1.0 - x * x));
}

// See: https://en.wikipedia.org/wiki/Schlick%27s_approximation
float FresnelSchlick(float minRef, float maxAdd, float power)
{
	return saturate(minRef + maxAdd * power);
}

float EnergyNorm(float gloss)
{
	return (gloss + 2.0) / 8.0;
}

float4 TrueLit(float3 dirToLight, float3 normal, float3 dirToEye, float gloss,
	float2 lamScaleBias, float4 aniso = 0.0)
{
	float4 ret = float4(1, 0, 0, 1);
	const float3 hv = normalize(dirToEye + dirToLight);
	const float nDotL = dot(normal, dirToLight);
	const float nDotH = dot(normal, hv);
	const float aDotH = SinAcos(dot(aniso.xyz, hv));
	const float3 spec = saturate(float3(EnergyNorm(gloss).xx, 1)*
		pow(saturate(float3(nDotH, aDotH, 1.0 - nDotL)), float3(gloss, gloss*8.0, 5)));
	ret.x = saturate(nDotL*-4.0); // For subsurface scattering effect.
	ret.y = saturate(nDotL*lamScaleBias.x + lamScaleBias.y);
	ret.z = saturate(nDotL*8.0)*max(spec.x, spec.y*aniso.w);
	ret.w = spec.z; // For fresnel effect.
	return ret;
}

float ComputePointLightIntensity(float distSq, float radiusSq, float invRadiusSq)
{
	const float scaleFivePercent = 19.0*invRadiusSq;
	const float invSquareLaw = 1.0 / (1.0 + distSq * scaleFivePercent);
	const float x = max(0.0, radiusSq - distSq); // Zero at radius.
	const float limit = x * x*invRadiusSq*invRadiusSq;
	return saturate(min(invSquareLaw, limit));
}

float ComputeSpotLightConeIntensity(float3 dirToLight, float3 lightDir, float coneOut, float invConeDelta)
{
	const float d = dot(-dirToLight, lightDir);
	return saturate((d - coneOut)*invConeDelta);
}
