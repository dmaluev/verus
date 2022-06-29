// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

RendererD3D11::RendererD3D11()
{
}

RendererD3D11::~RendererD3D11()
{
	Done();
}

void RendererD3D11::ReleaseMe()
{
	Free();
	TestAllocCount();
}

void RendererD3D11::Init()
{
	VERUS_INIT();

	_vRenderPasses.reserve(20);
	_vFramebuffers.reserve(40);

	InitD3D();
}

void RendererD3D11::Done()
{
	WaitIdle();

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		Renderer::I().ImGuiSetCurrentContext(nullptr);
	}

	DeleteFramebuffer(FBHandle::Make(-2));
	DeleteRenderPass(RPHandle::Make(-2));

	_vSamplers.clear();

	_pSwapChainBufferRTV.Reset();
	_pSwapChainBuffer.Reset();
	VERUS_COM_RELEASE_CHECK(_pSwapChain.Get());
	_pSwapChain.Reset();
	VERUS_COM_RELEASE_CHECK(_pDeviceContext.Get());
	_pDeviceContext.Reset();
	VERUS_COM_RELEASE_CHECK(_pDevice.Get());
	_pDevice.Reset();

	VERUS_DONE(RendererD3D11);
}

ComPtr<IDXGIFactory1> RendererD3D11::CreateFactory()
{
	HRESULT hr = 0;
	ComPtr<IDXGIFactory1> pFactory;
	if (FAILED(hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory)))) // DXGI 1.1, Windows 7.
		throw VERUS_RUNTIME_ERROR << "CreateDXGIFactory1(); hr=" << VERUS_HR(hr);
	return pFactory;
}

ComPtr<IDXGIAdapter1> RendererD3D11::GetAdapter(ComPtr<IDXGIFactory1> pFactory)
{
	ComPtr<IDXGIAdapter1> pAdapter;
	for (UINT index = 0; SUCCEEDED(pFactory->EnumAdapters1(index, &pAdapter)); ++index)
	{
		DXGI_ADAPTER_DESC1 adapterDesc = {};
		if (SUCCEEDED(pAdapter->GetDesc1(&adapterDesc)))
			break;
		pAdapter.Reset();
	}

	if (!pAdapter)
		throw VERUS_RUNTIME_ERROR << "GetAdapter(); Adapter not found";

	DXGI_ADAPTER_DESC1 adapterDesc = {};
	pAdapter->GetDesc1(&adapterDesc);
	const String description = Str::WideToUtf8(adapterDesc.Description);
	VERUS_LOG_INFO("Adapter desc: " << description);
	return pAdapter;
}

void RendererD3D11::CreateSwapChainBufferRTV()
{
	HRESULT hr = 0;
	if (FAILED(hr = _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&_pSwapChainBuffer))))
		throw VERUS_RUNTIME_ERROR << "GetBuffer(); hr=" << VERUS_HR(hr);
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	if (FAILED(hr = _pDevice->CreateRenderTargetView(_pSwapChainBuffer.Get(), &rtvDesc, &_pSwapChainBufferRTV)))
		throw VERUS_RUNTIME_ERROR << "CreateRenderTargetView(); hr=" << VERUS_HR(hr);
}

void RendererD3D11::InitD3D()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	// <SDL>
	SDL_Window* pWnd = renderer.GetMainWindow()->GetSDL();
	VERUS_RT_ASSERT(pWnd);
	SDL_SysWMinfo wmInfo = {};
	SDL_VERSION(&wmInfo.version);
	if (!SDL_GetWindowWMInfo(pWnd, &wmInfo))
		throw VERUS_RUNTIME_ERROR << "SDL_GetWindowWMInfo()";
	// </SDL>

	_featureLevel = D3D_FEATURE_LEVEL_11_0;

	ComPtr<IDXGIFactory1> pFactory = CreateFactory();
	ComPtr<IDXGIAdapter1> pAdapter = GetAdapter(pFactory);

	VERUS_LOG_INFO("Using feature level: 11_0");

	_swapChainBufferCount = settings._displayVSync ? 3 : 2;

	_swapChainDesc.BufferDesc.Width = renderer.GetSwapChainWidth();
	_swapChainDesc.BufferDesc.Height = renderer.GetSwapChainHeight();
	_swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	_swapChainDesc.SampleDesc.Count = 1;
	_swapChainDesc.SampleDesc.Quality = 0;
	_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_swapChainDesc.BufferCount = _swapChainBufferCount;
	_swapChainDesc.OutputWindow = wmInfo.info.win.window;
	_swapChainDesc.Windowed = (App::DisplayMode::windowed == settings._displayMode);
	_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT flags = 0;
