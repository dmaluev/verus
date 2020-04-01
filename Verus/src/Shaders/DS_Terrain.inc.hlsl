// Copyright (C) 2020, Dmitry Maluev (dmaluev@gmail.com)

VERUS_UBUFFER UB_DrawDepth
{
	mataff _matW;
	mataff _matWV;
	matrix _matVP;
	float4 _posEye_mapSideInv;
};

VERUS_UBUFFER UB_PerMaterialFS
{
	float4 _vSpecStrength[8];
	float4 _vDetailStrength[8];
};
