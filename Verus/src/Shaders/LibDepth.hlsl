// Convert non-linear z-buffer value to positive linear form:
float ToLinearDepth(float d, float4 zNearFarEx)
{
	// INFO: zNearFarEx.z = zFar/(zFar-zNear)
	// INFO: zNearFarEx.w = zFar*zNear/(zNear-zFar)
	return zNearFarEx.w / (d - zNearFarEx.z);
}

float4 ToLinearDepth(float4 d, float4 zNearFarEx)
{
	return zNearFarEx.w / (d - zNearFarEx.z);
}

float ComputeFog(float depth, float density)
{
	const float fog = 1.0 / exp(depth*density);
	return 1.0 - saturate(fog);
}

float4 ShadowCoords(float4 pos, matrix mat, float depth)
{
	// For CSM transformation is done in PS.
#if _SHADOW_QUALITY >= 3
	return float4(pos.xyz, depth);
#else
	return mul(pos, mat);
#endif
}
