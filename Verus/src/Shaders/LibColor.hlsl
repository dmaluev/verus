// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

// See: https://en.wikipedia.org/wiki/Grayscale
float Grayscale(float3 color)
{
	return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 Desaturate(float3 color, float alpha, float lum = 1.f)
{
	const float gray = Grayscale(color) * lum;
	return lerp(color, gray, alpha);
}

// See: http://www.chilliant.com/rgb2hsv.html
float3 ConvertRGBtoHCV(float3 color)
{
	const float4 p = (color.g < color.b) ? float4(color.bg, -1.f, 2.f / 3.f) : float4(color.gb, 0.f, -1.f / 3.f);
	const float4 q = (color.r < p.x) ? float4(p.xyw, color.r) : float4(color.r, p.yzx);
	const float c = q.x - min(q.w, q.y);
	const float h = abs((q.w - q.y) / (6.f * c + _SINGULARITY_FIX) + q.z);
	return float3(h, c, q.x);
}

float Contrast(float gray, float contrast)
{
	return saturate(((gray - 0.5f) * contrast) + 0.5f);
}

float3 Overlay(float3 colorA, float3 colorB)
{
	const float3 x = step(0.5f, colorA);
	return lerp(colorA * colorB * 2.f, 1.f - (2.f * (1.f - colorA) * (1.f - colorB)), x);
}

// <AlbedoPick>
float PickAlpha(float3 albedo, float4 pick, float sharp)
{
	const float3 d = albedo - pick.rgb;
	return saturate(1.f - dot(d, d) * sharp) * pick.a;
}

float PickAlphaRound(float4 pick, float2 tc)
{
	const float2 d = frac(tc) - pick.xy;
	return step(dot(d, d), pick.z) * pick.a;
}
// </AlbedoPick>

// Returns {alpha, specularity}:
float2 AlphaSwitch(float4 albedo, float2 tc, float2 alphaSwitch)
{
	if (all(frac(tc) >= alphaSwitch))
		return float2(albedo.a, Grayscale(albedo.rgb)); // 8-bit alpha.
	else
		return saturate(float2(albedo.a * 8.f, (albedo.a - 0.15f) / 0.85f)); // 5-bit alpha.
}

// See: http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float4 ColorToLinear(float4 x)
{
	const float3 rgb = x.rgb < 0.04045f ? x.rgb * (1.f / 12.92f) : pow(abs(x.rgb + 0.055f) / 1.055f, 2.4f);
	return float4(rgb, x.a);
}
float4 ColorToSRGB(float4 x)
{
	const float3 rgb = x.rgb < 0.0031308f ? 12.92f * x.rgb : 1.055f * pow(abs(x.rgb), 1.f / 2.4f) - 0.055f;
	return float4(rgb, x.a);
}

float3 ToneMappingReinhard(float3 x)
{
	return x / (1.f + x);
}

float3 ToneMappingInvReinhard(float3 x)
{
	return -x / (x - 1.f);
}

// See: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ToneMappingACES(float3 x)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 VerusToneMapping(float3 hdr, float filmicLook = 1.f)
{
	const float maxValue = max(max(hdr.r, hdr.g), hdr.b);
	const float desatMask = saturate(maxValue * 0.15f);
	hdr = lerp(hdr, maxValue, desatMask * desatMask); // Color crosstalk.
	const float3 ldr = lerp(1.f - exp(-hdr), ToneMappingACES(hdr), filmicLook);
	return saturate(ldr * 1.2f - 0.002f); // Add some clipping.
}
