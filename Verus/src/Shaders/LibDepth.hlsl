// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

// Convert non-linear z-buffer value to positive linear form:
float ToLinearDepth(float unormDepth, float4 zNearFarEx)
{
	// INFO: zNearFarEx.z = zFar/(zFar-zNear)
	// INFO: zNearFarEx.w = zFar*zNear/(zNear-zFar)
	return zNearFarEx.w / (unormDepth - zNearFarEx.z);
}
float2 ToLinearDepth(float2 unormDepth, float4 zNearFarEx)
{
	return zNearFarEx.w / (unormDepth - zNearFarEx.z);
}
float4 ToLinearDepth(float4 unormDepth, float4 zNearFarEx)
{
	return zNearFarEx.w / (unormDepth - zNearFarEx.z);
}

float ComputeFog(float depth, float density, float height = 0.0)
{
	const float opacity = 1.0 - saturate(height * (1.0 / 400.0));
	const float power = depth * lerp(0.0, density, opacity * opacity);
	const float2 fog = 1.0 / exp(float2(power, power * power));
	return 1.0 - saturate(lerp(fog.x, fog.y, 2.0 / 3.0));
}

float ChebyshevUpperBound(float2 moments, float z)
{
	const float p = step(z, moments.x);
	const float v = max(moments.y - moments.x * moments.x, 1e-10);
	const float d = z - moments.x;
	const float pMax = v / (v + d * d);
	return max(p, saturate((pMax - 0.2) * 1.25));
}

float3 ApplyShadowBiasing(float3 pos, float3 normal, float3 dirToLight,
	float depth, float normalDepthBias = 0.04)
{
	const float depthFactor = depth - 5.0;
	return pos +
		normal * normalDepthBias * max(1.0, depthFactor * 0.2) +
		dirToLight * max(0.0, depthFactor * 0.002);
}

float ComputePenumbraContrast(float deltaScale, float strength, float unormFragDepth, float unormBlockersDepth)
{
	const float delta = saturate(unormFragDepth - unormBlockersDepth);
	return 1.0 + max(0.0, 3.0 - delta * deltaScale) * strength;
}

// Percentage Closer Filtering:
float PCF(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 tc,
	float4 config)
{
	const float unormFragDepth = tc.z;
#if _SHADOW_QUALITY <= _Q_MEDIUM
	return texCmp.SampleCmpLevelZero(samCmp, tc.xy, unormFragDepth).r;
#elif _SHADOW_QUALITY <= _Q_HIGH
	// High quality, use 5x5 PCF:
	float sum = 0.0;
	[unroll] for (int y = -2; y <= 2; ++y)
	{
		[unroll] for (int x = -2; x <= 2; ++x)
		{
			const int test = abs(x) + abs(y);
			if (test == 4 || test % 2)
				continue;
			int2 offset = int2(x, y);
			sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, unormFragDepth, offset).r;
		}
	}
	sum *= (1.0 / 9.0); // 3x3-4 + 2x2

	if (sum * (1.0 - sum) != 0.0)
	{
		float2 unormBlockersDepth_blockerCount = 0.0;
		sum = 0.0;
		[unroll] for (int y = -2; y <= 2; ++y)
		{
			[unroll] for (int x = -2; x <= 2; ++x)
			{
				const int test = abs(x) + abs(y);
				if (test == 4)
					continue;
				sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, unormFragDepth, int2(x, y)).r;
				const float unormBlockerDepth = tex.SampleLevel(sam, tc.xy, 0.0, int2(x, y)).r;
				if (unormBlockerDepth < unormFragDepth)
					unormBlockersDepth_blockerCount += float2(unormBlockerDepth, 1);
			}
		}
		const float ret = sum *= (1.0 / 21.0); // 5x5-4
		if (unormBlockersDepth_blockerCount.y > 0.0)
		{
			const float unormBlockersDepth = unormBlockersDepth_blockerCount.x / unormBlockersDepth_blockerCount.y;
			const float contrast = ComputePenumbraContrast(config.x, config.y, unormFragDepth, unormBlockersDepth);
			return saturate((ret - 0.5) * contrast + 0.5);
		}
		else
		{
			return ret;
		}
	}
	else // Zero or one?
	{
		return sum;
	}
#else
	// Ultra quality, use 7x7 PCF:
	float sum = 0.0;
	[unroll] for (int y = -3; y <= 3; ++y)
	{
		[unroll] for (int x = -3; x <= 3; ++x)
		{
			const int test = abs(x) + abs(y);
			if (test == 6 || test % 2)
				continue;
			int2 offset = int2(x, y);
			sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, unormFragDepth, offset).r;
		}
	}
	sum *= (1.0 / 21.0); // 4x4-4 + 3x3

	if (sum * (1.0 - sum) != 0.0)
	{
		float2 unormBlockersDepth_blockerCount = 0.0;
		sum = 0.0;
		[unroll] for (int y = -3; y <= 3; ++y)
		{
			[unroll] for (int x = -3; x <= 3; ++x)
			{
				const int test = abs(x) + abs(y);
				if (test == 6)
					continue;
				int2 offset = int2(x, y);
				sum += texCmp.SampleCmpLevelZero(samCmp, tc.xy, unormFragDepth, offset).r;
				const float unormBlockerDepth = tex.SampleLevel(sam, tc.xy, 0.0, offset).r;
				if (unormBlockerDepth < unormFragDepth)
					unormBlockersDepth_blockerCount += float2(unormBlockerDepth, 1);
			}
		}
		const float ret = sum *= (1.0 / 45.0); // 7x7-4
		if (unormBlockersDepth_blockerCount.y > 0.0)
		{
			const float unormBlockersDepth = unormBlockersDepth_blockerCount.x / unormBlockersDepth_blockerCount.y;
			const float contrast = ComputePenumbraContrast(config.x, config.y, unormFragDepth, unormBlockersDepth);
			return saturate((ret - 0.5) * contrast + 0.5);
		}
		else
		{
			return ret;
		}
	}
	else // Zero or one?
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
	const float2 center = tc.xy * 2.0 - 1.0;
	const float presence = 1.0 - saturate(dot(center, center) * (9.0 / 4.0) - 1.0);
	return max(ret, 1.0 - presence);
}

