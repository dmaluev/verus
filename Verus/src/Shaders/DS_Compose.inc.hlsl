// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_ComposeVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_ComposeFS
{
	mataff _matInvV;
	matrix _matInvVP;
	float4 _ambientColor_exposure;
	float4 _backgroundColor;
	float4 _fogColor;
	float4 _zNearFarEx;
	float4 _waterDiffColorShallow;
	float4 _waterDiffColorDeep;
};
