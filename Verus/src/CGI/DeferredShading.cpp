#include "verus.h"

using namespace verus;
using namespace verus::CGI;

DeferredShading::UB_PerFrame   DeferredShading::s_ubPerFrame;
DeferredShading::UB_TexturesFS DeferredShading::s_ubTexturesFS;
DeferredShading::UB_PerMeshVS  DeferredShading::s_ubPerMeshVS;
DeferredShading::UB_ShadowFS   DeferredShading::s_ubShadowFS;
DeferredShading::UB_PerObject  DeferredShading::s_ubPerObject;
DeferredShading::UB_ComposeVS  DeferredShading::s_ubComposeVS;
DeferredShading::UB_ComposeFS  DeferredShading::s_ubComposeFS;

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
			RP::Attachment("LightAccDiff", Format::floatR11G11B10).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("LightAccSpec", Format::floatR11G11B10).LoadOpClear().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).LoadOpClear().Layout(ImageLayout::depthStencilAttachment, ImageLayout::depthStencilReadOnly),
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("GBuffer0", ImageLayout::colorAttachment),
					RP::Ref("GBuffer1", ImageLayout::colorAttachment),
					RP::Ref("GBuffer2", ImageLayout::colorAttachment)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
			RP::Subpass("Sp1").Input(
				{
					RP::Ref("GBuffer0", ImageLayout::fsReadOnly),
					RP::Ref("GBuffer1", ImageLayout::fsReadOnly),
					RP::Ref("GBuffer2", ImageLayout::fsReadOnly),
					RP::Ref("Depth", ImageLayout::depthStencilReadOnly)
				}).Color(
				{
					RP::Ref("LightAccDiff", ImageLayout::colorAttachment),
					RP::Ref("LightAccSpec", ImageLayout::colorAttachment),
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilReadOnly))
		},
		{
			RP::Dependency("Sp0", "Sp1").Mode(1)
		});
	_rphCompose = renderer->CreateRenderPass(
		{
			RP::Attachment("ComposedA", Format::floatR11G11B10).LoadOpDontCare().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("ComposedB", Format::floatR11G11B10).LoadOpDontCare().Layout(ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).Layout(ImageLayout::depthStencilReadOnly),
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("ComposedA", ImageLayout::colorAttachment),
					RP::Ref("ComposedB", ImageLayout::colorAttachment),
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilReadOnly))
		},
		{});
	_rphExtraCompose = renderer->CreateRenderPass(
		{
			RP::Attachment("ComposedA", Format::floatR11G11B10).Layout(ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).Layout(ImageLayout::depthStencilReadOnly, ImageLayout::depthStencilAttachment),
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("ComposedA", ImageLayout::colorAttachment)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment))
		},
		{});

	_shader[SHADER_LIGHT].Init("[Shaders]:DS.hlsl");
	_shader[SHADER_LIGHT]->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame));
	_shader[SHADER_LIGHT]->CreateDescriptorSet(1, &s_ubTexturesFS, sizeof(s_ubTexturesFS), 1,
		{
			Sampler::input,
			Sampler::input,
			Sampler::input,
			Sampler::input,
			Sampler::shadow,
			Sampler::nearestClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(2, &s_ubPerMeshVS, sizeof(s_ubPerMeshVS), 1000, {}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(3, &s_ubShadowFS, sizeof(s_ubShadowFS), 1000, {}, CGI::ShaderStageFlags::fs);
	_shader[SHADER_LIGHT]->CreateDescriptorSet(4, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	_shader[SHADER_LIGHT]->CreatePipelineLayout();

	_shader[SHADER_COMPOSE].Init("[Shaders]:DS_Compose.hlsl");
	_shader[SHADER_COMPOSE]->CreateDescriptorSet(0, &s_ubComposeVS, sizeof(s_ubComposeVS), 2, {}, CGI::ShaderStageFlags::vs);
	_shader[SHADER_COMPOSE]->CreateDescriptorSet(1, &s_ubComposeFS, sizeof(s_ubComposeFS), 2,
		{
			Sampler::nearestClampMipN,
			Sampler::nearestClampMipN,
			Sampler::linearClampMipN, // For bloom.
			Sampler::nearestClampMipN,
			Sampler::nearestClampMipN,
			Sampler::nearestClampMipN
		}, CGI::ShaderStageFlags::fs);
	_shader[SHADER_COMPOSE]->CreatePipelineLayout();

	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[SHADER_COMPOSE], "#Compose", _rphCompose);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_COMPOSE].Init(pipeDesc);
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
	texDesc._clearValue = Vector4(0);
	texDesc._format = Format::srgbR8G8B8A8;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_0].Init(texDesc);

	// GB1:
	texDesc._clearValue = Vector4(0);
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_1].Init(texDesc);

	// GB2:
	texDesc._clearValue = Vector4(0);
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = w;
	texDesc._height = h;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_2].Init(texDesc);
}

