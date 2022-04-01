// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwns<Mesh::SHADER_COUNT> Mesh::s_shader;
CGI::PipelinePwns<Mesh::PIPE_COUNT> Mesh::s_pipe;

Mesh::UB_PerFrame            Mesh::s_ubPerFrame;
Mesh::UB_PerMaterialFS       Mesh::s_ubPerMaterialFS;
Mesh::UB_PerMeshVS           Mesh::s_ubPerMeshVS;
Mesh::UB_SkeletonVS          Mesh::s_ubSkeletonVS;
Mesh::UB_PerObject           Mesh::s_ubPerObject;
Mesh::UB_SimplePerFrame      Mesh::s_ubSimplePerFrame;
Mesh::UB_SimplePerMaterialFS Mesh::s_ubSimplePerMaterialFS;
Mesh::UB_SimplePerMeshVS     Mesh::s_ubSimplePerMeshVS;
Mesh::UB_SimpleSkeletonVS    Mesh::s_ubSimpleSkeletonVS;
Mesh::UB_SimplePerObject     Mesh::s_ubSimplePerObject;

// Mesh:

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
	Done();
}

void Mesh::InitStatic()
{
	VERUS_QREF_CONST_SETTINGS;

	s_shader[SHADER_MAIN].Init("[Shaders]:DS_Mesh.hlsl");
	s_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame), settings.GetLimits()._mesh_ubPerFrameCapacity, {}, CGI::ShaderStageFlags::vs_hs_ds_fs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubPerMaterialFS, sizeof(s_ubPerMaterialFS), settings.GetLimits()._mesh_ubPerMaterialFSCapacity,
		{
			CGI::Sampler::aniso, // A
			CGI::Sampler::aniso, // N
			CGI::Sampler::aniso, // X
			CGI::Sampler::aniso, // Detail
			CGI::Sampler::aniso, // DetailN
			CGI::Sampler::lodBias // Strass
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(2, &s_ubPerMeshVS, sizeof(s_ubPerMeshVS), settings.GetLimits()._mesh_ubPerMeshVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(3, &s_ubSkeletonVS, sizeof(s_ubSkeletonVS), settings.GetLimits()._mesh_ubSkinningVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(4, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	s_shader[SHADER_MAIN]->CreatePipelineLayout();

	s_shader[SHADER_SIMPLE].Init("[Shaders]:SimpleMesh.hlsl");
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(0, &s_ubSimplePerFrame, sizeof(s_ubSimplePerFrame), settings.GetLimits()._mesh_ubPerFrameCapacity);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(1, &s_ubSimplePerMaterialFS, sizeof(s_ubSimplePerMaterialFS), settings.GetLimits()._mesh_ubPerMaterialFSCapacity,
		{
			CGI::Sampler::linearMipN,
			CGI::Sampler::linearMipN,
			CGI::Sampler::shadow
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(2, &s_ubSimplePerMeshVS, sizeof(s_ubSimplePerMeshVS), settings.GetLimits()._mesh_ubPerMeshVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(3, &s_ubSimpleSkeletonVS, sizeof(s_ubSimpleSkeletonVS), settings.GetLimits()._mesh_ubSkinningVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(4, &s_ubSimplePerObject, sizeof(s_ubSimplePerObject), 0);
	s_shader[SHADER_SIMPLE]->CreatePipelineLayout();
}

void Mesh::DoneStatic()
{
	s_pipe.Done();
	s_shader.Done();
}

void Mesh::Init(RcDesc desc)
{
	VERUS_RT_ASSERT(desc._url);

	if (desc._warpUrl)
	{
		_warpUrl = desc._warpUrl;
		Str::ReplaceExtension(_warpUrl, ".xwx");
	}

	_instanceCapacity = desc._instanceCapacity;
	_initShape = desc._initShape;

	BaseMesh::Init(desc._url);
}

void Mesh::Done()
{
	VERUS_DONE(Mesh);
}

void Mesh::Draw(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb)
{
	auto shader = GetShader();

	if (drawDesc._bindPipeline)
	{
		if (drawDesc._pipe != PIPE_COUNT)
			BindPipeline(drawDesc._pipe, cb);
		else
			BindPipeline(cb, drawDesc._allowTess); // Auto.
	}
	BindGeo(cb);

	// Set 0:
	UpdateUniformBufferPerFrame();
	if (drawDesc._pOverrideFogColor)
		s_ubSimplePerFrame._fogColor = drawDesc._pOverrideFogColor->GLM();
	cb->BindDescriptors(shader, 0);

	// Set 1:
	if (drawDesc._bindMaterial)
	{
		// Material buffer should already be updated. For example call Material::UpdateMeshUniformBuffer.
		cb->BindDescriptors(shader, 1, drawDesc._cshMaterial);
	}

	// Set 2:
	UpdateUniformBufferPerMeshVS();
	cb->BindDescriptors(shader, 2);

	// Set 3:
	if (drawDesc._bindSkeleton && _skeleton.IsInitialized())
	{
		UpdateUniformBufferSkeletonVS();
		cb->BindDescriptors(shader, 3);
	}

	// Set 4:
	UpdateUniformBufferPerObject(drawDesc._matW, drawDesc._userColor);
	cb->BindDescriptors(shader, 4);

	cb->DrawIndexed(GetIndexCount());
}

void Mesh::DrawSimple(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb)
{
	DrawSimpleMode mode = DrawSimpleMode::envMap;
	if (drawDesc._pipe >= PIPE_SIMPLE_PLANAR_REF && drawDesc._pipe <= PIPE_SIMPLE_PLANAR_REF_SKINNED)
		mode = DrawSimpleMode::planarReflection;

	auto shader = GetSimpleShader();

	if (drawDesc._bindPipeline)
		BindPipeline(drawDesc._pipe, cb);
	BindGeo(cb);

	// Set 0:
	UpdateUniformBufferSimplePerFrame(mode);
	if (drawDesc._pOverrideFogColor)
		s_ubSimplePerFrame._fogColor = drawDesc._pOverrideFogColor->GLM();
	cb->BindDescriptors(shader, 0);

	// Set 1:
	if (drawDesc._bindMaterial)
	{
		// Material buffer should already be updated. For example call Material::UpdateMeshUniformBuffer.
		cb->BindDescriptors(shader, 1, drawDesc._cshMaterial);
	}

	// Set 2:
	UpdateUniformBufferPerMeshVS();
	cb->BindDescriptors(shader, 2);

	// Set 3:
	if (drawDesc._bindSkeleton && _skeleton.IsInitialized())
	{
		UpdateUniformBufferSimpleSkeletonVS();
		cb->BindDescriptors(shader, 3);
	}

	// Set 4:
	UpdateUniformBufferPerObject(drawDesc._matW, drawDesc._userColor);
	cb->BindDescriptors(shader, 4);

	cb->DrawIndexed(GetIndexCount());
}

void Mesh::BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb)
{
	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WATER;

	if (!settings._gpuTessellation) // Fallback:
	{
		switch (pipe)
		{
		case PIPE_TESS:           pipe = PIPE_MAIN; break;
		case PIPE_TESS_INSTANCED: pipe = PIPE_INSTANCED; break;
		case PIPE_TESS_ROBOTIC:   pipe = PIPE_ROBOTIC; break;
		case PIPE_TESS_SKINNED:   pipe = PIPE_SKINNED; break;
		case PIPE_TESS_PLANT:     pipe = PIPE_PLANT; break;

		case PIPE_DEPTH_TESS:           pipe = PIPE_DEPTH; break;
		case PIPE_DEPTH_TESS_INSTANCED: pipe = PIPE_DEPTH_INSTANCED; break;
		case PIPE_DEPTH_TESS_ROBOTIC:   pipe = PIPE_DEPTH_ROBOTIC; break;
		case PIPE_DEPTH_TESS_SKINNED:   pipe = PIPE_DEPTH_SKINNED; break;
		case PIPE_DEPTH_TESS_PLANT:     pipe = PIPE_DEPTH_PLANT; break;
		};
	}

	if (!s_pipe[pipe])
	{
		static CSZ branches[] =
		{
			"#",
			"#Instanced",
			"#Robotic",
			"#Skinned",
			"#Plant",

			"#Tess",
			"#TessInstanced",
			"#TessRobotic",
			"#TessSkinned",
			"#TessPlant",

			"#Depth",
			"#DepthInstanced",
			"#DepthRobotic",
			"#DepthSkinned",
			"#DepthPlant",

			"#DepthTess",
			"#DepthTessInstanced",
			"#DepthTessRobotic",
			"#DepthTessSkinned",
			"#DepthTessPlant",

			"#SolidColor",
			"#SolidColorInstanced",
			"#SolidColorRobotic",
			"#SolidColorSkinned",

			// Simple:

			"#",
			"#Instanced",
			"#Robotic",
			"#Skinned",

			"#",
			"#Instanced",
			"#Robotic",
			"#Skinned",

			"#Texture",
			"#TextureInstanced",
			"#TextureRobotic",
			"#TextureSkinned"
		};
		VERUS_CT_ASSERT(VERUS_COUNT_OF(branches) == PIPE_COUNT);

		if (pipe >= PIPE_SIMPLE_TEX_ADD)
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], branches[pipe], renderer.GetDS().GetRenderPassHandle_ForwardRendering());
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
			pipeDesc._vertexInputBindingsFilter = _bindingsMask;
			pipeDesc._depthWriteEnable = false;
			s_pipe[pipe].Init(pipeDesc);
		}
		else if (pipe >= PIPE_SIMPLE_PLANAR_REF)
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], branches[pipe], water.GetRenderPassHandle());
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
			pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
			pipeDesc._vertexInputBindingsFilter = _bindingsMask;
			s_pipe[pipe].Init(pipeDesc);
		}
		else if (pipe >= PIPE_SIMPLE_ENV_MAP)
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], branches[pipe], atmo.GetCubeMapBaker().GetRenderPassHandle());
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
			pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
			pipeDesc._vertexInputBindingsFilter = _bindingsMask;
			s_pipe[pipe].Init(pipeDesc);
		}
		else
		{
			CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], branches[pipe], renderer.GetDS().GetRenderPassHandle());

			auto SetBlendEqsForDS = [&pipeDesc]()
			{
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
			};

			switch (pipe)
			{
			case PIPE_MAIN:
			case PIPE_INSTANCED:
			case PIPE_ROBOTIC:
			case PIPE_SKINNED:
			case PIPE_PLANT:
			{
				SetBlendEqsForDS();
			}
			break;
			case PIPE_TESS:
			case PIPE_TESS_INSTANCED:
			case PIPE_TESS_ROBOTIC:
			case PIPE_TESS_SKINNED:
			case PIPE_TESS_PLANT:
			{
				SetBlendEqsForDS();
				if (settings._gpuTessellation)
					pipeDesc._topology = CGI::PrimitiveTopology::patchList3;
			}
			break;
			case PIPE_DEPTH:
			case PIPE_DEPTH_INSTANCED:
			case PIPE_DEPTH_ROBOTIC:
			case PIPE_DEPTH_SKINNED:
			case PIPE_DEPTH_PLANT:
			{
				pipeDesc._colorAttachBlendEqs[0] = "";
				pipeDesc._renderPassHandle = atmo.GetShadowMapBaker().GetRenderPassHandle();
			}
			break;
			case PIPE_DEPTH_TESS:
			case PIPE_DEPTH_TESS_INSTANCED:
			case PIPE_DEPTH_TESS_ROBOTIC:
			case PIPE_DEPTH_TESS_SKINNED:
			case PIPE_DEPTH_TESS_PLANT:
			{
				pipeDesc._colorAttachBlendEqs[0] = "";
				pipeDesc._renderPassHandle = atmo.GetShadowMapBaker().GetRenderPassHandle();
				if (settings._gpuTessellation)
					pipeDesc._topology = CGI::PrimitiveTopology::patchList3;
			}
			break;
			case PIPE_WIREFRAME:
			case PIPE_WIREFRAME_INSTANCED:
			case PIPE_WIREFRAME_ROBOTIC:
			case PIPE_WIREFRAME_SKINNED:
			{
				SetBlendEqsForDS();
				pipeDesc._rasterizationState._polygonMode = CGI::PolygonMode::line;
				pipeDesc._rasterizationState._depthBiasEnable = true;
				pipeDesc._rasterizationState._depthBiasConstantFactor = -1000;
				pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
			}
			break;
			default:
			{
				SetBlendEqsForDS();
			}
			}
			pipeDesc._vertexInputBindingsFilter = _bindingsMask;
			s_pipe[pipe].Init(pipeDesc);
		}
	}
	cb->BindPipeline(s_pipe[pipe]);
}

