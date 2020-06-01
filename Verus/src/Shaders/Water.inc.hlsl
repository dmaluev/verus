// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_WaterVS
{
	mataff _matW;
	matrix _matVP;
	matrix _matScreen;
	float4 _eyePos_mapSideInv;
	float4 _waterScale_distToMipScale_landDistToMipScale_wavePhase;
};

VERUS_UBUFFER UB_WaterFS
{
	mataff _matV;
	float4 _phase_wavePhase_camScale;
	float4 _diffuseColorShallow;
	float4 _diffuseColorDeep;
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