void DeferredShading::InitByAtmosphere(TexturePtr texShadow)
{
	VERUS_QREF_RENDERER;

	_texShadowAtmo = texShadow;

	_shader[SHADER_LIGHT]->FreeDescriptorSet(_cshLight);
	_cshLight = _shader[SHADER_LIGHT]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			renderer.GetTexDepthStencil(),
			_texShadowAtmo,
			_texShadowAtmo
		});
}

void DeferredShading::InitByBloom(TexturePtr tex)
{
	VERUS_QREF_RENDERER;

	_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshToneMapping);
	_cshToneMapping = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			tex,
			_tex[TEX_COMPOSED_A],
			_tex[TEX_LIGHT_ACC_DIFF],
			_tex[TEX_LIGHT_ACC_SPEC]
		});
}

void DeferredShading::InitBySsao(TexturePtr tex)
{
	VERUS_QREF_RENDERER;

	_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshCompose);
	_cshCompose = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			tex,
			renderer.GetTexDepthStencil(),
			_tex[TEX_LIGHT_ACC_DIFF],
			_tex[TEX_LIGHT_ACC_SPEC]
		});
}

void DeferredShading::Done()
{
	VERUS_DONE(DeferredShading);
}

void DeferredShading::OnSwapChainResized(bool init, bool done)
{
	VERUS_QREF_RENDERER;

	if (done)
	{
		_shader[SHADER_LIGHT]->FreeDescriptorSet(_cshLight);
		VERUS_FOR(i, VERUS_COUNT_OF(_cshQuad))
			renderer.GetShaderQuad()->FreeDescriptorSet(_cshQuad[i]);
		_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshToneMapping);
		_shader[SHADER_COMPOSE]->FreeDescriptorSet(_cshCompose);

		renderer->DeleteFramebuffer(_fbhCompose);
		renderer->DeleteFramebuffer(_fbh);

		_tex[TEX_COMPOSED_B].Done();
		_tex[TEX_COMPOSED_A].Done();
		_tex[TEX_LIGHT_ACC_SPEC].Done();
		_tex[TEX_LIGHT_ACC_DIFF].Done();
		_tex[TEX_GBUFFER_2].Done();
		_tex[TEX_GBUFFER_1].Done();
		_tex[TEX_GBUFFER_0].Done();
	}

	if (init)
	{
		InitGBuffers(renderer.GetSwapChainWidth(), renderer.GetSwapChainHeight());

		TextureDesc texDesc;

		// Light accumulation buffers:
		// See: https://bartwronski.com/2017/04/02/small-float-formats-r11g11b10f-precision/
		texDesc._format = Format::floatR11G11B10;
		texDesc._width = renderer.GetSwapChainWidth();
		texDesc._height = renderer.GetSwapChainHeight();
		texDesc._flags = TextureDesc::Flags::colorAttachment;
		_tex[TEX_LIGHT_ACC_DIFF].Init(texDesc);
		_tex[TEX_LIGHT_ACC_SPEC].Init(texDesc);
		_tex[TEX_COMPOSED_A].Init(texDesc);
		_tex[TEX_COMPOSED_B].Init(texDesc);

		_fbh = renderer->CreateFramebuffer(_rph,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_GBUFFER_2],
				_tex[TEX_LIGHT_ACC_DIFF],
				_tex[TEX_LIGHT_ACC_SPEC],
				renderer.GetTexDepthStencil()
			},
			renderer.GetSwapChainWidth(),
			renderer.GetSwapChainHeight());
		_fbhCompose = renderer->CreateFramebuffer(_rphCompose,
			{
				_tex[TEX_COMPOSED_A],
				_tex[TEX_COMPOSED_B],
				renderer.GetTexDepthStencil()
			},
			renderer.GetSwapChainWidth(),
			renderer.GetSwapChainHeight());
		_fbhExtraCompose = renderer->CreateFramebuffer(_rphExtraCompose,
			{
				_tex[TEX_COMPOSED_A],
				renderer.GetTexDepthStencil()
			},
			renderer.GetSwapChainWidth(),
			renderer.GetSwapChainHeight());

		_cshCompose = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_GBUFFER_2],
				renderer.GetTexDepthStencil(),
				_tex[TEX_LIGHT_ACC_DIFF],
				_tex[TEX_LIGHT_ACC_SPEC]
			});
		_cshToneMapping = _shader[SHADER_COMPOSE]->BindDescriptorSetTextures(1,
			{
				_tex[TEX_GBUFFER_0],
				_tex[TEX_GBUFFER_1],
				_tex[TEX_GBUFFER_2],
				_tex[TEX_COMPOSED_A],
				_tex[TEX_LIGHT_ACC_DIFF],
				_tex[TEX_LIGHT_ACC_SPEC]
			});

		VERUS_FOR(i, VERUS_COUNT_OF(_cshQuad))
			_cshQuad[i] = renderer.GetShaderQuad()->BindDescriptorSetTextures(1, { _tex[TEX_GBUFFER_0 + i] });

		if (_texShadowAtmo)
			InitByAtmosphere(_texShadowAtmo);
	}
}

