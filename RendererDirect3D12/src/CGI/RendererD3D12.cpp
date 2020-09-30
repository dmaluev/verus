#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

RendererD3D12::RendererD3D12()
{
}

RendererD3D12::~RendererD3D12()
{
	Done();
}

void RendererD3D12::ReleaseMe()
{
	Free();
	TestAllocCount();
}

void RendererD3D12::Init()
{
	VERUS_INIT();

	_vRenderPasses.reserve(20);
	_vFramebuffers.reserve(40);

	InitD3D();
}

void RendererD3D12::Done()
{
	WaitIdle();

	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		Renderer::I().ImGuiSetCurrentContext(nullptr);
	}

	DeleteFramebuffer(FBHandle::Make(-2));
	DeleteRenderPass(RPHandle::Make(-2));

	_vSamplers.clear();

	_dhSamplers.Reset();
	_dhViews.Reset();

	if (_hFrameLatencyWaitableObject != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_hFrameLatencyWaitableObject);
		_hFrameLatencyWaitableObject = INVALID_HANDLE_VALUE;
	}
	if (_hFence != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_hFence);
		_hFence = INVALID_HANDLE_VALUE;
	}
	VERUS_COM_RELEASE_CHECK(_pFence.Get());
	_pFence.Reset();

	VERUS_FOR(i, s_ringBufferSize)
		_mapCommandAllocators[i].clear();
	_dhSwapChainBuffersRTVs.Reset();
	_vSwapChainBuffers.clear();
	VERUS_COM_RELEASE_CHECK(_pSwapChain.Get());
	_pSwapChain.Reset();
	VERUS_COM_RELEASE_CHECK(_pCommandQueue.Get());
	_pCommandQueue.Reset();
	VERUS_SMART_RELEASE(_pMaAllocator);
	VERUS_COM_RELEASE_CHECK(_pDevice.Get());
	_pDevice.Reset();

	VERUS_DONE(RendererD3D12);
}

void RendererD3D12::EnableDebugLayer()
{
	ComPtr<ID3D12Debug> pDebug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
		pDebug->EnableDebugLayer();
}

ComPtr<IDXGIFactory7> RendererD3D12::CreateDXGIFactory()
{
	HRESULT hr = 0;
	ComPtr<IDXGIFactory7> pFactory;
	UINT flags = 0;
#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (FAILED(hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&pFactory))))
		throw VERUS_RUNTIME_ERROR << "CreateDXGIFactory2(), hr=" << VERUS_HR(hr);
	return pFactory;
}

ComPtr<IDXGIAdapter4> RendererD3D12::GetAdapter(ComPtr<IDXGIFactory7> pFactory)
{
	HRESULT hr = 0;
	ComPtr<IDXGIAdapter4> pAdapter;
	if (FAILED(hr = pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter))))
		throw VERUS_RUNTIME_ERROR << "EnumAdapterByGpuPreference(), hr=" << VERUS_HR(hr);

	DXGI_ADAPTER_DESC1 adapterDesc = {};
	pAdapter->GetDesc1(&adapterDesc);
	const String description = Str::WideToUtf8(adapterDesc.Description);
	VERUS_LOG_INFO("Adapter desc: " << description);
	return pAdapter;
}

bool RendererD3D12::CheckFeatureSupportAllowTearing(ComPtr<IDXGIFactory7> pFactory)
{
	BOOL featureSupportData = FALSE;
	if (FAILED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &featureSupportData, sizeof(featureSupportData))))
		featureSupportData = FALSE;
	return featureSupportData == TRUE;
}

void RendererD3D12::CreateSwapChainBuffersRTVs()
{
	HRESULT hr = 0;
	_vSwapChainBuffers.resize(_swapChainBufferCount);
	_dhSwapChainBuffersRTVs.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _swapChainBufferCount);
	VERUS_U_FOR(i, _swapChainBufferCount)
	{
		ComPtr<ID3D12Resource> pBuffer;
		if (FAILED(hr = _pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBuffer))))
			throw VERUS_RUNTIME_ERROR << "GetBuffer(), hr=" << VERUS_HR(hr);
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		_pDevice->CreateRenderTargetView(pBuffer.Get(), &desc, _dhSwapChainBuffersRTVs.AtCPU(i));
		_vSwapChainBuffers[i] = pBuffer;
	}
}

