// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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

void Renderer::Init(PRendererDelegate pDelegate, bool allowInitShaders)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;

	_swapChainWidth = settings._displaySizeWidth;
	_swapChainHeight = settings._displaySizeHeight;

	_pRendererDelegate = pDelegate;

	_allowInitShaders = allowInitShaders;

	VERUS_RT_ASSERT(_allowInitShaders || !settings._displayOffscreenDraw);

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

	// Draw directly to swap chain buffer:
	const RP::Attachment::LoadOp colorLoadOp = settings._displayOffscreenDraw ? RP::Attachment::LoadOp::dontCare : RP::Attachment::LoadOp::clear;
	const RP::Attachment::LoadOp depthLoadOp = settings._displayOffscreenDraw ? RP::Attachment::LoadOp::load : RP::Attachment::LoadOp::clear;
	_rphSwapChain = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).SetLoadOp(colorLoadOp).Layout(ImageLayout::undefined, ImageLayout::presentSrc)
		},
		{
			RP::Subpass("Sp0").Color({RP::Ref("Color", ImageLayout::colorAttachment)})
		},
		{});
	_rphSwapChainWithDepth = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).SetLoadOp(colorLoadOp).Layout(ImageLayout::undefined, ImageLayout::presentSrc),
			RP::Attachment("Depth", Format::unormD24uintS8).SetLoadOp(depthLoadOp).Layout(ImageLayout::depthStencilAttachment),
		},
		{
			RP::Subpass("Sp0").Color(
			{
				RP::Ref("Color", ImageLayout::colorAttachment)
			}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
		},
		{});
	// Draw to offscreen buffer, then later to swap chain buffer:
	_rphOffscreen = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly, ImageLayout::fsReadOnly)
		},
		{
			RP::Subpass("Sp0").Color({RP::Ref("Color", ImageLayout::colorAttachment)})
		},
		{});
	_rphOffscreenWithDepth = _pBaseRenderer->CreateRenderPass(
		{
			RP::Attachment("Color", Format::srgbB8G8R8A8).LoadOpClear().Layout(ImageLayout::fsReadOnly, ImageLayout::fsReadOnly),
			RP::Attachment("Depth", Format::unormD24uintS8).Layout(ImageLayout::depthStencilAttachment),
		},
		{
			RP::Subpass("Sp0").Color(
			{
				RP::Ref("Color", ImageLayout::colorAttachment)
			}).DepthStencil(RP::Ref("Depth", ImageLayout::depthStencilAttachment)),
		},
		{});

	GeometryDesc geoDesc;
	geoDesc._name = "Renderer.Geo";
	const VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(Vertex, _pos), ViaType::floats, 2, ViaUsage::position, 0},
		VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_geoQuad.Init(geoDesc);

	if (_allowInitShaders)
	{
		_shader[SHADER_GENERATE_MIPS].Init("[Shaders]:GenerateMips.hlsl");
		_shader[SHADER_GENERATE_MIPS]->CreateDescriptorSet(0, &_ubGenerateMips, sizeof(_ubGenerateMips), settings.GetLimits()._generateMips_ubCapacity,
			{
				Sampler::linearClampMipN,
				Sampler::storage,
				Sampler::storage,
				Sampler::storage,
				Sampler::storage
			},
			ShaderStageFlags::cs);
		_shader[SHADER_GENERATE_MIPS]->CreatePipelineLayout();

		_shader[SHADER_QUAD].Init("[Shaders]:Quad.hlsl");
		_shader[SHADER_QUAD]->CreateDescriptorSet(0, &_ubQuadVS, sizeof(_ubQuadVS), settings.GetLimits()._quad_ubVSCapacity, {}, ShaderStageFlags::vs);
		_shader[SHADER_QUAD]->CreateDescriptorSet(1, &_ubQuadFS, sizeof(_ubQuadFS), settings.GetLimits()._quad_ubFSCapacity, { Sampler::linearClampMipN }, ShaderStageFlags::fs);
		_shader[SHADER_QUAD]->CreatePipelineLayout();
	}

	if (_allowInitShaders)
	{
		PipelineDesc pipeDesc(_shader[SHADER_GENERATE_MIPS], "#");
		_pipe[PIPE_GENERATE_MIPS].Init(pipeDesc);
	}
	if (_allowInitShaders)
	{
		PipelineDesc pipeDesc(_shader[SHADER_GENERATE_MIPS], "#Exposure");
		_pipe[PIPE_GENERATE_MIPS_EXPOSURE].Init(pipeDesc);
	}
	if (settings._displayOffscreenDraw)
	{
		PipelineDesc pipeDesc(_geoQuad, _shader[SHADER_QUAD], "#", _rphSwapChainWithDepth);
		pipeDesc._topology = PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_OFFSCREEN_COLOR].Init(pipeDesc);
	}

	OnSwapChainResized(true, false);

	_pBaseRenderer->ImGuiInit(_rphSwapChainWithDepth);
	ImGuiUpdateStyle();

	if (_allowInitShaders)
		_ds.Init();

	SetExposureValue(15);
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
		_tex.Done();
		_pipe.Done();
		_shader.Done();
		_geoQuad.Done();
		_commandBuffer.Done();

		Effects::Particles::DoneStatic();
		Scene::Forest::DoneStatic();
		Scene::Grass::DoneStatic();
		Scene::Terrain::DoneStatic();
		Scene::Mesh::DoneStatic();
		GUI::Font::DoneStatic();

		_pBaseRenderer->ReleaseMe();
		_pBaseRenderer = nullptr;
	}

	VERUS_SMART_DELETE(_pRendererDelegate);

	VERUS_DONE(Renderer);
}

