#include "Lib.hlsl"

struct VSI
{
	VK_LOCATION_POSITION int4 pos : POSITION;
	VK_LOCATION(16)      int4 tan : TEXCOORD8;
	VK_LOCATION(17)      int4 bin : TEXCOORD9;
};

struct VSO
{
	float4 pos : SV_Position;
	float3 nrm : NORMAL;
	float4 tan : TEXCOORD6;
	float4 bin : TEXCOORD7;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float2 posBlendFix = si.pos.yw;
	si.pos.yw = 0.0;
	const float3 pos = si.pos.xyz;
	const float3 posW = mul(float4(pos, 1), g_matW).xyz;

	so.pos = float4(0, 0, 0, 1);

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = float4(1, 0, 0, 1);

	return so;
}
#endif

//@main:T