#ifdef _DEBUG
	flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

	if (FAILED(hr = D3D11CreateDeviceAndSwapChain(
		pAdapter.Get(),
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		flags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&_swapChainDesc,
		&_pSwapChain,
		&_pDevice,
		&_featureLevel,
		&_pDeviceContext)))
		throw VERUS_RUNTIME_ERROR << "D3D11CreateDeviceAndSwapChain(); hr=" << VERUS_HR(hr);

#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	ComPtr<ID3D11InfoQueue> pInfoQueue;
	if (SUCCEEDED(_pDevice.As(&pInfoQueue)))
	{
		D3D11_MESSAGE_ID denyList[] =
		{
			D3D11_MESSAGE_ID_DEVICE_VSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_HSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_DSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_GSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_PSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_CSSETSHADERRESOURCES_HAZARD,
			D3D11_MESSAGE_ID_DEVICE_OMSETRENDERTARGETS_HAZARD,
			D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS
		};
		D3D11_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = VERUS_COUNT_OF(denyList);
		filter.DenyList.pIDList = denyList;
		pInfoQueue->AddStorageFilterEntries(&filter);
		pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
	}
#endif

	CreateSwapChainBufferRTV();

	CreateSamplers();
}

void RendererD3D11::CreateSamplers()
{
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	const bool tf = settings._gpuTrilinearFilter;
	auto ApplyTrilinearFilter = [tf](const D3D11_FILTER filter)
	{
		return tf ? static_cast<D3D11_FILTER>(filter + 1) : filter;
	};

	_vSamplers.resize(+Sampler::count);

	D3D11_SAMPLER_DESC desc = {};
	D3D11_SAMPLER_DESC init = {};
	init.Filter = ApplyTrilinearFilter(D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT);
	init.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	init.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	init.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	init.MipLODBias = 0;
	init.MaxAnisotropy = 0;
	init.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	init.MinLOD = 0;
	init.MaxLOD = D3D11_FLOAT32_MAX;

	desc = init;
	desc.Filter = D3D11_FILTER_ANISOTROPIC;
	desc.MipLODBias = -2;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::lodBias])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = (settings._sceneShadowQuality <= App::Settings::Quality::low) ?
		D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::shadow])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_ANISOTROPIC;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::aniso])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_ANISOTROPIC;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::anisoClamp])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_ANISOTROPIC;
	desc.MipLODBias = -0.5f;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::anisoSharp])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	// <Wrap>
	desc = init;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::linearMipL])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = ApplyTrilinearFilter(D3D11_FILTER_MIN_MAG_MIP_POINT);
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::nearestMipL])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::linearMipN])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::nearestMipN])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);
	// </Wrap>

	// <Clamp>
	desc = init;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::linearClampMipL])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = ApplyTrilinearFilter(D3D11_FILTER_MIN_MAG_MIP_POINT);
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::nearestClampMipL])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::linearClampMipN])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);

	desc = init;
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	if (FAILED(hr = _pDevice->CreateSamplerState(&desc, &_vSamplers[+Sampler::nearestClampMipN])))
		throw VERUS_RUNTIME_ERROR << "CreateSamplerState(); hr=" << VERUS_HR(hr);
	// </Clamp>
}

ID3D11SamplerState* RendererD3D11::GetD3DSamplerState(Sampler s) const
{
	return _vSamplers[+s].Get();
}

