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
	DeleteFramebuffer(-1);
	DeleteRenderPass(-1);

	_vSamplers.clear();

	_dhSamplers.Reset();
	_dhCbvSrvUav.Reset();

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

ComPtr<IDXGIFactory6> RendererD3D12::CreateDXGIFactory()
{
	HRESULT hr = 0;
	ComPtr<IDXGIFactory6> pFactory;
	UINT flags = 0;
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (FAILED(hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&pFactory))))
		throw VERUS_RUNTIME_ERROR << "CreateDXGIFactory2(), hr=" << VERUS_HR(hr);
	return pFactory;
}

ComPtr<IDXGIAdapter4> RendererD3D12::GetAdapter(ComPtr<IDXGIFactory6> pFactory)
{
	HRESULT hr = 0;
	ComPtr<IDXGIAdapter4> pAdapter;
	if (FAILED(hr = pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter))))
		throw VERUS_RUNTIME_ERROR << "EnumAdapterByGpuPreference(), hr=" << VERUS_HR(hr);
	return pAdapter;
}

bool RendererD3D12::CheckFeatureSupportAllowTearing(ComPtr<IDXGIFactory6> pFactory)
{
	BOOL featureSupportData = FALSE;
	if (FAILED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &featureSupportData, sizeof(featureSupportData))))
		featureSupportData = FALSE;
	return featureSupportData == TRUE;
}

void RendererD3D12::CreateSwapChainBuffersRTVs()
{
	HRESULT hr = 0;
	_vSwapChainBuffers.resize(_numSwapChainBuffers);
	_dhSwapChainBuffersRTVs.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _numSwapChainBuffers);
	VERUS_U_FOR(i, _numSwapChainBuffers)
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

#if defined(_DEBUG) || defined(VERUS_DEBUG)
	EnableDebugLayer();
#endif

	ComPtr<IDXGIFactory6> pFactory = CreateDXGIFactory();

	ComPtr<IDXGIAdapter4> pAdapter = GetAdapter(pFactory);

	if (FAILED(hr = D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_pDevice))))
		throw VERUS_RUNTIME_ERROR << "D3D12CreateDevice(), hr=" << VERUS_HR(hr);

#if defined(_DEBUG) || defined(VERUS_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(_pDevice.As(&pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	}
#endif

	_pCommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_numSwapChainBuffers = settings._screenVSync ? 3 : 2;

	_swapChainDesc.Width = settings._screenSizeWidth;
	_swapChainDesc.Height = settings._screenSizeHeight;
	_swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	_swapChainDesc.Stereo = FALSE;
	_swapChainDesc.SampleDesc.Count = 1;
	_swapChainDesc.SampleDesc.Quality = 0;
	_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_swapChainDesc.BufferCount = _numSwapChainBuffers;
	_swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	_swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	_swapChainDesc.Flags = CheckFeatureSupportAllowTearing(pFactory) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

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

	_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainBuffersRTVs();

	VERUS_FOR(i, s_ringBufferSize)
		_mapCommandAllocators[i][std::this_thread::get_id()] = CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_pFence = CreateFence();
	_hFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	_dhCbvSrvUav.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 5000, true);
	_dhSamplers.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 500, true);

	CreateSamplers();
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
		WaitForSingleObject(_hFence, INFINITE);
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

	_vSamplers.resize(+Sampler::count);

	D3D12_STATIC_SAMPLER_DESC desc = {};
	D3D12_STATIC_SAMPLER_DESC init = {};
	init.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
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
	desc.Filter = D3D12_FILTER_ANISOTROPIC;
	desc.MaxAnisotropy = settings._gpuAnisotropyLevel;
	_vSamplers[+Sampler::aniso] = desc;

	// <Wrap>
	desc = init;
	_vSamplers[+Sampler::linear3D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	_vSamplers[+Sampler::nearest3D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	_vSamplers[+Sampler::linear2D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	_vSamplers[+Sampler::nearest2D] = desc;
	// </Wrap>

	// <Clamp>
	desc = init;
	init.AddressU = init.AddressV = init.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::linearClamp3D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	init.AddressU = init.AddressV = init.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::nearestClamp3D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	init.AddressU = init.AddressV = init.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::linearClamp2D] = desc;

	desc = init;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	init.AddressU = init.AddressV = init.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	_vSamplers[+Sampler::nearestClamp2D] = desc;
	// </Clamp>
}

D3D12_STATIC_SAMPLER_DESC RendererD3D12::GetStaticSamplerDesc(Sampler s) const
{
	return _vSamplers[+s];
}

void RendererD3D12::BeginFrame(bool present)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	WaitForFenceValue(_fenceValues[_ringBufferIndex]);

	if (present)
		_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	auto pCommandAllocator = _mapCommandAllocators[_ringBufferIndex][std::this_thread::get_id()];
	if (FAILED(hr = pCommandAllocator->Reset()))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);

	renderer.GetCommandBuffer()->Begin();

	ID3D12DescriptorHeap* ppHeaps[] = { _dhCbvSrvUav.GetD3DDescriptorHeap(), _dhSamplers.GetD3DDescriptorHeap() };
	static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->SetDescriptorHeaps(2, ppHeaps);
}

