// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_ComposeVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_ComposeFS
{
	mataff _matV;
	mataff _matInvV;
	matrix _matInvVP;
	float4 _ambientColor_exposure;
	float4 _backgroundColor;
	float4 _fogColor;
	float4 _zNearFarEx;
	float4 _waterDiffColorShallow;
	float4 _waterDiffColorDeep;
	float4 _lightGlossScale;
};
