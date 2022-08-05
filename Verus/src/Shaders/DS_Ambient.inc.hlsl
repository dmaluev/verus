// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_AmbientVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER UB_AmbientFS
{
	mataff _matInvV;
	matrix _matInvP;
	float4 _ambientColorY0;
	float4 _ambientColorY1;
	float4 _invMapSide_minOcclusion;
};
