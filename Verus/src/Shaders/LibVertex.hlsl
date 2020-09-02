// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

float3 DequantizeUsingDeq3D(float3 v, float3 scale, float3 bias) { return v * scale + bias; }
float2 DequantizeUsingDeq2D(float2 v, float2 scale, float2 bias) { return v * scale + bias; }

float3 NormalizePosition(float3 v) { return v * (1.0 / 65535.0) + 0.5; }

mataff GetInstMatrix(float4 part0, float4 part1, float4 part2)
{
	return mataff(transpose(float3x4(
		part0,
		part1,
		part2)));
}

float3 Warp(float3 pos, float4 center_radInv, float3 offset)
{
	const float3 d = pos - center_radInv.xyz;
	const float scale = saturate(1.0 - dot(d, d) * center_radInv.w);
	return offset * scale;
}

float3 ApplyWarp(float3 pos, float4 zones[32], float4 mask)
{
	float3 ret = pos;
	if (pos.y > zones[1].w)
	{
		for (uint i = 0; i < 16; ++i)
			ret += Warp(pos, zones[i * 2], zones[i * 2 + 1].xyz) * mask[i / 4];
	}
	return ret;
}

float4 GetWarpScale()
{
	return float4(1, 1.0 / 32767.0, 1, 1);
}