void Mesh::BindPipeline(CGI::CommandBufferPtr cb, bool allowTess)
{
	VERUS_QREF_ATMO;

	static PIPE pipes[] =
	{
		PIPE_MAIN,
		PIPE_INSTANCED,
		PIPE_ROBOTIC,
		PIPE_SKINNED
	};
	static PIPE tessPipes[] =
	{
		PIPE_TESS,
		PIPE_TESS_INSTANCED,
		PIPE_TESS_ROBOTIC,
		PIPE_TESS_SKINNED
	};
	static PIPE depthPipes[] =
	{
		PIPE_DEPTH,
		PIPE_DEPTH_INSTANCED,
		PIPE_DEPTH_ROBOTIC,
		PIPE_DEPTH_SKINNED
	};
	static PIPE depthTessPipes[] =
	{
		PIPE_DEPTH_TESS,
		PIPE_DEPTH_TESS_INSTANCED,
		PIPE_DEPTH_TESS_ROBOTIC,
		PIPE_DEPTH_TESS_SKINNED
	};

	if (atmo.GetShadowMapBaker().IsBaking())
		BindPipeline(allowTess ? depthTessPipes[GetPipelineIndex()] : depthPipes[GetPipelineIndex()], cb);
	else
		BindPipeline(allowTess ? tessPipes[GetPipelineIndex()] : pipes[GetPipelineIndex()], cb);
}