void RendererD3D12::InitD3D()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	HRESULT hr = 0;

#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	EnableDebugLayer();
#endif

	ComPtr<IDXGIFactory7> pFactory = CreateDXGIFactory();

	ComPtr<IDXGIAdapter4> pAdapter = GetAdapter(pFactory);

	const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(hr = D3D12CreateDevice(pAdapter.Get(), featureLevel, IID_PPV_ARGS(&_pDevice))))
		throw VERUS_RUNTIME_ERROR << "D3D12CreateDevice(), hr=" << VERUS_HR(hr);

	VERUS_RT_ASSERT(D3D_FEATURE_LEVEL_11_0 == featureLevel);
	VERUS_LOG_INFO("Using feature level: 11_0");

	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = _pDevice.Get();
	if (FAILED(hr = D3D12MA::CreateAllocator(&allocatorDesc, &_pMaAllocator)))
		throw VERUS_RUNTIME_ERROR << "CreateAllocator(), hr=" << VERUS_HR(hr);

#if defined(_DEBUG) || defined(VERUS_RELEASE_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(_pDevice.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	}
#endif

	_pCommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	const bool allowTearing = CheckFeatureSupportAllowTearing(pFactory);

	_swapChainBufferCount = settings._displayVSync ? 3 : 2;

	_swapChainDesc.Width = settings._displaySizeWidth;
	_swapChainDesc.Height = settings._displaySizeHeight;
	_swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	_swapChainDesc.Stereo = FALSE;
	_swapChainDesc.SampleDesc.Count = 1;
	_swapChainDesc.SampleDesc.Quality = 0;
	_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_swapChainDesc.BufferCount = _swapChainBufferCount;
	_swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	_swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	_swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | (allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	SDL_Window* pWnd = renderer.GetMainWindow()->GetSDL();
	VERUS_RT_ASSERT(pWnd);
	SDL_SysWMinfo wmInfo = {};
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(pWnd, &wmInfo);
	const HWND hWnd = wmInfo.info.win.window;

	ComPtr<IDXGISwapChain1> pSwapChain1;
	if (FAILED(hr = pFactory->CreateSwapChainForHwnd(_pCommandQueue.Get(), hWnd, &_swapChainDesc, nullptr, nullptr, &pSwapChain1)))
		throw VERUS_RUNTIME_ERROR << "CreateSwapChainForHwnd(), hr=" << VERUS_HR(hr);
	if (FAILED(hr = pSwapChain1.As(&_pSwapChain)))
		throw VERUS_RUNTIME_ERROR << "QueryInterface(), hr=" << VERUS_HR(hr);

	_pSwapChain->SetMaximumFrameLatency(3);
	_hFrameLatencyWaitableObject = _pSwapChain->GetFrameLatencyWaitableObject();

	_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainBuffersRTVs();

	VERUS_FOR(i, s_ringBufferSize)
		_mapCommandAllocators[i][std::this_thread::get_id()] = CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_pFence = CreateFence();
	_hFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	_dhViews.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, settings.GetLimits()._d3d12_dhViewsCapacity, 16, true);
	_dhSamplers.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, settings.GetLimits()._d3d12_dhSamplersCapacity, 0, true);

	CreateSamplers();
}

void RendererD3D12::WaitForFrameLatencyWaitableObject()
{
	const DWORD ret = WaitForSingleObjectEx(_hFrameLatencyWaitableObject, 1000, FALSE);
	if (WAIT_OBJECT_0 != ret)
		throw VERUS_RUNTIME_ERROR << "WaitForFrameLatencyWaitableObject(), ret=" << VERUS_HR(ret);
}