void RendererD3D11::ImGuiInit(RPHandle renderPassHandle)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	IMGUI_CHECKVERSION();
	ImGuiContext* pContext = ImGui::CreateContext();
	renderer.ImGuiSetCurrentContext(pContext);
	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	io.IniFilename = nullptr;
	if (!settings._imguiFont.empty())
	{
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(_C(settings._imguiFont), vData);
		void* pFontData = IM_ALLOC(vData.size());
		memcpy(pFontData, vData.data(), vData.size());
		io.Fonts->AddFontFromMemoryTTF(pFontData, Utils::Cast32(vData.size()), static_cast<float>(settings.GetFontSize()), nullptr, io.Fonts->GetGlyphRangesCyrillic());
	}

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForD3D(renderer.GetMainWindow()->GetSDL());
	ImGui_ImplDX11_Init(_pDevice.Get(), _pDeviceContext.Get());
}

void RendererD3D11::ImGuiRenderDrawData()
{
	VERUS_QREF_RENDERER;
	renderer.UpdateUtilization();
	ImGui::Render();
	if (ImGui::GetDrawData())
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void RendererD3D11::ResizeSwapChain()
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	_pSwapChainBufferRTV.Reset();
	_pSwapChainBuffer.Reset();

	if (FAILED(hr = _pSwapChain->ResizeBuffers(
		_swapChainDesc.BufferCount,
		renderer.GetSwapChainWidth(),
		renderer.GetSwapChainHeight(),
		_swapChainDesc.BufferDesc.Format,
		_swapChainDesc.Flags)))
		throw VERUS_RUNTIME_ERROR << "ResizeBuffers(); hr=" << VERUS_HR(hr);

	CreateSwapChainBufferRTV();
}

