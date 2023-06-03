// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_AmbientNodeView
{
	mataff _matToUV;
	mataff _matV;
	matrix _matVP;
	matrix _matInvP;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER_STRUCT UB_AmbientNodeSubpassFS
{
	float4 _firstInstance;
};

VERUS_UBUFFER_STRUCT UB_AmbientNodeMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_SBUFFER_STRUCT SB_AmbientNodeInstanceData
{
	mataff _matToBoxSpace;
	float4 _wall_wallScale_cylinder_sphere;
};

VERUS_UBUFFER_STRUCT UB_AmbientNodeObject
{
	mataff _matW;
	float4 _color;
};
