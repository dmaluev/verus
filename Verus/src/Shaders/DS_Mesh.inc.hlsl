// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_PerView
{
	mataff _matV;
	matrix _matVP;
	matrix _matP;
	float4 _viewportSize;
	float4 _eyePosWV_invTessDistSq;
};

VERUS_UBUFFER_STRUCT UB_PerMaterialFS
{
	float4 _anisoSpecDir_detail_emission;
	float4 _motionBlur_nmContrast_roughDiffuse_sssHue;
	float4 _detailScale_strassScale;
	float4 _solidA;
	float4 _solidN;
	float4 _solidX;
	float4 _sssPick;
	float4 _tc0ScaleBias;
	float4 _userColor;
	float4 _userPick;
	float4 _xAnisoSpecScaleBias_xMetallicScaleBias;
	float4 _xRoughnessScaleBias_xWrapDiffuseScaleBias;
};

VERUS_UBUFFER_STRUCT UB_PerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
	float4 _tc0DeqScaleBias;
	float4 _tc1DeqScaleBias;
};

VERUS_UBUFFER_STRUCT UB_SkeletonVS
{
	float4 _vWarpZones[32];
	mataff _vMatBones[VERUS_MAX_BONES];
};

VERUS_UBUFFER_STRUCT UB_PerObject
{
	mataff _matW;
	float4 _userColor;
};
