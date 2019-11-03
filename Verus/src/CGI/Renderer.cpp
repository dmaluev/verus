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

	GeometryDesc geoDesc;
	const InputElementDesc ied[] =
	{
		{0, offsetof(Vertex, _pos), IeType::floats, 2, IeUsage::position, 0},
		InputElementDesc::End()
	};
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_geoQuad.Init(geoDesc);

	_shader[S_GENERATE_MIPS].Init("[Shaders]:GenerateMips.hlsl");
	_shader[S_GENERATE_MIPS]->CreateDescriptorSet(0, &_ubGenerateMips, sizeof(_ubGenerateMips), 100,
		{ Sampler::linearClamp2D, Sampler::storage, Sampler::storage, Sampler::storage, Sampler::storage }, ShaderStageFlags::cs);
	_shader[S_GENERATE_MIPS]->CreatePipelineLayout();

	_shader[S_QUAD].Init("[Shaders]:Quad.hlsl");
	_shader[S_QUAD]->CreateDescriptorSet(0, &_ubQuadVS, sizeof(_ubQuadVS), 100, {}, ShaderStageFlags::vs);
	_shader[S_QUAD]->CreateDescriptorSet(1, &_ubQuadFS, sizeof(_ubQuadFS), 100, { Sampler::linearClamp2D }, ShaderStageFlags::fs);
	_shader[S_QUAD]->CreatePipelineLayout();

	PipelineDesc pipeDesc(_shader[S_GENERATE_MIPS], "#");
	_pipeGenerateMips.Init(pipeDesc);

	TextureDesc texDesc;
	texDesc._clearValue = Vector4(1);
	texDesc._format = Format::unormD24uintS8;
	texDesc._width = settings._screenSizeWidth;
	texDesc._height = settings._screenSizeHeight;
	_texDepthStencil.Init(texDesc);

	_rpSwapChain = _pBaseRenderer->CreateRenderPass(
		{ RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().Layout(ImageLayout::undefined, ImageLayout::presentSrc) },
		{ RP::Subpass("Sp0").Color({RP::Ref("Color", ImageLayout::colorAttachment)}) },
		{});
	_rpSwapChainDepth = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().Layout(ImageLayout::undefined, ImageLayout::presentSrc),
			RP::Attachment("Depth", Format::unormD24uintS8).Layout(ImageLayout::depthStencilAttachment),
		},
		{
			RP::Subpass("Sp0").Color(
			{
				RP::Ref("Color", ImageLayout::colorAttachment)
			}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
		},
		{});
	_fbSwapChain.resize(_pBaseRenderer->GetNumSwapChainBuffers());
	VERUS_FOR(i, _fbSwapChain.size())
		_fbSwapChain[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChain, {}, settings._screenSizeWidth, settings._screenSizeHeight, i);
	_fbSwapChainDepth.resize(_pBaseRenderer->GetNumSwapChainBuffers());
	VERUS_FOR(i, _fbSwapChainDepth.size())
		_fbSwapChainDepth[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChainDepth, { _texDepthStencil }, settings._screenSizeWidth, settings._screenSizeHeight, i);

	_pBaseRenderer->ImGuiInit(_rpSwapChainDepth);

	_ds.Init();
}

void Renderer::InitCmd()
{
	const Vertex quadVerts[] =
	{
		glm::vec2(-1, +1), // TL.
		glm::vec2(-1, -1), // BL.
		glm::vec2(+1, +1), // TR.
		glm::vec2(+1, -1)  // BR.
	};
	_geoQuad->CreateVertexBuffer(VERUS_ARRAY_LENGTH(quadVerts), 0);
	_geoQuad->UpdateVertexBuffer(quadVerts, 0);
}

void Renderer::Done()
{
	if (_pBaseRenderer)
	{
		_pBaseRenderer->WaitIdle();
		_ds.Done();
		_texDepthStencil.Done();
		_pipeGenerateMips.Done();
		_shader.Done();
		_geoQuad.Done();
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
	_fps = _fps * 0.75f + timer.GetDeltaTimeInv() * 0.25f;
}

void Renderer::DrawQuad(PBaseCommandBuffer pCB)
{
	if (!pCB)
		pCB = &(*_commandBuffer);
	pCB->BindVertexBuffers(_geoQuad);
	pCB->Draw(4, 1);
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

void Renderer::ImGuiSetCurrentContext(ImGuiContext* pContext)
{
	ImGui::SetCurrentContext(pContext);
}
