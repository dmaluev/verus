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
	VERUS_DONE(RendererD3D12);
}

void RendererD3D12::EnableDebugLayer()
{
	CComPtr<ID3D12Debug> pDebug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
		pDebug->EnableDebugLayer();
}

CComPtr<IDXGIFactory7> RendererD3D12::CreateDXGIFactory()
{
	HRESULT hr = 0;
	CComPtr<IDXGIFactory7> pFactory;
	UINT flags = 0;
#if defined(_DEBUG) || defined(VERUS_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (FAILED(hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&pFactory))))
		throw VERUS_RUNTIME_ERROR << "CreateDXGIFactory2(), hr=" << VERUS_HR(hr);
	return pFactory;
}

CComPtr<IDXGIAdapter4> RendererD3D12::GetAdapter(CComPtr<IDXGIFactory7> pFactory)
{
	HRESULT hr = 0;
	CComPtr<IDXGIAdapter4> pAdapter;
	if (FAILED(hr = pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter))))
		throw VERUS_RUNTIME_ERROR << "EnumAdapterByGpuPreference(), hr=" << VERUS_HR(hr);
	return pAdapter;
}

bool RendererD3D12::CheckFeatureSupportAllowTearing(CComPtr<IDXGIFactory7> pFactory)
{
	BOOL data = FALSE;
	if (FAILED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &data, sizeof(data))))
		data = FALSE;
	return data == TRUE;
}

void RendererD3D12::CreateSwapChainBuffersRTVs()
{
	HRESULT hr = 0;
	_swapChainBuffersRTVs = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, _swapChainDesc.BufferCount);
	_vSwapChainBuffers.resize(_swapChainDesc.BufferCount);
	auto dh = _swapChainBuffersRTVs->GetCPUDescriptorHandleForHeapStart();
	VERUS_U_FOR(i, _swapChainDesc.BufferCount)
	{
		CComPtr<ID3D12Resource> pBuffer;
		if (FAILED(hr = _pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBuffer))))
			throw VERUS_RUNTIME_ERROR << "GetBuffer(), hr=" << VERUS_HR(hr);
		_pDevice->CreateRenderTargetView(pBuffer, nullptr, dh);
		_vSwapChainBuffers[i] = pBuffer;
		dh.ptr += _descHandleIncSizeRTV;
	}
}

void RendererD3D12::InitD3D()
{
	VERUS_QREF_SETTINGS;

	HRESULT hr = 0;

#if defined(_DEBUG) || defined(VERUS_DEBUG)
	EnableDebugLayer();
#endif

	CComPtr<IDXGIFactory7> pFactory = CreateDXGIFactory();

	CComPtr<IDXGIAdapter4> pAdapter = GetAdapter(pFactory);

	if (FAILED(hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_pDevice))))
		throw VERUS_RUNTIME_ERROR << "D3D12CreateDevice(), hr=" << VERUS_HR(hr);

	_pCommandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_swapChainDesc.Width = settings._screenSizeWidth;
	_swapChainDesc.Height = settings._screenSizeHeight;
	_swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	_swapChainDesc.Stereo = FALSE;
	_swapChainDesc.SampleDesc.Count = 1;
	_swapChainDesc.SampleDesc.Quality = 0;
	_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	_swapChainDesc.BufferCount = 2;
	_swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	_swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	_swapChainDesc.Flags = CheckFeatureSupportAllowTearing(pFactory) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	CComPtr<IDXGISwapChain1> pSwapChain1;
	if (FAILED(hr = pFactory->CreateSwapChainForHwnd(_pCommandQueue, GetActiveWindow(), &_swapChainDesc, nullptr, nullptr, &pSwapChain1)))
		throw VERUS_RUNTIME_ERROR << "CreateSwapChainForHwnd(), hr=" << VERUS_HR(hr);
	if (FAILED(hr = pSwapChain1.QueryInterface(&_pSwapChain)))
		throw VERUS_RUNTIME_ERROR << "QueryInterface(), hr=" << VERUS_HR(hr);

	_descHandleIncSizeRTV = _pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_currentBackBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();

	CreateSwapChainBuffersRTVs();

	_vCommandAllocators.resize(_swapChainDesc.BufferCount);
	VERUS_U_FOR(i, _swapChainDesc.BufferCount)
		_vCommandAllocators[i] = CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);

	_pCommandList = CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, _vCommandAllocators[_currentBackBufferIndex]);

	_pFence = CreateFence();
	_hFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_vFenceValues.resize(_swapChainDesc.BufferCount);
}

