// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

DeferredShading::UB_PerFrame      DeferredShading::s_ubPerFrame;
DeferredShading::UB_TexturesFS    DeferredShading::s_ubTexturesFS;
DeferredShading::UB_PerMeshVS     DeferredShading::s_ubPerMeshVS;
DeferredShading::UB_ShadowFS      DeferredShading::s_ubShadowFS;
DeferredShading::UB_PerObject     DeferredShading::s_ubPerObject;

DeferredShading::UB_AmbientVS     DeferredShading::s_ubAmbientVS;
DeferredShading::UB_AmbientFS     DeferredShading::s_ubAmbientFS;

DeferredShading::UB_ComposeVS     DeferredShading::s_ubComposeVS;
DeferredShading::UB_ComposeFS     DeferredShading::s_ubComposeFS;

DeferredShading::UB_ReflectionVS  DeferredShading::s_ubReflectionVS;
DeferredShading::UB_ReflectionFS  DeferredShading::s_ubReflectionFS;

DeferredShading::UB_BakeSpritesVS DeferredShading::s_ubBakeSpritesVS;
DeferredShading::UB_BakeSpritesFS DeferredShading::s_ubBakeSpritesFS;

DeferredShading::DeferredShading()
{
}

DeferredShading::~DeferredShading()
{
	Done();
}