void Renderer::Update()
{
	VERUS_QREF_TIMER;

	if (!_tex[TEX_OFFSCREEN_COLOR])
	{
		_exposure[0] = _exposure[1] = 1;
		return;
	}

	UINT32 color = VERUS_COLOR_RGBA(127, 127, 127, 255);
	if (_autoExposure && _tex[TEX_OFFSCREEN_COLOR]->ReadbackSubresource(&color))
	{
		float floatColor[4];
		Convert::ColorInt32ToFloat(color, floatColor);

		const glm::vec3 glmColor(floatColor[2], floatColor[1], floatColor[0]);
		const glm::vec3 gray = glm::saturation(0.f, glmColor);

		const float alpha = Math::Max(0.001f, floatColor[3]);
		const float actual = gray.r;
		const float expScale = Math::Clamp(_exposure[1] * (1 / 15.f), 0.f, 1.f);
		const float target = -0.3f + 0.6f * expScale * expScale; // Dark scene exposure compensation.
		const float important = (actual - 0.5f * (1 - alpha)) / alpha;
		const float delta = abs(target - important);
		const float speed = delta * sqrt(delta) * 15;

		if (important < target * 0.95f)
			_exposure[1] -= speed * dt;
		else if (important > target * (1 / 0.95f))
			_exposure[1] += speed * dt;

		_exposure[1] = Math::Clamp(_exposure[1], 0.f, 15.f);
		SetExposureValue(_exposure[1]);
	}
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
	_fps = Math::Lerp(_fps, timer.GetDeltaTimeInv(), 0.25f);
}

bool Renderer::OnWindowSizeChanged(int w, int h)
{
	if (_swapChainWidth == w && _swapChainHeight == h)
		return false;

	_pBaseRenderer->WaitIdle();

	_swapChainWidth = w;
	_swapChainHeight = h;
	_pBaseRenderer->ResizeSwapChain();

	OnSwapChainResized(true, true);
	if (_ds.IsInitialized())
		_ds.OnSwapChainResized(true, true);
	if (Scene::Water::IsValidSingleton())
		Scene::Water::I().OnSwapChainResized();
	if (Effects::Bloom::IsValidSingleton())
		Effects::Bloom::I().OnSwapChainResized();
	if (Effects::Ssao::IsValidSingleton())
		Effects::Ssao::I().OnSwapChainResized();
	if (Effects::Ssr::IsValidSingleton())
		Effects::Ssr::I().OnSwapChainResized();
	if (Effects::Blur::IsValidSingleton())
		Effects::Blur::I().OnSwapChainResized();

	return true;
}

