// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_BloomVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER_STRUCT UB_BloomFS
{
	float4 _colorScale_colorBias_exposure;
};

VERUS_UBUFFER_STRUCT UB_BloomLightShaftsFS
{
	matrix _matInvVP;
	float4 _dirToSun;
	float4 _sunColor;
	float4 _eyePos;
	float4 _maxDist_sunGloss_wideStrength_sunStrength;

	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSliceBounds;
	float4 _shadowConfig;
};
