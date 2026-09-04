#include "pch.h"
#include "DXWindow.h"
#include "WinApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	DXWindow window(L"ARender", 1280, 720);
	return WinApp::Run(&window, hInstance, nCmdShow);
}