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
	float4 _csmSplitRanges;
	float4 _shadowConfig;
};

VERUS_UBUFFER UB_SimplePerMaterialFS
{
	float4 _alphaSwitch_anisoSpecDir;
	float4 _detail_emission_gloss_hairDesat;
	float4 _detailScale_strassScale;
	float4 _emissionPick;
	float4 _eyePick;
	float4 _glossPick;
	float4 _glossScaleBias_specScaleBias;
	float4 _hairPick;
	float4 _lamScaleBias_lightPass_motionBlur;
	float4 _metalPick;
	float4 _skinPick;
	float4 _tc0ScaleBias;
	float4 _texEnableAlbedo;
	float4 _texEnableNormal;
	float4 _userColor;
	float4 _userPick;
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
