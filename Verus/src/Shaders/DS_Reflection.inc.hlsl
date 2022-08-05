// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_ReflectionVS
{
	mataff _matW;
	mataff _matV;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER UB_ReflectionFS
{
	float4 _dummy;
};