void DeferredShading::Init()
{
	VERUS_INIT();
	VERUS_LOG_INFO("<DeferredShadingInit>");
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_rph = renderer->CreateRenderPass(
		{
			RP::Attachment("GBuffer0", Format::srgbR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer1", Format::unormR10G10B10A2).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer2", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer3", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("LightAccAmb", Format::floatR11G11B10).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("LightAccDiff", Format::floatR11G11B10).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("LightAccSpec", Format::floatR11G11B10).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).LoadOpClear().Layout(ImageLayout::depthStencilAttachment, ImageLayout::depthStencilReadOnly)
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("GBuffer0", ImageLayout::colorAttachment),
					RP::Ref("GBuffer1", ImageLayout::colorAttachment),
					RP::Ref("GBuffer2", ImageLayout::colorAttachment),
					RP::Ref("GBuffer3", ImageLayout::colorAttachment)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
			RP::Subpass("Sp1").Input(
				{
					RP::Ref("GBuffer0", ImageLayout::fsReadOnly),
					RP::Ref("GBuffer1", ImageLayout::fsReadOnly),
					RP::Ref("GBuffer2", ImageLayout::fsReadOnly),
					RP::Ref("GBuffer3", ImageLayout::fsReadOnly),
					RP::Ref("Depth", ImageLayout::depthStencilReadOnly)
				}).Color(
				{
					RP::Ref("LightAccAmb", ImageLayout::colorAttachment),
					RP::Ref("LightAccDiff", ImageLayout::colorAttachment),
					RP::Ref("LightAccSpec", ImageLayout::colorAttachment)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilReadOnly))
		},
		{
			RP::Dependency("Sp0", "Sp1").Mode(1)
		});
	_rphCompose = renderer->CreateRenderPass(
		{
			RP::Attachment("ComposedA", Format::floatR11G11B10).LoadOpDontCare().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("ComposedB", Format::floatR11G11B10).LoadOpDontCare().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer3", Format::unormR8G8B8A8).LoadOpDontCare().Layout(ImageLayout::fsReadOnly)
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("ComposedA", ImageLayout::colorAttachment),
					RP::Ref("ComposedB", ImageLayout::colorAttachment),
					RP::Ref("GBuffer3", ImageLayout::colorAttachment)
				})
		},
		{});
	// Extra stuff, which is not using deferred shading, for example water and sky:
	_rphForwardRendering = renderer->CreateRenderPass(
		{
			RP::Attachment("ComposedA", Format::floatR11G11B10).Layout(ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).Layout(ImageLayout::depthStencilReadOnly, ImageLayout::depthStencilAttachment)
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("ComposedA", ImageLayout::colorAttachment)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment))
		},
		{});
	// Reflection is added using additive blending:
	_rphReflection = renderer->CreateSimpleRenderPass(Format::floatR11G11B10, RP::Attachment::LoadOp::load);

	_shader[SHADER_LIGHT].Init("[Shaders]:DS.hlsl");
	_shader[SHADER_LIGHT]->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame), settings._limits._ds_ubPerFrameCapacity);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(1, &s_ubTexturesFS, sizeof(s_ubTexturesFS), settings._limits._ds_ubTexturesFSCapacity,
		{
			Sampler::input, // GBuffer0
			Sampler::input, // GBuffer1
			Sampler::input, // GBuffer2
			Sampler::input, // GBuffer3
			Sampler::input, // Depth
			Sampler::shadow, // ShadowCmp
			Sampler::nearestClampMipN // Shadow
		}, ShaderStageFlags::fs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(2, &s_ubPerMeshVS, sizeof(s_ubPerMeshVS), settings._limits._ds_ubPerMeshVSCapacity, {}, ShaderStageFlags::vs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(3, &s_ubShadowFS, sizeof(s_ubShadowFS), settings._limits._ds_ubShadowFSCapacity, {}, ShaderStageFlags::fs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(4, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	_shader[SHADER_LIGHT]->CreatePipelineLayout();

	_shader[SHADER_AMBIENT].Init("[Shaders]:DS_Ambient.hlsl");
	_shader[SHADER_AMBIENT]->CreateDescriptorSet(0, &s_ubAmbientVS, sizeof(s_ubAmbientVS), 2);
	_shader[SHADER_AMBIENT]->CreateDescriptorSet(1, &s_ubAmbientFS, sizeof(s_ubAmbientFS), 2,
		{
			Sampler::input, // GBuffer0
			Sampler::input, // GBuffer1
			Sampler::input, // GBuffer2
			Sampler::input, // GBuffer3
			Sampler::input, // Depth
			CGI::Sampler::linearClampMipN, // TerrainHeightmap
			CGI::Sampler::anisoClamp // TerrainBlend
		}, ShaderStageFlags::fs);
	_shader[SHADER_AMBIENT]->CreatePipelineLayout();

	ShaderDesc shaderDesc("[Shaders]:DS_Compose.hlsl");
	String userDefines;
	if (settings._postProcessCinema)
		userDefines += " CINEMA";
	if (settings._postProcessBloom)
		userDefines += " BLOOM";
	if (!userDefines.empty())
	{
		userDefines = userDefines.substr(1);
		shaderDesc._userDefines = _C(userDefines);
	}
	_shader[SHADER_COMPOSE].Init(shaderDesc);
	_shader[SHADER_COMPOSE]->CreateDescriptorSet(0, &s_ubComposeVS, sizeof(s_ubComposeVS), 4, {}, ShaderStageFlags::vs);
	_shader[SHADER_COMPOSE]->CreateDescriptorSet(1, &s_ubComposeFS, sizeof(s_ubComposeFS), 4,
		{
			Sampler::nearestClampMipN, // GBuffer0
			Sampler::nearestClampMipN, // GBuffer1
			Sampler::linearClampMipN, // GBuffer2|Composed
			Sampler::nearestClampMipN, // Depth
			Sampler::nearestClampMipN, // Ambient
			Sampler::nearestClampMipN, // Diffuse
			Sampler::linearClampMipN // Specular|Bloom
		}, ShaderStageFlags::fs);
	_shader[SHADER_COMPOSE]->CreatePipelineLayout();

	_shader[SHADER_REFLECTION].Init("[Shaders]:DS_Reflection.hlsl");
	_shader[SHADER_REFLECTION]->CreateDescriptorSet(0, &s_ubReflectionVS, sizeof(s_ubReflectionVS), 2);
	_shader[SHADER_REFLECTION]->CreateDescriptorSet(1, &s_ubReflectionFS, sizeof(s_ubReflectionFS), 2,
		{
			Sampler::nearestClampMipN,
			Sampler::nearestClampMipN
		}, ShaderStageFlags::fs);
	_shader[SHADER_REFLECTION]->CreatePipelineLayout();

	_shader[SHADER_BAKE_SPRITES].Init("[Shaders]:DS_BakeSprites.hlsl");
	_shader[SHADER_BAKE_SPRITES]->CreateDescriptorSet(0, &s_ubBakeSpritesVS, sizeof(s_ubBakeSpritesVS), 1000);
	_shader[SHADER_BAKE_SPRITES]->CreateDescriptorSet(1, &s_ubBakeSpritesFS, sizeof(s_ubBakeSpritesFS), 1000,
		{
			Sampler::nearestClampMipN, // GBuffer0
			Sampler::nearestClampMipN, // GBuffer1
			Sampler::nearestClampMipN, // GBuffer2
			Sampler::nearestClampMipN // GBuffer3
		}, ShaderStageFlags::fs);
	_shader[SHADER_BAKE_SPRITES]->CreatePipelineLayout();

	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_AMBIENT], "#", _rph, 1);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._colorAttachWriteMasks[1] = "";
		pipeDesc._colorAttachWriteMasks[2] = "";
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_AMBIENT].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_COMPOSE], "#Compose", _rphCompose);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._colorAttachWriteMasks[1] = "rgb";
		pipeDesc._colorAttachWriteMasks[2] = "b";
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_COMPOSE].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_REFLECTION], "#", _rphReflection);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_REFLECTION].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_REFLECTION], "#DebugCubeMap", _rphReflection);
		pipeDesc._colorAttachWriteMasks[0] = "rgb";
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_REFLECTION_DEBUG].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_COMPOSE], "#ToneMapping", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_TONE_MAPPING].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), renderer.GetShaderQuad(), "#", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_QUAD].Init(pipeDesc);
	}

	OnSwapChainResized(true, false);

	VERUS_LOG_INFO("</DeferredShadingInit>");
}

