#pragma once

class DXWindow;

class WinApp
{
public:
	static int Run(HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow);
	static HWND GetHwnd() { return mHwnd; }

protected:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static void Create(DXWindow* pWindow, HINSTANCE hInstance, int nCmdShow);

private:
	static HWND mHwnd;
};

