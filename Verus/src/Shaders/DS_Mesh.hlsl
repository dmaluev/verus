#include "Lib.hlsl"
#include "LibVertex.hlsl"
#include "DS_Mesh.inc.hlsl"

ConstantBuffer<UB_PerFrame>    perFrame    : register(b0, space0);
ConstantBuffer<UB_PerMaterial> perMaterial : register(b0, space1);
ConstantBuffer<UB_PerMesh>     perMesh     : register(b0, space2);
VK_PUSH_CONSTANT
ConstantBuffer<UB_PerObject>   perObject   : register(b0, space3);

struct VSI
{
	// Binding 0:
	VK_LOCATION_POSITION int4 pos   : POSITION;
	VK_LOCATION_NORMAL   float3 nrm : NORMAL;
	VK_LOCATION(8)       int4 tc0   : TEXCOORD0;
	// Binding 1:
#ifdef DEF_SKELETON
	VK_LOCATION(12)      int4 bw : TEXCOORD4;
	VK_LOCATION(13)      int4 bi : TEXCOORD5;
#endif
	// Binding 2:
	VK_LOCATION(14)      float4 tan : TEXCOORD6;
	VK_LOCATION(15)      float4 bin : TEXCOORD7;
	// Binding 3:
#if 0
	VK_LOCATION(9)       int4 tc1     : TEXCOORD1;
	VK_LOCATION_COLOR0   float4 color : COLOR0;
#endif
};

struct VSO
{
	float4 pos : SV_Position;
	float4 color : COLOR0;
};

struct FSO
{
	float4 color : SV_Target0;
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

#ifdef DEF_INSTANCED
#else
	const mataff matW = perObject._matW;
	const float4 userColor = perObject._userColor;
#endif

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, perMesh._posDeqScale.xyz, perMesh._posDeqBias.xyz);
	const float2 intactTc0 = DequantizeUsingDeq2D(si.tc0.xy, perMesh._tc0DeqScaleBias.xy, perMesh._tc0DeqScaleBias.zw);

	const float3 pos = intactPos;

	float3 posW;
	{
#ifdef DEF_SKELETON
#elif DEF_SKELETON_RIGID
#else
		posW = mul(float4(pos, 1), matW).xyz;
#endif
	}

	so.pos = mul(float4(posW, 1), perFrame._matVP);
	so.color = float4(intactTc0, 0, 1);

	return so;
}
#endif

#ifdef _FS
FSO mainFS(VSO si)
{
	FSO so;

	so.color = si.color;

	return so;
}
#endif

//@main:T
