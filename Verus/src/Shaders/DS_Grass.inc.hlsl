// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_GrassVS
{
	mataff _matW;
	mataff _matWV;
	matrix _matVP;
	matrix _matP;
	float4 _phase_mapSideInv_bushMask;
	float4 _posEye;
	float4 _viewportSize;
	float4 _warp_turb;
};

VERUS_UBUFFER UB_GrassFS
{
	float4 _dummy;
};
