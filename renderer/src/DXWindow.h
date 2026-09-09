#pragma once

using Microsoft::WRL::ComPtr;

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
	void OnRun(UINT64 nFrameCount);
	void OnDestroy();
	void OnResize(UINT width, UINT height);

	inline bool IsExit() const { return bExit; }

private:
	void OnUpdate();
	void OnRender();

	void LoadPipeline();
	void LoadAssets();

	void CreateBuffers();
	void ReleaseBuffers();

	void PopulateCommandList();
	void WaitForPreviousFrame();

	// Window properties
	std::wstring strTitle;
	UINT nWidth;
	UINT nHeight;
	float fAspectRatio;
	bool bExit = false;

	// DirectX properties
	static const UINT FrameCount = 2;

	ComPtr<ID3D12Device> pDevice;
	ComPtr<IDXGISwapChain3> pSwapChain;
	ComPtr<ID3D12Resource> pRenderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> pCommandAllocator;
	ComPtr<ID3D12CommandQueue> pCommandQueue;
	ComPtr<ID3D12DescriptorHeap> pRtvHeap;
	ComPtr<ID3D12PipelineState> pPipelineState;
	ComPtr<ID3D12GraphicsCommandList> pCommandList;
	UINT nRtvDescriptorSize = 0;

#ifdef _DEBUG
	ComPtr<ID3D12Debug6> pDxDebuger;
	ComPtr<IDXGIDebug1> pDxgiDebuger;
#endif

	UINT nFrameIndex = 0;
	HANDLE hFenceEvent = nullptr;
	ComPtr<ID3D12Fence> pFence;
	UINT64 nFenceValue = 0;
};