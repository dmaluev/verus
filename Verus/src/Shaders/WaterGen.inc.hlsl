// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_Gen
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER_STRUCT UB_GenHeightmapFS
{
	float4 _phase;
	float4 _amplitudes;
};

VERUS_UBUFFER_STRUCT UB_GenNormalsFS
{
	float4 _textureSize;
	float4 _waterScale;
};
