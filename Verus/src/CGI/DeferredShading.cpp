#include "verus.h"

using namespace verus;
using namespace verus::CGI;

DeferredShading::UB_PerFrame         DeferredShading::s_ubPerFrame;
DeferredShading::UB_PerMesh          DeferredShading::s_ubPerMesh;
DeferredShading::UB_PerObject        DeferredShading::s_ubPerObject;
DeferredShading::UB_ComposePerObject DeferredShading::s_ubComposePerObject;

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
	VERUS_LOG_INFO("<Deferred-Shading-Init>");
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_rp = renderer->CreateRenderPass(
		{
			RP::Attachment("GBuffer0", Format::srgbR8G8B8A8).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("GBuffer1", Format::floatR32).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("GBuffer2", Format::unormR10G10B10A2).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("GBuffer3", Format::unormR8G8B8A8).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("LightAccDiff", Format::unormR10G10B10A2).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("LightAccSpec", Format::unormR10G10B10A2).LoadOpClear().Layout(ImageLayout::shaderReadOnlyOptimal),
			RP::Attachment("Depth", Format::unormD24uintS8).LoadOpClear().Layout(ImageLayout::depthStencilAttachmentOptimal),
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("GBuffer0", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer1", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer2", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer3", ImageLayout::colorAttachmentOptimal)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachmentOptimal)),
			RP::Subpass("Sp1").Input(
				{
					RP::Ref("GBuffer0", ImageLayout::shaderReadOnlyOptimal),
					RP::Ref("GBuffer1", ImageLayout::shaderReadOnlyOptimal),
					RP::Ref("GBuffer2", ImageLayout::shaderReadOnlyOptimal),
					RP::Ref("GBuffer3", ImageLayout::shaderReadOnlyOptimal)
				}).Color(
				{
					RP::Ref("LightAccDiff", ImageLayout::colorAttachmentOptimal),
					RP::Ref("LightAccSpec", ImageLayout::colorAttachmentOptimal),
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachmentOptimal))
		},
		{
			RP::Dependency("Sp0", "Sp1").Mode(1)
		});

	ShaderDesc shaderDesc;
	shaderDesc._url = "[Shaders]:DS.hlsl";
	_shader[S_LIGHT].Init(shaderDesc);
	_shader[S_LIGHT]->CreateDescriptorSet(0, &s_ubPerFrame, sizeof(s_ubPerFrame), 1,
		{
			Sampler::input,
			Sampler::input,
			Sampler::input,
			Sampler::input
		});
	_shader[S_LIGHT]->CreateDescriptorSet(1, &s_ubPerMesh, sizeof(s_ubPerMesh), 1000);
	_shader[S_LIGHT]->CreateDescriptorSet(2, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	_shader[S_LIGHT]->CreatePipelineLayout();

	shaderDesc._url = "[Shaders]:DS_Compose.hlsl";
	_shader[S_COMPOSE].Init(shaderDesc);
	_shader[S_COMPOSE]->CreateDescriptorSet(0, &s_ubComposePerObject, sizeof(s_ubComposePerObject), 1,
		{
			Sampler::nearest2D,
			Sampler::nearest2D,
			Sampler::nearest2D,
			Sampler::nearest2D,
			Sampler::nearest2D,
			Sampler::nearest2D
		});
	_shader[S_COMPOSE]->CreatePipelineLayout();

	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), _shader[S_COMPOSE], "T", renderer.GetRenderPass_SwapChainDepth());
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_COMPOSE].Init(pipeDesc);
	}
	{
		PipelineDesc pipeDesc(renderer.GetGeoQuad(), renderer.GetShaderQuad(), "T", renderer.GetRenderPass_SwapChainDepth());
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc._depthTestEnable = false;
		_pipe[PIPE_QUAD].Init(pipeDesc);
	}

	InitGBuffers(settings._screenSizeWidth, settings._screenSizeHeight);

	TextureDesc texDesc;

	// Light accumulation buffers:
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	texDesc._flags = TextureDesc::Flags::colorAttachment;
	_tex[TEX_LIGHT_ACC_DIFF].Init(texDesc);
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	texDesc._flags = TextureDesc::Flags::colorAttachment;
	_tex[TEX_LIGHT_ACC_SPEC].Init(texDesc);

	_fb = renderer->CreateFramebuffer(_rp,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3],
			_tex[TEX_LIGHT_ACC_DIFF],
			_tex[TEX_LIGHT_ACC_SPEC],
			renderer.GetTexDepthStencil()
		},
		settings._screenSizeWidth,
		settings._screenSizeHeight);

	_csidLight = _shader[S_LIGHT]->BindDescriptorSetTextures(0,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3]
		});
	_csidCompose = _shader[S_COMPOSE]->BindDescriptorSetTextures(0,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3],
			_tex[TEX_LIGHT_ACC_DIFF],
			_tex[TEX_LIGHT_ACC_SPEC]
		});
	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_csidQuad))
		_csidQuad[i] = renderer.GetShaderQuad()->BindDescriptorSetTextures(0, { _tex[TEX_GBUFFER_0 + i] });

	VERUS_LOG_INFO("</Deferred-Shading-Init>");
}

