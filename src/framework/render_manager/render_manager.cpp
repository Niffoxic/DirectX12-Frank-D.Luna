#include "render_manager.h"

#include "framework/exception/dx_exception.h"
#include "framework/windows_manager/windows_manager.h"
#include "utility/logger/logger.h"

#include <vector>

using namespace framework;

framework::DxRenderManager::DxRenderManager(DxWindowsManager* windows)
	: m_pWindowsManager(windows)
{}

framework::DxRenderManager::~DxRenderManager()
{
	if (m_pDevice)
	{
		FlushCommandQueue();
	}
}

bool framework::DxRenderManager::Initialize()
{
	if (!InitDirectX()) return false;
	OnResize();
	return true;
}

bool framework::DxRenderManager::Release()
{
	return true;
}

void framework::DxRenderManager::OnFrameBegin(float deltaTime)
{
	for (auto& [key, fn] : m_drawCallbacks)
	{
		fn(deltaTime);
	}
}

void framework::DxRenderManager::OnFrameEnd()
{
	
}

EMsaaState framework::DxRenderManager::GetMsaaState() const noexcept
{
	return EMsaaState::x1;
}

ID3D12Resource* framework::DxRenderManager::GetBackBuffer() const noexcept
{
	return m_pSwapChainBuffer[m_nCurrentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE framework::DxRenderManager::GetBackBufferHandle() const noexcept
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(m_nCurrentBackBuffer) * static_cast<SIZE_T>(m_nRtvDescriptorSize);
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE framework::DxRenderManager::GetDepthStencilHandle() const noexcept
{
	return m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void framework::DxRenderManager::SetMsaaState(const EMsaaState state)
{
	CreateSwapChain();
	OnResize();
}

int framework::DxRenderManager::AddDrawCB(DrawCB&& cb)
{
	auto key = ++DRAW_KEY_GEN;
	m_drawCallbacks[ key ] = std::move(cb);
	return key;
}

void framework::DxRenderManager::RemoveDrawCB(const int key)
{
	if (m_drawCallbacks.contains(key))
	{
		m_drawCallbacks.erase(key);
	}
}

bool framework::DxRenderManager::InitDirectX()
{
#if defined(DEBUG) || defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12Debug> debugger;
	THROW_DX_IF_FAILS(D3D12GetDebugInterface(IID_PPV_ARGS(debugger.GetAddressOf())));
	debugger->EnableDebugLayer();
#endif

	THROW_DX_IF_FAILS(CreateDXGIFactory1(IID_PPV_ARGS(&m_pDxgiFactory)));

	HRESULT result = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0,
									   IID_PPV_ARGS(m_pDevice.GetAddressOf()));

	if (FAILED(result))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter{ nullptr };
		THROW_DX_IF_FAILS(m_pDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(adapter.GetAddressOf())));

		THROW_DX_IF_FAILS(D3D12CreateDevice(
			adapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(m_pDevice.GetAddressOf())
		));
	}

	THROW_DX_IF_FAILS(m_pDevice->CreateFence(
		0u, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(m_pFence.GetAddressOf())));

	m_nRtvDescriptorSize	   = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_nDsvDescriptorSize	   = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_nCbvSrvUavDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ConfigureMSAA					();
	LogAdapters						();
	CreateCommandObjects			();
	CreateSwapChain					();
	CreateRenderTargetDescriptorHeap();
	CreateDepthStencilDescriptorHeap();
	CreateRenderTargetViews			();
	CreateDepthStencilViews			();

	return true;
}

bool framework::DxRenderManager::ConfigureMSAA()
{
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels{};
	levels.Format			= m_backBufferFormat;
	levels.Flags			= D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	levels.NumQualityLevels = 0u;
	levels.SampleCount		= 1u;

	HRESULT hr = m_pDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&levels,
		sizeof(levels)
	);

	if (FAILED(hr))
	{
		logger::error("Failed to fetch feature level!");
		return false;
	}
	return true;
}

bool framework::DxRenderManager::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC qDesc{};
	qDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
	qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	THROW_DX_IF_FAILS(m_pDevice->CreateCommandQueue(
		&qDesc,
		IID_PPV_ARGS(m_pCommandQueue.GetAddressOf())));

	THROW_DX_IF_FAILS(m_pDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_pCommandAlloc.GetAddressOf())
	));

	THROW_DX_IF_FAILS(m_pDevice->CreateCommandList(
		0u,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pCommandAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(m_pCommandList.GetAddressOf())
	));

	m_pCommandList->Close();

	return true;
}