void RendererD3D11::BeginFrame()
{
	_swapChainBufferIndex = -1;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void RendererD3D11::AcquireSwapChainImage()
{
	_swapChainBufferIndex = 0;
}

void RendererD3D11::EndFrame()
{
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	UpdateScheduled();

	ImGui::EndFrame();

	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;

	// <Present>
	if (_swapChainBufferIndex >= 0)
	{
		UINT syncInterval = settings._displayVSync ? 1 : 0;
		UINT flags = 0;
		if (FAILED(hr = _pSwapChain->Present(syncInterval, flags)))
			throw VERUS_RUNTIME_ERROR << "Present(); hr=" << VERUS_HR(hr);
	}
	// </Present>
}

void RendererD3D11::WaitIdle()
{
	if (_pDeviceContext)
		_pDeviceContext->Flush();
}

void RendererD3D11::OnMinimized()
{
	WaitIdle();
	_pSwapChain->Present(0, 0);
}

// Resources:

PBaseCommandBuffer RendererD3D11::InsertCommandBuffer()
{
	return TStoreCommandBuffers::Insert();
}

PBaseGeometry RendererD3D11::InsertGeometry()
{
	return TStoreGeometry::Insert();
}

PBasePipeline RendererD3D11::InsertPipeline()
{
	return TStorePipelines::Insert();
}

PBaseShader RendererD3D11::InsertShader()
{
	return TStoreShaders::Insert();
}

PBaseTexture RendererD3D11::InsertTexture()
{
	return TStoreTextures::Insert();
}

void RendererD3D11::DeleteCommandBuffer(PBaseCommandBuffer p)
{
	TStoreCommandBuffers::Delete(static_cast<PCommandBufferD3D11>(p));
}

void RendererD3D11::DeleteGeometry(PBaseGeometry p)
{
	TStoreGeometry::Delete(static_cast<PGeometryD3D11>(p));
}

void RendererD3D11::DeletePipeline(PBasePipeline p)
{
	TStorePipelines::Delete(static_cast<PPipelineD3D11>(p));
}

void RendererD3D11::DeleteShader(PBaseShader p)
{
	TStoreShaders::Delete(static_cast<PShaderD3D11>(p));
}

void RendererD3D11::DeleteTexture(PBaseTexture p)
{
	TStoreTextures::Delete(static_cast<PTextureD3D11>(p));
}

RPHandle RendererD3D11::CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD)
{
	RP::D3DRenderPass renderPass;

	renderPass._vAttachments.reserve(ilA.size());
	for (const auto& attachment : ilA)
	{
		RP::D3DAttachment d3dAttach;
		d3dAttach._format = attachment._format;
		d3dAttach._sampleCount = attachment._sampleCount;
		d3dAttach._loadOp = attachment._loadOp;
		d3dAttach._storeOp = attachment._storeOp;
		d3dAttach._stencilLoadOp = attachment._stencilLoadOp;
		d3dAttach._stencilStoreOp = attachment._stencilStoreOp;
		renderPass._vAttachments.push_back(std::move(d3dAttach));
	}

	auto GetAttachmentIndexByName = [&ilA](CSZ name) -> int
	{
		if (!name)
			return -1;
		int index = 0;
		for (const auto& attachment : ilA)
		{
			if (!strcmp(attachment._name, name))
				return index;
			index++;
		}
		throw VERUS_RECOVERABLE << "CreateRenderPass(); Attachment not found";
	};

	renderPass._vSubpasses.reserve(ilS.size());
	for (const auto& subpass : ilS)
	{
		RP::D3DSubpass d3dSubpass;

		d3dSubpass._vInput.reserve(subpass._ilInput.size());
		for (const auto& input : subpass._ilInput)
		{
			RP::D3DRef ref;
			ref._index = GetAttachmentIndexByName(input._name);
			d3dSubpass._vInput.push_back(ref);
		}

		d3dSubpass._vColor.reserve(subpass._ilColor.size());
		for (const auto& color : subpass._ilColor)
		{
			RP::D3DRef ref;
			ref._index = GetAttachmentIndexByName(color._name);
			d3dSubpass._vColor.push_back(ref);

			if (-1 == renderPass._vAttachments[ref._index]._clearSubpassIndex)
				renderPass._vAttachments[ref._index]._clearSubpassIndex = Utils::Cast32(renderPass._vSubpasses.size());
		}

		if (subpass._depthStencil._name)
		{
			d3dSubpass._depthStencil._index = GetAttachmentIndexByName(subpass._depthStencil._name);
			d3dSubpass._depthStencilReadOnly = (ImageLayout::depthStencilReadOnly == subpass._depthStencil._layout);

			if (-1 == renderPass._vAttachments[d3dSubpass._depthStencil._index]._clearSubpassIndex)
				renderPass._vAttachments[d3dSubpass._depthStencil._index]._clearSubpassIndex = Utils::Cast32(renderPass._vSubpasses.size());
		}

		d3dSubpass._vPreserve.reserve(subpass._ilPreserve.size());
		for (const auto& preserve : subpass._ilPreserve)
			d3dSubpass._vPreserve.push_back(GetAttachmentIndexByName(preserve._name));

		renderPass._vSubpasses.push_back(std::move(d3dSubpass));
	}

	const int nextIndex = GetNextRenderPassIndex();
	if (nextIndex >= _vRenderPasses.size())
		_vRenderPasses.push_back(std::move(renderPass));
	else
		_vRenderPasses[nextIndex] = renderPass;

	return RPHandle::Make(nextIndex);
}

