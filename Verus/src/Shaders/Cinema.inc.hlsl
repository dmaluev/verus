// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_CinemaVS
{
	mataff _matW;
	mataff _matV;
	mataff _matP;
};

VERUS_UBUFFER_STRUCT UB_CinemaFS
{
	float4 _brightness_noiseStrength;
};
