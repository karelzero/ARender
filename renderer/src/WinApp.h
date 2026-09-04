#pragma once

class DXWindow;

class WinApp
{
public:
	static int Run(DXWindow* pWindow, HINSTANCE hInstance, int nCmdShow);
	static HWND GetHwnd() { return mHwnd; }

protected:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	static HWND mHwnd;
};

