// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_PerFrame
{
	mataff _matQuad;
	mataff _matToUV;
	mataff _matV;
	matrix _matVP;
	matrix _matInvP;
	float4 _toUV;
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
	matrix _matSunShadow;
	matrix _matSunShadowCSM1;
	matrix _matSunShadowCSM2;
	matrix _matSunShadowCSM3;
	float4 _shadowConfig;
	float4 _splitRanges;
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	float4 _color;
};
