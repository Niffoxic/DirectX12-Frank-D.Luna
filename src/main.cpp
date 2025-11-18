#include "application/application.h"
#include "framework/exception/win_exception.h"

#include "framework/framework.h"
#include "framework/windows_manager/windows_manager.h"


int WINAPI WinMain(_In_		HINSTANCE hInstance,
				   _In_opt_ HINSTANCE prevInstance,
				   _In_		LPSTR lpCmdLine,
				   _In_		int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(prevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

    try
    {
        framework::DX12_WINDOWS_MANAGER_CREATE_DESC WindowsDesc{};
        WindowsDesc.Height      = 720u;
        WindowsDesc.Width       = 1280u;
        WindowsDesc.IconId      = 0u;
        WindowsDesc.WindowTitle = L"DirectX 12 Application";
        WindowsDesc.ScreenState = framework::EScreenState::Windowed;

        framework::DX_FRAMEWORK_CONSTRUCT_DESC engineDesc{};
        engineDesc.WindowsDesc = WindowsDesc;

        framework::Application application{ engineDesc };

        if (!application.Init()) return E_FAIL;

        return application.Execute();
    }
    catch (const framework::BaseException& ex)
    {
        std::wstring msg(ex.what(), ex.what() + std::strlen(ex.what()));
        MessageBox(nullptr, msg.c_str(), L"PixelFox Exception", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        std::wstring msg(ex.what(), ex.what() + std::strlen(ex.what()));
        MessageBox(nullptr, msg.c_str(), L"Standard Exception", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }
    catch (...)
    {
        MessageBox(nullptr, L"Unknown fatal error occurred.", L"PixelFox Crash", MB_ICONERROR | MB_OK);
        return EXIT_FAILURE;
    }
}
