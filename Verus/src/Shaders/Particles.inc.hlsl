// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

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
