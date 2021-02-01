// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_CinemaVS
{
	mataff _matW;
	mataff _matV;
	mataff _matP;
};

VERUS_UBUFFER UB_CinemaFS
{
	float4 _brightness_noiseStrength;
};