void DeferredShading::InitGBuffers(int w, int h)
{
	TextureDesc texDesc;

	// GB0:
	texDesc._clearValue = GetClearValueForGBuffer0();
	texDesc._name = "DeferredShading.GBuffer0";
	texDesc._format = Format::srgbR8G8B8A8;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_0].Init(texDesc);

	// GB1:
	texDesc._clearValue = GetClearValueForGBuffer1();
	texDesc._name = "DeferredShading.GBuffer1";
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_1].Init(texDesc);

	// GB2:
	texDesc._clearValue = GetClearValueForGBuffer2();
	texDesc._name = "DeferredShading.GBuffer2";
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_2].Init(texDesc);

	// GB3:
	texDesc._clearValue = GetClearValueForGBuffer3();
	texDesc._name = "DeferredShading.GBuffer3";
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_3].Init(texDesc);
}

void DeferredShading::InitByAtmosphere(TexturePtr texShadow)
{
	VERUS_QREF_RENDERER;

	if (texShadow)
		_texAtmoShadow = texShadow;

	_shader[SHADER_LIGHT]->FreeDescriptorSet(_cshLight);
	_cshLight = _shader[SHADER_LIGHT]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3],
			renderer.GetTexDepthStencil(),
			_texAtmoShadow ? _texAtmoShadow : _tex[TEX_GBUFFER_0],
			_texAtmoShadow ? _texAtmoShadow : _tex[TEX_GBUFFER_1]
		});
}

void DeferredShading::InitByBloom(TexturePtr tex)
{
	_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshToneMapping);
	_cshToneMapping = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
		{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_COMPOSED_A],
				_tex[TEX_COMPOSED_A],
				tex,
				tex,
				tex
		});
}

void DeferredShading::InitByTerrain(TexturePtr texHeightmap, TexturePtr texBlend, int mapSide)
{
	VERUS_QREF_RENDERER;

	if (texHeightmap)
		_texTerrainHeightmap = texHeightmap;
	if (texBlend)
		_texTerrainBlend = texBlend;
	if (mapSide > 1)
		_terrainMapSide = mapSide;

	_shader[SHADER_AMBIENT]->FreeDescriptorSet(_cshAmbient);
	_cshAmbient = _shader[SHADER_AMBIENT]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3],
			renderer.GetTexDepthStencil(),
			_texTerrainHeightmap ? _texTerrainHeightmap : _tex[TEX_GBUFFER_0],
			_texTerrainBlend ? _texTerrainBlend : _tex[TEX_GBUFFER_1]
		});
}

void DeferredShading::Done()
{
	VERUS_DONE(DeferredShading);
}

