// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_ParticlesVS
{
	matrix _matP;
	matrix _matWVP;
	float4 _viewportSize;
	float4 _brightness;
};

VERUS_UBUFFER UB_ParticlesFS
{
	float4 _tilesetSize;
};
