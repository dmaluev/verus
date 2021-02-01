// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

// Convert non-linear z-buffer value to positive linear form:
float ToLinearDepth(float dz, float4 zNearFarEx)
{
	// INFO: zNearFarEx.z = zFar/(zFar-zNear)
	// INFO: zNearFarEx.w = zFar*zNear/(zNear-zFar)
	return zNearFarEx.w / (dz - zNearFarEx.z);
}
float2 ToLinearDepth(float2 dz, float4 zNearFarEx)
{
	return zNearFarEx.w / (dz - zNearFarEx.z);
}
float4 ToLinearDepth(float4 dz, float4 zNearFarEx)
{
	return zNearFarEx.w / (dz - zNearFarEx.z);
}

float ComputeFog(float depth, float density, float height = 0.f)
{
	const float strength = 1.f - saturate(height * 0.003f);
	const float power = depth * lerp(0.f, density, strength * strength);
	const float fog = 1.f / exp(power * power);
	return 1.f - saturate(fog);
}

float3 AdjustPosForShadow(float3 pos, float3 normal, float3 dirToLight, float depth, float offset = 0.f)
{
	const float scale = depth - 5.f;
	return pos +
		normal * 0.015f * max(1.f, scale * 0.2f) +
		dirToLight * max(0.f, scale * 0.002f + offset);
}

float ComputePenumbraContrast(float deltaScale, float strength, float dzFrag, float dzBlockers)
{
	const float delta = saturate(dzFrag - dzBlockers);
	return 1.f + max(0.f, 3.f - delta * deltaScale) * strength;
}

float PCF(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 tc,
	float4 config)
{
	const float dzFrag = tc.z;
#if _SHADOW_QUALITY <= _Q_MEDIUM
	return texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag).r;
#elif _SHADOW_QUALITY <= _Q_HIGH
	// High quality, use 5x5 PCF:
	float sum = 0.f;
	[unroll] for (int y = -2; y <= 2; y += 2)
	{
		[unroll] for (int x = -2; x <= 2; x += 2)
		{
			int2 offset = int2(x, y);
			if (abs(x) + abs(y) == 4)
				offset = clamp(offset, -1, 1);
			sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag, offset).r;
		}
	}
	sum *= (1.f / 9.f); // 3x3
	if (sum * (1.f - sum) != 0.f)
	{
		float2 dzBlockers_blockerCount = 0.f;
		sum = 0.f;
		[unroll] for (int y = -2; y <= 2; ++y)
		{
			[unroll] for (int x = -2; x <= 2; ++x)
			{
				if (abs(x) + abs(y) == 4)
					continue;
				sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag, int2(x, y)).r;
				const float dzBlocker = tex.SampleLevel(sam, tc.xy, 0, int2(x, y)).r;
				if (dzBlocker < dzFrag)
					dzBlockers_blockerCount += float2(dzBlocker, 1);
			}
		}
		const float ret = sum *= (1.f / 21.f); // 5x5 - 4
		if (dzBlockers_blockerCount.y > 0.f)
		{
			const float dzBlockers = dzBlockers_blockerCount.x / dzBlockers_blockerCount.y;
			const float contrast = ComputePenumbraContrast(config.x, config.y, dzFrag, dzBlockers);
			return saturate((ret - 0.5f) * contrast + 0.5f);
		}
		else
		{
			return ret;
		}
	}
	else
	{
		return sum;
	}
#else
	// Ultra quality, use 7x7 PCF:
	float sum = 0.f;
	[unroll] for (int y = -3; y <= 3; y += 2)
	{
		[unroll] for (int x = -3; x <= 3; x += 2)
		{
			int2 offset = int2(x, y);
			if (abs(x) + abs(y) == 6)
				offset = clamp(offset, -2, 2);
			sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag, offset).r;
		}
	}
	sum *= (1.f / 16.f); // 4x4
	if (sum * (1.f - sum) != 0.f)
	{
		float2 dzBlockers_blockerCount = 0.f;
		sum = 0.f;
		[unroll] for (int y = -3; y <= 3; ++y)
		{
			[unroll] for (int x = -3; x <= 3; ++x)
			{
				if (abs(x) + abs(y) == 6)
					continue;
				sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, dzFrag, int2(x, y)).r;
				const float dzBlocker = tex.SampleLevel(sam, tc.xy, 0, int2(x, y)).r;
				if (dzBlocker < dzFrag)
					dzBlockers_blockerCount += float2(dzBlocker, 1);
			}
		}
		const float ret = sum *= (1.f / 45.f); // 7x7 - 4
		if (dzBlockers_blockerCount.y > 0.f)
		{
			const float dzBlockers = dzBlockers_blockerCount.x / dzBlockers_blockerCount.y;
			const float contrast = ComputePenumbraContrast(config.x, config.y, dzFrag, dzBlockers);
			return saturate((ret - 0.5f) * contrast + 0.5f);
		}
		else
		{
			return ret;
		}
	}
	else
	{
		return sum;
	}