float SimpleShadowMap(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 tc)
{
	const float ret = SimplePCF(texCmp, samCmp, tc);
	const float2 center = tc.xy * 2.0 - 1.0;
	const float presence = 1.0 - saturate(dot(center, center) * (9.0 / 4.0) - 1.0);
	return max(ret, 1.0 - presence);
}

bool IsClippedCSM(float4 clipSpacePos)
{
	const float4 absPos = abs(clipSpacePos);
	return all(absPos.xyz <= absPos.w * float3(1.01, 1.01, 1.0));
}

float ShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	Texture2D tex,
	SamplerState sam,
	float3 inPos,
	float3 biasedPos,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3,
	matrix matScreen,
	float4 csmSliceBounds,
	float4 config)
{
#if _SHADOW_QUALITY >= _Q_HIGH
	float ret = 1.0;
	float contrast = 1.0;
	const float contrastScale = config.w;
	const float4 clipSpacePos = mul(float4(inPos, 1), matScreen);
	const float depth = clipSpacePos.w; // W is linear depth from 0.

	if (IsClippedCSM(clipSpacePos))
	{
		if (depth >= csmSliceBounds.x)
		{
			contrast = contrastScale * contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat0).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);

			const float presenceDist = (csmSliceBounds.x + csmSliceBounds.w) * 0.5;
			const float presence = 1.0 - saturate((depth - presenceDist) / (csmSliceBounds.w - presenceDist));
			ret = max(ret, 1.0 - presence);
		}
		else if (depth >= csmSliceBounds.y)
		{
			contrast = contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat1).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}
		else if (depth >= csmSliceBounds.z)
		{
			contrast = contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat2).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}
		else
		{
			const float3 tc = mul(float4(biasedPos, 1), mat3).xyz;
			ret = PCF(texCmp, samCmp, tex, sam, tc, config);
		}

		return saturate((ret - 0.5) * contrast + 0.5);
	}
	else
		return 1.0;
#else
	const float3 tc = mul(float4(biasedPos, 1), mat0).xyz;
	if (all(bool4(tc.xy >= 0.0, tc.xy < 1.0)))
		return ShadowMap(texCmp, samCmp, tex, sam, tc, config);
	else
		return 1.0;
#endif
}

float SimpleShadowMapCSM(
	Texture2D texCmp,
	SamplerComparisonState samCmp,
	float3 inPos,
	float3 biasedPos,
	matrix mat0,
	matrix mat1,
	matrix mat2,
	matrix mat3,
	matrix matScreen,
	float4 csmSliceBounds,
	float4 config,
	bool clipping = true)
{
#if _SHADOW_QUALITY >= _Q_HIGH
	float ret = 1.0;
	float contrast = 1.0;
	const float contrastScale = config.w;
	const float4 clipSpacePos = mul(float4(inPos, 1), matScreen);
	const float depth = clipSpacePos.w; // W is linear depth from 0.

	if (!clipping || IsClippedCSM(clipSpacePos))
	{
		if (depth >= csmSliceBounds.x)
		{
			contrast = contrastScale * contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat0).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);

			const float presenceDist = (csmSliceBounds.x + csmSliceBounds.w) * 0.5;
			const float presence = 1.0 - saturate((depth - presenceDist) / (csmSliceBounds.w - presenceDist));
			ret = max(ret, 1.0 - presence);
		}
		else if (depth >= csmSliceBounds.y)
		{
			contrast = contrastScale * contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat1).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}
		else if (depth >= csmSliceBounds.z)
		{
			contrast = contrastScale;
			const float3 tc = mul(float4(biasedPos, 1), mat2).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}
		else
		{
			const float3 tc = mul(float4(biasedPos, 1), mat3).xyz;
			ret = SimplePCF(texCmp, samCmp, tc);
		}

		return saturate((ret - 0.5) * contrast + 0.5);
	}
	else
		return 1.0;
#else
	const float3 tc = mul(float4(biasedPos, 1), mat0).xyz;
	if (all(bool4(tc.xy >= 0.0, tc.xy < 1.0)))
		return SimpleShadowMap(texCmp, samCmp, tc);
	else
		return 1.0;
#endif
}

// Variance Shadow Map:
float ShadowMapVSM(
	Texture2D tex,
	SamplerState sam,
	float3 biasedPos,
	matrix mat)
{
	float4 tc = mul(float4(biasedPos, 1), mat);
	tc.xy /= tc.w;
	const float2 moments = tex.SampleLevel(sam, tc.xy, 0.0).rg;
	return ChebyshevUpperBound(moments, tc.w); // W is linear depth from 0.
}
