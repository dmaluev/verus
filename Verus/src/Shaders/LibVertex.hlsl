float3 DequantizeUsingDeq3D(float3 v, float3 scale, float3 bias) { return v * scale + bias; }
float2 DequantizeUsingDeq2D(float2 v, float2 scale, float2 bias) { return v * scale + bias; }

mataff GetInstMatrix(float4 part0, float4 part1, float4 part2)
{
	return mataff(transpose(float3x4(
		part0,
		part1,
		part2)));
}
