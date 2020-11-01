// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.

#define _PARTITION_METHOD "fractional_even"

// Hull shader's output, also domain shader's input.
// Includes vertex position in world-view space and 2 bezier control points in same space.
// If all points are not visible, then clipped will be 1:
#define _HSO_STRUCT struct HSO { \
	float3 posWV[3]              : POSITIONS; \
	float oppositeEdgeTessFactor : TESSFACTOR; \
	float clipped                : CLIPPED; \
	VSO vso; }

// Patch constant function's output, also domain shader's input:
struct PCFO
{
	float tessFactors[3]   : SV_TessFactor;
	float insideTessFactor : SV_InsideTessFactor;
	float3 b111            : B111; // Bezier control point in the middle of a patch.
};

#define _HS_COPY(a) so.vso.a = inputPatch[id].a

#define _IN_DS const OutputPatch<HSO, 3> outputPatch, PCFO si, float3 domainLocation : SV_DomainLocation

// Domain shader's copy is actually an interpolation:
#define _DS_LERP(a) domainLocation.x * outputPatch[0].a + domainLocation.y * outputPatch[1].a + domainLocation.z * outputPatch[2].a
#define _DS_COPY(a) so.a = _DS_LERP(vso.a)

#define _HS_PN_BODY(nrmWV, matP, viewportSize) \
	const uint thisID = id; \
	const uint nextID = (thisID < 2) ? thisID + 1 : 0; \
	so.posWV[0] = inputPatch[thisID].pos.xyz; \
	so.posWV[1] = ComputeBezierPoint(inputPatch[thisID].pos.xyz, inputPatch[nextID].pos.xyz, inputPatch[thisID].nrmWV.xyz); \
	so.posWV[2] = ComputeBezierPoint(inputPatch[nextID].pos.xyz, inputPatch[thisID].pos.xyz, inputPatch[nextID].nrmWV.xyz); \
	so.clipped = ComputeClipping(matP, so.posWV[0], so.posWV[1], so.posWV[2]); \
	so.oppositeEdgeTessFactor = ComputeEdgeTessFactor(matP, inputPatch[thisID].pos.xyz, inputPatch[nextID].pos.xyz, viewportSize.xy); \
	const float3 normal = (inputPatch[thisID].nrmWV.xyz + inputPatch[nextID].nrmWV.xyz) * 0.5; \
	so.oppositeEdgeTessFactor = (normal.z < -0.75) ? 0.0 : max(1.0, so.oppositeEdgeTessFactor * (1.0 - abs(normal.z * 0.75)));

#define _HS_PNAEN_BODY(nrmWV, matP, viewportSize) \
	const uint thisID = id; \
	const uint nextID = (thisID < 2) ? thisID + 1 : 0; \
	const uint thisEX = 3 + 2 * id; \
	const uint nextEX = thisEX + 1; \
	const float kA = max(_SINGULARITY_FIX, 1.0 - dot(inputPatch[thisID].nrmWV.xyz, inputPatch[thisEX].nrmWV.xyz)); \
	const float kB = max(_SINGULARITY_FIX, 1.0 - dot(inputPatch[nextID].nrmWV.xyz, inputPatch[nextEX].nrmWV.xyz)); \
	const float sum = kA + kB; \
	const float ratio = kB / sum; \
	float3 myCP, exCP; \
	so.posWV[0] = inputPatch[thisID].pos.xyz; \
	myCP = ComputeBezierPoint(inputPatch[thisID].pos.xyz, inputPatch[nextID].pos.xyz, inputPatch[thisID].nrmWV.xyz); \
	exCP = ComputeBezierPoint(inputPatch[thisEX].pos.xyz, inputPatch[nextEX].pos.xyz, inputPatch[thisEX].nrmWV.xyz); \
	so.posWV[1] = lerp(myCP, exCP, ratio); \
	myCP = ComputeBezierPoint(inputPatch[nextID].pos.xyz, inputPatch[thisID].pos.xyz, inputPatch[nextID].nrmWV.xyz); \
	exCP = ComputeBezierPoint(inputPatch[nextEX].pos.xyz, inputPatch[thisEX].pos.xyz, inputPatch[nextEX].nrmWV.xyz); \
	so.posWV[2] = lerp(myCP, exCP, ratio); \
	so.clipped = ComputeClipping(matP, so.posWV[0], so.posWV[1], so.posWV[2]); \
	so.oppositeEdgeTessFactor = ComputeEdgeTessFactor(matP, inputPatch[thisID].pos.xyz, inputPatch[nextID].pos.xyz, viewportSize.xy); \
	const float3 normal = (inputPatch[thisID].nrmWV.xyz + inputPatch[thisEX].nrmWV.xyz + inputPatch[nextID].nrmWV.xyz + inputPatch[nextEX].nrmWV.xyz) * 0.25; \
	so.oppositeEdgeTessFactor = (normal.z < -0.75) ? 0.0 : max(1.0, so.oppositeEdgeTessFactor * (1.0 - abs(normal.z * 0.75)));

