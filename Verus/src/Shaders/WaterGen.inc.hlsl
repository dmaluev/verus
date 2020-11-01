// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_Gen
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_GenHeightmapFS
{
	float4 _phase;
	float4 _amplitudes;
};

VERUS_UBUFFER UB_GenNormalsFS
{
	float4 _textureSize;
	float4 _waterScale;
};
