#include "Lib.hlsl"
#include "LibColor.hlsl"
#include "LibDeferredShading.hlsl"
#include "LibLighting.hlsl"
#include "LibVertex.hlsl"
#include "DS.inc.hlsl"

ConstantBuffer<UB_PerFrame>  g_perFrame  : register(b0, space0);
ConstantBuffer<UB_PerMesh>   g_perMesh   : register(b0, space2);
VK_PUSH_CONSTANT
ConstantBuffer<UB_PerObject> g_perObject : register(b0, space4);

Texture2D    g_texGBuffer0 : register(t1, space0);
SamplerState g_samGBuffer0 : register(s1, space0);
Texture2D    g_texGBuffer1 : register(t2, space0);
SamplerState g_samGBuffer1 : register(s2, space0);
Texture2D    g_texGBuffer2 : register(t3, space0);
SamplerState g_samGBuffer2 : register(s3, space0);
Texture2D    g_texGBuffer3 : register(t4, space0);
SamplerState g_samGBuffer3 : register(s4, space0);

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

	_PER_INSTANCE_DATA
};

struct VSO
{
	float4 pos							/**/ : SV_Position;
	float4 posFS						/**/ : TEXCOORD0;
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	float3 radius_radiusSq_invRadiusSq	/**/ : TEXCOORD1;
	float3 lightPosWV					/**/ : TEXCOORD2;
#endif
#if defined(DEF_DIR) || defined(DEF_SPOT)
	float4 lightDirWV_invConeDelta		/**/ : TEXCOORD3;
#endif
	float4 color_coneOut				/**/ : TEXCOORD4;
#ifdef DEF_DIR
	float3 dirZenithWV					/**/ : TEXCOORD5;
#endif
};

#ifdef _VS
VSO mainVS(VSI si)
{
	VSO so;

	const float3 intactPos = DequantizeUsingDeq3D(si.pos.xyz, g_perMesh._posDeqScale.xyz, g_perMesh._posDeqBias.xyz);

	// World matrix, instance data:
#ifdef DEF_INSTANCED
	const mataff matW = GetInstMatrix(
		si.matPart0,
		si.matPart1,
		si.matPart2);
	const float3 color = si.instData.rgb;
	const float coneIn = si.instData.a;
#else
	const mataff matW = g_perObject._matW;
	const float3 color = g_perObject.rgb;
	const float coneIn = g_perObject.a;
#endif

#ifdef DEF_DIR
	const float glossScale = 0.5;
#else
	const float glossScale = 1.0;
#endif

	const matrix matWV = mul(ToFloat4x4(matW), ToFloat4x4(g_perFrame._matV));

	const float3x3 matW33 = (float3x3)matW;
	const float3x3 matV33 = (float3x3)g_perFrame._matV;
	const float3x3 matWV33 = (float3x3)matWV;

#ifdef DEF_DIR
	so.pos = float4(intactPos, 1);
#else
	const float3 posW = mul(float4(intactPos, 1), matW);
	so.pos = mul(float4(posW, 1), g_perFrame._matVP);
#endif
	so.posFS = so.pos;

	// <MoreLightParams>
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float3 posUnit = mul(float3(0, 0, 1), matW33); // Need to know the scale.
	so.radius_radiusSq_invRadiusSq.y = dot(posUnit, posUnit);
	so.radius_radiusSq_invRadiusSq.x = sqrt(so.radius_radiusSq_invRadiusSq.y);
	so.radius_radiusSq_invRadiusSq.z = 1.0 / so.radius_radiusSq_invRadiusSq.y;
	const float4 posOrigin = float4(0, 0, 0, 1);
	so.lightPosWV = mul(posOrigin, matWV).xyz;
#endif
	so.color_coneOut = float4(color, glossScale);
#ifdef DEF_DIR
	const float3 posUnit = mul(float3(0, 0, 1), matWV33); // Assume no scale in matWV33.
	so.lightDirWV_invConeDelta = float4(posUnit, 1);
	so.dirZenithWV = mul(float3(0, 1, 0), matV33);
#elif DEF_SPOT
	so.lightDirWV_invConeDelta.xyz = normalize(mul(float3(0, 0, 1), matWV33));
	const float3 posCone = mul(float3(0, 1, 1), matW33);
	const float3 dirCone = normalize(posCone);
	const float3 dirUnit = normalize(posUnit);
	const float coneOut = dot(dirUnit, dirCone);
	const float invConeDelta = 1.0 / (coneIn - coneOut);
	so.lightDirWV_invConeDelta.w = invConeDelta;
	so.color_coneOut.a = coneOut;
#endif
	// </MoreLightParams>

	return so;
}
#endif