bool framework::DxRenderManager::CreateSwapChain()
{
	assert(m_pWindowsManager && "Cant create swapchain windows manager is null");

	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Width = m_pWindowsManager->GetWindowsWidth();
	sd.Height = m_pWindowsManager->GetWindowsHeight();
	sd.Format = m_backBufferFormat;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.Scaling = DXGI_SCALING_STRETCH;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	sd.Flags = 0; // maybe ALLOW_TEARING if supported

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	THROW_DX_IF_FAILS(m_pDxgiFactory->CreateSwapChainForHwnd(
		m_pCommandQueue.Get(),                  // NOT the device
		m_pWindowsManager->GetWindowsHandle(),
		&sd,
		nullptr,
		nullptr,
		&swapChain1
	));

	THROW_DX_IF_FAILS(swapChain1.As(&m_pSwapChain));

	return true;
}

bool framework::DxRenderManager::CreateRenderTargetDescriptorHeap()
{
	assert(m_pDevice && "Device is null cant create RTV!");

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask		= 0u;
	desc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT;

	THROW_DX_IF_FAILS(m_pDevice->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(m_pRtvHeap.GetAddressOf())));

	m_nRtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	return true;
}

bool framework::DxRenderManager::CreateRenderTargetViews()
{
	assert(m_pSwapChain && "Swapchain is not created but called to allocate");
	
	auto handle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
	{
		THROW_DX_IF_FAILS(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pSwapChainBuffer[ i ])));
		m_pDevice->CreateRenderTargetView(m_pSwapChainBuffer[ i ].Get(), nullptr, handle);
		handle.ptr += SIZE_T(m_nRtvDescriptorSize);
	}
	return true;
}

bool framework::DxRenderManager::CreateDepthStencilDescriptorHeap()
{
	assert(m_pDevice && "Device is null cant create DSV!");

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	desc.Flags		    = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask	    = 0u;
	desc.NumDescriptors = 1u;

	THROW_DX_IF_FAILS(m_pDevice->CreateDescriptorHeap(
		&desc, IID_PPV_ARGS(m_pDsvHeap.GetAddressOf())));

	return true;
}