void Mesh::BindPipelineInstanced(CGI::CommandBufferPtr cb, bool allowTess, bool plant)
{
	VERUS_QREF_ATMO;
	if (atmo.GetShadowMapBaker().IsBaking())
	{
		if (allowTess)
			BindPipeline(plant ? PIPE_DEPTH_TESS_PLANT : PIPE_DEPTH_TESS_INSTANCED, cb);
		else
			BindPipeline(plant ? PIPE_DEPTH_PLANT : PIPE_DEPTH_INSTANCED, cb);
	}
	else
	{
		if (allowTess)
			BindPipeline(plant ? PIPE_TESS_PLANT : PIPE_TESS_INSTANCED, cb);
		else
			BindPipeline(plant ? PIPE_PLANT : PIPE_INSTANCED, cb);
	}
}

void Mesh::BindGeo(CGI::CommandBufferPtr cb, UINT32 bindingsFilter)
{
	cb->BindVertexBuffers(_geo, bindingsFilter ? bindingsFilter : _bindingsMask);
	cb->BindIndexBuffer(_geo);
}

void Mesh::UpdateUniformBufferPerFrame(float invTessDist)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
	Point3 eyePosWV = sm.GetCamera()->GetMatrixV() * eyePos;

	s_ubPerFrame._matV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubPerFrame._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubPerFrame._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubPerFrame._viewportSize = renderer.GetCommandBuffer()->GetViewportSize().GLM();
	s_ubPerFrame._eyePosWV_invTessDistSq = float4(eyePosWV.GLM(), invTessDist * invTessDist);
}