#ifdef _FS
DS_ACC_FSO mainFS(VSO si)
{
	DS_ACC_FSO so;

	// Clip-space position:
#ifdef DEF_DIR
	const float3 posCS = si.posFS.xyz;
#else
	const float3 posCS = si.posFS.xyz / si.posFS.w;
#endif

	// GBuffer tex. coord. from posCS:
	const float2 tc0 = posCS.xy*g_perFrame._toUV.xy + g_perFrame._toUV.zw;

	// For Omni & Spot: light's radius and position:
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	const float radius = si.radius_radiusSq_invRadiusSq.x;
	const float radiusSq = si.radius_radiusSq_invRadiusSq.y;
	const float invRadiusSq = si.radius_radiusSq_invRadiusSq.z;
	const float3 lightPosWV = si.lightPosWV;
#endif

	// For Dir & Spot: light's direction & cone:
#ifdef DEF_DIR
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
	const float3 dirZenithWV = si.dirZenithWV;
#elif DEF_SPOT
	const float3 lightDirWV = si.lightDirWV_invConeDelta.xyz;
	const float invConeDelta = si.lightDirWV_invConeDelta.w;
	const float coneOut = si.color_coneOut.a;
#endif

#ifdef DEF_SPOT
	const float glossScale = 0.5;
#else
	const float glossScale = si.color_coneOut.a;
#endif

	// GBuffer1:
	const float rawGBuffer1 = g_texGBuffer1.SampleLevel(g_samGBuffer1, tc0, 0).r;
	const float3 posWV = DS_GetPosition(rawGBuffer1, g_perFrame._matInvP, posCS.xy);

	so.target0 = 0.0;
	so.target1 = 0.0;

#if defined(DEF_OMNI) || defined(DEF_SPOT)
	if (lightPosWV.z - posWV.z + radius >= 0.0) // Optimize finite volume lights.
#endif
	{
		// Light's diffuse & specular color:
		const float3 colorDiff = si.color_coneOut.rgb;
		const float3 colorSpec = saturate(colorDiff + dot(colorDiff, 0.1));

		const float4 rawGBuffer0 = g_texGBuffer0.SampleLevel(g_samGBuffer0, tc0, 0);
		const float3 dirToEyeWV = normalize(-posWV);

		// GBuffer2:
		const float4 rawGBuffer2 = g_texGBuffer2.SampleLevel(g_samGBuffer2, tc0, 0);
		const float3 normalWV = DS_GetNormal(rawGBuffer2);
		const float2 emission = DS_GetEmission(rawGBuffer2);
		const float2 metal = DS_GetMetallicity(rawGBuffer2);

		// GBuffer3:
		const float4 rawGBuffer3 = g_texGBuffer3.SampleLevel(g_samGBuffer3, tc0, 0);
		const float3 anisoWV = DS_GetAnisoSpec(rawGBuffer3);
		const float2 lamScaleBias = DS_GetLamScaleBias(rawGBuffer3);
		const float spec = rawGBuffer3.b;
		const float gloss64 = rawGBuffer3.a*64.0;
		const float gloss64Scaled = gloss64 * glossScale;

		// Special:
		const float skinAlpha = emission.y;
		const float hairAlpha = metal.y;
		const float eyeAlpha = saturate(1.0 - gloss64);
		const float rim = pow(1.0 - normalWV.z, 2.0)*(0.05 + 0.2*skinAlpha);
		const float glossEye = 25.0*glossScale;

		const float gloss = lerp(gloss64Scaled*gloss64Scaled*gloss64Scaled, glossEye*glossEye*glossEye, eyeAlpha);

#ifdef DEF_DIR
		const float3 dirToLightWV = -lightDirWV;
		const float lightFalloff = 1.0;

		const float3 reflectWV = reflect(-dirToEyeWV, normalWV);
		const float zenith = dot(dirZenithWV, reflectWV);
		const float zenithMask = saturate(zenith*gloss64Scaled*0.15);
		const float3 skyColor = 1.0;
#else
		const float3 toLightWV = lightPosWV - posWV;
		const float3 dirToLightWV = normalize(toLightWV);
		const float distToLightSq = dot(toLightWV, toLightWV);
		const float lightFalloff = ComputePointLightIntensity(distToLightSq, radiusSq, invRadiusSq);
#endif
#ifdef DEF_SPOT // Extra step for spot light:
		const float coneIntensity = ComputeSpotLightConeIntensity(dirToLightWV, lightDirWV, coneOut, invConeDelta);
#else
		const float coneIntensity = 1.0;
#endif

		const float4 litRet = TrueLit(dirToLightWV, normalWV, dirToEyeWV,
			lerp(gloss*(2.0 - lightFalloff), 12.0, hairAlpha),
			lerp(lamScaleBias, float2(1, 0.4), hairAlpha),
			float4(anisoWV, hairAlpha));

		// Shadow:
#ifdef DEF_DIR
		const float shadowAlpha = 1.0;
#else
		const float shadowAlpha = 1.0;
#endif

		const float intensityDiff = lightFalloff * coneIntensity*shadowAlpha;
		const float intensitySpec = saturate(intensityDiff*2.0);
		const float maxSpecAdd = lerp(0.2*spec, 0.1 + spec * 0.5, metal.x);

		// Subsurface scattering effect for front & back faces:
		const float3 colorSkinSSS_Face = float3(1.6, 1.2, 0.3);
		const float3 colorSkinSSS_Back = float3(1.6, 0.8, 0.5);
		const float3 colorDiffSSS_Face = lerp(colorDiff, colorDiff*colorSkinSSS_Face, skinAlpha*pow(1.0 - litRet.y, 2.0));
		const float3 colorDiffSSS_Back = lerp(colorDiffSSS_Face, colorDiff*colorSkinSSS_Back, skinAlpha*litRet.x);

		const float grey = Greyscale(rawGBuffer0.rgb);
		const float3 metalSpec = saturate(rawGBuffer0.rgb / (grey + _SINGULARITY_FIX));

		const float3 matSpec = lerp(float3(1, 1, 1), metalSpec, saturate(metal.x + (hairAlpha - eyeAlpha)*0.4))*FresnelSchlick(spec, maxSpecAdd, litRet.w);
		const float3 maxDiff = colorDiffSSS_Back * intensityDiff;
		const float3 maxSpec = matSpec * colorSpec + float3(-0.03, 0.0, 0.05)*skinAlpha;
#ifdef DEF_DIR
		const float skyIntensity = 1.0;
		const float3 skySpec = skyColor * matSpec*skyIntensity;
#endif

		// Very bright light:
		const float3 overbright = saturate(dot(maxDiff, 1.0 / 3.0) - 1.0); // Intensity above one.
		const float3 diffBoost = overbright * maxDiff*litRet.y*(0.25 + rawGBuffer0.rgb);

#ifdef DEF_DIR
		so.target0.rgb = maxDiff * litRet.y; // Lambert's cosine law.
		so.target1.rgb = maxSpec * saturate(litRet.z*intensitySpec + rim) + diffBoost + skySpec;
#else
		so.target0.rgb = maxDiff * litRet.y; // Lambert's cosine law.
		so.target1.rgb = maxSpec * saturate(litRet.z*intensitySpec) + diffBoost;
#endif
		so.target0.rgb = max(so.target0.rgb, emission.x*GetEmissionScale());
		so.target1.rgb = min(so.target1.rgb, 1.0 - emission.x*GetEmissionScale());
		so.target0.a = 1.0;
		so.target1.a = 1.0;
	}
#if defined(DEF_OMNI) || defined(DEF_SPOT)
	else
	{
		clip(-1.0);
	}
#endif

	return so;
}
#endif

//@main:TInstancedDir INSTANCED DIR
//@main:TInstancedOmni INSTANCED OMNI
//@main:TInstancedSpot INSTANCED SPOT
