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

	InitD3D();
}

void RendererD3D12::Done()
{
	WaitIdle();

	_dhSampler.Reset();
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
	_dhDepthStencilBufferView.Reset();
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
		_pDevice->CreateRenderTargetView(pBuffer.Get(), nullptr, _dhSwapChainBuffersRTVs.AtCPU(i));
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

	_pCommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_numSwapChainBuffers = settings._screenVSync ? 3 : 2;

	_swapChainDesc.Width = settings._screenSizeWidth;
	_swapChainDesc.Height = settings._screenSizeHeight;
	_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(pWnd, &wmInfo);
	HWND hWnd = wmInfo.info.win.window;

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
	_dhSampler.Create(_pDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 500, true);
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

ComPtr<ID3D12DescriptorHeap> RendererD3D12::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num)
{
	HRESULT hr = 0;
	ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = num;
	if (FAILED(hr = _pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap))))
		throw VERUS_RUNTIME_ERROR << "CreateDescriptorHeap(), hr=" << VERUS_HR(hr);
	return pDescriptorHeap;
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

D3D12_RESOURCE_BARRIER RendererD3D12::MakeResourceBarrierTransition(ID3D12Resource* pResource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = pResource;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.StateBefore = before;
	rb.Transition.StateAfter = after;
	return rb;
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

	ID3D12DescriptorHeap* ppHeaps[] = { _dhCbvSrvUav.GetD3DDescriptorHeap(), _dhSampler.GetD3DDescriptorHeap() };
	static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->SetDescriptorHeaps(2, ppHeaps);

	if (present)
	{
		auto pBackBuffer = _vSwapChainBuffers[_swapChainBufferIndex];
		const auto rb = MakeResourceBarrierTransition(pBackBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->ResourceBarrier(1, &rb);
	}
}

void RendererD3D12::EndFrame(bool present)
{
	VERUS_QREF_RENDERER;
	HRESULT hr = 0;

	if (present)
	{
		auto pBackBuffer = _vSwapChainBuffers[_swapChainBufferIndex];
		const auto rb = MakeResourceBarrierTransition(pBackBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->ResourceBarrier(1, &rb);
	}

	renderer.GetCommandBuffer()->End();

	ID3D12CommandList* ppCommandLists[] = { static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList() };
	_pCommandQueue->ExecuteCommandLists(VERUS_ARRAY_LENGTH(ppCommandLists), ppCommandLists);

	_fenceValues[_ringBufferIndex] = QueueSignal();
	_ringBufferIndex = (_ringBufferIndex + 1) % s_ringBufferSize;
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
}

void RendererD3D12::Clear(UINT32 flags)
{
	VERUS_QREF_RENDERER;

	auto handle = _dhSwapChainBuffersRTVs.AtCPU(_swapChainBufferIndex);
	static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->ClearRenderTargetView(handle, renderer.GetClearColor().ToPointer(), 0, nullptr);
	static_cast<CommandBufferD3D12*>(&(*renderer.GetCommandBuffer()))->GetD3DGraphicsCommandList()->OMSetRenderTargets(1, &handle, FALSE, nullptr);
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
