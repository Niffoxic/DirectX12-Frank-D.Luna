#pragma once

#include "framework/windows_manager/windows_manager.h"
#include "framework/render_manager/render_manager.h"
#include "utility/timer/timer.h"

#include <memory>
#include <sal.h>

namespace framework
{
	typedef struct _DX_FRAMEWORK_CONSTRUCT_DESC
	{
		_In_ DX12_WINDOWS_MANAGER_CREATE_DESC WindowsDesc;
	} DX_FRAMEWORK_CONSTRUCT_DESC;

	class IFramework
	{
	public:
		 IFramework(_In_ const DX_FRAMEWORK_CONSTRUCT_DESC& desc);
		virtual ~IFramework();

		_NODISCARD _Check_return_ _Success_(return != false)
		bool Init();

		_Success_(return == S_OK)
		HRESULT Execute();

	protected:
		//~ Application Must Implement them
		_NODISCARD _Check_return_
		virtual bool InitApplication() = 0;
		virtual void BeginPlay		() = 0;
		virtual void Release		() = 0;

		virtual void Tick(_In_ float deltaTime) = 0;

	private:
		_NODISCARD _Check_return_
		bool CreateManagers(_In_ const DX_FRAMEWORK_CONSTRUCT_DESC& desc);

		void CreateUtilities     ();
		void InitManagers		 ();
		void ReleaseManagers     ();
		void ManagerFrameBegin   (_In_ float deltaTime);
		void ManagerFrameEnd     ();
		void SubscribeToEvents	 ();

	protected:
		GameTimer m_timer{};
		std::unique_ptr<DxWindowsManager> m_pWindowsManager{ nullptr };
		std::unique_ptr<DxRenderManager>  m_pRenderManager { nullptr };

	private:
		bool m_bEnginePaused{ false };
	};
} // namespace framework
