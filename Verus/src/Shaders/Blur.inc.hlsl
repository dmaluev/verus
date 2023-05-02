// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_BlurVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER_STRUCT UB_BlurFS
{
	float4 _tcViewScaleBias;
	float3 _radius_invRadius_stride;
	int    _sampleCount;
};

VERUS_UBUFFER_STRUCT UB_ExtraBlurFS
{
	matrix _matInvVP;
	matrix _matPrevVP;
	float4 _zNearFarEx;
	float4 _textureSize;
	float4 _blurDir;
	float4 _focusDist;
};