// Patch Constant Function, here central bezier control point is calculated:
#define _HS_PCF_BODY(matP) \
	so.tessFactors[0] = outputPatch[1].oppositeEdgeTessFactor; \
	so.tessFactors[1] = outputPatch[2].oppositeEdgeTessFactor; \
	so.tessFactors[2] = outputPatch[0].oppositeEdgeTessFactor; \
	so.insideTessFactor = max(max(so.tessFactors[0], so.tessFactors[1]), so.tessFactors[2]); \
	const float3 b300 = outputPatch[0].posWV[0]; \
	const float3 b210 = outputPatch[0].posWV[1]; \
	const float3 b120 = outputPatch[0].posWV[2]; \
	const float3 b030 = outputPatch[1].posWV[0]; \
	const float3 b021 = outputPatch[1].posWV[1]; \
	const float3 b012 = outputPatch[1].posWV[2]; \
	const float3 b003 = outputPatch[2].posWV[0]; \
	const float3 b102 = outputPatch[2].posWV[1]; \
	const float3 b201 = outputPatch[2].posWV[2]; \
	const float3 e = (b210 + b120 + b021 + b012 + b102 + b201) * (1.0 / 6.0); \
	const float3 v = (b003 + b030 + b300) * (1.0 / 3.0); \
	so.b111 = e + (e - v) * 0.5; \
	const float b111Clipped = IsClipped(ApplyProjection(so.b111, matP)); \
	if (outputPatch[0].clipped && outputPatch[1].clipped && outputPatch[2].clipped && b111Clipped) { so.tessFactors[0] = 0.0; }

#define _DS_INIT_FLAT_POS \
	const float3 flatPosWV = \
	outputPatch[0].posWV[0] * domainLocation.x + \
	outputPatch[1].posWV[0] * domainLocation.y + \
	outputPatch[2].posWV[0] * domainLocation.z

#define _DS_INIT_SMOOTH_POS \
	const float3 uvw = domainLocation; \
	const float3 uvwSq = uvw * uvw; \
	const float3 uvwSq3 = uvwSq * 3.0; \
	const float3 smoothPosWV = \
	outputPatch[0].posWV[0] * uvwSq.x * uvw.x + \
	outputPatch[1].posWV[0] * uvwSq.y * uvw.y + \
	outputPatch[2].posWV[0] * uvwSq.z * uvw.z + \
	outputPatch[0].posWV[1] * uvwSq3.x * uvw.y + \
	outputPatch[0].posWV[2] * uvwSq3.y * uvw.x + \
	outputPatch[1].posWV[1] * uvwSq3.y * uvw.z + \
	outputPatch[1].posWV[2] * uvwSq3.z * uvw.y + \
	outputPatch[2].posWV[1] * uvwSq3.z * uvw.x + \
	outputPatch[2].posWV[2] * uvwSq3.x * uvw.z + \
	si.b111 * uvw.x * uvw.y * uvw.z * 6.0

float3 ComputeBezierPoint(float3 posA, float3 posB, float3 nrmA)
{
	// Project a 1/3 midpoint on the plane, which is defined by the nearest vertex and it's normal.
	const float extrudeLen = dot(posB - posA, nrmA);
	return (2.0 * posA + posB - extrudeLen * nrmA) * (1.0 / 3.0);
}

// Optimized version of the projection transform:
float4 ApplyProjection(float3 posWV, matrix matP)
{
	return mul(float4(posWV, 1), matP);
	//float4 clipSpacePos;
	//clipSpacePos[0] = matP[0][0] * posWV[0];
	//clipSpacePos[1] = matP[1][1] * posWV[1];
	//clipSpacePos[2] = matP[2][2] * posWV[2] + matP[3][2];
	//clipSpacePos[3] = -posWV[2];
	//return clipSpacePos;
}

float2 ProjectAndScale(float3 posWV, matrix matP, float2 viewportSize)
{
	const float4 clipSpacePos = ApplyProjection(posWV, matP);
	const float2 ndcPos = clipSpacePos.xy / clipSpacePos.w;
	return ndcPos * viewportSize * (32.0 / 1080.0);
}

float IsClipped(float4 clipSpacePos)
{
	clipSpacePos.w *= 2.0; // Safe area, because patch can have all points outside, but still be visible.
	return (all(clipSpacePos.xyz >= -clipSpacePos.w) && all(clipSpacePos.xyz <= clipSpacePos.w)) ? 0.0 : 1.0;
}

float ComputeClipping(matrix matP, float3 posA, float3 posB, float3 posC)
{
	const float4 clipSpacePosA = ApplyProjection(posA, matP);
	const float4 clipSpacePosB = ApplyProjection(posB, matP);
	const float4 clipSpacePosC = ApplyProjection(posC, matP);
	return min(min(IsClipped(clipSpacePosA), IsClipped(clipSpacePosB)), IsClipped(clipSpacePosC));
}

float ComputeEdgeTessFactor(matrix matP, float3 posA, float3 posB, float2 viewportSize)
{
	const float2 scaledNdcPosA = ProjectAndScale(posA, matP, viewportSize).xy;
	const float2 scaledNdcPosB = ProjectAndScale(posB, matP, viewportSize).xy;
	const float tessFactor = distance(scaledNdcPosA, scaledNdcPosB);
	return tessFactor;
}

float4 MulTessPos(float4 posW, mataff matV, matrix matVP)
{
#ifdef DEF_TESS
	return float4(mul(posW, matV), 1);
#else
	return mul(posW, matVP);
#endif
}
