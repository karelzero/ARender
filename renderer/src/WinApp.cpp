#include "pch.h"
#include "WinApp.h"
#include "DXWindow.h"


HWND WinApp::mHwnd;

int WinApp::Run(HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	int argc;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);

	DXWindow dxWindow(L"ARender", 1280, 720);
	dxWindow.ParseCommandLineArgs(argc, argv);
	LocalFree(argv);

	// Window must be created before DirectX initialization because the swap chain needs a window handle.
	Create(&dxWindow, hInstance, nCmdShow);
	dxWindow.OnInit();

	// Main message loop
	MSG msg = {};
	UINT64 nFrameCount = 0;
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		dxWindow.OnRun(nFrameCount++);
	}

	// Cleanup
	dxWindow.OnDestroy();

	return static_cast<char>(0);
}

void WinApp::Create(DXWindow* pWindow, HINSTANCE hInstance, int nCmdShow)
{
	// Init window class
	WNDCLASSEX windClass = {};
	windClass.cbSize = sizeof(WNDCLASSEX);
	windClass.style = CS_HREDRAW | CS_VREDRAW;
	windClass.lpfnWndProc = WndProc;
	windClass.hInstance = hInstance;
	windClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windClass.lpszClassName = L"ARender";
	RegisterClassEx(&windClass);

	// Create window
	RECT rc = { 0, 0, static_cast<LONG>(pWindow->GetWidth()), static_cast<LONG>(pWindow->GetHeight()) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	mHwnd = CreateWindow(
		windClass.lpszClassName,
		pWindow->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr, nullptr, hInstance, pWindow);

	// Show window
	ShowWindow(mHwnd, nCmdShow);
}

LRESULT WinApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	DXWindow* dxWindow = reinterpret_cast<DXWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg)
	{
		case WM_CREATE:
		{
			// Handle window creation
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));

			return 0;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
		default:
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
