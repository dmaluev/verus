// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_TerrainVS
{
	mataff _matW;
	mataff _matWV; // For normal.
	mataff _matV; // For tess.
	matrix _matVP;
	matrix _matP; // For tess.
	float4 _eyePos_invMapSide;
	float4 _viewportSize;
};

VERUS_UBUFFER UB_TerrainFS
{
	mataff _matWV; // For basis.
	float4 _vDetailStrength[8];
	float4 _vRoughStrength[8];
	float4 _lamScaleBias;
};
