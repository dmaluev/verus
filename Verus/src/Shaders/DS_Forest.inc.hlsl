// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_ForestVS
{
	matrix _matP;
	matrix _matWVP;
	float4 _viewportSize;
	float4 _eyePos;
	float4 _mainCameraEyePos;
};

VERUS_UBUFFER UB_ForestFS
{
	float4 _dummy;
};