ComPtr<ID3D12CommandQueue> RendererD3D12::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	HRESULT hr = 0;
	ComPtr<ID3D12CommandQueue> pCommandQueue;
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	if (FAILED(hr = _pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandQueue(), hr=" << VERUS_HR(hr);
	return pCommandQueue;
}

ComPtr<ID3D12CommandAllocator> RendererD3D12::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type)
{
	HRESULT hr = 0;
	ComPtr<ID3D12CommandAllocator> pCommandAllocator;
	if (FAILED(hr = _pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCommandAllocator))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandAllocator(), hr=" << VERUS_HR(hr);
	return pCommandAllocator;
}

ComPtr<ID3D12GraphicsCommandList3> RendererD3D12::CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> pCommandAllocator)
{
	HRESULT hr = 0;
	ComPtr<ID3D12GraphicsCommandList3> pGraphicsCommandList;
	if (FAILED(hr = _pDevice->CreateCommandList(0, type, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pGraphicsCommandList))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandList(), hr=" << VERUS_HR(hr);
	if (FAILED(hr = pGraphicsCommandList->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);
	return pGraphicsCommandList;
}

ComPtr<ID3D12Fence> RendererD3D12::CreateFence()
{
	HRESULT hr = 0;
	ComPtr<ID3D12Fence> pFence;
	if (FAILED(hr = _pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence))))
		throw VERUS_RUNTIME_ERROR << "CreateFence(), hr=" << VERUS_HR(hr);
	return pFence;
}

UINT64 RendererD3D12::QueueSignal()
{
	HRESULT hr = 0;
	const UINT64 value = _nextFenceValue++;
	if (FAILED(hr = _pCommandQueue->Signal(_pFence.Get(), value)))
		throw VERUS_RUNTIME_ERROR << "Signal(), hr=" << VERUS_HR(hr);
	return value;
}

void RendererD3D12::WaitForFenceValue(UINT64 value)
{
	HRESULT hr = 0;
	if (_pFence->GetCompletedValue() < value)
	{
		if (FAILED(hr = _pFence->SetEventOnCompletion(value, _hFence)))
			throw VERUS_RUNTIME_ERROR << "SetEventOnCompletion(), hr=" << VERUS_HR(hr);
		WaitForSingleObjectEx(_hFence, INFINITE, FALSE);
	}
}

void RendererD3D12::QueueWaitIdle()
{
	if (_pCommandQueue)
	{
		const UINT64 value = QueueSignal();
		WaitForFenceValue(value);
	}
}

void RendererD3D12::CreateSamplers()
{
	VERUS_QREF_CONST_SETTINGS;

	const bool tf = settings._gpuTrilinearFilter;
	auto ApplyTrilinearFilter = [tf](const D3D12_FILTER filter)
	{
		return tf ? static_cast<D3D12_FILTER>(filter + 1) : filter;
	};

	_vSamplers.resize(+Sampler::count);

	D3D12_STATIC_SAMPLER_DESC desc = {};
	D3D12_STATIC_SAMPLER_DESC init = {};
	init.Filter = ApplyTrilinearFilter(D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT);
	init.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	init.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	init.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	init.MipLODBias = 0;
	init.MaxAnisotropy = 0;
	init.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	init.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	init.MinLOD = 0;
	init.MaxLOD = D3D12_FLOAT32_MAX;

	desc = init;
	desc.Filter = (settings._sceneShadowQuality <= App::Settings::ShadowQuality::nearest) ?
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	_vSamplers[+Sampler::shadow] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_ANISOTROPIC;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	_vSamplers[+Sampler::aniso] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_ANISOTROPIC;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	_vSamplers[+Sampler::anisoClamp] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_ANISOTROPIC;
	desc.MipLODBias = -0.5f;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	_vSamplers[+Sampler::anisoSharp] = desc;

	// <Wrap>
	desc = init;
	_vSamplers[+Sampler::linearMipL] = desc;

	desc = init;
	desc.Filter = ApplyTrilinearFilter(D3D12_FILTER_MIN_MAG_MIP_POINT);
	_vSamplers[+Sampler::nearestMipL] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	_vSamplers[+Sampler::linearMipN] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	_vSamplers[+Sampler::nearestMipN] = desc;
	// </Wrap>

	// <Clamp>
	desc = init;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::linearClampMipL] = desc;

	desc = init;
	desc.Filter = ApplyTrilinearFilter(D3D12_FILTER_MIN_MAG_MIP_POINT);
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::nearestClampMipL] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::linearClampMipN] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	desc.AddressU = desc.AddressV = desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::nearestClampMipN] = desc;
	// </Clamp>
}