void Renderer::OnSwapChainResized(bool init, bool done)
{
	if (done)
	{
		if (_shader[SHADER_QUAD])
			_shader[SHADER_QUAD]->FreeDescriptorSet(_cshOffscreenColor);

		_pBaseRenderer->DeleteFramebuffer(_fbhOffscreenWithDepth);
		_pBaseRenderer->DeleteFramebuffer(_fbhOffscreen);

		VERUS_FOR(i, _fbhSwapChainWithDepth.size())
			_pBaseRenderer->DeleteFramebuffer(_fbhSwapChainWithDepth[i]);
		VERUS_FOR(i, _fbhSwapChain.size())
			_pBaseRenderer->DeleteFramebuffer(_fbhSwapChain[i]);

		_tex.Done();
	}

	if (init)
	{
		VERUS_QREF_CONST_SETTINGS;

		TextureDesc texDesc;
		if (settings._displayOffscreenDraw)
		{
			texDesc._name = "Renderer.OffscreenColor";
			texDesc._format = Format::srgbB8G8R8A8;
			texDesc._width = _swapChainWidth;
			texDesc._height = _swapChainHeight;
			texDesc._mipLevels = 0;
			texDesc._flags = TextureDesc::Flags::colorAttachment | TextureDesc::Flags::generateMips | TextureDesc::Flags::exposureMips;
			texDesc._readbackMip = -1;
			_tex[TEX_OFFSCREEN_COLOR].Init(texDesc);
		}
		texDesc.Reset();
		texDesc._clearValue = Vector4(1);
		texDesc._name = "Renderer.DepthStencil";
		texDesc._format = Format::unormD24uintS8;
		texDesc._width = _swapChainWidth;
		texDesc._height = _swapChainHeight;
		texDesc._flags = TextureDesc::Flags::inputAttachment | TextureDesc::Flags::depthSampledW;
		_tex[TEX_DEPTH_STENCIL].Init(texDesc);

		_fbhSwapChain.resize(_pBaseRenderer->GetSwapChainBufferCount());
		VERUS_FOR(i, _fbhSwapChain.size())
			_fbhSwapChain[i] = _pBaseRenderer->CreateFramebuffer(_rphSwapChain, {}, _swapChainWidth, _swapChainHeight, i);
		_fbhSwapChainWithDepth.resize(_pBaseRenderer->GetSwapChainBufferCount());
		VERUS_FOR(i, _fbhSwapChainWithDepth.size())
			_fbhSwapChainWithDepth[i] = _pBaseRenderer->CreateFramebuffer(_rphSwapChainWithDepth, { _tex[TEX_DEPTH_STENCIL] }, _swapChainWidth, _swapChainHeight, i);

		if (settings._displayOffscreenDraw)
		{
			_fbhOffscreen = _pBaseRenderer->CreateFramebuffer(_rphSwapChain, { _tex[TEX_OFFSCREEN_COLOR] }, _swapChainWidth, _swapChainHeight);
			_fbhOffscreenWithDepth = _pBaseRenderer->CreateFramebuffer(_rphSwapChainWithDepth, { _tex[TEX_OFFSCREEN_COLOR], _tex[TEX_DEPTH_STENCIL] }, _swapChainWidth, _swapChainHeight);

			_cshOffscreenColor = _shader[SHADER_QUAD]->BindDescriptorSetTextures(1, { _tex[TEX_OFFSCREEN_COLOR] });
		}
	}
}

void Renderer::DrawQuad(PBaseCommandBuffer pCB)
{
	if (!pCB)
		pCB = _commandBuffer.Get();
	pCB->BindVertexBuffers(_geoQuad);
	pCB->Draw(4);
}

void Renderer::DrawOffscreenColor(PBaseCommandBuffer pCB, bool endRenderPass)
{
	if (!App::Settings::I()._displayOffscreenDraw)
		return;

	if (!pCB)
		pCB = _commandBuffer.Get();

	_tex[TEX_OFFSCREEN_COLOR]->GenerateMips();

	_ubQuadVS._matW = Math::QuadMatrix().UniformBufferFormat();
	_ubQuadVS._matV = Math::ToUVMatrix().UniformBufferFormat();

	pCB->BeginRenderPass(_rphSwapChainWithDepth, _fbhSwapChainWithDepth[_pBaseRenderer->GetSwapChainBufferIndex()], { Vector4(0), Vector4(1) });

	pCB->BindPipeline(_pipe[PIPE_OFFSCREEN_COLOR]);
	pCB->BindVertexBuffers(_geoQuad);
	_shader[SHADER_QUAD]->BeginBindDescriptors();
	pCB->BindDescriptors(_shader[SHADER_QUAD], 0);
	pCB->BindDescriptors(_shader[SHADER_QUAD], 1, _cshOffscreenColor);
	_shader[SHADER_QUAD]->EndBindDescriptors();
	pCB->Draw(4);

	if (endRenderPass)
		pCB->EndRenderPass();
}