void Mesh::UpdateUniformBufferPerMeshVS()
{
	memcpy(&s_ubPerMeshVS._posDeqScale, _posDeq + 0, 12);
	memcpy(&s_ubPerMeshVS._posDeqBias, _posDeq + 3, 12);
	memcpy(&s_ubPerMeshVS._tc0DeqScaleBias, _tc0Deq, 16);
	memcpy(&s_ubPerMeshVS._tc1DeqScaleBias, _tc1Deq, 16);

	memcpy(&s_ubSimplePerMeshVS._posDeqScale, _posDeq + 0, 12);
	memcpy(&s_ubSimplePerMeshVS._posDeqBias, _posDeq + 3, 12);
	memcpy(&s_ubSimplePerMeshVS._tc0DeqScaleBias, _tc0Deq, 16);
	memcpy(&s_ubSimplePerMeshVS._tc1DeqScaleBias, _tc1Deq, 16);
}

void Mesh::UpdateUniformBufferSkeletonVS()
{
	VERUS_ZERO_MEM(s_ubSkeletonVS._vWarpZones);
	s_ubSkeletonVS._vWarpZones[1].w = FLT_MAX;
	if (_warp.IsInitialized())
	{
		Vector4 warpZones[32];
		VERUS_ZERO_MEM(warpZones);
		_warp.Fill(warpZones);
		memcpy(&s_ubSkeletonVS._vWarpZones, warpZones, sizeof(warpZones));
	}
	_skeleton.UpdateUniformBufferArray(s_ubSkeletonVS._vMatBones);
}

