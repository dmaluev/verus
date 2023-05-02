// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_SsaoVS
{
	mataff _matW;
	mataff _matV;
	mataff _matP;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER_STRUCT UB_SsaoFS
{
	float4 _tcViewScaleBias;
	float4 _zNearFarEx;
	float4 _camScale;
	float4 _smallRad_largeRad_weightScale_weightBias;
};
