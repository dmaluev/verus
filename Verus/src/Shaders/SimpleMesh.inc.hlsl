// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_SimplePerFrame
{
	matrix _matVP;
	float4 _eyePos_clipDistanceOffset;
	float4 _ambientColor;
	float4 _fogColor;
	float4 _dirToSun;
	float4 _sunColor;

	matrix _matShadow;
	matrix _matShadowCSM1;
	matrix _matShadowCSM2;
	matrix _matShadowCSM3;
	matrix _matScreenCSM;
	float4 _csmSliceBounds;
	float4 _shadowConfig;
};

VERUS_UBUFFER UB_SimplePerMaterialFS
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

VERUS_UBUFFER UB_SimplePerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
	float4 _tc0DeqScaleBias;
	float4 _tc1DeqScaleBias;
};

VERUS_UBUFFER UB_SimpleSkeletonVS
{
	mataff _vMatBones[VERUS_MAX_BONES];
};

VERUS_UBUFFER UB_SimplePerObject
{
	mataff _matW;
	float4 _userColor;
};