#endif
}

float SimplePCF(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 tc)
{
	return texCmp.SampleCmpLevelZero(samCmp, tc.xy, tc.z).r;
}

float ShadowMap(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 tc,
	float4 config)
{
	const float ret = PCF(texCmp, samCmp, tex, sam, tc, config);
	const float2 center = tc.xy * 2.f - 1.f;
	return max(ret, saturate(dot(center, center) * (9.f / 4.f) - 1.f));
}

float SimpleShadowMap(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 tc)
{
	const float ret = SimplePCF(texCmp, samCmp, tc);
	const float2 center = tc.xy * 2.f - 1.f;
	return max(ret, saturate(dot(center, center) * (9.f / 4.f) - 1.f));
}

bool IsClippedCSM(float4 clipSpacePos)
{
	const float4 absPos = abs(clipSpacePos);
	return all(absPos.xyz <= absPos.w * float3(1.01f, 1.01f, 1.f));
}

float ShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 intactPos,
	float3 biasedPos,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3,
	matrix matScreen,
	float4 csmSplitRanges,
	float4 config)
{
#if _SHADOW_QUALITY >= _Q_HIGH
	float ret = 1.f;
	float contrast = 1.f;
	const float contrastScale = config.w;
	const float4 clipSpacePos = mul(float4(intactPos, 1.f), matScreen);
	const float depth = clipSpacePos.w;

	if (IsClippedCSM(clipSpacePos))
	{
		if (depth >= csmSplitRanges.x)
		{
			const float fadeStart = (csmSplitRanges.x + csmSplitRanges.w) * 0.5f;
			const float fade = saturate((depth - fadeStart) / (csmSplitRanges.w - fadeStart));

			contrast = contrastScale * contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat0).xyz;
			ret = max(PCF(texCmp, samCmp, tex, sam, tc, config), fade);
		}
		else if (depth >= csmSplitRanges.y)
		{
			contrast = contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat1).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}
		else if (depth >= csmSplitRanges.z)
		{
			contrast = contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat2).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}
		else
		{
			const float3 tc = mul(float4(biasedPos, 1.f), mat3).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}

		return saturate((ret - 0.5f) * contrast + 0.5f);
	}
	else
		return 1.f;
#else
	const float3 tc = mul(float4(biasedPos, 1.f), mat0).xyz;
	if (all(tc.xy >= 0.f) && all(tc.xy < 1.f))
		return ShadowMap(texCmp, samCmp, tex, sam, tc, config);
	else
		return 1.f;
#endif
}

float SimpleShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 intactPos,
	float3 biasedPos,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3,
	matrix matScreen,
	float4 csmSplitRanges,
	float4 config,
	bool clipping = true)
{
#if _SHADOW_QUALITY >= _Q_HIGH
	float ret = 1.f;
	float contrast = 1.f;
	const float contrastScale = config.w;
	const float4 clipSpacePos = mul(float4(intactPos, 1.f), matScreen);
	const float depth = clipSpacePos.w;

	if (!clipping || IsClippedCSM(clipSpacePos))
	{
		if (depth >= csmSplitRanges.x)
		{
			const float fadeStart = (csmSplitRanges.x + csmSplitRanges.w) * 0.5f;
			const float fade = saturate((depth - fadeStart) / (csmSplitRanges.w - fadeStart));

			contrast = contrastScale * contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat0).xyz;
			ret = max(SimplePCF(texCmp, samCmp, tc), fade);
		}
		else if (depth >= csmSplitRanges.y)
		{
			contrast = contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat1).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}
		else if (depth >= csmSplitRanges.z)
		{
			contrast = contrastScale;
			const float3 tc = mul(float4(biasedPos, 1.f), mat2).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}
		else
		{
			const float3 tc = mul(float4(biasedPos, 1.f), mat3).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}

		return saturate((ret - 0.5f) * contrast + 0.5f);
	}
	else
		return 1.f;
#else
	const float3 tc = mul(float4(biasedPos, 1.f), mat0).xyz;
	if (all(tc.xy >= 0.f) && all(tc.xy < 1.f))
		return SimpleShadowMap(texCmp, samCmp, tc);
	else
		return 1.f;
#endif
}
