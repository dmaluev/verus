// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_PerFrame
{
	mataff _matV;
	matrix _matVP;
	matrix _matP;
	float4 _viewportSize;
};

VERUS_UBUFFER UB_PerMaterialFS
{
	float4 _texEnableAlbedo;
	float4 _texEnableNormal;
	float4 _skinPick;
	float4 _hairPick;
	float4 _emitPick;
	float4 _emitXPick;
	float4 _metalPick;
	float4 _glossXPick;
	float4 _eyePick;
	float4 _hairParams;
	float4 _lsb_gloss_lp;
	float4 _ssb_as;
	float4 _userPick;
	float4 _ds_scale;
	float4 _motionBlur_glossX;
	float4 _bushEffect;
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
