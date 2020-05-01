// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_TerrainVS
{
	mataff _matW;
	mataff _matWV; // For normal.
	mataff _matV; // For tess.
	matrix _matVP;
	matrix _matP; // For tess.
	float4 _eyePos_mapSideInv;
	float4 _viewportSize;
};

VERUS_UBUFFER UB_TerrainFS
{
	mataff _matWV; // For basis.
	float4 _vSpecStrength[8];
	float4 _vDetailStrength[8];
	float4 _lamScaleBias;
};
