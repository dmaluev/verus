// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_ComposeVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_ComposeFS
{
	mataff _matInvV;
	float4 _colorAmbient;
	float4 _colorBackground;
	float4 _fogColor;
	float4 _zNearFarEx;
};
