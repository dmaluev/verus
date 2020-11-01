// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER UB_PerFrame
{
	mataff _matV;
	matrix _matVP;
	matrix _matP;
	float4 _viewportSize;
	float4 _eyePosWV_invTessDistSq;
};

VERUS_UBUFFER UB_PerMaterialFS
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

VERUS_UBUFFER UB_PerMeshVS
{
	float4 _posDeqScale;
	float4 _posDeqBias;
	float4 _tc0DeqScaleBias;
	float4 _tc1DeqScaleBias;
};

VERUS_UBUFFER UB_SkeletonVS
{
	float4 _vWarpZones[32];
	mataff _vMatBones[VERUS_MAX_BONES];
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	float4 _userColor;
};
