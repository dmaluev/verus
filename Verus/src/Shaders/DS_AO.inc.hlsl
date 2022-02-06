// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_AOPerFrame
{
	mataff _matToUV;
	mataff _matV;
	matrix _matVP;
	matrix _matInvP;
};

VERUS_UBUFFER UB_AOTexturesFS
{
	float4 _dummy;
};

VERUS_UBUFFER UB_AOPerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER UB_AOPerObject
{
	mataff _matW;
	float4 _color;
};