bool DeferredShading::IsLoaded()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();
	return
		dl.Get(CGI::LightType::dir).IsLoaded() &&
		dl.Get(CGI::LightType::omni).IsLoaded() &&
		dl.Get(CGI::LightType::spot).IsLoaded();
}

void DeferredShading::Draw(int gbuffer)
{
	VERUS_QREF_RENDERER;

	const float w = static_cast<float>(renderer.GetSwapChainWidth() / 2);
	const float h = static_cast<float>(renderer.GetSwapChainHeight() / 2);

	renderer.GetUbQuadVS()._matW = Math::QuadMatrix().UniformBufferFormat();
	renderer.GetUbQuadVS()._matV = Math::ToUVMatrix().UniformBufferFormat();

	auto cb = renderer.GetCommandBuffer();
	auto shader = renderer.GetShaderQuad();

	cb->BindPipeline(_pipe[PIPE_QUAD]);

	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	if (-1 == gbuffer)
	{
		cb->SetViewport({ Vector4(0, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[0]);
		renderer.DrawQuad(&(*cb));

		cb->SetViewport({ Vector4(w, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[1]);
		renderer.DrawQuad(&(*cb));

		cb->SetViewport({ Vector4(0, h, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[2]);
		renderer.DrawQuad(&(*cb));

		cb->SetViewport({ Vector4(0, 0, static_cast<float>(renderer.GetSwapChainWidth()), static_cast<float>(renderer.GetSwapChainHeight())) });
	}
	else if (-2 == gbuffer)
	{
		cb->SetViewport({ Vector4(0, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[3]);
		renderer.DrawQuad(&(*cb));

		cb->SetViewport({ Vector4(w, 0, w, h) });
		cb->BindDescriptors(shader, 1, _cshQuad[4]);
		renderer.DrawQuad(&(*cb));

		cb->SetViewport({ Vector4(0, 0, static_cast<float>(renderer.GetSwapChainWidth()), static_cast<float>(renderer.GetSwapChainHeight())) });
	}
	else
	{
		cb->BindDescriptors(shader, 1, _cshQuad[gbuffer]);
		renderer.DrawQuad(&(*cb));
	}
	shader->EndBindDescriptors();
}

void DeferredShading::BeginGeometryPass(bool onlySetRT, bool spriteBaking)
{
	VERUS_QREF_RENDERER;
	TexturePtr depth = nullptr;
	if (onlySetRT)
	{
	}
	else
	{
		VERUS_QREF_ATMO;
		VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass);
		_activeGeometryPass = true;
		_frame = renderer.GetFrameCount();

		renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh,
			{
				_tex[TEX_GBUFFER_0]->GetClearValue(),
				_tex[TEX_GBUFFER_1]->GetClearValue(),
				_tex[TEX_GBUFFER_2]->GetClearValue(),
				_tex[TEX_LIGHT_ACC_DIFF]->GetClearValue(),
				_tex[TEX_LIGHT_ACC_SPEC]->GetClearValue(),
				renderer.GetTexDepthStencil()->GetClearValue()
			});
	}
}

void DeferredShading::EndGeometryPass(bool resetRT)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(_activeGeometryPass && !_activeLightingPass);
	_activeGeometryPass = false;
}

bool DeferredShading::BeginLightingPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetFrameCount() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass);
	_activeLightingPass = true;

	renderer.GetCommandBuffer()->NextSubpass();
	_shader[SHADER_LIGHT]->BeginBindDescriptors();

	if (!IsLoaded())
		return false;

	if (!_async_initPipe)
	{
		_async_initPipe = true;

		VERUS_QREF_HELPERS;
		Scene::RDeferredLights dl = helpers.GetDeferredLights();

		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::dir).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedDir", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc.DisableDepthTest();
			_pipe[PIPE_INSTANCED_DIR].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::omni).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedOmni", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._rasterizationState._cullMode = CullMode::front;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthCompareOp = CompareOp::greater;
			pipeDesc._depthWriteEnable = false;
			_pipe[PIPE_INSTANCED_OMNI].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::spot).GetGeometry(), _shader[SHADER_LIGHT], "#InstancedSpot", _rph, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
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
	VERUS_RT_ASSERT(!_activeGeometryPass && _activeLightingPass);
	_activeLightingPass = false;

	_shader[SHADER_LIGHT]->EndBindDescriptors();
	renderer.GetCommandBuffer()->EndRenderPass();
}

void DeferredShading::BeginCompose()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_ATMO;
	VERUS_QREF_WATER;

	const Matrix4 matInvVP = VMath::inverse(sm.GetCamera()->GetMatrixVP());

	s_ubComposeVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubComposeVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubComposeFS._matInvV = sm.GetCamera()->GetMatrixVi().UniformBufferFormat();
	s_ubComposeFS._matInvVP = matInvVP.UniformBufferFormat();
	s_ubComposeFS._ambientColor_exposure = float4(atmo.GetAmbientColor().GLM(), renderer.GetExposure());
	s_ubComposeFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubComposeFS._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();
	s_ubComposeFS._waterDiffColorShallow = float4(water.GetDiffuseColorShallow().GLM(), water.GetFogDensity());
	s_ubComposeFS._waterDiffColorDeep = float4(water.GetDiffuseColorDeep().GLM(), water.IsUnderwater() ? 1.f : 0.f);

	auto cb = renderer.GetCommandBuffer();

	cb->BeginRenderPass(_rphCompose, _fbhCompose,
		{
			_tex[TEX_COMPOSED_A]->GetClearValue(),
			_tex[TEX_COMPOSED_B]->GetClearValue(),
			renderer.GetTexDepthStencil()->GetClearValue()
		});

	cb->BindPipeline(_pipe[PIPE_COMPOSE]);

	_shader[SHADER_COMPOSE]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 0);
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 1, _cshCompose);
	_shader[SHADER_COMPOSE]->EndBindDescriptors();

	renderer.DrawQuad(&(*cb));

	cb->EndRenderPass();

	cb->BeginRenderPass(_rphExtraCompose, _fbhExtraCompose,
		{
			_tex[TEX_COMPOSED_A]->GetClearValue(),
			renderer.GetTexDepthStencil()->GetClearValue()
		});
}