D3D12_STATIC_SAMPLER_DESC RendererD3D12::GetStaticSamplerDesc(Sampler s) const
{
	return _vSamplers[+s];
}

void RendererD3D12::ImGuiInit(RPHandle renderPassHandle)
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
	auto hp = _dhViews.GetStaticHandlePair(0);
	ImGui_ImplDX12_Init(
		_pDevice.Get(),
		s_ringBufferSize,
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		_dhViews.GetD3DDescriptorHeap(),
		hp._hCPU,
		hp._hGPU);
}

void RendererD3D12::ImGuiRenderDrawData()
{
	VERUS_QREF_RENDERER;
	ImGui::Render();
	auto pCmdList = static_cast<CommandBufferD3D12*>(renderer.GetCommandBuffer().Get())->GetD3DGraphicsCommandList();
	if (ImGui::GetDrawData())
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCmdList);
}

void RendererD3D12::ResizeSwapChain()
{
	VERUS_QREF_RENDERER;

	_dhSwapChainBuffersRTVs.Reset();
	_vSwapChainBuffers.clear();

	_pSwapChain->ResizeBuffers(
		_swapChainDesc.BufferCount,
		renderer.GetSwapChainWidth(),
		renderer.GetSwapChainHeight(),
		_swapChainDesc.Format,
		_swapChainDesc.Flags);

	_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainBuffersRTVs();
}

void RendererD3D12::BeginFrame(bool present)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	if (present)
	{
		WaitForFrameLatencyWaitableObject();
		_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();
	}

	auto pCommandAllocator = _mapCommandAllocators[_ringBufferIndex][std::this_thread::get_id()];
	if (FAILED(hr = pCommandAllocator->Reset()))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);

	auto cb = renderer.GetCommandBuffer();
	cb->Begin();
	SetDescriptorHeaps(cb.Get());

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplSDL2_NewFrame(renderer.GetMainWindow()->GetSDL());
	ImGui::NewFrame();
}

void RendererD3D12::EndFrame(bool present)
{
	VERUS_QREF_RENDERER;

	UpdateScheduled();

	ImGui::EndFrame();

	auto cb = renderer.GetCommandBuffer();
	cb->End();

	ID3D12CommandList* ppCommandLists[] = { static_cast<CommandBufferD3D12*>(cb.Get())->GetD3DGraphicsCommandList() };
	_pCommandQueue->ExecuteCommandLists(VERUS_COUNT_OF(ppCommandLists), ppCommandLists);
}