void Mesh::UpdateUniformBufferPerObject(RcTransform3 tr, RcVector4 color)
{
	s_ubPerObject._matW = tr.UniformBufferFormat();
	s_ubPerObject._userColor = color.GLM();

	s_ubSimplePerObject._matW = s_ubPerObject._matW;
	s_ubSimplePerObject._userColor = s_ubPerObject._userColor;
}

void Mesh::UpdateUniformBufferSimplePerFrame(DrawSimpleMode mode)
{
	VERUS_QREF_ATMO;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_WATER;

	RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
	const float clipDistanceOffset = (water.IsUnderwater() || DrawSimpleMode::envMap == mode) ? static_cast<float>(USHRT_MAX) : 0.f;

	s_ubSimplePerFrame._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubSimplePerFrame._eyePos_clipDistanceOffset = float4(eyePos.GLM(), clipDistanceOffset);
	s_ubSimplePerFrame._ambientColor = float4(atmo.GetAmbientColor().GLM(), 0);
	s_ubSimplePerFrame._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubSimplePerFrame._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubSimplePerFrame._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubSimplePerFrame._matShadow = atmo.GetShadowMapBaker().GetShadowMatrix(0).UniformBufferFormat();
	s_ubSimplePerFrame._matShadowCSM1 = atmo.GetShadowMapBaker().GetShadowMatrix(1).UniformBufferFormat();
	s_ubSimplePerFrame._matShadowCSM2 = atmo.GetShadowMapBaker().GetShadowMatrix(2).UniformBufferFormat();
	s_ubSimplePerFrame._matShadowCSM3 = atmo.GetShadowMapBaker().GetShadowMatrix(3).UniformBufferFormat();
	s_ubSimplePerFrame._matScreenCSM = atmo.GetShadowMapBaker().GetScreenMatrixVP().UniformBufferFormat();
	s_ubSimplePerFrame._csmSplitRanges = atmo.GetShadowMapBaker().GetSplitRanges().GLM();
	memcpy(&s_ubSimplePerFrame._shadowConfig, &atmo.GetShadowMapBaker().GetConfig(), sizeof(s_ubSimplePerFrame._shadowConfig));
}

void Mesh::UpdateUniformBufferSimpleSkeletonVS()
{
	_skeleton.UpdateUniformBufferArray(s_ubSimpleSkeletonVS._vMatBones);
}

