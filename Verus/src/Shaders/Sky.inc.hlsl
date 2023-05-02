// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_PerView
{
	float4 _time_cloudiness;
	float4 _ambientColor;
	float4 _fogColor;
	float4 _dirToSun;
	float4 _sunColor;
	float4 _phaseAB;
};

VERUS_UBUFFER_STRUCT UB_PerMaterialFS
{
	float4 _dummy;
};

VERUS_UBUFFER_STRUCT UB_PerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER_STRUCT UB_PerObject
{
	mataff _matW;
	matrix _matWVP;
};
