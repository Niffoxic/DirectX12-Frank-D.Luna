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
}

void framework::DxRenderManager::OnFrameEnd()
{
}

EMsaaState framework::DxRenderManager::GetMsaaState() const noexcept
{
	return m_eMsaa;
}

ID3D12Resource* framework::DxRenderManager::GetBackBuffer() const noexcept
{
	return m_pSwapChainBuffer[m_nCurrentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE framework::DxRenderManager::GetBackBufferView() const noexcept
{
	return D3D12_CPU_DESCRIPTOR_HANDLE();
}

D3D12_CPU_DESCRIPTOR_HANDLE framework::DxRenderManager::DepthStencilView() const noexcept
{
	return D3D12_CPU_DESCRIPTOR_HANDLE();
}

void framework::DxRenderManager::SetMsaaState(const EMsaaState state)
{
	if (m_eMsaa == state) return;

	m_eMsaa = state;

	CreateSwapChain();
	OnResize();
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
	levels.SampleCount		= static_cast<uint16_t>(m_eMsaa);

	THROW_DX_IF_FAILS(m_pDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&levels,
		sizeof(levels)
	));

	m_nMsaaQuality = levels.NumQualityLevels;
	assert(m_nMsaaQuality > 0u && "MSAA quality level isnt valid!");
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

	DXGI_SWAP_CHAIN_DESC desc{};
	desc.Flags		  = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	desc.BufferCount  = 1u;
	desc.Windowed	  = true;
	desc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = m_pWindowsManager->GetWindowsHandle();
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	desc.BufferDesc.Width					= m_pWindowsManager->GetWindowsWidth();
	desc.BufferDesc.Height					= m_pWindowsManager->GetWindowsHeight();
	desc.BufferDesc.RefreshRate.Denominator = 1u;
	desc.BufferDesc.RefreshRate.Numerator	= 60u;
	desc.BufferDesc.Scaling				    = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.BufferDesc.ScanlineOrdering		= DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Format					= m_backBufferFormat;
	
	desc.SampleDesc.Count   = static_cast<UINT>(m_eMsaa);
	desc.SampleDesc.Quality = m_nMsaaQuality;

	THROW_DX_IF_FAILS(m_pDxgiFactory->CreateSwapChain(
		m_pDevice.Get(),
		&desc,
		m_pSwapChain.GetAddressOf()
	));

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
	for (size_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; i++)
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

	//~ Allocate Buffers
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Format				= m_depthStencilFormat;
	desc.MipLevels			= 1u;
	desc.Alignment			= 0u;
	desc.Width				= m_pWindowsManager->GetWindowsWidth();
	desc.Height				= m_pWindowsManager->GetWindowsHeight();
	desc.DepthOrArraySize   = 1u;
	desc.SampleDesc.Quality = 1u; // TODO: Create Dynamic msaa config
	desc.SampleDesc.Count   = 0u;
	desc.Flags				= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	desc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE clear{};
	clear.Color[ 0 ]		   = 0.14f;
	clear.Color[ 1 ]		   = 0.71f;
	clear.Color[ 2 ]		   = 0.04f;
	clear.Color[ 3 ]		   = 1.f;
	clear.DepthStencil.Depth   = 1.f;
	clear.DepthStencil.Stencil = 0u;
	clear.Format			   = m_depthStencilFormat;
	
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
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&clear,
		IID_PPV_ARGS(m_pDepthStenchilBuffer.GetAddressOf())
	));

	//~ Create Logical Wrapper
	D3D12_DEPTH_STENCIL_VIEW_DESC view{};
	view.Flags				= D3D12_DSV_FLAG_NONE;
	view.Format				= m_depthStencilFormat;
	view.Texture2D.MipSlice = 0u;
	view.ViewDimension		= D3D12_DSV_DIMENSION_TEXTURE2D;

	auto handle = m_pDsvHeap->GetCPUDescriptorHandleForHeapStart();
	m_pDevice->CreateDepthStencilView(m_pDepthStenchilBuffer.Get(),
									  &view,
									  handle);

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
		m_pWindowsManager->GetWindowsWidth(),
		m_pWindowsManager->GetWindowsHeight() };

	return true;
}

void framework::DxRenderManager::FlushCommandQueue()
{
	m_nCurrentFence++;

	THROW_DX_IF_FAILS(m_pCommandQueue->Signal(m_pFence.Get(), m_nCurrentFence));

	if (m_pFence->GetCompletedValue() < m_nCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, 0u, EVENT_ALL_ACCESS);
		THROW_DX_IF_FAILS(m_pFence->SetEventOnCompletion(m_nCurrentFence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
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
		LogAdapterOuputs(adapters[ i ]);
		if (adapters[ i ])
		{
			adapters[ i ]->Release();
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