void DeferredShading::EndCompose()
{
	VERUS_QREF_RENDERER;

	renderer.GetCommandBuffer()->EndRenderPass();
}

void DeferredShading::ToneMapping(RcVector4 bgColor)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_ATMO;

	s_ubComposeVS._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubComposeVS._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubComposeFS._matInvV = sm.GetCamera()->GetMatrixVi().UniformBufferFormat();
	s_ubComposeFS._ambientColor_exposure = float4(atmo.GetAmbientColor().GLM(), renderer.GetExposure());
	s_ubComposeFS._backgroundColor = bgColor.GLM();
	s_ubComposeFS._fogColor = Vector4(0.5f, 0.5f, 0.5f, 0.002f).GLM();
	s_ubComposeFS._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();

	auto cb = renderer.GetCommandBuffer();

	cb->BindPipeline(_pipe[PIPE_TONE_MAPPING]);

	_shader[SHADER_COMPOSE]->BeginBindDescriptors();
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 0);
	cb->BindDescriptors(_shader[SHADER_COMPOSE], 1, _cshToneMapping);
	_shader[SHADER_COMPOSE]->EndBindDescriptors();

	renderer.DrawQuad(&(*cb));
}

void DeferredShading::AntiAliasing()
{
}

bool DeferredShading::IsLightUrl(CSZ url)
{
	return
		!strcmp(url, "DIR") ||
		!strcmp(url, "OMNI") ||
		!strcmp(url, "SPOT");
}

