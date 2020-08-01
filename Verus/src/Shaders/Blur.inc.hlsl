// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_BlurVS
{
	mataff _matW;
	mataff _matV;
};

VERUS_UBUFFER UB_BlurFS
{
	float4 _dummy;
};

VERUS_UBUFFER UB_ExtraBlurFS
{
	matrix _matInvVP;
	matrix _matPrevVP;
	float4 _zNearFarEx;
	float4 _textureSize;
};