void RendererD3D12::Present()
{
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	UINT syncInterval = settings._displayVSync ? 1 : 0;
	const bool allowTearing = !!(_swapChainDesc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
	UINT flags = 0;
	if (!syncInterval && allowTearing && App::DisplayMode::exclusiveFullscreen != settings._displayMode)
		flags |= DXGI_PRESENT_ALLOW_TEARING;
	if (FAILED(hr = _pSwapChain->Present(syncInterval, flags)))
		throw VERUS_RUNTIME_ERROR << "Present(), hr=" << VERUS_HR(hr);
}

void RendererD3D12::Sync(bool present)
{
	_fenceValues[_ringBufferIndex] = QueueSignal();
	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;

	WaitForFenceValue(_fenceValues[_ringBufferIndex]);
}

void RendererD3D12::WaitIdle()
{
	QueueWaitIdle();
}

// Resources:

PBaseCommandBuffer RendererD3D12::InsertCommandBuffer()
{
	return TStoreCommandBuffers::Insert();
}

PBaseGeometry RendererD3D12::InsertGeometry()
{
	return TStoreGeometry::Insert();
}

PBasePipeline RendererD3D12::InsertPipeline()
{
	return TStorePipelines::Insert();
}

PBaseShader RendererD3D12::InsertShader()
{
	return TStoreShaders::Insert();
}

PBaseTexture RendererD3D12::InsertTexture()
{
	return TStoreTextures::Insert();
}

void RendererD3D12::DeleteCommandBuffer(PBaseCommandBuffer p)
{
	TStoreCommandBuffers::Delete(static_cast<PCommandBufferD3D12>(p));
}

void RendererD3D12::DeleteGeometry(PBaseGeometry p)
{
	TStoreGeometry::Delete(static_cast<PGeometryD3D12>(p));
}

void RendererD3D12::DeletePipeline(PBasePipeline p)
{
	TStorePipelines::Delete(static_cast<PPipelineD3D12>(p));
}

void RendererD3D12::DeleteShader(PBaseShader p)
{
	TStoreShaders::Delete(static_cast<PShaderD3D12>(p));
}

void RendererD3D12::DeleteTexture(PBaseTexture p)
{
	TStoreTextures::Delete(static_cast<PTextureD3D12>(p));
}

RPHandle RendererD3D12::CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD)
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
		d3dAttach._initialState = ToNativeImageLayout(attachment._initialLayout);
		d3dAttach._finalState = ToNativeImageLayout(attachment._finalLayout);
		renderPass._vAttachments.push_back(std::move(d3dAttach));
	}

	auto GetAttachmentIndexByName = [&ilA](CSZ name) -> uint32_t
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
		throw VERUS_RECOVERABLE << "CreateRenderPass(), attachment not found";
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
			ref._state = ToNativeImageLayout(input._layout);
			d3dSubpass._vInput.push_back(ref);
		}

		d3dSubpass._vColor.reserve(subpass._ilColor.size());
		for (const auto& color : subpass._ilColor)
		{
			RP::D3DRef ref;
			ref._index = GetAttachmentIndexByName(color._name);
			ref._state = ToNativeImageLayout(color._layout);
			d3dSubpass._vColor.push_back(ref);

			if (-1 == renderPass._vAttachments[ref._index]._clearSubpassIndex)
				renderPass._vAttachments[ref._index]._clearSubpassIndex = Utils::Cast32(renderPass._vSubpasses.size());
		}

		if (subpass._depthStencil._name)
		{
			d3dSubpass._depthStencil._index = GetAttachmentIndexByName(subpass._depthStencil._name);
			d3dSubpass._depthStencil._state = ToNativeImageLayout(subpass._depthStencil._layout);

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

FBHandle RendererD3D12::CreateFramebuffer(RPHandle renderPassHandle, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex)
{
	RP::RcD3DRenderPass renderPass = GetRenderPass(renderPassHandle);
	RP::D3DFramebuffer framebuffer;
	framebuffer._width = w;
	framebuffer._height = h;

	const Vector<TexturePtr> vTex(il);
	auto GetResource = [this, &vTex, swapChainBufferIndex](int index)
	{
		if (swapChainBufferIndex >= 0)
		{
			if (index)
			{
				auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index - 1]);
				return texD3D12.GetD3DResource();
			}
			else
				return _vSwapChainBuffers[swapChainBufferIndex].Get();
		}
		else
		{
			auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index]);
			return texD3D12.GetD3DResource();
		}
	};
	auto GetRTV = [this, &vTex, swapChainBufferIndex](int index)
	{
		if (swapChainBufferIndex >= 0)
		{
			if (index)
			{
				auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index - 1]);
				return texD3D12.GetDescriptorHeapRTV().AtCPU(0);
			}
			else
				return _dhSwapChainBuffersRTVs.AtCPU(swapChainBufferIndex);
		}
		else
		{
			auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index]);
			return texD3D12.GetDescriptorHeapRTV().AtCPU(0);
		}
	};
	auto GetDSV = [this, &vTex, swapChainBufferIndex](int index)
	{
		if (swapChainBufferIndex >= 0)
		{
			if (index)
			{
				auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index - 1]);
				return texD3D12.GetDescriptorHeapDSV().AtCPU(0);
			}
			else
			{
				VERUS_RT_ASSERT("Using swapChainBufferIndex for DSV");
				return CD3DX12_CPU_DESCRIPTOR_HANDLE();
			}
		}
		else
		{
			auto& texD3D12 = static_cast<RTextureD3D12>(*vTex[index]);
			return texD3D12.GetDescriptorHeapDSV().AtCPU(0);
		}
	};

	framebuffer._vResources.reserve(renderPass._vAttachments.size());
	VERUS_FOR(i, renderPass._vAttachments.size())
		framebuffer._vResources.push_back(GetResource(i));

	framebuffer._vSubpasses.reserve(renderPass._vSubpasses.size());
	for (const auto& subpass : renderPass._vSubpasses)
	{
		RP::D3DFramebufferSubpass fs;

		int resCount = Utils::Cast32(subpass._vInput.size() + subpass._vColor.size());
		if (subpass._depthStencil._index >= 0)
			resCount++;
		fs._vResources.reserve(resCount);

		if (!subpass._vInput.empty())
		{
			int index = 0;
			for (const auto& ref : subpass._vInput)
			{
				fs._vResources.push_back(GetResource(ref._index));
				index++;
			}
		}
		if (!subpass._vColor.empty())
		{
			fs._dhRTVs.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Utils::Cast32(subpass._vColor.size()));
			int index = 0;
			for (const auto& ref : subpass._vColor)
			{
				fs._vResources.push_back(GetResource(ref._index));
				_pDevice->CopyDescriptorsSimple(1, fs._dhRTVs.AtCPU(index), GetRTV(ref._index), D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				index++;
			}
		}
		if (subpass._depthStencil._index >= 0)
		{
			fs._vResources.push_back(GetResource(subpass._depthStencil._index));
			fs._dhDSV.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
			_pDevice->CopyDescriptorsSimple(1, fs._dhDSV.AtCPU(0), GetDSV(subpass._depthStencil._index), D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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

void RendererD3D12::DeleteRenderPass(RPHandle handle)
{
	if (handle.IsSet())
		_vRenderPasses[handle.Get()] = RP::D3DRenderPass();
	else if (-2 == handle.Get())
		_vRenderPasses.clear();
}

void RendererD3D12::DeleteFramebuffer(FBHandle handle)
{
	if (handle.IsSet())
		_vFramebuffers[handle.Get()] = RP::D3DFramebuffer();
	else if (-2 == handle.Get())
		_vFramebuffers.clear();
}

int RendererD3D12::GetNextRenderPassIndex() const
{
	const int count = Utils::Cast32(_vRenderPasses.size());
	VERUS_FOR(i, count)
	{
		if (_vRenderPasses[i]._vSubpasses.empty())
			return i;
	}
	return count;
}

int RendererD3D12::GetNextFramebufferIndex() const
{
	const int count = Utils::Cast32(_vFramebuffers.size());
	VERUS_FOR(i, count)
	{
		if (_vFramebuffers[i]._vSubpasses.empty())
			return i;
	}
	return count;
}

RP::RcD3DRenderPass RendererD3D12::GetRenderPass(RPHandle handle) const
{
	return _vRenderPasses[handle.Get()];
}

RP::RcD3DFramebuffer RendererD3D12::GetFramebuffer(FBHandle handle) const
{
	return _vFramebuffers[handle.Get()];
}

void RendererD3D12::SetDescriptorHeaps(PBaseCommandBuffer p)
{
	ID3D12DescriptorHeap* ppHeaps[] = { _dhViews.GetD3DDescriptorHeap(), _dhSamplers.GetD3DDescriptorHeap() };
	static_cast<CommandBufferD3D12*>(p)->GetD3DGraphicsCommandList()->SetDescriptorHeaps(2, ppHeaps);
}