CComPtr<ID3D12CommandQueue> RendererD3D12::CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	HRESULT hr = 0;
	CComPtr<ID3D12CommandQueue> pCommandQueue;
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	if (FAILED(hr = _pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandQueue(), hr=" << VERUS_HR(hr);
	return pCommandQueue;
}

CComPtr<ID3D12CommandAllocator> RendererD3D12::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type)
{
	HRESULT hr = 0;
	CComPtr<ID3D12CommandAllocator> pCommandAllocator;
	if (FAILED(hr = _pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCommandAllocator))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandAllocator(), hr=" << VERUS_HR(hr);
	return pCommandAllocator;
}

CComPtr<ID3D12GraphicsCommandList> RendererD3D12::CreateCommandList(D3D12_COMMAND_LIST_TYPE type, CComPtr<ID3D12CommandAllocator> pCommandAllocator)
{
	HRESULT hr = 0;
	CComPtr<ID3D12GraphicsCommandList> pGraphicsCommandList;
	if (FAILED(hr = _pDevice->CreateCommandList(0, type, pCommandAllocator, nullptr, IID_PPV_ARGS(&pGraphicsCommandList))))
		throw VERUS_RUNTIME_ERROR << "CreateCommandList(), hr=" << VERUS_HR(hr);
	if (FAILED(hr = pGraphicsCommandList->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);
	return pGraphicsCommandList;
}

CComPtr<ID3D12DescriptorHeap> RendererD3D12::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT num)
{
	HRESULT hr = 0;
	CComPtr<ID3D12DescriptorHeap> pDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = num;
	if (FAILED(hr = _pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap))))
		throw VERUS_RUNTIME_ERROR << "CreateDescriptorHeap(), hr=" << VERUS_HR(hr);
	return pDescriptorHeap;
}

CComPtr<ID3D12Fence> RendererD3D12::CreateFence()
{
	HRESULT hr = 0;
	CComPtr<ID3D12Fence> pFence;
	if (FAILED(hr = _pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence))))
		throw VERUS_RUNTIME_ERROR << "CreateFence(), hr=" << VERUS_HR(hr);
	return pFence;
}

UINT64 RendererD3D12::Signal()
{
	HRESULT hr = 0;
	const UINT64 value = ++_fenceValue;
	if (FAILED(hr = _pCommandQueue->Signal(_pFence, value)))
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

void RendererD3D12::Flush()
{
	const UINT64 value = Signal();
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

void RendererD3D12::PrepareDraw()
{
	auto pBackBuffer = _vSwapChainBuffers[_currentBackBufferIndex];
	auto pCommandAllocator = _vCommandAllocators[_currentBackBufferIndex];

	pCommandAllocator->Reset();
	_pCommandList->Reset(pCommandAllocator, nullptr);

	const auto rb = MakeResourceBarrierTransition(pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_pCommandList->ResourceBarrier(1, &rb);
}

void RendererD3D12::Clear(UINT32 flags)
{
	static float x = 0;
	x += 0.0001f;
	x = fmod(x, 1.f);
	FLOAT clearColor[] = { x, 0.5f, 0.25f, 1.0f };

	auto dh = _swapChainBuffersRTVs->GetCPUDescriptorHandleForHeapStart();
	dh.ptr += _currentBackBufferIndex * _descHandleIncSizeRTV;

	_pCommandList->ClearRenderTargetView(dh, clearColor, 0, nullptr);
}

void RendererD3D12::Present()
{
	HRESULT hr = 0;

	bool g_VSync = false;
	bool g_TearingSupported = false;

	auto pBackBuffer = _vSwapChainBuffers[_currentBackBufferIndex];

	const auto rb = MakeResourceBarrierTransition(pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_pCommandList->ResourceBarrier(1, &rb);

	if (FAILED(hr = _pCommandList->Close()))
		throw VERUS_RUNTIME_ERROR << "Close(), hr=" << VERUS_HR(hr);

	ID3D12CommandList* ppCommandLists[] = { _pCommandList };
	_pCommandQueue->ExecuteCommandLists(VERUS_ARRAY_LENGTH(ppCommandLists), ppCommandLists);

	_vFenceValues[_currentBackBufferIndex] = Signal();
	UINT syncInterval = g_VSync ? 1 : 0;
	UINT flags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	if (FAILED(hr = _pSwapChain->Present(syncInterval, flags)))
		throw VERUS_RUNTIME_ERROR << "Present(), hr=" << VERUS_HR(hr);

	_currentBackBufferIndex = _pSwapChain->GetCurrentBackBufferIndex();
	WaitForFenceValue(_vFenceValues[_currentBackBufferIndex]);
}