void DeferredShading::OnSwapChainResized(bool init, bool done)
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	if (done)
	{
		VERUS_FOR(i, VERUS_COUNT_OF(_cshQuad))
			renderer.GetShaderQuad()->FreeDescriptorSet(_cshQuad[i]);
		_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshToneMapping);
		_shader[SHADER_REFLECTION]->FreeDescriptorSet(_cshReflection);
		_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshCompose);
		_shader[SHADER_AMBIENT]->FreeDescriptorSet(_cshAmbient);
		_shader[SHADER_LIGHT]->FreeDescriptorSet(_cshLight);

		renderer->DeleteFramebuffer(_fbhReflection);
		renderer->DeleteFramebuffer(_fbhForwardRendering);
		renderer->DeleteFramebuffer(_fbhCompose);
		renderer->DeleteFramebuffer(_fbh);

		_tex[TEX_COMPOSED_B].Done();
		_tex[TEX_COMPOSED_A].Done();
		_tex[TEX_LIGHT_ACC_SPECULAR].Done();
		_tex[TEX_LIGHT_ACC_DIFFUSE].Done();
		_tex[TEX_LIGHT_ACC_AMBIENT].Done();
		_tex[TEX_GBUFFER_3].Done();
		_tex[TEX_GBUFFER_2].Done();
		_tex[TEX_GBUFFER_1].Done();
		_tex[TEX_GBUFFER_0].Done();
	}

	if (init)
	{
		const int scaledSwapChainWidth = settings.Scale(renderer.GetSwapChainWidth());
		const int scaledSwapChainHeight = settings.Scale(renderer.GetSwapChainHeight());

		InitGBuffers(scaledSwapChainWidth, scaledSwapChainHeight);

		TextureDesc texDesc;

		// Light accumulation buffers:
		// See: https://bartwronski.com/2017/04/02/small-float-formats-r11g11b10f-precision/
		texDesc._format = Format::floatR11G11B10;
		texDesc._width = scaledSwapChainWidth;
		texDesc._height = scaledSwapChainHeight;
		texDesc._flags = TextureDesc::Flags::colorAttachment;
		texDesc._name = "DeferredShading.LightAccAmbient";
		_tex[TEX_LIGHT_ACC_AMBIENT].Init(texDesc);
		texDesc._name = "DeferredShading.LightAccDiffuse";
		_tex[TEX_LIGHT_ACC_DIFFUSE].Init(texDesc);
		texDesc._name = "DeferredShading.LightAccSpecular";
		_tex[TEX_LIGHT_ACC_SPECULAR].Init(texDesc);
		texDesc._name = "DeferredShading.ComposedA";
		_tex[TEX_COMPOSED_A].Init(texDesc);
		texDesc._name = "DeferredShading.ComposedB";
		_tex[TEX_COMPOSED_B].Init(texDesc);

		_fbh = renderer->CreateFramebuffer(_rph,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_GBUFFER_2],
				_tex[TEX_GBUFFER_3],
				_tex[TEX_LIGHT_ACC_AMBIENT],
				_tex[TEX_LIGHT_ACC_DIFFUSE],
				_tex[TEX_LIGHT_ACC_SPECULAR],
				renderer.GetTexDepthStencil()
			},
			scaledSwapChainWidth, scaledSwapChainHeight);
		_fbhCompose = renderer->CreateFramebuffer(_rphCompose,
			{
				_tex[TEX_COMPOSED_A],
				_tex[TEX_COMPOSED_B],
				_tex[TEX_GBUFFER_3]
			},
			scaledSwapChainWidth, scaledSwapChainHeight);
		_fbhForwardRendering = renderer->CreateFramebuffer(_rphForwardRendering,
			{
				_tex[TEX_COMPOSED_A],
				renderer.GetTexDepthStencil()
			},
			scaledSwapChainWidth, scaledSwapChainHeight);
		_fbhReflection = renderer->CreateFramebuffer(_rphReflection,
			{
				_tex[TEX_COMPOSED_A]
			},
			scaledSwapChainWidth, scaledSwapChainHeight);

		_cshCompose = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_GBUFFER_2],
				renderer.GetTexDepthStencil(),
				_tex[TEX_LIGHT_ACC_AMBIENT],
				_tex[TEX_LIGHT_ACC_DIFFUSE],
				_tex[TEX_LIGHT_ACC_SPECULAR]
			});
		_cshReflection = _shader[SHADER_REFLECTION]->BindDescriptorSetTextures(1,
			{
				_tex[TEX_GBUFFER_1],
				_tex[TEX_LIGHT_ACC_SPECULAR]
			});
		_cshToneMapping = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_COMPOSED_A],
				_tex[TEX_COMPOSED_A],
				_tex[TEX_LIGHT_ACC_AMBIENT],
				_tex[TEX_LIGHT_ACC_DIFFUSE],
				_tex[TEX_LIGHT_ACC_SPECULAR]
			});

		VERUS_FOR(i, VERUS_COUNT_OF(_cshQuad))
			_cshQuad[i] = renderer.GetShaderQuad()->BindDescriptorSetTextures(1, { _tex[TEX_GBUFFER_0 + i] });

		if (_texAtmoShadow)
			InitByAtmosphere(_texAtmoShadow);

		InitByTerrain(_texTerrainHeightmap, _texTerrainBlend, _terrainMapSide);
	}
}

