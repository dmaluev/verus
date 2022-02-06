// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

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

	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSplitRanges;
	float4 _shadowConfig;
};
