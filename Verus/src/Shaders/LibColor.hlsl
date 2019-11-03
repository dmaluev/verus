// See: http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float4 ColorToLinear(float4 x)
{
	const float3 rgb = x.rgb < 0.04045 ? x.rgb * (1.0 / 12.92) : pow(abs(x.rgb + 0.055) / 1.055, 2.4);
	return float4(rgb, x.a);
}
float4 ColorToSRGB(float4 x)
{
	const float3 rgb = x.rgb < 0.0031308 ? 12.92 * x.rgb : 1.055 * pow(abs(x.rgb), 1.0 / 2.4) - 0.055;
	return float4(rgb, x.a);
}

// See: https://en.wikipedia.org/wiki/Grayscale
float Grayscale(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 Desaturate(float3 color, float alpha, float lum = 1.0)
{
	const float gray = Grayscale(color) * lum;
	return lerp(color, gray, alpha);
}

// See: http://www.chilliant.com/rgb2hsv.html
float3 ConvertRGBtoHCV(float3 color)
{
	const float4 p = (color.g < color.b) ? float4(color.bg, -1.0, 2.0 / 3.0) : float4(color.gb, 0.0, -1.0 / 3.0);
	const float4 q = (color.r < p.x) ? float4(p.xyw, color.r) : float4(color.r, p.yzx);
	const float c = q.x - min(q.w, q.y);
	const float h = abs((q.w - q.y) / (6.0 * c + _SINGULARITY_FIX) + q.z);
	return float3(h, c, q.x);
}

float3 Overlay(float3 colorA, float3 colorB)
{
	const float3 x = step(0.5, colorA);
	return lerp(colorA * colorB * 2.0, 1.0 - (2.0 * (1.0 - colorA) * (1.0 - colorB)), x);
}

// <AlbedoPick>
float PickAlpha(float3 albedo, float4 pick, float sharp)
{
	const float3 d = albedo - pick.rgb;
	return saturate(1.0 - dot(d, d) * sharp) * pick.a;
}

float PickAlphaHue(float3 albedo, float4 pick, float sharp)
{
	const float hueA = ConvertRGBtoHCV(albedo).x;
	const float hueB = ConvertRGBtoHCV(pick.rgb).x;
	const float d = abs(hueA - hueB);
	return saturate(1.0 - min(d, 1.0 - d) * sharp) * pick.a;
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
		return saturate(float2(albedo.a * 8.0, (albedo.a - 0.15) / 0.85)); // 5-bit alpha.
}
