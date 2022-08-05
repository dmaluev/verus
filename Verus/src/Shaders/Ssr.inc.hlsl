// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_SsrVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER UB_SsrFS
{
	mataff _matInvV;
	matrix _matPTex;
	matrix _matInvP;
	float4 _tcViewScaleBias;
	float4 _fogColor;
	float4 _zNearFarEx;
	float4 _radius_depthBias_thickness_equalizeDist;
};