bool DeferredShading::IsLoaded()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();
	return
		dl.Get(LightType::dir).IsLoaded() &&
		dl.Get(LightType::omni).IsLoaded() &&
		dl.Get(LightType::spot).IsLoaded();
}

void DeferredShading::ResetInstanceCount()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();
	dl.Get(LightType::dir).ResetInstanceCount();
	dl.Get(LightType::omni).ResetInstanceCount();
	dl.Get(LightType::spot).ResetInstanceCount();
}

void DeferredShading::Draw(int gbuffer,
	RcVector4 rMultiplexer,
	RcVector4 gMultiplexer,
	RcVector4 bMultiplexer,
	RcVector4 aMultiplexer)
{
	VERUS_QREF_RENDERER;

	const float w = static_cast<float>(renderer.GetSwapChainWidth() / 2);
	const float h = static_cast<float>(renderer.GetSwapChainHeight() / 2);

	auto cb = renderer.GetCommandBuffer();
	auto shader = renderer.GetShaderQuad();

	renderer.GetUbQuadVS()._matW = Math::QuadMatrix().UniformBufferFormat();
	renderer.GetUbQuadVS()._matV = Math::ToUVMatrix().UniformBufferFormat();
	renderer.GetUbQuadFS()._rMultiplexer = rMultiplexer.GLM();
	renderer.GetUbQuadFS()._gMultiplexer = gMultiplexer.GLM();
	renderer.GetUbQuadFS()._bMultiplexer = bMultiplexer.GLM();
	renderer.GetUbQuadFS()._aMultiplexer = aMultiplexer.GLM();

	cb->BindPipeline(_pipe[PIPE_QUAD]);

	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	if (-1 == gbuffer)
	{
		cb->SetViewport({ Vector4(0, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[0]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(w, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[1]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(0, h, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[2]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(w, h, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[3]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(0, 0, static_cast<float>(renderer.GetSwapChainWidth()), static_cast<float>(renderer.GetSwapChainHeight())) });
	}
	else if (-2 == gbuffer)
	{
		cb->SetViewport({ Vector4(0, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[4]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(w, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[5]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(0, h, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[6]);
		renderer.DrawQuad(cb.Get());

		cb->SetViewport({ Vector4(0, 0, static_cast<float>(renderer.GetSwapChainWidth()), static_cast<float>(renderer.GetSwapChainHeight())) });
	}
	else
	{
		cb->BindDescriptors(shader, 1, _cshQuad[gbuffer]);
		renderer.DrawQuad(cb.Get());
	}
	shader->EndBindDescriptors();
}

void DeferredShading::BeginGeometryPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass && !_activeForwardRendering);
	_activeGeometryPass = true;
	_frame = renderer.GetFrameCount();

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh,
		{
			_tex[TEX_GBUFFER_0]->GetClearValue(),
			_tex[TEX_GBUFFER_1]->GetClearValue(),
			_tex[TEX_GBUFFER_2]->GetClearValue(),
			_tex[TEX_GBUFFER_3]->GetClearValue(),
			_tex[TEX_LIGHT_ACC_AMBIENT]->GetClearValue(),
			_tex[TEX_LIGHT_ACC_DIFFUSE]->GetClearValue(),
			_tex[TEX_LIGHT_ACC_SPECULAR]->GetClearValue(),
			renderer.GetTexDepthStencil()->GetClearValue()
		});
}

void DeferredShading::EndGeometryPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(_activeGeometryPass && !_activeLightingPass && !_activeForwardRendering);
	_activeGeometryPass = false;
}

bool DeferredShading::BeginLightingPass(bool ambient, bool terrainOcclusion)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass && !_activeForwardRendering);
	_activeLightingPass = true;

	auto cb = renderer.GetCommandBuffer();

	cb->NextSubpass();

	if (ambient)
	{
		VERUS_QREF_ATMO;
		VERUS_QREF_SM;

		s_ubAmbientVS._matW = Math::QuadMatrix().UniformBufferFormat();
		s_ubAmbientVS._matV = Math::ToUVMatrix().UniformBufferFormat();
		s_ubAmbientFS._matInvV = sm.GetCamera()->GetMatrixInvV().UniformBufferFormat();
		s_ubAmbientFS._matInvP = sm.GetCamera()->GetMatrixInvP().UniformBufferFormat();
		s_ubAmbientFS._ambientColorY0 = float4(atmo.GetAmbientColorY0().GLM(), 0);
		s_ubAmbientFS._ambientColorY1 = float4(atmo.GetAmbientColorY1().GLM(), 0);
		s_ubAmbientFS._invMapSide_minOcclusion.x = 1.f / _terrainMapSide;
		s_ubAmbientFS._invMapSide_minOcclusion.y = terrainOcclusion ? 0.f : 1.f;

		cb->BindPipeline(_pipe[PIPE_AMBIENT]);
		_shader[SHADER_AMBIENT]->BeginBindDescriptors();
		cb->BindDescriptors(_shader[SHADER_AMBIENT], 0);
		cb->BindDescriptors(_shader[SHADER_AMBIENT], 1, _cshAmbient);
		_shader[SHADER_AMBIENT]->EndBindDescriptors();
		renderer.DrawQuad(cb.Get());
	}

	_shader[SHADER_LIGHT]->BeginBindDescriptors();

	if (!IsLoaded())
		return false;

	if (!_async_initPipe)
	{
		_async_initPipe = true;

		VERUS_QREF_HELPERS;
		Scene::RDeferredLights dl = helpers.GetDeferredLights();

		{
			PipelineDesc pipeDesc(dl.Get(LightType::dir).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedDir", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachWriteMasks[0] = "rgb";
			pipeDesc._colorAttachWriteMasks[1] = "rgb";
			pipeDesc._colorAttachWriteMasks[2] = "rgb";
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc.DisableDepthTest();
			_pipe[PIPE_INSTANCED_DIR].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(LightType::omni).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedOmni", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachWriteMasks[0] = "rgb";
			pipeDesc._colorAttachWriteMasks[1] = "rgb";
			pipeDesc._colorAttachWriteMasks[2] = "rgb";
			pipeDesc._rasterizationState._cullMode = CullMode::front;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthCompareOp = CompareOp::greater;
			pipeDesc._depthWriteEnable = false;
			_pipe[PIPE_INSTANCED_OMNI].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(LightType::spot).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedSpot", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachWriteMasks[0] = "rgb";
			pipeDesc._colorAttachWriteMasks[1] = "rgb";
			pipeDesc._colorAttachWriteMasks[2] = "rgb";
			pipeDesc._rasterizationState._cullMode = CullMode::front;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthCompareOp = CompareOp::greater;
			pipeDesc._depthWriteEnable = false;
			_pipe[PIPE_INSTANCED_SPOT].Init(pipeDesc);
		}
	}

	return true;
}

void DeferredShading::EndLightingPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && _activeLightingPass && !_activeForwardRendering);
	_activeLightingPass = false;

	_shader[SHADER_LIGHT]->EndBindDescriptors();
	renderer.GetCommandBuffer()->EndRenderPass();
}

void DeferredShading::BeginComposeAndForwardRendering(bool underwaterMask)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_ATMO;
	VERUS_QREF_WATER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass && !_activeForwardRendering);
	_activeForwardRendering = true;

	const Matrix4 matInvVP = VMath::inverse(sm.GetCamera()->GetMatrixVP());

	auto cb = renderer.GetCommandBuffer();

	s_ubComposeVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubComposeVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubComposeFS._matInvVP = matInvVP.UniformBufferFormat();
	s_ubComposeFS._exposure_underwaterMask.x = renderer.GetExposure();
	s_ubComposeFS._exposure_underwaterMask.y = underwaterMask ? 1.f : 0.f;
	s_ubComposeFS._backgroundColor = _backgroundColor.GLM();
	s_ubComposeFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubComposeFS._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();
	s_ubComposeFS._waterDiffColorShallow = float4(water.GetDiffuseColorShallow().GLM(), water.GetFogDensity());
	s_ubComposeFS._waterDiffColorDeep = float4(water.GetDiffuseColorDeep().GLM(), water.IsUnderwater() ? 1.f : 0.f);

	// Compose buffers, that is perform "final color = albedo * diffuse + specular" computation. Result is still HDR:
	cb->BeginRenderPass(_rphCompose, _fbhCompose,
		{
			_tex[TEX_COMPOSED_A]->GetClearValue(),
			_tex[TEX_COMPOSED_B]->GetClearValue(),
			_tex[TEX_GBUFFER_3]->GetClearValue()
		});

	cb->BindPipeline(_pipe[PIPE_COMPOSE]);
	_shader[SHADER_COMPOSE]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 0);
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 1, _cshCompose);
	_shader[SHADER_COMPOSE]->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();

	// Begin drawing extra stuff, which is not using deferred shading, for example water (updates depth buffer) and sky:
	cb->BeginRenderPass(_rphForwardRendering, _fbhForwardRendering,
		{
			_tex[TEX_COMPOSED_A]->GetClearValue(),
			renderer.GetTexDepthStencil()->GetClearValue()
		});
}

