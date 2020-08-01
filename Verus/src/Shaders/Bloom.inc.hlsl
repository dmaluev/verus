// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_BloomVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_BloomFS
{
	float4 _exposure;
};

VERUS_UBUFFER UB_BloomGodRaysFS
{
	matrix _matInvVP;
	matrix _matSunShadow;
	matrix _matSunShadowCSM1;
	matrix _matSunShadowCSM2;
	matrix _matSunShadowCSM3;
	float4 _shadowConfig;
	float4 _splitRanges;
	float4 _dirToSun;
	float4 _sunColor;
	float4 _eyePos;
};
