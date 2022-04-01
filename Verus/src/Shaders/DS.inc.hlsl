// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_PerFrame
{
	mataff _matToUV;
	mataff _matV;
	mataff _matInvV;
	matrix _matVP;
	matrix _matInvP;
};

VERUS_UBUFFER UB_TexturesFS
{
	float4 _dummy;
};

VERUS_UBUFFER UB_PerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER UB_ShadowFS
{
	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSplitRanges;
	float4 _shadowConfig;
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	float4 _color;
};