void DeferredShading::EndComposeAndForwardRendering()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass && _activeForwardRendering);
	_activeForwardRendering = false;

	renderer.GetCommandBuffer()->EndRenderPass();
}

void DeferredShading::DrawReflection()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SSR;

	auto cb = renderer.GetCommandBuffer();

	s_ubReflectionVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubReflectionVS._matV = Math::ToUVMatrix().UniformBufferFormat();

	cb->BeginRenderPass(_rphReflection, _fbhReflection, { _tex[TEX_COMPOSED_A]->GetClearValue() });

	cb->BindPipeline(ssr.IsCubeMapDebugMode() ? _pipe[PIPE_REFLECTION_DEBUG] : _pipe[PIPE_REFLECTION]);
	_shader[SHADER_REFLECTION]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_REFLECTION], 0);
	cb->BindDescriptors(_shader[SHADER_REFLECTION], 1, _cshReflection);
	_shader[SHADER_REFLECTION]->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());

	cb->EndRenderPass();
}

void DeferredShading::ToneMapping()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();

	s_ubComposeVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubComposeVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubComposeFS._exposure_underwaterMask.x = renderer.GetExposure();

	// Convert HDR image to SDR. First multiply by exposure, then apply tone mapping curve:
	cb->BindPipeline(_pipe[PIPE_TONE_MAPPING]);
	_shader[SHADER_COMPOSE]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 0);
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 1, _cshToneMapping);
	_shader[SHADER_COMPOSE]->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

