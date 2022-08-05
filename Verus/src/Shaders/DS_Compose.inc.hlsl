// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_ComposeVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER UB_ComposeFS
{
	matrix _matInvVP;
	float4 _exposure_underwaterMask;
	float4 _backgroundColor;
	float4 _fogColor;
	float4 _zNearFarEx;
	float4 _waterDiffColorShallow;
	float4 _waterDiffColorDeep;
};
