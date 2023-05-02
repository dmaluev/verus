// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_ForestVS
{
	matrix _matP;
	matrix _matWVP;
	float4 _viewportSize;
	float4 _eyePos;
	float4 _headPos;
	float4 _matRoll;
};

VERUS_UBUFFER_STRUCT UB_ForestFS
{
	float4 _dummy;
};