bool DeferredShading::IsLightUrl(CSZ url)
{
	return
		!strcmp(url, "DIR") ||
		!strcmp(url, "OMNI") ||
		!strcmp(url, "SPOT");
}

void DeferredShading::OnNewLightType(CommandBufferPtr cb, LightType type, bool wireframe)
{
	VERUS_QREF_ATMO;
	VERUS_QREF_SM;

	s_ubPerFrame._matToUV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubPerFrame._matV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubPerFrame._matInvV = sm.GetCamera()->GetMatrixInvV().UniformBufferFormat();
	s_ubPerFrame._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubPerFrame._matInvP = sm.GetCamera()->GetMatrixInvP().UniformBufferFormat();

	switch (type)
	{
	case LightType::dir:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_DIR]);
	}
	break;
	case LightType::omni:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_OMNI]);
	}
	break;
	case LightType::spot:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_SPOT]);
	}
	break;
	}

	cb->BindDescriptors(_shader[SHADER_LIGHT], 0);
	cb->BindDescriptors(_shader[SHADER_LIGHT], 1, _cshLight);

	// Shadow:
	s_ubShadowFS._matShadow = atmo.GetShadowMapBaker().GetShadowMatrixDS(0).UniformBufferFormat();
	s_ubShadowFS._matShadowCSM1 = atmo.GetShadowMapBaker().GetShadowMatrixDS(1).UniformBufferFormat();
	s_ubShadowFS._matShadowCSM2 = atmo.GetShadowMapBaker().GetShadowMatrixDS(2).UniformBufferFormat();
	s_ubShadowFS._matShadowCSM3 = atmo.GetShadowMapBaker().GetShadowMatrixDS(3).UniformBufferFormat();
	s_ubShadowFS._matScreenCSM = atmo.GetShadowMapBaker().GetScreenMatrixP().UniformBufferFormat();
	s_ubShadowFS._csmSplitRanges = atmo.GetShadowMapBaker().GetSplitRanges().GLM();
	memcpy(&s_ubShadowFS._shadowConfig, &atmo.GetShadowMapBaker().GetConfig(), sizeof(s_ubShadowFS._shadowConfig));
	cb->BindDescriptors(_shader[SHADER_LIGHT], 3);
}

void DeferredShading::BindDescriptorsPerMeshVS(CommandBufferPtr cb)
{
	cb->BindDescriptors(_shader[SHADER_LIGHT], 2);
}

void DeferredShading::Load()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();

	Scene::Mesh::Desc meshDesc;
	meshDesc._instanceCapacity = 10000;

	meshDesc._url = "[Models]:DS/Dir.x3d";
	dl.Get(LightType::dir).Init(meshDesc);

	meshDesc._url = "[Models]:DS/Omni.x3d";
	dl.Get(LightType::omni).Init(meshDesc);

	meshDesc._url = "[Models]:DS/Spot.x3d";
	dl.Get(LightType::spot).Init(meshDesc);
}

