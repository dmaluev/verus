// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_GrassVS
{
	mataff _matW;
	mataff _matWV;
	matrix _matVP;
	matrix _matP;
	float4 _phase_invMapSide_bushMask;
	float4 _posEye;
	float4 _viewportSize;
	float4 _warp_turb;
};

VERUS_UBUFFER UB_GrassFS
{
	float4 _dummy;
};
