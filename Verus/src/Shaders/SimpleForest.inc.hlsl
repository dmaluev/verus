// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

VERUS_UBUFFER_STRUCT UB_SimpleForestVS
{
	matrix _matP;
	matrix _matWVP;
	float4 _viewportSize;
	float4 _eyePos_clipDistanceOffset;
	float4 _mainCameraEyePos_pointSpriteFlipY;
};

VERUS_UBUFFER_STRUCT UB_SimpleForestFS
{
	mataff _matInvV;
	float4 _normalFlip;
	float4 _tcFlip;
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
