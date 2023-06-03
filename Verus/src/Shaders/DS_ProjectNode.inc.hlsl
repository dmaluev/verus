// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_ProjectNodeView
{
	mataff _matToUV;
	matrix _matVP;
	matrix _matInvP;
	float4 _tcViewScaleBias;
};

VERUS_UBUFFER_STRUCT UB_ProjectNodeSubpassFS
{
	float4 _dummy;
};

VERUS_UBUFFER_STRUCT UB_ProjectNodeMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER_STRUCT UB_ProjectNodeTextureFS
{
	float4 _dummy;
};

VERUS_UBUFFER_STRUCT UB_ProjectNodeObject
{
	mataff _matW;
	float4 _levels;
	mataff _matToBoxSpace;
};