void Mesh::CreateDeviceBuffers()
{
	_bindingsMask = 0;

	CGI::GeometryDesc geoDesc;
	geoDesc._name = _C(_url);
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(VertexInputBinding0, _pos), CGI::ViaType::shorts, 4, CGI::ViaUsage::position, 0},
		{0, offsetof(VertexInputBinding0, _tc0), CGI::ViaType::shorts, 2, CGI::ViaUsage::texCoord, 0},
		{0, offsetof(VertexInputBinding0, _nrm), CGI::ViaType::ubytes, 4, CGI::ViaUsage::normal, 0},
		{1, offsetof(VertexInputBinding1, _bw),  CGI::ViaType::shorts, 4, CGI::ViaUsage::blendWeights, 0},
		{1, offsetof(VertexInputBinding1, _bi),  CGI::ViaType::shorts, 4, CGI::ViaUsage::blendIndices, 0},
		{2, offsetof(VertexInputBinding2, _tan), CGI::ViaType::shorts, 4, CGI::ViaUsage::tangent, 0},
		{2, offsetof(VertexInputBinding2, _bin), CGI::ViaType::shorts, 4, CGI::ViaUsage::binormal, 0},
		{3, offsetof(VertexInputBinding3, _tc1), CGI::ViaType::shorts, 2, CGI::ViaUsage::texCoord, 1},
		{3, offsetof(VertexInputBinding3, _clr), CGI::ViaType::ubytes, 4, CGI::ViaUsage::color, 0},

		{-4, offsetof(PerInstanceData, _matPart0), CGI::ViaType::floats, 4, CGI::ViaUsage::instData, 0},
		{-4, offsetof(PerInstanceData, _matPart1), CGI::ViaType::floats, 4, CGI::ViaUsage::instData, 1},
		{-4, offsetof(PerInstanceData, _matPart2), CGI::ViaType::floats, 4, CGI::ViaUsage::instData, 2},
		{-4, offsetof(PerInstanceData, _instData), CGI::ViaType::floats, 4, CGI::ViaUsage::instData, 3},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(VertexInputBinding0), sizeof(VertexInputBinding1), sizeof(VertexInputBinding2), sizeof(VertexInputBinding3), sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	geoDesc._32BitIndices = _vIndices.empty();
	_geo.Init(geoDesc);

	// Vertex buffer, binding 0:
	_bindingsMask |= (1 << 0);
	_geo->CreateVertexBuffer(Utils::Cast32(_vBinding0.size()), 0);
	_geo->UpdateVertexBuffer(_vBinding0.data(), 0);

	// Vertex buffer, binding 1 (skinning):
	if (!_vBinding1.empty())
	{
		_bindingsMask |= (1 << 1);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding1.size()), 1);
		_geo->UpdateVertexBuffer(_vBinding1.data(), 1);
	}

	// Vertex buffer, binding 2 (tangent space):
	if (!_vBinding2.empty())
	{
		_bindingsMask |= (1 << 2);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding2.size()), 2);
		_geo->UpdateVertexBuffer(_vBinding2.data(), 2);
	}

	// Vertex buffer, binding 3 (lightmap):
	if (!_vBinding3.empty())
	{
		_bindingsMask |= (1 << 3);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding3.size()), 3);
		_geo->UpdateVertexBuffer(_vBinding3.data(), 3);
	}

	// Index buffer:
	if (!_vIndices.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices.size()));
		_geo->UpdateIndexBuffer(_vIndices.data());
	}
	else if (!_vIndices32.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices32.size()));
		_geo->UpdateIndexBuffer(_vIndices32.data());
	}

	// Instance buffer:
	if (_instanceCapacity > 0)
	{
		_vInstanceBuffer.resize(_instanceCapacity);
		_bindingsMask |= (1 << 4);
		_geo->CreateVertexBuffer(Utils::Cast32(_vInstanceBuffer.size()), 4);
	}
}

void Mesh::UpdateVertexBuffer(const void* p, int binding)
{
	_geo->UpdateVertexBuffer(p, binding);
}

void Mesh::ResetInstanceCount()
{
	_instanceCount = 0;
	_firstInstance = 0;
}

void Mesh::PushInstance(RcTransform3 matW, RcVector4 instData)
{
	VERUS_RT_ASSERT(!_vInstanceBuffer.empty());
	if (!_vertCount)
		return;
	if (IsInstanceBufferFull())
		return;
	matW.InstFormat(&_vInstanceBuffer[_instanceCount]._matPart0);
	_vInstanceBuffer[_instanceCount]._instData = instData;
	_instanceCount++;
}

bool Mesh::IsInstanceBufferFull()
{
	VERUS_RT_ASSERT(!_vInstanceBuffer.empty());
	if (!_vertCount)
		return false;
	return _instanceCount >= _instanceCapacity;
}

bool Mesh::IsInstanceBufferEmpty(bool fromFirstInstance)
{
	VERUS_RT_ASSERT(!_vInstanceBuffer.empty());
	if (!_vertCount)
		return true;
	return GetInstanceCount(fromFirstInstance) <= 0;
}

void Mesh::UpdateInstanceBuffer()
{
	VERUS_RT_ASSERT(!_vInstanceBuffer.empty());
	_geo->UpdateVertexBuffer(&_vInstanceBuffer[_firstInstance], 4, nullptr, GetInstanceCount(true), _firstInstance);
}
