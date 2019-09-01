#include "verus.h"

using namespace verus;
using namespace verus::CGI;

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

	ShaderDesc shaderDesc;
	shaderDesc._url = "[Shaders]:DS.hlsl";
	_shader[S_LIGHT].Init(shaderDesc);

	shaderDesc._url = "[Shaders]:DS_Compose.hlsl";
	_shader[S_COMPOSE].Init(shaderDesc);

	InitGBuffers(settings._screenSizeWidth, settings._screenSizeHeight);

	TextureDesc texDesc;

	// Light accumulation buffers:
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	_tex[TEX_LIGHT_ACC_DIFF].Init(texDesc);
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	_tex[TEX_LIGHT_ACC_SPEC].Init(texDesc);

	_renderPassID = renderer->CreateRenderPass(
		{
			RP::Attachment("GBuffer0", Format::unormR10G10B10A2).LoadOpClear().FinalLayout(ImageLayout::presentSrc),
			RP::Attachment("GBuffer1", Format::floatR32).LoadOpClear().FinalLayout(ImageLayout::presentSrc),
			RP::Attachment("GBuffer2", Format::unormR8G8B8A8).LoadOpClear().FinalLayout(ImageLayout::presentSrc),
			RP::Attachment("GBuffer3", Format::unormR8G8B8A8).LoadOpClear().FinalLayout(ImageLayout::presentSrc),
			RP::Attachment("Depth", Format::unormD24uintS8).LoadOpClear().Layout(ImageLayout::depthStencilAttachmentOptimal),
		},
		{
			RP::Subpass("Sp0").Color(
				{
					RP::Ref("GBuffer0", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer1", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer2", ImageLayout::colorAttachmentOptimal),
					RP::Ref("GBuffer3", ImageLayout::colorAttachmentOptimal)
				}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachmentOptimal))
		},
		{});
	_framebufferID = renderer->CreateFramebuffer(_renderPassID,
		{
			_tex[TEX_GBUFFER_0],
			_tex[TEX_GBUFFER_1],
			_tex[TEX_GBUFFER_2],
			_tex[TEX_GBUFFER_3]
		},
		settings._screenSizeWidth, settings._screenSizeHeight);

	VERUS_LOG_INFO("</Deferred-Shading-Init>");
}

void DeferredShading::InitGBuffers(int w, int h)
{
	_width = w;
	_height = h;

	TextureDesc texDesc;

	// GB0:
	texDesc._format = Format::unormR10G10B10A2;
	texDesc._width = _width;
	texDesc._height = _height;
	_tex[TEX_GBUFFER_0].Init(texDesc);

	// GB1:
	texDesc._format = Format::floatR32;
	texDesc._width = _width;
	texDesc._height = _height;
	_tex[TEX_GBUFFER_1].Init(texDesc);

	// GB2:
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = _width;
	texDesc._height = _height;
	_tex[TEX_GBUFFER_2].Init(texDesc);

	// GB3:
	texDesc._format = Format::unormR8G8B8A8;
	texDesc._width = _width;
	texDesc._height = _height;
	_tex[TEX_GBUFFER_3].Init(texDesc);
}

void DeferredShading::Done()
{
	VERUS_DONE(DeferredShading);
}

void DeferredShading::Draw(int gbuffer)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	const int w = settings._screenSizeWidth / 2;
	const int h = settings._screenSizeHeight / 2;

	if (-1 == gbuffer)
	{
	}
	else if (-2 == gbuffer)
	{
	}
	else
	{
	}
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
	}
}

void DeferredShading::EndGeometryPass(bool resetRT)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(_activeGeometryPass && !_activeLightingPass);
	_activeGeometryPass = false;
}

void DeferredShading::BeginLightingPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && !_activeLightingPass);
	_activeLightingPass = true;
}

void DeferredShading::EndLightingPass()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(renderer.GetNumFrames() == _frame);
	VERUS_RT_ASSERT(!_activeGeometryPass && _activeLightingPass);
	_activeLightingPass = false;
}

void DeferredShading::Compose()
{
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
}

void DeferredShading::UpdateBufferPerMesh()
{
}

void DeferredShading::UpdateBufferPerFrame()
{
}

void DeferredShading::EndLights()
{
}

void DeferredShading::Load()
{
}

TexturePtr DeferredShading::GetGBuffer(int index)
{
	return _tex[index];
}