void RendererD3D12::EndFrame(bool present)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	renderer.GetCommandBuffer()->End();

	ID3D12CommandList* ppCommandLists[] = { static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList() };
	_pCommandQueue->ExecuteCommandLists(VERUS_ARRAY_LENGTH(ppCommandLists), ppCommandLists);

	if (!present)
	{
		_fenceValues[_ringBufferIndex] = QueueSignal();
		_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;
	}
}

void RendererD3D12::Present()
{
	VERUS_QREF_CONST_SETTINGS;
	HRESULT hr = 0;

	const bool allowTearing = !!(_swapChainDesc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
	const UINT syncInterval = settings._screenVSync ? 1 : 0;
	const UINT flags = (allowTearing && !settings._screenVSync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
	if (FAILED(hr = _pSwapChain->Present(syncInterval, flags)))
		throw VERUS_RUNTIME_ERROR << "Present(), hr=" << VERUS_HR(hr);

	_fenceValues[_ringBufferIndex] = QueueSignal();
	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;
}

void RendererD3D12::Clear(UINT32 flags)
{
	VERUS_QREF_RENDERER;

	//auto handle = _dhSwapChainBuffersRTVs.AtCPU(_swapChainBufferIndex);
	//static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->ClearRenderTargetView(handle, renderer.GetClearColor().ToPointer(), 0, nullptr);
	//static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->OMSetRenderTargets(1, &handle, FALSE, nullptr);
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

int RendererD3D12::CreateRenderPass(std::initializer_list<RP::Attachment> ilA, std::initializer_list<RP::Subpass> ilS, std::initializer_list<RP::Dependency> ilD)
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
		throw VERUS_RECOVERABLE << "CreateRenderPass(), Attachment not found";
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

	const int id = GetNextRenderPassID();
	if (id >= _vRenderPasses.size())
		_vRenderPasses.push_back(std::move(renderPass));
	else
		_vRenderPasses[id] = renderPass;

	return id;
}

int RendererD3D12::CreateFramebuffer(int renderPassID, std::initializer_list<TexturePtr> il, int w, int h, int swapChainBufferIndex)
{
	RP::RcD3DRenderPass renderPass = GetRenderPassByID(renderPassID);
	RP::D3DFramebuffer framebuffer;

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

		int numRes = Utils::Cast32(subpass._vColor.size());
		if (subpass._depthStencil._index >= 0)
			numRes++;
		fs._vResources.reserve(numRes);

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

	const int id = GetNextFramebufferID();
	if (id >= _vFramebuffers.size())
		_vFramebuffers.push_back(std::move(framebuffer));
	else
		_vFramebuffers[id] = framebuffer;

	return id;
}

void RendererD3D12::DeleteRenderPass(int id)
{
	if (id >= 0)
		_vRenderPasses[id] = RP::D3DRenderPass();
	else
		_vRenderPasses.clear();
}

void RendererD3D12::DeleteFramebuffer(int id)
{
	if (id >= 0)
		_vFramebuffers[id] = RP::D3DFramebuffer();
	else
		_vFramebuffers.clear();
}

int RendererD3D12::GetNextRenderPassID() const
{
	const int num = Utils::Cast32(_vRenderPasses.size());
	VERUS_FOR(i, num)
	{
		if (_vRenderPasses[i]._vSubpasses.empty())
			return i;
	}
	return num;
}

int RendererD3D12::GetNextFramebufferID() const
{
	const int num = Utils::Cast32(_vFramebuffers.size());
	VERUS_FOR(i, num)
	{
		if (_vFramebuffers[i]._vSubpasses.empty())
			return i;
	}
	return num;
}

RP::RcD3DRenderPass RendererD3D12::GetRenderPassByID(int id) const
{
	return _vRenderPasses[id];
}

RP::RcD3DFramebuffer RendererD3D12::GetFramebufferByID(int id) const
{
	return _vFramebuffers[id];
}
