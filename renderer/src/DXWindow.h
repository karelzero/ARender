#pragma once

class DXWindow
{
public:
	DXWindow(const std::wstring& title, UINT width, UINT height);
	~DXWindow();

	UINT GetWidth() const { return nWidth; }
	UINT GetHeight() const { return nHeight; }
	const WCHAR* GetTitle() const { return strTitle.c_str(); }

	void ParseCommandLineArgs(int argc, WCHAR** argv);

	void OnInit();
	void OnUpdate();
	void OnRender();
	void OnDestroy();

private:
	// Window properties
	std::wstring strTitle;
	UINT nWidth;
	UINT nHeight;
	float fAspectRatio;

	// DirectX properties
	static const UINT FrameCount = 2;

	ID3D12Device* pDevice = nullptr;
	IDXGISwapChain3* pSwapChain = nullptr;
	ID3D12Resource* pRenderTargets[FrameCount] = {};
	ID3D12CommandAllocator* pCommandAllocator = nullptr;
	ID3D12CommandQueue* pCommandQueue = nullptr;
	ID3D12DescriptorHeap* pRtvHeap = nullptr;
	ID3D12PipelineState* pPipelineState = nullptr;
	ID3D12GraphicsCommandList* pCommandList = nullptr;
	UINT nRtvDescriptorSize = 0;

	UINT nFrameIndex = 0;
	HANDLE hFenceEvent = nullptr;
	ID3D12Fence* pFence = nullptr;
	UINT64 nFenceValue = 0;
};