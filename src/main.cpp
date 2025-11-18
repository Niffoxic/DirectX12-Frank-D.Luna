#include <windows.h>


int WINAPI WinMain(_In_		HINSTANCE hInstance,
				   _In_opt_ HINSTANCE prevInstance,
				   _In_		LPSTR lpCmdLine,
				   _In_		int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(prevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	MessageBox(nullptr,
			   L"Check",
			   L"Working fine right",
			   MB_OK);

	return S_OK;
}
