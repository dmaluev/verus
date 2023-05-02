// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_QuadVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER_STRUCT UB_QuadFS
{
	float4 _rMultiplexer;
	float4 _gMultiplexer;
	float4 _bMultiplexer;
	float4 _aMultiplexer;
};