bool framework::DxRenderManager::CreateDepthStencilViews()
{
	assert(m_pDevice	&& "Device is null cant create DSV!");
	assert(m_pSwapChain && "Swapchain is not created but called to allocate");
	assert(m_pDsvHeap	&& "DSV Heap is not created!");

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Format				= m_depthStencilFormat;
	desc.MipLevels			= 1u;
	desc.Alignment			= 0u;
	desc.Width				= m_pWindowsManager->GetWindowsWidth();
	desc.Height				= m_pWindowsManager->GetWindowsHeight();
	desc.DepthOrArraySize	= 1u;
	desc.SampleDesc.Count	= 1u;
	desc.SampleDesc.Quality = 0u;
	desc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clear{};
	clear.Format				= m_depthStencilFormat;
	clear.DepthStencil.Depth	= 1.0f;
	clear.DepthStencil.Stencil	= 0;

	D3D12_HEAP_PROPERTIES properties{};
	properties.Type					= D3D12_HEAP_TYPE_DEFAULT;
	properties.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	properties.VisibleNodeMask		= 1u;
	properties.CreationNodeMask		= 1u;

	THROW_DX_IF_FAILS(m_pDevice->CreateCommittedResource(
		&properties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clear,
		IID_PPV_ARGS(m_pDepthStenchilBuffer.GetAddressOf())
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC view{};
	view.Flags				= D3D12_DSV_FLAG_NONE;
	view.Format				= m_depthStencilFormat;
	view.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;
	view.Texture2D.MipSlice = 0u;

	auto handle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_pDevice->CreateDepthStencilView(m_pDepthStenchilBuffer.Get(), &view, handle);

	return true;
}

bool framework::DxRenderManager::CreateViewport()
{
	assert(m_pWindowsManager && "Cant create viewport windows manager is null");
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width    = static_cast<float>(m_pWindowsManager->GetWindowsWidth());
	m_viewport.Height   = static_cast<float>(m_pWindowsManager->GetWindowsHeight());
	m_viewport.MaxDepth = 1.0f;
	m_viewport.MinDepth = 0.0f;

	m_scissorRect = { 0, 0, 
		m_pWindowsManager->GetWindowsWidth (),
		m_pWindowsManager->GetWindowsHeight() };

	return true;
}

void framework::DxRenderManager::FlushCommandQueue()
{
	if (!m_pFence) return;
	if (!m_pCommandQueue) return;
	m_nCurrentFence++;
	THROW_DX_IF_FAILS(m_pCommandQueue->Signal(m_pFence.Get(), m_nCurrentFence));

	if (m_pFence->GetCompletedValue() < m_nCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0u, EVENT_ALL_ACCESS);
		THROW_DX_IF_FAILS(m_pFence->SetEventOnCompletion(m_nCurrentFence, eventHandle));

		DWORD waitResult = WaitForSingleObject(eventHandle, INFINITE);
		DWORD winErr = GetLastError();
		CloseHandle(eventHandle);

		if (waitResult != WAIT_OBJECT_0)
		{
			logger::error("Fence wait failed! WaitForSingleObject returned: {}", waitResult);
			logger::error("Win32 error: {}", winErr);
			THROW_DX_IF_FAILS(HRESULT_FROM_WIN32(winErr));
		}
	}
}

void framework::DxRenderManager::OnResize()
{
	assert(m_pDevice	     && "Cant Resize: Device is null!");
	assert(m_pSwapChain      && "Cant Resize: Swap Chain is null!");
	assert(m_pCommandAlloc   && "Cant Resize: Command Allocator is null!");
	assert(m_pCommandList    && "Cant Resize: Command list is null!");
	assert(m_pWindowsManager && "Cant Resize: Windows Manager list is null!");

	//~ Flush current cmds
	FlushCommandQueue();
	THROW_DX_IF_FAILS(m_pCommandList->Reset(m_pCommandAlloc.Get(), nullptr));

	for (size_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
	{
		m_pSwapChainBuffer[ i ].Reset();
	}
	m_pDepthStenchilBuffer.Reset();

	THROW_DX_IF_FAILS(m_pSwapChain->ResizeBuffers(
		SWAP_CHAIN_BUFFER_COUNT,
		m_pWindowsManager->GetWindowsWidth (), 
		m_pWindowsManager->GetWindowsHeight(),
		m_backBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	m_nCurrentBackBuffer = 0u;
                                                                              
	//~ Recreate
	CreateRenderTargetViews();
	CreateDepthStencilViews();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_pDepthStenchilBuffer.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_pCommandList->ResourceBarrier(1u, &barrier);
	m_pCommandList->Close();

	ID3D12CommandList* cmdLists[]{ m_pCommandList.Get() };
	UINT cmdSize = ARRAYSIZE(cmdLists);
	m_pCommandQueue->ExecuteCommandLists(cmdSize, cmdLists);
	FlushCommandQueue();
}

void framework::DxRenderManager::LogAdapters()
{
	UINT i = 0u;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapters{};

	while (m_pDxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc{};
		adapter->GetDesc(&desc);
		std::wstring text = desc.Description;
		std::string info = std::string(text.begin(), text.end());
		logger::info("Adapter: {}", info);

		adapters.push_back(adapter);
		++i;
	}

	for (size_t j = 0; j < adapters.size(); j++)
	{
		LogAdapterOuputs(adapters[ j ]);
		if (adapters[ j ])
		{
			adapters[ j ]->Release();
		}
	}
}

void framework::DxRenderManager::LogAdapterOuputs(IDXGIAdapter* adapter)
{
	UINT i = 0u;
	IDXGIOutput* output = nullptr;

	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc{};
		output->GetDesc(&desc);

		std::wstring txt = desc.DeviceName;
		std::string info = std::string(txt.begin(), txt.end());
		logger::info("Display Output: {}", info);

		LogOutputDisplayModes(output, m_backBufferFormat);

		if (output)
		{
			output->Release();
		}
		++i;
	}
}

void framework::DxRenderManager::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	if (!output) return;

	UINT count = 0u;
	UINT flags = 0u;

	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modes(count);
	output->GetDisplayModeList(format, flags, &count, modes.data());

	for (auto& mode : modes)
	{
		UINT n = mode.RefreshRate.Numerator;
		UINT d = mode.RefreshRate.Denominator;
		
		logger::info("Width: {}\nHeight: {}\n Refresh: {}/{}",
					 mode.Width, mode.Height, n, d);
	}
}
