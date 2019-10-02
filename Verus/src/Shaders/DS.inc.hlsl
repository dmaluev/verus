VERUS_UBUFFER UB_PerFrame
{
	mataff _matQuad;
	mataff _matToUV;
	mataff _matV;
	matrix _matVP;
	matrix _matInvP;
	float4 _toUV;
};

VERUS_UBUFFER UB_PerMesh
{
	float4 _posDeqScale;
	float4 _posDeqBias;
};

VERUS_UBUFFER UB_PerObject
{
	mataff _matW;
	float4 _color;
};