void Renderer::DrawOffscreenColorSwitchRenderPass(PBaseCommandBuffer pCB)
{
	if (!App::Settings::I()._displayOffscreenDraw)
		return;

	if (!pCB)
		pCB = _commandBuffer.Get();

	pCB->EndRenderPass();

	DrawOffscreenColor(pCB, false);
}

TexturePtr Renderer::GetTexOffscreenColor() const
{
	return _tex[TEX_OFFSCREEN_COLOR];
}

TexturePtr Renderer::GetTexDepthStencil() const
{
	return _tex[TEX_DEPTH_STENCIL];
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

	const ImVec4 darkColor(0.01f, 0.01f, 0.01f, 1.00f);
	const ImVec4 windowColor(0.02f, 0.02f, 0.02f, 0.99f);
	const ImVec4 baseColor(0.02f, 0.02f, 0.02f, 1.00f);
	const ImVec4 borderColor(0.05f, 0.05f, 0.05f, 1.00f);
	const ImVec4 hoveredColor(0.09f, 0.09f, 0.09f, 1.00f);
	const ImVec4 activeColor(0.19f, 0.19f, 0.19f, 1.00f);
	const ImVec4 disabledColor(0.39f, 0.39f, 0.39f, 1.00f);
	const ImVec4 satColorA(0.92f, 0.43f, 0.02f, 1.00f);
	const ImVec4 satColorB(0.89f, 0.13f, 0.00f, 1.00f);
	const ImVec4 titleColor(0.19f, 0.03f, 0.00f, 1.00f);

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
	colors[ImGuiCol_TextDisabled] = disabledColor;
	colors[ImGuiCol_WindowBg] = windowColor;
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_PopupBg] = windowColor;
	colors[ImGuiCol_Border] = borderColor;
	colors[ImGuiCol_BorderShadow] = ImVec4(0.05f, 0.05f, 0.05f, 0.08f);
	colors[ImGuiCol_FrameBg] = darkColor;
	colors[ImGuiCol_FrameBgHovered] = baseColor;
	colors[ImGuiCol_FrameBgActive] = borderColor;
	colors[ImGuiCol_TitleBg] = darkColor;
	colors[ImGuiCol_TitleBgActive] = titleColor;
	colors[ImGuiCol_TitleBgCollapsed] = darkColor;
	colors[ImGuiCol_MenuBarBg] = darkColor;

	colors[ImGuiCol_ScrollbarBg] = baseColor;
	colors[ImGuiCol_ScrollbarGrab] = borderColor;
	colors[ImGuiCol_ScrollbarGrabHovered] = hoveredColor;
	colors[ImGuiCol_ScrollbarGrabActive] = activeColor;

	colors[ImGuiCol_CheckMark] = satColorA;
	colors[ImGuiCol_SliderGrab] = hoveredColor;
	colors[ImGuiCol_SliderGrabActive] = satColorA;
	colors[ImGuiCol_Button] = borderColor;
	colors[ImGuiCol_ButtonHovered] = hoveredColor;
	colors[ImGuiCol_ButtonActive] = activeColor;
	colors[ImGuiCol_Header] = borderColor;
	colors[ImGuiCol_HeaderHovered] = hoveredColor;
	colors[ImGuiCol_HeaderActive] = activeColor;

	colors[ImGuiCol_Separator] = borderColor;
	colors[ImGuiCol_SeparatorHovered] = hoveredColor;
	colors[ImGuiCol_SeparatorActive] = satColorB;
	colors[ImGuiCol_ResizeGrip] = borderColor;
	colors[ImGuiCol_ResizeGripHovered] = hoveredColor;
	colors[ImGuiCol_ResizeGripActive] = satColorB;

	colors[ImGuiCol_Tab] = baseColor;
	colors[ImGuiCol_TabHovered] = hoveredColor;
	colors[ImGuiCol_TabActive] = titleColor;
	colors[ImGuiCol_TabUnfocused] = darkColor;
	colors[ImGuiCol_TabUnfocusedActive] = borderColor;

	colors[ImGuiCol_PlotLines] = disabledColor;
	colors[ImGuiCol_PlotLinesHovered] = satColorB;
	colors[ImGuiCol_PlotHistogram] = disabledColor;
	colors[ImGuiCol_PlotHistogramHovered] = satColorB;

	colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_DragDropTarget] = satColorB;
	colors[ImGuiCol_NavHighlight] = satColorB;
	colors[ImGuiCol_NavWindowingHighlight] = satColorB;
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

	if (settings._displayAllowHighDPI)
		style.ScaleAllSizes(settings._highDpiScale);
}

