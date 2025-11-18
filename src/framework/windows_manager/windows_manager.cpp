#include "windows_manager.h"

#include "utility/logger/logger.h"

#include "framework/exception/win_exception.h"
#include "framework/event/event_queue.h"
#include "framework/event/event_windows.h"

using namespace framework;

namespace
{
	_Function_class_(WINDOWS_CALLBACK)
	static LRESULT CALLBACK WindowProcThunk(
		_In_ HWND   hwnd,
		_In_ UINT   msg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam)
	{
		if (auto that = reinterpret_cast<DxWindowsManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
		{
			return that->MessageHandler(hwnd, msg, wParam, lParam);
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	_Function_class_(WINDOWS_CALLBACK)
	static LRESULT CALLBACK WindowProcSetup(
		_In_ HWND   hwnd,
		_In_ UINT   message,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam)
	{
		if (message == WM_NCCREATE)
		{
			CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
			DxWindowsManager* that = reinterpret_cast<DxWindowsManager*>(create->lpCreateParams);

			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
			SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcThunk));

			return that->MessageHandler(hwnd, message, wParam, lParam);
		}

		return DefWindowProc(hwnd, message, wParam, lParam);
	}
} // namespace anonymous

DxWindowsManager::DxWindowsManager()
{
	m_pKeyboard = std::make_unique<DxKeyboardInputs>();
	m_pMouse    = std::make_unique<DxMouseInputs>();
}

DxWindowsManager::~DxWindowsManager()
{
	if (!Release())
	{
		logger::error("Failed to Release Windows Resources cleanly!");
	}
}

_Use_decl_annotations_
DxWindowsManager::DxWindowsManager(const DX12_WINDOWS_MANAGER_CREATE_DESC& desc)
	: DxWindowsManager()
{
	m_config.Height		 = desc.Height;
	m_config.Width		 = desc.Width;
	m_config.Title		 = desc.WindowTitle;
	m_config.ScreenState = desc.ScreenState;
	m_config.IconID		 = desc.IconId;
}

_Use_decl_annotations_
EProcessedMessageState DxWindowsManager::ProcessMessages() noexcept
{
	MSG message{};

	while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		if (message.message == WM_QUIT)
		{
			return EProcessedMessageState::ExitMessage;
		}

		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return EProcessedMessageState::ExecuteMessage;
}

_Use_decl_annotations_
bool DxWindowsManager::Initialize()
{
	if (!InitWindowScreen()) return false;
	
	if (auto handle = GetWindowsHandle())
	{
		if (m_pMouse) m_pMouse->AttachWindowHandle(handle);
	}
	return true;
}

_Use_decl_annotations_
bool DxWindowsManager::Release() noexcept
{
	return true;
}

_Use_decl_annotations_
void DxWindowsManager::OnFrameBegin(float deltaTime) noexcept
{
	if (m_pKeyboard) m_pKeyboard->OnFrameBegin(deltaTime);
	if (m_pMouse)	 m_pMouse	->OnFrameBegin(deltaTime);
}

void DxWindowsManager::OnFrameEnd() noexcept
{
	if (m_pKeyboard) m_pKeyboard->OnFrameEnd();
	if (m_pMouse)	 m_pMouse   ->OnFrameEnd();
}

_Use_decl_annotations_
HWND DxWindowsManager::GetWindowsHandle() const noexcept
{
	return m_pWindowsHandle;
}

_Use_decl_annotations_
HINSTANCE DxWindowsManager::GetWindowsInstance() const noexcept
{
	return m_pWindowsInstance;
}

_Use_decl_annotations_
DxKeyboardInputs* framework::DxWindowsManager::GetKeyboard() const
{
	return m_pKeyboard.get();
}

_Use_decl_annotations_
DxMouseInputs* framework::DxWindowsManager::GetMouse() const
{
	return m_pMouse.get();
}

_Use_decl_annotations_
float DxWindowsManager::GetAspectRatio() const noexcept
{
	return static_cast<float>(m_config.Width) 
		 / static_cast<float>(m_config.Height);
}

_Use_decl_annotations_
void DxWindowsManager::SetScreenState(const EScreenState state) noexcept
{
	if (state == m_config.ScreenState) return; // same state
	m_config.ScreenState = state;

	//~ transition states
	if (m_config.ScreenState == EScreenState::Fullscreen)
	{
		TransitionToFullScreen();
	} else TransitionToWindowedScreen();

	if (auto handle = GetWindowsHandle()) UpdateWindow(handle);

	RECT rt{};
	if (auto handle = GetWindowsHandle())
	{
		GetClientRect(handle, &rt);
	} else return;

	UINT width  = rt.right - rt.left;
	UINT height = rt.bottom - rt.top;

	//~ Post Event to the queue
	if (m_config.ScreenState == EScreenState::Fullscreen)
		EventQueue::Post<FULL_SCREEN_EVENT>({ width, height });
	else
		EventQueue::Post<WINDOWED_SCREEN_EVENT>({ width, height });
}

_Use_decl_annotations_
void DxWindowsManager::SetWindowTitle(const std::wstring& title) noexcept
{
	if (auto handle = GetWindowsHandle())
	{
		m_config.Title = title;
		SetWindowText(handle, title.c_str());
	}
}

_Use_decl_annotations_
void DxWindowsManager::SetWindowMessageOnTitle(const std::wstring& message) const noexcept
{
	if (auto handle = GetWindowsHandle())
	{
		std::wstring convert = m_config.Title + L" " + message;
		SetWindowText(handle, convert.c_str());
	}
}

_Use_decl_annotations_
bool DxWindowsManager::InitWindowScreen()
{
	m_pWindowsInstance = GetModuleHandle(nullptr);

	WNDCLASSEX wc{};
	wc.cbSize	   = sizeof(WNDCLASSEX);
	wc.style	   = CS_OWNDC;
	wc.lpfnWndProc = WindowProcSetup;
	wc.cbClsExtra  = 0;
	wc.cbWndExtra  = sizeof(LONG_PTR);
	wc.hInstance   = m_pWindowsInstance;

	//~ Set Icon
	if (m_config.IconID)
	{
		wc.hIcon   = LoadIcon(m_pWindowsInstance, MAKEINTRESOURCE(m_config.IconID));
		wc.hIconSm = LoadIcon(m_pWindowsInstance, MAKEINTRESOURCE(m_config.IconID));
	} else
	{
		wc.hIcon   = LoadIcon(nullptr, IDI_APPLICATION);
		wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	}
	wc.hCursor		 = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = m_config.Title.c_str();

	if (!RegisterClassEx(&wc))
	{
		THROW_WIN();
		return false;
	}

	DWORD style = WS_OVERLAPPEDWINDOW;

	RECT rect
	{ 0, 0,
	  static_cast<LONG>(m_config.Width),
	  static_cast<LONG>(m_config.Height)
	};

	if (!AdjustWindowRect(&rect, style, FALSE))
	{
		THROW_WIN();
		return false;
	}

	int adjustedWidth = rect.right - rect.left;
	int adjustedHeight = rect.bottom - rect.top;

	m_pWindowsHandle = CreateWindowEx(
		0,
		wc.lpszClassName,
		m_config.Title.c_str(),
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		adjustedWidth, adjustedHeight,
		nullptr,
		nullptr,
		m_pWindowsInstance,
		this);

	if (!m_pWindowsHandle)
	{
		THROW_WIN();
		return false;
	}

	ShowWindow  (m_pWindowsHandle, SW_SHOW);
	UpdateWindow(m_pWindowsHandle);

	return true;
}

_Use_decl_annotations_
LRESULT DxWindowsManager::MessageHandler(HWND   hwnd,
										 UINT   message,
										 WPARAM wParam,
										 LPARAM lParam) noexcept
{
	if (m_pKeyboard)
	{
		if (m_pKeyboard->ProcessMessage(message, wParam, lParam)) return S_OK;
	}
	if (m_pMouse)
	{
		if (m_pMouse->ProcessMessage(message, wParam, lParam)) return S_OK;
	}

	switch (message)
	{
	case WM_SIZE:
	{
		m_config.Width  = LOWORD(lParam);
		m_config.Height = HIWORD(lParam);
		EventQueue::Post<WINDOW_RESIZE_EVENT>({ m_config.Width, m_config.Height });
		return S_OK;
	}
	case WM_ENTERSIZEMOVE: // clicked mouse on title bar
	case WM_KILLFOCUS:
	{
		EventQueue::Post<WINDOW_PAUSE_EVENT>({ true });
		return S_OK;
	}
	case WM_EXITSIZEMOVE: // not clicking anymore
	case WM_SETFOCUS:
	{
		EventQueue::Post<WINDOW_PAUSE_EVENT>({ false });
		return S_OK;
	}
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return S_OK;
	}
	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return S_OK;
}

void DxWindowsManager::TransitionToFullScreen() noexcept
{
	if (m_config.ScreenState != EScreenState::Fullscreen) return; //~ its windowed

	auto handle = GetWindowsHandle();
	if (!handle) return;
	GetWindowPlacement(handle, &m_WindowPlacement);

	SetWindowLong(handle, GWL_STYLE, WS_POPUP);
	SetWindowPos(
		handle,
		HWND_TOP,
		0, 0,
		GetSystemMetrics(SM_CXSCREEN),
		GetSystemMetrics(SM_CYSCREEN),
		SWP_FRAMECHANGED | SWP_SHOWWINDOW
	);
}

void DxWindowsManager::TransitionToWindowedScreen() const noexcept
{
	if (m_config.ScreenState != EScreenState::Windowed) return; //~ its full screen

	auto handle = GetWindowsHandle();
	if (!handle) return;

	SetWindowLong(handle, GWL_STYLE, WS_OVERLAPPEDWINDOW);
	SetWindowPlacement(handle, &m_WindowPlacement);
	SetWindowPos
	(
		handle,
		nullptr,
		0, 0,
		0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
	);
}