TexturePtr DeferredShading::GetGBuffer(int index)
{
	return _tex[index];
}

TexturePtr DeferredShading::GetLightAccAmbientTexture()
{
	return _tex[TEX_LIGHT_ACC_AMBIENT];
}

TexturePtr DeferredShading::GetLightAccDiffuseTexture()
{
	return _tex[TEX_LIGHT_ACC_DIFFUSE];
}

TexturePtr DeferredShading::GetLightAccSpecularTexture()
{
	return _tex[TEX_LIGHT_ACC_SPECULAR];
}

TexturePtr DeferredShading::GetComposedTextureA()
{
	return _tex[TEX_COMPOSED_A];
}

TexturePtr DeferredShading::GetComposedTextureB()
{
	return _tex[TEX_COMPOSED_B];
}

Vector4 DeferredShading::GetClearValueForGBuffer0(float albedo)
{
	return Vector4(albedo, albedo, albedo, 0.5f);
}

Vector4 DeferredShading::GetClearValueForGBuffer1(float normal, float motionBlur)
{
	return Vector4(normal, normal, 0, motionBlur);
}

Vector4 DeferredShading::GetClearValueForGBuffer2(float roughness)
{
	return Vector4(1, roughness, 0, 0);
}

Vector4 DeferredShading::GetClearValueForGBuffer3(float tangent)
{
	return Vector4(tangent, tangent, 0, 0);
}

void DeferredShading::BakeSprites(TexturePtr texGBufferIn[4], TexturePtr texGBufferOut[4], PBaseCommandBuffer pCB)
{
	VERUS_QREF_RENDERER;

	if (!pCB)
		pCB = renderer.GetCommandBuffer().Get();

	RcVector4 size = texGBufferOut[0]->GetSize();
	const int w = static_cast<int>(size.getX());
	const int h = static_cast<int>(size.getY());

	_rphBakeSprites = renderer->CreateRenderPass(
		{
			RP::Attachment("GBuffer0", Format::srgbR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer1", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer2", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("GBuffer3", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly)
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("GBuffer0", ImageLayout::colorAttachment),
					RP::Ref("GBuffer1", ImageLayout::colorAttachment),
					RP::Ref("GBuffer2", ImageLayout::colorAttachment),
					RP::Ref("GBuffer3", ImageLayout::colorAttachment)
				})
		},
		{});

	_fbhBakeSprites = renderer->CreateFramebuffer(_rphBakeSprites,
		{
			texGBufferOut[0],
			texGBufferOut[1],
			texGBufferOut[2],
			texGBufferOut[3]
		},
		w, h);

	_cshBakeSprites = _shader[SHADER_BAKE_SPRITES]->BindDescriptorSetTextures(1,
		{
			texGBufferIn[0],
			texGBufferIn[1],
			texGBufferIn[2],
			texGBufferIn[3]
		});

	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_BAKE_SPRITES], "#", _rphBakeSprites);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_BAKE_SPRITES].Init(pipeDesc);
	}

	s_ubBakeSpritesVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubBakeSpritesVS._matV = Math::ToUVMatrix().UniformBufferFormat();

	pCB->BeginRenderPass(_rphBakeSprites, _fbhBakeSprites,
		{
			texGBufferOut[0]->GetClearValue(),
			texGBufferOut[1]->GetClearValue(),
			texGBufferOut[2]->GetClearValue(),
			texGBufferOut[3]->GetClearValue()
		});

	pCB->BindPipeline(_pipe[PIPE_BAKE_SPRITES]);
	_shader[SHADER_BAKE_SPRITES]->BeginBindDescriptors();
	pCB->BindDescriptors(_shader[SHADER_BAKE_SPRITES], 0);
	pCB->BindDescriptors(_shader[SHADER_BAKE_SPRITES], 1, _cshBakeSprites);
	_shader[SHADER_BAKE_SPRITES]->EndBindDescriptors();
	renderer.DrawQuad(pCB);

	pCB->EndRenderPass();
}

void DeferredShading::BakeSpritesCleanup()
{
	VERUS_QREF_RENDERER;

	_pipe[PIPE_BAKE_SPRITES].Done();
	_shader[SHADER_BAKE_SPRITES]->FreeDescriptorSet(_cshBakeSprites);
	renderer->DeleteFramebuffer(_fbhBakeSprites);
	renderer->DeleteRenderPass(_rphBakeSprites);
}
