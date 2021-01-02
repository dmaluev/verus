// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_SsrVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_SsrFS
{
	mataff _matInvV;
	matrix _matPTex;
	matrix _matInvP;
	float4 _zNearFarEx;
	float4 _radius_depthBias_thickness_equalizeDist;
};
