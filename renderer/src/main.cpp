#include "pch.h"
#include "WinApp.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	return WinApp::Run(hInstance, lpCmdLine, nCmdShow);
}