void DeferredShading::OnNewLightType(LightType type, bool wireframe)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_ATMO;

	auto cb = renderer.GetCommandBuffer();

	switch (type)
	{
	case CGI::LightType::dir:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_DIR]);
	}
	break;
	case CGI::LightType::omni:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_OMNI]);
	}
	break;
	case CGI::LightType::spot:
	{
		cb->BindPipeline(_pipe[PIPE_INSTANCED_SPOT]);
	}
	break;
	}

	cb->BindDescriptors(_shader[SHADER_LIGHT], 0);
	cb->BindDescriptors(_shader[SHADER_LIGHT], 1, _cshLight);

	// Shadow:
	s_ubShadowFS._matSunShadow = atmo.GetShadowMap().GetShadowMatrixDS(0).UniformBufferFormat();
	s_ubShadowFS._matSunShadowCSM1 = atmo.GetShadowMap().GetShadowMatrixDS(1).UniformBufferFormat();
	s_ubShadowFS._matSunShadowCSM2 = atmo.GetShadowMap().GetShadowMatrixDS(2).UniformBufferFormat();
	s_ubShadowFS._matSunShadowCSM3 = atmo.GetShadowMap().GetShadowMatrixDS(3).UniformBufferFormat();
	memcpy(&s_ubShadowFS._shadowConfig, &atmo.GetShadowMap().GetConfig(), sizeof(s_ubShadowFS._shadowConfig));
	s_ubShadowFS._splitRanges = atmo.GetShadowMap().GetSplitRanges().GLM();
	cb->BindDescriptors(_shader[SHADER_LIGHT], 3);
}

void DeferredShading::UpdateUniformBufferPerFrame()
{
	VERUS_QREF_SM;

	// Standard quad:
	s_ubPerFrame._matQuad = Math::QuadMatrix().UniformBufferFormat();
	s_ubPerFrame._matToUV = Math::ToUVMatrix().UniformBufferFormat();

	s_ubPerFrame._matV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubPerFrame._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubPerFrame._matInvP = Matrix4(VMath::inverse(sm.GetCamera()->GetMatrixP())).UniformBufferFormat();
	s_ubPerFrame._toUV = float4(0, 0, 0, 0);
}

void DeferredShading::BindDescriptorsPerMeshVS()
{
	VERUS_QREF_RENDERER;
	renderer.GetCommandBuffer()->BindDescriptors(_shader[SHADER_LIGHT], 2);
}

void DeferredShading::Load()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();

	Scene::Mesh::Desc meshDesc;
	meshDesc._instanceCapacity = 1000;

	meshDesc._url = "[Models]:DS/Dir.x3d";
	dl.Get(CGI::LightType::dir).Init(meshDesc);

	meshDesc._url = "[Models]:DS/Omni.x3d";
	dl.Get(CGI::LightType::omni).Init(meshDesc);

	meshDesc._url = "[Models]:DS/Spot.x3d";
	dl.Get(CGI::LightType::spot).Init(meshDesc);
}

TexturePtr DeferredShading::GetGBuffer(int index)
{
	return _tex[index];
}

TexturePtr DeferredShading::GetComposedTextureA()
{
	return _tex[TEX_COMPOSED_A];
}

TexturePtr DeferredShading::GetComposedTextureB()
{
	return _tex[TEX_COMPOSED_B];
}
