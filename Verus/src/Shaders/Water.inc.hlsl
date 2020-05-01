// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_WaterVS
{
	mataff _matW;
	matrix _matVP;
	float4 _eyePos;
	float4 _waterScale_distToMipScale_mapSideInv_landDistToMipScale;
};

VERUS_UBUFFER UB_WaterFS
{
	matrix _matReflect;
	float4 _csmParams;
	float4 _phases_mapSideInv;
	float4 _posEye;
	float4 _colorAmbient;
	float4 _fogColor;

	float4 _phase;
	float4 _dirToSun;
	float4 _sunColor;
};