FBHandle RendererD3D11::CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex, CubeMapFace cubeMapFace)
{
	RP::RcD3DRenderPass renderPass = GetRenderPass(renderPassHandle);
	RP::D3DFramebuffer framebuffer;
	framebuffer._width = w;
	framebuffer._height = h;
	framebuffer._cubeMapFace = cubeMapFace;

	const Vector<TexturePtr> vTex(il);
	auto GetRTV = [this, &vTex, swapChainBufferIndex, cubeMapFace](int index) -> ID3D11RenderTargetView*
	{
		if (swapChainBufferIndex >= 0)
		{
			if (index)
			{
				auto& texD3D11 = static_cast<RTextureD3D11>(*vTex[index - 1]);
				return texD3D11.GetRTV();
			}
			else
			{
				return _pSwapChainBufferRTV.Get();
			}
		}
		else
		{
			auto& texD3D11 = static_cast<RTextureD3D11>(*vTex[index]);
			switch (cubeMapFace)
			{
			case CubeMapFace::all:  return texD3D11.GetRTV(index); break;
			case CubeMapFace::none: return texD3D11.GetRTV(); break;
			default:                return texD3D11.GetRTV(!index ? +cubeMapFace : 0);
			}
		}
	};
	auto GetDSV = [this, &vTex, swapChainBufferIndex](int index, bool depthStencilReadOnly) -> ID3D11DepthStencilView*
	{
		if (swapChainBufferIndex >= 0)
		{
			if (index)
			{
				auto& texD3D11 = static_cast<RTextureD3D11>(*vTex[index - 1]);
				return texD3D11.GetDSV(depthStencilReadOnly);
			}
			else
			{
				VERUS_RT_ASSERT("Using swapChainBufferIndex for DSV");
				return nullptr;
			}
		}
		else
		{
			auto& texD3D11 = static_cast<RTextureD3D11>(*vTex[index]);
			return texD3D11.GetDSV(depthStencilReadOnly);
		}
	};

	VERUS_FOR(i, renderPass._vAttachments.size())
	{
		if (!i && -1 == swapChainBufferIndex)
			framebuffer._mipLevels = vTex.front()->GetMipLevelCount();
	}

	framebuffer._vSubpasses.reserve(renderPass._vSubpasses.size());
	for (const auto& subpass : renderPass._vSubpasses)
	{
		RP::D3DFramebufferSubpass fs;

		if (!subpass._vColor.empty())
		{
			fs._vRTVs.resize(subpass._vColor.size());
			int index = 0;
			for (const auto& ref : subpass._vColor)
			{
				fs._vRTVs[index] = GetRTV(ref._index);
				index++;
			}
		}
		if (subpass._depthStencil._index >= 0)
		{
			fs._pDSV = GetDSV(subpass._depthStencil._index, subpass._depthStencilReadOnly);
		}

		framebuffer._vSubpasses.push_back(std::move(fs));
	}

	const int nextIndex = GetNextFramebufferIndex();
	if (nextIndex >= _vFramebuffers.size())
		_vFramebuffers.push_back(std::move(framebuffer));
	else
		_vFramebuffers[nextIndex] = framebuffer;

	return FBHandle::Make(nextIndex);
}

void RendererD3D11::DeleteRenderPass(RPHandle handle)
{
	if (handle.IsSet())
		_vRenderPasses[handle.Get()] = RP::D3DRenderPass();
	else if (-2 == handle.Get())
		_vRenderPasses.clear();
}

void RendererD3D11::DeleteFramebuffer(FBHandle handle)
{
	if (handle.IsSet())
		_vFramebuffers[handle.Get()] = RP::D3DFramebuffer();
	else if (-2 == handle.Get())
		_vFramebuffers.clear();
}

int RendererD3D11::GetNextRenderPassIndex() const
{
	const int count = Utils::Cast32(_vRenderPasses.size());
	VERUS_FOR(i, count)
	{
		if (_vRenderPasses[i]._vSubpasses.empty())
			return i;
	}
	return count;
}

int RendererD3D11::GetNextFramebufferIndex() const
{
	const int count = Utils::Cast32(_vFramebuffers.size());
	VERUS_FOR(i, count)
	{
		if (_vFramebuffers[i]._vSubpasses.empty())
			return i;
	}
	return count;
}

RP::RcD3DRenderPass RendererD3D11::GetRenderPass(RPHandle handle) const
{
	return _vRenderPasses[handle.Get()];
}

RP::RcD3DFramebuffer RendererD3D11::GetFramebuffer(FBHandle handle) const
{
	return _vFramebuffers[handle.Get()];
}
