// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_SsaoVS
{
	mataff _matW;
	mataff _matV;
	mataff _matP;
};

VERUS_UBUFFER UB_SsaoFS
{
	float4 _zNearFarEx;
	float4 _camScale;
};
