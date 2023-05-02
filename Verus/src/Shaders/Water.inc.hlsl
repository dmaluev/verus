// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_WaterVS
{
	mataff _matW;
	matrix _matVP;
	matrix _matScreen;
	float4 _headPos;
	float4 _eyePos_invMapSide;
	float4 _waterScale_distToMipScale_landDistToMipScale_wavePhase;
};

VERUS_UBUFFER_STRUCT UB_WaterFS
{
	mataff _matV;
	float4 _phase_wavePhase_camScale;
	float4 _diffuseColorShallow;
	float4 _diffuseColorDeep;
	float4 _ambientColor;
	float4 _fogColor;
	float4 _dirToSun;
	float4 _sunColor;

	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSliceBounds;
	float4 _shadowConfig;
};
