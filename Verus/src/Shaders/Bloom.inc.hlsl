// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_BloomVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_BloomFS
{
	float4 _exposure;
	float4 _colorScale_colorBias;
};

VERUS_UBUFFER UB_BloomLightShaftsFS
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
	float4 _csmSplitRanges;
	float4 _shadowConfig;
};
