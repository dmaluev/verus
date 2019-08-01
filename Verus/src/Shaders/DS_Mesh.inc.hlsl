VERUS_UBUFFER UB_PerFrame
{
	mataff _matV;
	matrix _matP;
	matrix _matVP;
};

VERUS_UBUFFER UB_PerMaterial
{
	float4 _lsb_gloss_lp;
};

VERUS_UBUFFER UB_PerMesh
{
	float4 _posDeqScale;
	float4 _posDeqBias;
	float4 _tc0DeqScaleBias;
	float4 _tc1DeqScaleBias;
};

VERUS_UBUFFER UB_Skinning
{
	mataff _vMatBones[VERUS_MAX_NUM_BONES];
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	float4 _userColor;
};
