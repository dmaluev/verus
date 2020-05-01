// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_PerFrame
{
	float4 _color;
	float4 _time_cloudiness_expo;
	float4 _dirToSun;
	float4 _phaseAB;
	float4 _fogColor;
};

VERUS_UBUFFER UB_PerMaterialFS
{
	float4 _dummy;
};

VERUS_UBUFFER UB_PerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	matrix _matWVP;
	float4 _add;
};
