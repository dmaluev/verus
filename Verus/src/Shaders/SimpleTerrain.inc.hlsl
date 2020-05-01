// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_SimpleTerrainVS
{
	mataff _matW;
	matrix _matVP;
	float4 _eyePos_mapSideInv;
};

VERUS_UBUFFER UB_SimpleTerrainFS
{
	float4 _vSpecStrength[8];
	float4 _lamScaleBias;
	float4 _dirToSun;
	float4 _sunColor;
};
