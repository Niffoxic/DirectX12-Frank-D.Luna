#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <functional>
#include <unordered_map>

namespace framework
{
	class DxWindowsManager;

	enum class EMsaaState: int
	{
		x1 = 0,
		x2 = 2,
		x4 = 4,
		x8 = 8
	};

	class DxRenderManager
	{
		using DrawCB = std::function<void(float)>;
	public:
		 DxRenderManager(DxWindowsManager* windows);
		~DxRenderManager();

		DxRenderManager(const DxRenderManager&) = delete;
		DxRenderManager(DxRenderManager&&)		= delete;

		DxRenderManager& operator=(const DxRenderManager&) = delete;
		DxRenderManager& operator=(DxRenderManager&&)	   = delete;

		//~ operation
		bool Initialize();
		bool Release   ();

		void OnFrameBegin(float deltaTime);
		void OnFrameEnd  ();

		//~ Getters
		EMsaaState					GetMsaaState		 () const noexcept;
		ID3D12Resource*				GetBackBuffer		 () const noexcept;
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferHandle  () const noexcept;
		D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilHandle() const noexcept;

		//~ Setters
		void SetMsaaState(const EMsaaState state);
		int  AddDrawCB(DrawCB&& cb);
		void RemoveDrawCB(const int key);

		//~ operations
		void FlushCommandQueue();
		void OnResize();

		//~ helpers
		void LogAdapters();
		void LogAdapterOuputs(IDXGIAdapter* adapter);
		void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	private:
		//~ create resources
		bool InitDirectX						 ();
		bool ConfigureMSAA						 ();
		bool CreateCommandObjects				 ();
		bool CreateSwapChain					 ();
		bool CreateRenderTargetDescriptorHeap	 ();
		bool CreateRenderTargetViews			 ();
		bool CreateDepthStencilDescriptorHeap	 ();
		bool CreateDepthStencilViews			 ();
		bool CreateViewport						 ();

	public:
		DxWindowsManager* m_pWindowsManager;

		//~ resources
	
		// device resource
		D3D_DRIVER_TYPE m_driverType        { D3D_DRIVER_TYPE_HARDWARE      };
		DXGI_FORMAT     m_backBufferFormat  { DXGI_FORMAT_R8G8B8A8_UNORM    };
		DXGI_FORMAT     m_depthStencilFormat{ DXGI_FORMAT_D24_UNORM_S8_UINT };

		Microsoft::WRL::ComPtr<IDXGIFactory4>   m_pDxgiFactory{ nullptr };
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_pSwapChain	 { nullptr };
		Microsoft::WRL::ComPtr<ID3D12Device>   m_pDevice	 { nullptr };

		static constexpr unsigned SWAP_CHAIN_BUFFER_COUNT{ 2u };
		
		unsigned m_nCurrentBackBuffer{ 0u };
		Microsoft::WRL::ComPtr<ID3D12Resource> m_pSwapChainBuffer[ SWAP_CHAIN_BUFFER_COUNT ];

		// sync resource
		Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{ nullptr };
		UINT m_nCurrentFence{ 0u };

		// cmd resource
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>		  m_pCommandQueue{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_pCommandAlloc{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList { nullptr };

		//~ Render Resource
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRtvHeap			   { nullptr };
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDsvHeap			   { nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource>		 m_pDepthStenchilBuffer{ nullptr };

		D3D12_VIEWPORT m_viewport{};
		D3D12_RECT     m_scissorRect{};

		UINT m_nRtvDescriptorSize	   { 0u };
		UINT m_nDsvDescriptorSize	   { 0u };
		UINT m_nCbvSrvUavDescriptorSize{ 0u };

		//~ draw callbacks
		std::unordered_map<int, DrawCB> m_drawCallbacks{};
		inline static unsigned int DRAW_KEY_GEN{ 0 };
	};
} // namespace framework
