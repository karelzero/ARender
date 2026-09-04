#include "pch.h"
#include "DXWindow.h"

DXWindow::DXWindow(const std::wstring& title, UINT width, UINT height)
	: strTitle(title), nWidth(width), nHeight(height)
{
	fAspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

DXWindow::~DXWindow()
{
}

void DXWindow::ParseCommandLineArgs(int argc, WCHAR** argv)
{
	for (int i = 0; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-smoke", wcslen(argv[i]) == 0))
		{
			strTitle += L" - SmokeTest";
		}
	}
}

void DXWindow::OnInit()
{
}

void DXWindow::OnUpdate()
{
}

void DXWindow::OnRender()
{
}

void DXWindow::OnDestroy()
{
}
