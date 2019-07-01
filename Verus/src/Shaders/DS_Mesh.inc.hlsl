struct UB_PerFrame
{
	mataff _matV;
	matrix _matP;
	matrix _matVP;
};

struct UB_PerMaterial
{
	float4 lsb_gloss_lp;
};

struct UB_PerMesh
{
	float4 _posDeqScale;
	float4 _posDeqBias;
	float4 _tc0DeqScaleBias;
	float4 _tc1DeqScaleBias;
};

struct UB_PerObject
{
	mataff _matW;
	float4 _userColor;
};