void DeferredShading::InitGBuffers(int w, int h)
{
	_width = w;
	_height = h;

	TextureDesc texDesc;

	// GB0:
	texDesc._clearValue = Vector4(0, 0, 0, 1);
	texDesc._format = Format::srgbR8G8B8A8;
	texDesc._width = _width;
	texDesc._height = _height;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_0].Init(texDesc);

	// GB1:
	texDesc._clearValue = Vector4(1);
	texDesc._format = Format::floatR32;
	texDesc._width = _width;
	texDesc._height = _height;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_1].Init(texDesc);

	// GB2:
	texDesc._clearValue = Vector4(0.5f, 0.5f, 0.5f, 0.5f);
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = _width;
	texDesc._height = _height;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_2].Init(texDesc);

	// GB3:
	texDesc._clearValue = Vector4(1, 1, 1, 1);
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = _width;
	texDesc._height = _height;
	texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::inputAttachment;
	_tex[TEX_GBUFFER_3].Init(texDesc);
}

void DeferredShading::Done()
{
	VERUS_DONE(DeferredShading);
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
	VERUS_QREF_CONST_SETTINGS;

	const float w = static_cast<float>(settings._screenSizeWidth / 2);
	const float h = static_cast<float>(settings._screenSizeHeight / 2);

	renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_QUAD]);

	renderer.GetShaderQuad()->BeginBindDescriptors();
	renderer.GetUbQuad()._matW = Math::QuadMatrix().UniformBufferFormat();
	renderer.GetUbQuad()._matV = Math::ToUVMatrix().UniformBufferFormat();

	if (-1 == gbuffer)
	{
		renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[0]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(w, 0, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[1]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(0, h, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[2]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(w, h, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[3]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, w * 2, h * 2) });
	}
	else if (-2 == gbuffer)
	{
		renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[4]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(w, 0, w, h) });
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[5]);
		renderer.DrawQuad();

		renderer.GetCommandBuffer()->SetViewport({ Vector4(0, 0, w * 2, h * 2) });
	}
	else
	{
		renderer.GetCommandBuffer()->BindDescriptors(renderer.GetShaderQuad(), 0, _csidQuad[gbuffer]);
		renderer.DrawQuad();
	}

	renderer.GetShaderQuad()->EndBindDescriptors();
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
		_frame = renderer.GetNumFrames();

		renderer.GetCommandBuffer()->BeginRenderPass(_rp, _fb,
			{
				_tex[TEX_GBUFFER_0]->GetClearValue(),
				_tex[TEX_GBUFFER_1]->GetClearValue(),
				_tex[TEX_GBUFFER_2]->GetClearValue(),
				_tex[TEX_GBUFFER_3]->GetClearValue(),
				_tex[TEX_LIGHT_ACC_DIFF]->GetClearValue(),
				_tex[TEX_LIGHT_ACC_SPEC]->GetClearValue(),
				renderer.GetTexDepthStencil()->GetClearValue()
			});
	}
}

