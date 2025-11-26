#ifndef DX12_WINDOWS_MANAGER_H
#define DX12_WINDOWS_MANAGER_H

#include <sal.h>
#include <string>
#include <windows.h>
#include <memory>

#include "inputs/keyboard.h"
#include "inputs/mouse.h"

namespace framework
{
	enum class EScreenState: bool
	{
		Windowed   = false,
		Fullscreen = true
	};

	enum class EProcessedMessageState : uint8_t
	{
		ExitMessage = 0,
		ExecuteMessage,
		Unknown
	};

	typedef struct _DX12_WINDOWS_MANAGER_CREATE_DESC
	{
								 std::wstring WindowTitle{ L"DirectX12" };
		_Field_range_(100, 1920) UINT		  Width      { 1280u };
		_Field_range_(100, 1080) UINT		  Height     { 720u };
		_Field_range_(0, 200)    UINT		  IconId     { 0u }; //~ 0 means no icon is attached
								 EScreenState ScreenState{ EScreenState::Windowed };
	} DX12_WINDOWS_MANAGER_CREATE_DESC;

	class DxWindowsManager
	{
	public:
		//~ ctor and dtor
		 DxWindowsManager(_In_ const DX12_WINDOWS_MANAGER_CREATE_DESC& desc);
		 ~DxWindowsManager() noexcept;
		//~ copy and move
		DxWindowsManager(_In_ const DxWindowsManager&) = delete;
		DxWindowsManager(_Inout_ DxWindowsManager&&)   = delete;

		DxWindowsManager& operator=(_In_ const DxWindowsManager&) = delete;
		DxWindowsManager& operator=(_Inout_ DxWindowsManager&&)	  = delete;

		_NODISCARD _Check_return_ _Success_(return != EProcessedMessageState::Unknown)
		static EProcessedMessageState ProcessMessages() noexcept;

		_NODISCARD _Check_return_ _Success_(return != false)
		bool Initialize();

		_NODISCARD _Check_return_ _Success_(return != false)
		bool Release   () noexcept;

		void OnFrameBegin(_In_ float deltaTime) noexcept;
		void OnFrameEnd() noexcept;

		DxMouseInputs	 Mouse{};
		DxKeyboardInputs Keyboard{};

		//~ Getters
		_NODISCARD _Ret_maybenull_ _Success_(return != nullptr)
		HWND GetWindowsHandle		() const noexcept;
		
		_NODISCARD _Ret_maybenull_ _Success_(return != nullptr)
		HINSTANCE GetWindowsInstance() const noexcept;

		_NODISCARD _Check_return_
		float GetAspectRatio		() const noexcept;

		_NODISCARD _Check_return_ __forceinline
		EScreenState GetScreenState	() const noexcept { return m_config.ScreenState;   }

		_NODISCARD _Check_return_ __forceinline
		int GetWindowsWidth			() const noexcept { return m_config.Width;  }
		
		_NODISCARD _Check_return_ __forceinline
		int GetWindowsHeight		() const noexcept { return m_config.Height; }

		//~ Setters
		void SetScreenState			(_In_ const EScreenState state)			 noexcept;
		void SetWindowTitle			(_In_ const std::wstring& title)		 noexcept;
		void SetWindowMessageOnTitle(_In_ const std::wstring& message) const noexcept;

	private:
		_NODISCARD _Check_return_ _Must_inspect_result_ _Success_(return != 0)
		bool InitWindowScreen();

		void TransitionToFullScreen	   ()		noexcept;
		void TransitionToWindowedScreen() const noexcept;

		//~ message proc
		_NODISCARD
		LRESULT MessageHandler(
			_In_ HWND   hwnd,
			_In_ UINT   msg,
			_In_ WPARAM wParam,
			_In_ LPARAM lParam) noexcept;

		_Function_class_(WINDOWS_CALLBACK)
		static LRESULT CALLBACK WindowProcThunk(
			_In_ HWND   hwnd,
			_In_ UINT   msg,
			_In_ WPARAM wParam,
			_In_ LPARAM lParam) noexcept;

		_Function_class_(WINDOWS_CALLBACK)
		static LRESULT CALLBACK WindowProcSetup(
				_In_ HWND   hwnd,
				_In_ UINT   message,
				_In_ WPARAM wParam,
				_In_ LPARAM lParam);

	private:
		struct
		{
			std::wstring Title		{ L"DirectX 12 Application" };
			std::wstring ClassName	{ L"DXFramework" };
			UINT		 Width		{ 0u };
			UINT		 Height		{ 0u };
			UINT	     IconID		{ 0u };
			EScreenState ScreenState{ EScreenState::Windowed };
		} m_config;

		HWND			m_pWindowsHandle  { nullptr };
		HINSTANCE		m_pWindowsInstance{ nullptr };
		WINDOWPLACEMENT m_WindowPlacement { sizeof(m_WindowPlacement) };
	};

} // namespace framework

#endif // DX12_WINDOWS_MANAGER_H
