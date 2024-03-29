// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_View
{
	mataff _matToUV;
	mataff _matV;
	mataff _matInvV;
	matrix _matVP;
	matrix _matInvP;
	float4 _tcViewScaleBias;
	float4 _ambientColor;
};

VERUS_UBUFFER_STRUCT UB_SubpassFS
{
	float4 _dummy;
};

VERUS_UBUFFER_STRUCT UB_ShadowFS
{
	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSliceBounds;
	float4 _shadowConfig;
};

VERUS_UBUFFER_STRUCT UB_MeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_SBUFFER_STRUCT SB_InstanceData
{
	matrix _matShadow;
};

VERUS_UBUFFER_STRUCT UB_Object
{
	mataff _matW;
	float4 _color;
};