void DeferredShading::EndGeometryPass(bool resetRT)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(_activeGeometryPass && !_activeLightingPass);
	_activeGeometryPass = false;
}

bool DeferredShading::BeginLightingPass()
{
	if (!IsLoaded())
		return false;

	if (!_async_initPipe)
	{
		_async_initPipe = true;

		VERUS_QREF_HELPERS;
		Scene::RDeferredLights dl = helpers.GetDeferredLights();

		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::dir).GetGeometry(), _shader[S_LIGHT], "TInstancedDir", _rp, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthTestEnable = false;
			_pipe[PIPE_INSTANCED_DIR].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::omni).GetGeometry(), _shader[S_LIGHT], "TInstancedOmni", _rp, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._rasterizationState._cullMode = CullMode::front;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthCompareOp = CompareOp::greater;
			pipeDesc._depthWriteEnable = false;
			_pipe[PIPE_INSTANCED_OMNI].Init(pipeDesc);
		}
		{
			PipelineDesc pipeDesc(dl.Get(CGI::LightType::spot).GetGeometry(), _shader[S_LIGHT], "TInstancedSpot", _rp, 1);
			pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_ADD;
			pipeDesc._rasterizationState._cullMode = CullMode::front;
			pipeDesc._vertexInputBindingsFilter = (1 << 0) | (1 << 4);
			pipeDesc._depthCompareOp = CompareOp::greater;
			pipeDesc._depthWriteEnable = false;
			_pipe[PIPE_INSTANCED_SPOT].Init(pipeDesc);
		}
	}

	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass);
	_activeLightingPass = true;

	renderer.GetCommandBuffer()->NextSubpass();

	_shader[S_LIGHT]->BeginBindDescriptors();

	return true;
}

void DeferredShading::EndLightingPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && _activeLightingPass);
	_activeLightingPass = false;

	_shader[S_LIGHT]->EndBindDescriptors();
	renderer.GetCommandBuffer()->EndRenderPass();
}

void DeferredShading::Compose()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_COMPOSE]);

	_shader[S_COMPOSE]->BeginBindDescriptors();
	s_ubComposePerObject._matW = Math::QuadMatrix().UniformBufferFormat();
	s_ubComposePerObject._matV = Math::ToUVMatrix().UniformBufferFormat();
	s_ubComposePerObject._matInvV = sm.GetCamera()->GetMatrixVi().UniformBufferFormat();
	s_ubComposePerObject._colorAmbient = Vector4(0.1f, 0.1f, 0.2f, 1).GLM();
	s_ubComposePerObject._fogColor = Vector4(0.5f, 0.5f, 0.5f, 0.002f).GLM();
	s_ubComposePerObject._zNearFarEx = sm.GetCamera()->GetZNearFarEx().GLM();
	renderer.GetCommandBuffer()->BindDescriptors(_shader[S_COMPOSE], 0, _csidCompose);
	_shader[S_COMPOSE]->EndBindDescriptors();

	renderer.DrawQuad();
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

	switch (type)
	{
	case CGI::LightType::dir:
	{
		renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_INSTANCED_DIR]);
	}
	break;
	case CGI::LightType::omni:
	{
		renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_INSTANCED_OMNI]);
	}
	break;
	case CGI::LightType::spot:
	{
		renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_INSTANCED_SPOT]);
	}
	break;
	}

	renderer.GetCommandBuffer()->BindDescriptors(_shader[S_LIGHT], 0, _csidLight);
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

void DeferredShading::BindDescriptorsPerMesh()
{
	VERUS_QREF_RENDERER;
	renderer.GetCommandBuffer()->BindDescriptors(_shader[S_LIGHT], 1);
}

void DeferredShading::Load()
{
	VERUS_QREF_HELPERS;
	Scene::RDeferredLights dl = helpers.GetDeferredLights();

	Scene::Mesh::Desc meshDesc;
	meshDesc._maxNumInstances = 1000;

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
