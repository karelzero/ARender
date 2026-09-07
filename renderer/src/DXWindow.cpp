#include "pch.h"
#include "DXWindow.h"
#include "DXHelper.h"
#include "WinApp.h"

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
#ifdef _DEBUG
	// Enable the D3D12 debug layer.
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDxDebuger))))
	{
		pDxDebuger->EnableDebugLayer();

		if(SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDxgiDebuger))))
		{
			pDxgiDebuger->EnableLeakTrackingForThread();
		}
	}
#endif

	LoadPipeline();
	LoadAssets();
}

void DXWindow::OnRun(UINT64 nFrameCount)
{
	OnUpdate();
	OnRender();
}

void DXWindow::OnDestroy()
{
	WaitForPreviousFrame();

	pCommandQueue.Reset();
	pRtvHeap.Reset();
	pCommandAllocator.Reset();
	pCommandList.Reset();
	pSwapChain.Reset();

	CloseHandle(hFenceEvent);
	pFence.Reset();
	pDevice.Reset();

#ifdef _DEBUG
	if (pDxgiDebuger)
	{
		pDxgiDebuger->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
	}

	pDxDebuger.Reset();
	pDxgiDebuger.Reset();
#endif
}

void DXWindow::OnUpdate()
{
}

void DXWindow::OnRender()
{
	ThrowIfFailed(pSwapChain->Present(1, 0));
	WaitForPreviousFrame();
}

void DXWindow::LoadPipeline()
{
	// Create the D3D12 device.
	ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));

	// Create command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pCommandQueue)));

	// Create swap chain.
	ComPtr<IDXGIFactory4> pFactory;
	ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = nWidth;
	swapChainDesc.Height = nHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(pFactory->CreateSwapChainForHwnd(
		pCommandQueue.Get(),
		WinApp::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain));

	ThrowIfFailed(swapChain.As(&pSwapChain));
	nFrameIndex = pSwapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&pRtvHeap)));

	nRtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create frame resources.

	ThrowIfFailed(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCommandAllocator)));
}

void DXWindow::LoadAssets()
{
	// Create command list.
	ThrowIfFailed(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pCommandList)));
	ThrowIfFailed(pCommandList->Close());

	// Create fence.
	ThrowIfFailed(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));
	nFenceValue = 1;

	hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hFenceEvent)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void DXWindow::WaitForPreviousFrame()
{
	UINT64 fenceToWaitFor = nFenceValue;
	ThrowIfFailed(pCommandQueue->Signal(pFence.Get(), nFenceValue));
	nFenceValue++;

	if (pFence->GetCompletedValue() < fenceToWaitFor)
	{
		ThrowIfFailed(pFence->SetEventOnCompletion(fenceToWaitFor, hFenceEvent));
		WaitForSingleObject(hFenceEvent, INFINITE);
	}

	nFrameIndex = pSwapChain->GetCurrentBackBufferIndex();
}
