#include "verus.h"

using namespace verus;
using namespace verus::CGI;

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
	Done();
}

PBaseRenderer Renderer::operator->()
{
	VERUS_RT_ASSERT(_pBaseRenderer);
	return _pBaseRenderer;
}

bool Renderer::IsLoaded()
{
	return IsValidSingleton() && !!I()._pBaseRenderer;
}

void Renderer::Init(PRendererDelegate pDelegate)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;

	_pRendererDelegate = pDelegate;

	CSZ dll = "RendererVulkan.dll";
	switch (settings._gapi)
	{
	case 12:
	{
		dll = "RendererDirect3D12.dll";
		VERUS_LOG_INFO("Using Direct3D 12");
	}
	break;
	default:
		VERUS_LOG_INFO("Using Vulkan");
	}
	BaseRendererDesc desc;
	_pBaseRenderer = BaseRenderer::Load(dll, desc);

	_gapi = _pBaseRenderer->GetGapi();

	_commandBuffer.Init();

	CGI::ShaderDesc shaderDesc;
	shaderDesc._url = "[Shaders]:GenerateMips.hlsl";
	_shaderGenerateMips.Init(shaderDesc);
	_shaderGenerateMips->CreateDescriptorSet(0, &_ubGenerateMips, sizeof(_ubGenerateMips), 100,
		{ Sampler::linearClamp2D, Sampler::storage, Sampler::storage, Sampler::storage, Sampler::storage }, ShaderStageFlags::cs);
	_shaderGenerateMips->CreatePipelineLayout();

	CGI::PipelineDesc pipeDesc(_shaderGenerateMips, "T");
	_pipeGenerateMips.Init(pipeDesc);

	TextureDesc td;
	td._format = Format::unormD24uintS8;
	td._width = settings._screenSizeWidth;
	td._height = settings._screenSizeHeight;
	_texDepthStencil.Init(td);

	_rpSwapChain = _pBaseRenderer->CreateRenderPass(
		{ RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().FinalLayout(ImageLayout::presentSrc) },
		{ RP::Subpass("Sp0").Color({RP::Ref("Color", ImageLayout::colorAttachmentOptimal)}) },
		{});
	_rpSwapChainDepth = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().FinalLayout(ImageLayout::presentSrc),
			RP::Attachment("Depth", Format::unormD24uintS8).LoadOpClear().Layout(ImageLayout::depthStencilAttachmentOptimal),
		},
		{
			RP::Subpass("Sp0").Color(
			{
				RP::Ref("Color", ImageLayout::colorAttachmentOptimal)
			}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachmentOptimal)),
		},
		{});
	_fbSwapChain.resize(_pBaseRenderer->GetNumSwapChainBuffers());
	VERUS_FOR(i, _fbSwapChain.size())
		_fbSwapChain[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChain, {}, settings._screenSizeWidth, settings._screenSizeHeight, i);
	_fbSwapChainDepth.resize(_pBaseRenderer->GetNumSwapChainBuffers());
	VERUS_FOR(i, _fbSwapChainDepth.size())
		_fbSwapChainDepth[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChainDepth, { _texDepthStencil }, settings._screenSizeWidth, settings._screenSizeHeight, i);

	_ds.Init();
}

void Renderer::Done()
{
	if (_pBaseRenderer)
	{
		_pBaseRenderer->WaitIdle();
		_ds.Done();
		_texDepthStencil.Done();
		_pipeGenerateMips.Done();
		_shaderGenerateMips.Done();
		_commandBuffer.Done();

		Scene::Terrain::DoneStatic();
		Scene::Mesh::DoneStatic();

		_pBaseRenderer->ReleaseMe();
		_pBaseRenderer = nullptr;
	}

	VERUS_SMART_DELETE(_pRendererDelegate);

	VERUS_DONE(Renderer);
}

void Renderer::Draw()
{
	if (_pRendererDelegate)
		_pRendererDelegate->Renderer_OnDraw();
}

void Renderer::Present()
{
	if (_pRendererDelegate)
		_pRendererDelegate->Renderer_OnPresent();
	_numFrames++;

	VERUS_QREF_TIMER;
	_fps = _fps * 0.75f + timer.GetDeltaTimeInv()*0.25f;
}

void Renderer::OnShaderError(CSZ s)
{
	throw VERUS_RUNTIME_ERROR << "Shader Error:\n" << s;
}

void Renderer::OnShaderWarning(CSZ s)
{
	VERUS_LOG_WARN("Shader Warning:\n" << s);
}

float Renderer::GetWindowAspectRatio() const
{
	VERUS_QREF_CONST_SETTINGS;
	return static_cast<float>(settings._screenSizeWidth) / static_cast<float>(settings._screenSizeHeight);
}
