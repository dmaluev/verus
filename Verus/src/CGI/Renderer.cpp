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

	_swapChainWidth = settings._screenSizeWidth;
	_swapChainHeight = settings._screenSizeHeight;

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

	OnSwapChainResized(true, false);

	_pBaseRenderer->ImGuiInit(_rpSwapChainDepth);
	ImGuiUpdateStyle();

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
	_geoQuad->CreateVertexBuffer(VERUS_COUNT_OF(quadVerts), 0);
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
	_frameCount++;

	VERUS_QREF_TIMER;
	_fps = _fps * 0.75f + timer.GetDeltaTimeInv() * 0.25f;
}

void Renderer::OnWindowResized(int w, int h)
{
	_pBaseRenderer->WaitIdle();

	_swapChainWidth = w;
	_swapChainHeight = h;
	_pBaseRenderer->ResizeSwapChain();

	OnSwapChainResized(true, true);
	_ds.OnSwapChainResized(true, true);
}

void Renderer::OnSwapChainResized(bool init, bool done)
{
	if (done)
	{
		VERUS_FOR(i, _fbSwapChainDepth.size())
			_pBaseRenderer->DeleteFramebuffer(_fbSwapChainDepth[i]);
		VERUS_FOR(i, _fbSwapChain.size())
			_pBaseRenderer->DeleteFramebuffer(_fbSwapChain[i]);

		_texDepthStencil.Done();
	}

	if (init)
	{
		TextureDesc texDesc;
		texDesc._clearValue = Vector4(1);
		texDesc._format = Format::unormD24uintS8;
		texDesc._width = _swapChainWidth;
		texDesc._height = _swapChainHeight;
		_texDepthStencil.Init(texDesc);

		_fbSwapChain.resize(_pBaseRenderer->GetSwapChainBufferCount());
		VERUS_FOR(i, _fbSwapChain.size())
			_fbSwapChain[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChain, {}, _swapChainWidth, _swapChainHeight, i);
		_fbSwapChainDepth.resize(_pBaseRenderer->GetSwapChainBufferCount());
		VERUS_FOR(i, _fbSwapChainDepth.size())
			_fbSwapChainDepth[i] = _pBaseRenderer->CreateFramebuffer(_rpSwapChainDepth, { _texDepthStencil }, _swapChainWidth, _swapChainHeight, i);
	}
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

float Renderer::GetSwapChainAspectRatio() const
{
	VERUS_QREF_CONST_SETTINGS;
	return static_cast<float>(_swapChainWidth) / static_cast<float>(_swapChainHeight);
}

void Renderer::ImGuiSetCurrentContext(ImGuiContext* pContext)
{
	ImGui::SetCurrentContext(pContext);
}

void Renderer::ImGuiUpdateStyle()
{
	VERUS_QREF_CONST_SETTINGS;

	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowRounding = 4;
	style.WindowTitleAlign.x = 0.5f;
	style.ChildRounding = 2;
	style.PopupRounding = 2;
	style.FramePadding.y = 4;
	style.FrameRounding = 2;
	style.FrameBorderSize = 1;
	style.ScrollbarSize = 16;
	style.ScrollbarRounding = 2;
	style.GrabMinSize = 12;
	style.GrabRounding = 2;
	style.TabRounding = 0;
	style.TabBorderSize = 1;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.99f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.99f);
	colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.05f, 0.05f, 0.05f, 0.08f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.19f, 0.03f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.92f, 0.43f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.92f, 0.43f, 0.02f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.92f, 0.43f, 0.02f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.13f);
	colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
	colors[ImGuiCol_Header] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.19f, 0.03f, 0.00f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.89f, 0.13f, 0.00f, 1.00f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

	if (settings._screenAllowHighDPI)
		style.ScaleAllSizes(settings._highDpiScale);
}
