// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

float ComputeNormalZ(float2 v)
{
	return sqrt(saturate(1.0 - dot(v.xy, v.xy)));
}

float3 NormalMapFromBC5(float4 normalSam, float contrast = -1.0)
{
	float3 normal = (normalSam.rgb - 0.5) * contrast;
	normal.z = ComputeNormalZ(normal.xy);
	return normal;
}

float3 ComputeNormals(Texture2D tex, SamplerState sam, float2 tc, float damp, int scale = 1)
{
	const float tl = damp * tex.Sample(sam, tc, int2(-scale, -scale)).r;
	const float  l = damp * tex.Sample(sam, tc, int2(-scale, 0)).r;
	const float bl = damp * tex.Sample(sam, tc, int2(-scale, +scale)).r;
	const float  t = damp * tex.Sample(sam, tc, int2(0, -scale)).r;
	const float  b = damp * tex.Sample(sam, tc, int2(0, +scale)).r;
	const float tr = damp * tex.Sample(sam, tc, int2(+scale, -scale)).r;
	const float  r = damp * tex.Sample(sam, tc, int2(+scale, 0)).r;
	const float br = damp * tex.Sample(sam, tc, int2(+scale, +scale)).r;

	const float dX = (-47.0 * tl - 162.0 * l - 47.0 * bl) + (47.0 * tr + 162.0 * r + 47.0 * br); // Left + Right.
	const float dY = (-47.0 * tl - 162.0 * t - 47.0 * tr) + (47.0 * bl + 162.0 * b + 47.0 * br); // Top + Bottom.

	const float3 normal = normalize(float3(dX, dY, 1));

	return normal * 0.5 + 0.5;
}