RPHandle Renderer::GetRenderPassHandle_SwapChain() const
{
	return _rphSwapChain;
}

RPHandle Renderer::GetRenderPassHandle_SwapChainWithDepth() const
{
	return _rphSwapChainWithDepth;
}

RPHandle Renderer::GetRenderPassHandle_Offscreen() const
{
	return _rphOffscreen;
}

RPHandle Renderer::GetRenderPassHandle_OffscreenWithDepth() const
{
	return _rphOffscreenWithDepth;
}

RPHandle Renderer::GetRenderPassHandle_Auto() const
{
	return App::Settings::I()._displayOffscreenDraw ? _rphOffscreen : _rphSwapChain;
}

RPHandle Renderer::GetRenderPassHandle_AutoWithDepth() const
{
	return App::Settings::I()._displayOffscreenDraw ? _rphOffscreenWithDepth : _rphSwapChainWithDepth;
}

FBHandle Renderer::GetFramebufferHandle_SwapChain(int index) const
{
	return _fbhSwapChain[index];
}

FBHandle Renderer::GetFramebufferHandle_SwapChainWithDepth(int index) const
{
	return _fbhSwapChainWithDepth[index];
}

FBHandle Renderer::GetFramebufferHandle_Offscreen(int index) const
{
	return _fbhOffscreen;
}

FBHandle Renderer::GetFramebufferHandle_OffscreenWithDepth(int index) const
{
	return _fbhOffscreenWithDepth;
}

FBHandle Renderer::GetFramebufferHandle_Auto(int index) const
{
	return App::Settings::I()._displayOffscreenDraw ? _fbhOffscreen : _fbhSwapChain[index];
}

FBHandle Renderer::GetFramebufferHandle_AutoWithDepth(int index) const
{
	return App::Settings::I()._displayOffscreenDraw ? _fbhOffscreenWithDepth : _fbhSwapChainWithDepth[index];
}

ShaderPtr Renderer::GetShaderGenerateMips() const
{
	return _shader[SHADER_GENERATE_MIPS];
}

PipelinePtr Renderer::GetPipelineGenerateMips(bool exposure) const
{
	return _pipe[exposure ? PIPE_GENERATE_MIPS_EXPOSURE : PIPE_GENERATE_MIPS];
}

Renderer::UB_GenerateMips& Renderer::GetUbGenerateMips()
{
	return _ubGenerateMips;
}

GeometryPtr Renderer::GetGeoQuad() const
{
	return _geoQuad;
}

ShaderPtr Renderer::GetShaderQuad() const
{
	return _shader[SHADER_QUAD];
}

Renderer::UB_QuadVS& Renderer::GetUbQuadVS()
{
	return _ubQuadVS;
}

Renderer::UB_QuadFS& Renderer::GetUbQuadFS()
{
	return _ubQuadFS;
}

void Renderer::SetExposureValue(float ev)
{
	_exposure[1] = ev;
	_exposure[0] = 1.f / exp2(_exposure[1] - 1);
}

void Renderer::UpdateUtilization()
{
	if (!_showUtilization)
		return;
	_vUtilization.clear();
	_pBaseRenderer->UpdateUtilization();
	std::sort(_vUtilization.begin(), _vUtilization.end(), [](RcUtilization a, RcUtilization b)
		{
			if (a._fraction != b._fraction)
				return a._fraction < b._fraction;
			if (a._total != b._total)
				return a._total > b._total;
			return a._value < b._value;
		});
	for (const auto& x : _vUtilization)
	{
		StringStream ss;
		ss << "(" << x._value << "/" << x._total << ")";
		ImGui::ProgressBar(x._fraction, ImVec2(0, 0), _C(ss.str()));
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::Text(_C(x._name));
	}
}

void Renderer::AddUtilization(CSZ name, INT64 value, INT64 total)
{
	if (!total)
		return;
	Utilization u;
	u._name = name;
	u._value = value;
	u._total = total;
	u._fraction = value / static_cast<float>(total);
	_vUtilization.push_back(std::move(u));
}
