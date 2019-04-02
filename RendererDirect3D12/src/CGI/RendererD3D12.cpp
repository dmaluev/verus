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
	if (_pCommandQueue)
		QueueWaitIdle();
	if (_hFence != INVALID_HANDLE_VALUE)
	{
		CloseHandle(_hFence);
		_hFence = INVALID_HANDLE_VALUE;
	}
	_pFence.Reset();
	VERUS_FOR(i, ringBufferSize)
		_pCommandLists[i].Reset();
	VERUS_FOR(i, ringBufferSize)
		_mapCommandAllocators[i].clear();
	_dSwapChainBuffersRTVs.Reset();
	_vSwapChainBuffers.clear();
	_pSwapChain.Reset();
	_pCommandQueue.Reset();
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
	BOOL data = FALSE;
	if (FAILED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &data, sizeof(data))))
		data = FALSE;
	return data == TRUE;
}

void RendererD3D12::CreateSwapChainBuffersRTVs()
{
	HRESULT hr = 0;
	_vSwapChainBuffers.resize(_numSwapChainBuffers);
	_dSwapChainBuffersRTVs = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _numSwapChainBuffers);
	auto dh = _dSwapChainBuffersRTVs->GetCPUDescriptorHandleForHeapStart();
	VERUS_U_FOR(i, _numSwapChainBuffers)
	{
		ComPtr<ID3D12Resource> pBuffer;
		if (FAILED(hr = _pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBuffer))))
			throw VERUS_RUNTIME_ERROR << "GetBuffer(), hr=" << VERUS_HR(hr);
		_pDevice->CreateRenderTargetView(pBuffer.Get(), nullptr, dh);
		_vSwapChainBuffers[i] = pBuffer;
		dh.ptr += _descHandleIncSizeRTV;
	}
}

void RendererD3D12::InitD3D()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SETTINGS;

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

	_descHandleIncSizeRTV = _pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainBuffersRTVs();

	VERUS_FOR(i, ringBufferSize)
		_mapCommandAllocators[i][std::this_thread::get_id()] = CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);

	VERUS_FOR(i, ringBufferSize)
		_pCommandLists[i] = CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, _mapCommandAllocators[i][std::this_thread::get_id()]);

	_pFence = CreateFence();
	_hFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
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

ComPtr<ID3D12GraphicsCommandList> RendererD3D12::CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> pCommandAllocator)
{
	HRESULT hr = 0;
	ComPtr<ID3D12GraphicsCommandList> pGraphicsCommandList;
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
	const UINT64 value = QueueSignal();
	WaitForFenceValue(value);
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

void RendererD3D12::BeginFrame()
{
	HRESULT hr = 0;

	WaitForFenceValue(_fenceValues[_ringBufferIndex]);

	_swapChainBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	auto pCommandAllocator = _mapCommandAllocators[_ringBufferIndex][std::this_thread::get_id()];
	if (FAILED(hr = pCommandAllocator->Reset()))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);

	if (FAILED(hr = _pCommandLists[_ringBufferIndex]->Reset(pCommandAllocator.Get(), nullptr)))
		throw VERUS_RUNTIME_ERROR << "Reset(), hr=" << VERUS_HR(hr);
	auto pBackBuffer = _vSwapChainBuffers[_swapChainBufferIndex];
	const auto rb = MakeResourceBarrierTransition(pBackBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_pCommandLists[_ringBufferIndex]->ResourceBarrier(1, &rb);
}

void RendererD3D12::EndFrame()
{
	HRESULT hr = 0;

	auto pBackBuffer = _vSwapChainBuffers[_swapChainBufferIndex];
	const auto rb = MakeResourceBarrierTransition(pBackBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_pCommandLists[_ringBufferIndex]->ResourceBarrier(1, &rb);

	if (FAILED(hr = _pCommandLists[_ringBufferIndex]->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);
	ID3D12CommandList* ppCommandLists[] = { _pCommandLists[_ringBufferIndex].Get() };
	_pCommandQueue->ExecuteCommandLists(VERUS_ARRAY_LENGTH(ppCommandLists), ppCommandLists);

	_fenceValues[_ringBufferIndex] = QueueSignal();
}

void RendererD3D12::Present()
{
	HRESULT hr = 0;

	bool g_VSync = false;
	bool g_TearingSupported = false;

	const UINT syncInterval = g_VSync ? 1 : 0;
	const UINT flags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	if (FAILED(hr = _pSwapChain->Present(syncInterval, flags)))
		throw VERUS_RUNTIME_ERROR << "Present(), hr=" << VERUS_HR(hr);

	_ringBufferIndex = (_ringBufferIndex + 1) % ringBufferSize;
}

void RendererD3D12::Clear(UINT32 flags)
{
	VERUS_QREF_RENDERER;

	auto dh = _dSwapChainBuffersRTVs->GetCPUDescriptorHandleForHeapStart();
	dh.ptr += _swapChainBufferIndex * _descHandleIncSizeRTV;

	_pCommandLists[_ringBufferIndex]->ClearRenderTargetView(dh, renderer.GetClearColor().ToPointer(), 0, nullptr);
}
