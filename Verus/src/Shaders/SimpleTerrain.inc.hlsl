// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_SimpleTerrainVS
{
	mataff _matW;
	matrix _matVP;
	float4 _eyePos;
	float4 _mapSideInv_clipDistanceOffset;
};

VERUS_UBUFFER UB_SimpleTerrainFS
{
	float4 _vSpecStrength[8];
	float4 _lamScaleBias;
	float4 _ambientColor;
	float4 _fogColor;
	float4 _dirToSun;
	float4 _sunColor;
	matrix _matSunShadow;
	matrix _matSunShadowCSM1;
	matrix _matSunShadowCSM2;
	matrix _matSunShadowCSM3;
	float4 _shadowConfig;
	float4 _splitRanges;
};
