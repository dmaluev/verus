float3 DequantizeUsingDeq3D(float3 v, float3 scale, float3 bias) { return v * scale + bias; }
float2 DequantizeUsingDeq2D(float2 v, float2 scale, float2 bias) { return v * scale + bias; }
