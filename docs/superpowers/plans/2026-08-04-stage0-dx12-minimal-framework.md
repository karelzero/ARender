# 阶段 0：DX12 最小可用框架 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 搭出一个能画场景的 DX12 空壳框架（`framework/`），最终交付一个旋转纹理立方体，作为后续所有复刻项目的地基。

**Architecture:** 单 CMake 项目、WIN32 可执行文件。`DXContext` 封装设备/队列/swap chain/RTV/DSV/fence；`Mesh`、`Pipeline`、`Texture2D` 三个小类各自单一职责；`main.cpp` 驱动窗口与帧循环。所有 buffer 用 upload heap（最简、无 barrier）；纹理用 default heap + 一次性上传列表。

**Tech Stack:** C++20、DirectX 12（Windows SDK 自带，含 DirectXMath）、HLSL 5.1（运行时 `D3DCompileFromFile`）、CMake + VS2022 生成器。

## Global Constraints

- 只用 DX12 最小集：swap chain/命令队列、PSO、root signature 与 descriptor、vertex/index buffer 与 upload heap、depth buffer、基本 barrier。**禁止**：多队列、自定义内存分配器、bindless、DirectXTK、d3dx12.h、vcpkg 等一切外部依赖。
- 环境：Windows + Visual Studio 2022（Desktop development with C++）+ Windows SDK（含 `DirectXMath.h`）+ CMake ≥ 3.24。
- 验证方式（本阶段无单元测试，图形输出用"冒烟模式"代替）：每个 Task 必须构建通过，且 `./framework.exe --smoke`（跑 120 帧自动退出）退出码为 0；Debug 配置下 D3D12 debug layer 打开，Output 窗口不得有 ERROR 级输出。每个 Task 末尾附"肉眼验收"项。
- 时间硬上限 2 周（约 15 小时）。卡住超过 2 小时的问题记录到笔记"待回头补"，继续推进。
- 构建目录 `framework/build/`（已被 .gitignore 覆盖）；shader 由 CMake POST_BUILD 拷贝到 exe 旁。
- 运行验证命令统一在 exe 目录下执行（shader 用相对路径加载）：`(cd framework/build/Debug && ./framework.exe --smoke)`。

---

### Task 1: CMake 脚手架 + Win32 窗口 + 冒烟模式

**Files:**
- Create: `framework/CMakeLists.txt`
- Create: `framework/src/main.cpp`

**Interfaces:**
- Consumes: 无
- Produces: 可构建的 CMake 项目；`wWinMain` 入口支持 `--smoke` 参数（120 帧后自动退出，返回 0）。后续所有 Task 在此 main.cpp 上演进。

- [ ] **Step 1: 写 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.24)
project(ARenderFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(framework WIN32
    src/main.cpp
)
target_link_libraries(framework PRIVATE d3d12 dxgi d3dcompiler dxguid)
```

- [ ] **Step 2: 写 main.cpp（窗口 + 消息循环 + --smoke）**

```cpp
#include <windows.h>
#include <cwchar>

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t* kClass = L"ARenderFramework";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);

    const int kWidth = 1280, kHeight = 720;
    RECT rc = { 0, 0, kWidth, kHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, kClass, L"ARender Framework",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nCmdShow);

    const bool smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
    int frames = 0;
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            ++frames;
            if (smoke && frames >= 120) PostQuitMessage(0);
        }
    }
    return 0;
}
```

- [ ] **Step 3: 配置并构建**

Run:
```bash
cmake -S framework -B framework/build -G "Visual Studio 17 2022" -A x64
cmake --build framework/build --config Debug
```
Expected: 配置成功（找不到 VS2022 时先安装/修复工具链），编译链接 0 error。

- [ ] **Step 4: 冒烟验证**

Run: `(cd framework/build/Debug && ./framework.exe --smoke); echo $?`
Expected: 窗口闪现约 2 秒后自动关闭，输出 `0`。

- [ ] **Step 5: 肉眼验收**

Run: `framework/build/Debug/framework.exe`
Expected: 出现 1280x720 空白窗口，可正常关闭。

- [ ] **Step 6: Commit**

```bash
git add framework/CMakeLists.txt framework/src/main.cpp
git commit -m "Stage0/Task1: CMake scaffold + Win32 window with smoke mode"
```

---

### Task 2: DXContext——设备、命令队列、swap chain、RTV、fence，清屏并 Present

**Files:**
- Create: `framework/src/dx_util.h`
- Create: `framework/src/dx_context.h`
- Create: `framework/src/dx_context.cpp`
- Modify: `framework/CMakeLists.txt`（add_executable 增加 `src/dx_context.cpp`）
- Modify: `framework/src/main.cpp`（整文件替换为下方代码）

**Interfaces:**
- Consumes: Task 1 的窗口与循环。
- Produces（后续 Task 依赖的接口，签名不再变）:
  - `bool DXContext::Initialize(HWND, uint32_t width, uint32_t height)`
  - `void DXContext::Shutdown()`
  - `void DXContext::BeginFrame(const float clearColor[4])`
  - `void DXContext::EndFrame()`
  - `ID3D12Device* DXContext::Device() const`
  - `ID3D12GraphicsCommandList* DXContext::CommandList() const`
  - `DXGI_FORMAT DXContext::BackBufferFormat() const`
  - `static constexpr uint32_t DXContext::FrameCount = 2`
  - `void ThrowIfFailed(HRESULT)`（dx_util.h）

- [ ] **Step 1: 写 dx_util.h**

```cpp
#pragma once
#include <windows.h>
#include <stdexcept>
#include <string>

inline void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr))
        throw std::runtime_error("HRESULT failed: 0x" + std::to_string((unsigned)hr));
}
```

- [ ] **Step 2: 写 dx_context.h**

```cpp
#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class DXContext {
public:
    static constexpr uint32_t FrameCount = 2;

    bool Initialize(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    void BeginFrame(const float clearColor[4]);
    void EndFrame();

    ID3D12Device* Device() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* CommandList() const { return m_commandList.Get(); }
    DXGI_FORMAT BackBufferFormat() const { return DXGI_FORMAT_R8G8B8A8_UNORM; }

private:
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTV() const;
    void WaitForGpu();
    void MoveToNextFrame();

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12Resource> m_backBuffers[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Fence> m_fence;
    uint64_t m_fenceValues[FrameCount] = {};
    HANDLE m_fenceEvent = nullptr;
    uint32_t m_backBufferIndex = 0;
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
```

- [ ] **Step 3: 写 dx_context.cpp**

```cpp
#include "dx_context.h"
#include "dx_util.h"

bool DXContext::Initialize(HWND hwnd, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        debug->EnableDebugLayer();
#endif

    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)));

    // 第一个能建 device 的硬件 adapter；失败退回 WARP（软光栅）
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            break;
    }
    if (!m_device) {
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&m_commandQueue)));

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = FrameCount;
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> sc1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &sd, nullptr, nullptr, &sc1));
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(sc1.As(&m_swapChain));
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = FrameCount;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < FrameCount; ++i) {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtv);
        rtv.ptr += m_rtvDescriptorSize;
    }

    for (auto& a : m_commandAllocators)
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)));
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[m_backBufferIndex].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[m_backBufferIndex] = 1;
    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return m_fenceEvent != nullptr;
}

void DXContext::Shutdown() {
    WaitForGpu();
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
}

void DXContext::BeginFrame(const float clearColor[4]) {
    ThrowIfFailed(m_commandAllocators[m_backBufferIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), nullptr));

    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_backBuffers[m_backBufferIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = CurrentRTV();
    m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DXContext::EndFrame() {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_backBuffers[m_backBufferIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);
    ThrowIfFailed(m_swapChain->Present(1, 0));
    MoveToNextFrame();
}

D3D12_CPU_DESCRIPTOR_HANDLE DXContext::CurrentRTV() const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += SIZE_T(m_backBufferIndex) * m_rtvDescriptorSize;
    return h;
}

void DXContext::WaitForGpu() {
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_backBufferIndex]));
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
    ++m_fenceValues[m_backBufferIndex];
}

void DXContext::MoveToNextFrame() {
    const uint64_t current = m_fenceValues[m_backBufferIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), current));
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    if (m_fence->GetCompletedValue() < m_fenceValues[m_backBufferIndex]) {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_backBufferIndex], m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_fenceValues[m_backBufferIndex] = current + 1;
}
```

- [ ] **Step 4: CMakeLists.txt 的 add_executable 改为**

```cmake
add_executable(framework WIN32
    src/main.cpp
    src/dx_context.cpp
)
```

- [ ] **Step 5: main.cpp 整文件替换为**

```cpp
#include <windows.h>
#include <cwchar>
#include "dx_context.h"
#include "dx_util.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    try {
        const wchar_t* kClass = L"ARenderFramework";
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);

        const int kWidth = 1280, kHeight = 720;
        RECT rc = { 0, 0, kWidth, kHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        HWND hwnd = CreateWindowExW(0, kClass, L"ARender Framework",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
        ShowWindow(hwnd, nCmdShow);

        DXContext ctx;
        if (!ctx.Initialize(hwnd, kWidth, kHeight)) return 1;

        const bool smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
        const float clearColor[4] = { 0.10f, 0.20f, 0.40f, 1.0f };
        int frames = 0;
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                ctx.BeginFrame(clearColor);
                D3D12_VIEWPORT vp = { 0, 0, (float)kWidth, (float)kHeight, 0, 1 };
                D3D12_RECT sc = { 0, 0, kWidth, kHeight };
                ctx.CommandList()->RSSetViewports(1, &vp);
                ctx.CommandList()->RSSetScissorRects(1, &sc);
                ctx.EndFrame();
                ++frames;
                if (smoke && frames >= 120) PostQuitMessage(0);
            }
        }
        ctx.Shutdown();
        return 0;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "ARender error", MB_ICONERROR);
        return 1;
    }
}
```

- [ ] **Step 6: 构建 + 冒烟验证**

Run:
```bash
cmake --build framework/build --config Debug
(cd framework/build/Debug && ./framework.exe --smoke); echo $?
```
Expected: 编译 0 error；输出 `0`；debug Output 无 D3D12 ERROR。

- [ ] **Step 7: 肉眼验收**

Run: `framework/build/Debug/framework.exe`
Expected: 窗口被清成深蓝色（0.1, 0.2, 0.4），无闪烁。

- [ ] **Step 8: Commit**

```bash
git add framework/
git commit -m "Stage0/Task2: DXContext with device/swapchain/RTV/fence, clear to blue"
```

---

### Task 3: Shader + Root Signature + PSO + Vertex Buffer——彩色三角形

**Files:**
- Create: `framework/src/mesh.h`
- Create: `framework/src/mesh.cpp`
- Create: `framework/src/pipeline.h`
- Create: `framework/src/pipeline.cpp`
- Create: `framework/shaders/cube.hlsl`
- Modify: `framework/CMakeLists.txt`（加源文件 + shader 拷贝）
- Modify: `framework/src/main.cpp`（整文件替换）

**Interfaces:**
- Consumes: Task 2 的 `DXContext` 接口。
- Produces:
  - `struct Vertex { float px,py,pz; float cr,cg,cb,ca; float u,v; }`（36 字节，整个阶段 0 固定格式）
  - `bool Mesh::Create(DXContext&, const Vertex* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount)`
  - `void Mesh::Draw(ID3D12GraphicsCommandList*) const`
  - `bool Pipeline::Create(DXContext&, const wchar_t* shaderPath)`（编译 `VSMain`/`PSMain`）
  - `void Pipeline::Bind(ID3D12GraphicsCommandList*) const`（设 root signature + PSO）

- [ ] **Step 1: 写 mesh.h**

```cpp
#pragma once
#include "dx_context.h"
#include "dx_util.h"

struct Vertex {
    float px, py, pz;        // offset 0
    float cr, cg, cb, ca;    // offset 12
    float u, v;              // offset 28
};                            // sizeof = 36

class Mesh {
public:
    bool Create(DXContext& ctx, const Vertex* vertices, uint32_t vertexCount,
                const uint16_t* indices, uint32_t indexCount);
    void Draw(ID3D12GraphicsCommandList* cmd) const;

private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbv = {};
    D3D12_INDEX_BUFFER_VIEW m_ibv = {};
    uint32_t m_indexCount = 0;
};
```

- [ ] **Step 2: 写 mesh.cpp（upload heap，常驻 GENERIC_READ，无需 barrier）**

```cpp
#include "mesh.h"
#include <cstring>

static ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, const void* data, UINT byteSize) {
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = byteSize;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> buffer;
    ThrowIfFailed(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));
    void* p = nullptr;
    ThrowIfFailed(buffer->Map(0, nullptr, &p));
    memcpy(p, data, byteSize);
    buffer->Unmap(0, nullptr);
    return buffer;
}

bool Mesh::Create(DXContext& ctx, const Vertex* vertices, uint32_t vertexCount,
                  const uint16_t* indices, uint32_t indexCount) {
    m_indexCount = indexCount;
    const UINT vbSize = UINT(sizeof(Vertex) * vertexCount);
    const UINT ibSize = UINT(sizeof(uint16_t) * indexCount);

    m_vertexBuffer = CreateUploadBuffer(ctx.Device(), vertices, vbSize);
    m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbv.SizeInBytes = vbSize;
    m_vbv.StrideInBytes = sizeof(Vertex);

    m_indexBuffer = CreateUploadBuffer(ctx.Device(), indices, ibSize);
    m_ibv.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_ibv.SizeInBytes = ibSize;
    m_ibv.Format = DXGI_FORMAT_R16_UINT;
    return true;
}

void Mesh::Draw(ID3D12GraphicsCommandList* cmd) const {
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbv);
    cmd->IASetIndexBuffer(&m_ibv);
    cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
}
```

- [ ] **Step 3: 写 pipeline.h**

```cpp
#pragma once
#include "dx_context.h"

class Pipeline {
public:
    bool Create(DXContext& ctx, const wchar_t* shaderPath);
    void Bind(ID3D12GraphicsCommandList* cmd) const;

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pso;
};
```

- [ ] **Step 4: 写 pipeline.cpp（本 Task 版本：空 root signature、无深度）**

```cpp
#include "pipeline.h"
#include "dx_util.h"
#include <d3dcompiler.h>

static ComPtr<ID3DBlob> CompileShader(const wchar_t* path, const char* entry, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompileFromFile(path, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return blob;
}

bool Pipeline::Create(DXContext& ctx, const wchar_t* shaderPath) {
    ComPtr<ID3DBlob> vs = CompileShader(shaderPath, "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileShader(shaderPath, "PSMain", "ps_5_1");

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    ThrowIfFailed(ctx.Device()->CreateRootSignature(0, sig->GetBufferPointer(),
        sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { layout, _countof(layout) };
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = ctx.BackBufferFormat();
    pso.SampleDesc.Count = 1;
    ThrowIfFailed(ctx.Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
    return true;
}

void Pipeline::Bind(ID3D12GraphicsCommandList* cmd) const {
    cmd->SetGraphicsRootSignature(m_rootSignature.Get());
    cmd->SetPipelineState(m_pso.Get());
}
```

- [ ] **Step 5: 写 shaders/cube.hlsl（本 Task 版本：顶点色直出）**

```hlsl
struct VSIn {
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 1.0f);
    o.color = i.color;
    o.uv = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
    return i.color;
}
```

- [ ] **Step 6: CMakeLists.txt 整文件替换为**

```cmake
cmake_minimum_required(VERSION 3.24)
project(ARenderFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(framework WIN32
    src/main.cpp
    src/dx_context.cpp
    src/mesh.cpp
    src/pipeline.cpp
)
target_link_libraries(framework PRIVATE d3d12 dxgi d3dcompiler dxguid)

add_custom_command(TARGET framework POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:framework>/shaders)
```

- [ ] **Step 7: main.cpp 整文件替换为（在 Task 2 基础上加 pipeline + 三角形）**

```cpp
#include <windows.h>
#include <cwchar>
#include "dx_context.h"
#include "dx_util.h"
#include "mesh.h"
#include "pipeline.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    try {
        const wchar_t* kClass = L"ARenderFramework";
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);

        const int kWidth = 1280, kHeight = 720;
        RECT rc = { 0, 0, kWidth, kHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        HWND hwnd = CreateWindowExW(0, kClass, L"ARender Framework",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
        ShowWindow(hwnd, nCmdShow);

        DXContext ctx;
        if (!ctx.Initialize(hwnd, kWidth, kHeight)) return 1;

        Pipeline pipeline;
        pipeline.Create(ctx, L"shaders/cube.hlsl");

        const Vertex triVerts[] = {
            { -0.5f, -0.5f, 0.0f,  1,0,0,1,  0,0 },
            {  0.0f,  0.5f, 0.0f,  0,1,0,1,  0,0 },
            {  0.5f, -0.5f, 0.0f,  0,0,1,1,  0,0 },
        };
        const uint16_t triIndices[] = { 0, 1, 2 };
        Mesh tri;
        tri.Create(ctx, triVerts, 3, triIndices, 3);

        const bool smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
        const float clearColor[4] = { 0.10f, 0.20f, 0.40f, 1.0f };
        int frames = 0;
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                ctx.BeginFrame(clearColor);
                ID3D12GraphicsCommandList* cmd = ctx.CommandList();
                D3D12_VIEWPORT vp = { 0, 0, (float)kWidth, (float)kHeight, 0, 1 };
                D3D12_RECT sc = { 0, 0, kWidth, kHeight };
                cmd->RSSetViewports(1, &vp);
                cmd->RSSetScissorRects(1, &sc);
                pipeline.Bind(cmd);
                tri.Draw(cmd);
                ctx.EndFrame();
                ++frames;
                if (smoke && frames >= 120) PostQuitMessage(0);
            }
        }
        ctx.Shutdown();
        return 0;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "ARender error", MB_ICONERROR);
        return 1;
    }
}
```

- [ ] **Step 8: 构建 + 冒烟验证**

Run:
```bash
cmake --build framework/build --config Debug
(cd framework/build/Debug && ./framework.exe --smoke); echo $?
```
Expected: 编译 0 error；输出 `0`；无 D3D12 ERROR。

- [ ] **Step 9: 肉眼验收**

Run: `framework/build/Debug/framework.exe`
Expected: 深蓝背景中央一个 RGB 渐变三角形。

- [ ] **Step 10: Commit**

```bash
git add framework/
git commit -m "Stage0/Task3: shader+root signature+PSO+vertex buffer, colored triangle"
```

---

### Task 4: Constant Buffer + 矩阵 + 深度缓冲——旋转彩色立方体

**Files:**
- Modify: `framework/src/dx_context.h`（追加深度相关成员/方法，见 Step 1）
- Modify: `framework/src/dx_context.cpp`（追加 `CreateDepthBuffer`/`DSV`，替换 `BeginFrame`，见 Step 2）
- Modify: `framework/src/pipeline.cpp`（root signature 加 CBV；PSO 开深度，见 Step 3 整文件替换）
- Modify: `framework/shaders/cube.hlsl`（加 cbuffer，见 Step 4 整文件替换）
- Modify: `framework/src/main.cpp`（整文件替换，见 Step 5）

**Interfaces:**
- Consumes: Task 3 全部接口。
- Produces:
  - `bool DXContext::CreateDepthBuffer()`（初始化后、渲染前调用一次）
  - `D3D12_CPU_DESCRIPTOR_HANDLE DXContext::DSV() const`
  - 约定：root parameter 0 = CBV（b0，VERTEX 可见），内容为转置后的 MVP 矩阵（`DirectX::XMMATRIX`，64 字节，CB 尺寸 256 对齐）。

- [ ] **Step 1: dx_context.h——在 `public:` 区 `EndFrame();` 之后追加两个声明，在 `private:` 成员区末尾追加两个成员**

在 `void EndFrame();` 之后插入：
```cpp
    bool CreateDepthBuffer();
```

在 `DXGI_FORMAT BackBufferFormat() const { ... }` 之后插入：
```cpp
    D3D12_CPU_DESCRIPTOR_HANDLE DSV() const;
```

在 private 成员 `uint32_t m_height = 0;` 之后插入：
```cpp
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthBuffer;
```

- [ ] **Step 2: dx_context.cpp——替换 `BeginFrame` 整个函数，并在文件末尾追加两个函数**

`BeginFrame` 替换为：
```cpp
void DXContext::BeginFrame(const float clearColor[4]) {
    ThrowIfFailed(m_commandAllocators[m_backBufferIndex]->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_backBufferIndex].Get(), nullptr));

    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_backBuffers[m_backBufferIndex].Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &b);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = CurrentRTV();
    if (m_depthBuffer) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = DSV();
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    } else {
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
}
```

文件末尾追加：
```cpp
bool DXContext::CreateDepthBuffer() {
    D3D12_DESCRIPTOR_HEAP_DESC hd = {};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    hd.NumDescriptors = 1;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_dsvHeap)));

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = m_width;
    rd.Height = m_height;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_D32_FLOAT;
    rd.SampleDesc.Count = 1;
    D3D12_CLEAR_VALUE cv = {};
    cv.Format = DXGI_FORMAT_D32_FLOAT;
    cv.DepthStencil.Depth = 1.0f;
    ThrowIfFailed(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&m_depthBuffer)));
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), nullptr,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DXContext::DSV() const {
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}
```

- [ ] **Step 3: pipeline.cpp 整文件替换为（root sig 加 CBV；PSO 开深度）**

```cpp
#include "pipeline.h"
#include "dx_util.h"
#include <d3dcompiler.h>

static ComPtr<ID3DBlob> CompileShader(const wchar_t* path, const char* entry, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompileFromFile(path, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return blob;
}

bool Pipeline::Create(DXContext& ctx, const wchar_t* shaderPath) {
    ComPtr<ID3DBlob> vs = CompileShader(shaderPath, "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileShader(shaderPath, "PSMain", "ps_5_1");

    // root parameter 0: CBV(b0)，VERTEX 可见
    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = 0;
    param.Descriptor.RegisterSpace = 0;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.NumParameters = 1;
    rsd.pParameters = &param;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    ThrowIfFailed(ctx.Device()->CreateRootSignature(0, sig->GetBufferPointer(),
        sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { layout, _countof(layout) };
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = ctx.BackBufferFormat();
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    ThrowIfFailed(ctx.Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
    return true;
}

void Pipeline::Bind(ID3D12GraphicsCommandList* cmd) const {
    cmd->SetGraphicsRootSignature(m_rootSignature.Get());
    cmd->SetPipelineState(m_pso.Get());
}
```

- [ ] **Step 4: cube.hlsl 整文件替换为（加 cbuffer，做 MVP 变换）**

```hlsl
cbuffer FrameCB : register(b0) {
    float4x4 gMvp;
};

struct VSIn {
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), gMvp);
    o.color = i.color;
    o.uv = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
    return i.color;
}
```

- [ ] **Step 5: main.cpp 整文件替换为（深度 + CB + 立方体 + 旋转）**

```cpp
#include <windows.h>
#include <cwchar>
#include <cstring>
#include <DirectXMath.h>
#include "dx_context.h"
#include "dx_util.h"
#include "mesh.h"
#include "pipeline.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

// 24 顶点（每面 4 个，uv 独立），36 索引；每面一种颜色便于观察深度
static const Vertex kCubeVerts[] = {
    // +Z 白
    { -0.5f,-0.5f, 0.5f, 1,1,1,1, 0,1 }, {  0.5f,-0.5f, 0.5f, 1,1,1,1, 1,1 },
    {  0.5f, 0.5f, 0.5f, 1,1,1,1, 1,0 }, { -0.5f, 0.5f, 0.5f, 1,1,1,1, 0,0 },
    // -Z 红
    {  0.5f,-0.5f,-0.5f, 1,0.2f,0.2f,1, 0,1 }, { -0.5f,-0.5f,-0.5f, 1,0.2f,0.2f,1, 1,1 },
    { -0.5f, 0.5f,-0.5f, 1,0.2f,0.2f,1, 1,0 }, {  0.5f, 0.5f,-0.5f, 1,0.2f,0.2f,1, 0,0 },
    // +X 绿
    {  0.5f,-0.5f, 0.5f, 0.2f,1,0.2f,1, 0,1 }, {  0.5f,-0.5f,-0.5f, 0.2f,1,0.2f,1, 1,1 },
    {  0.5f, 0.5f,-0.5f, 0.2f,1,0.2f,1, 1,0 }, {  0.5f, 0.5f, 0.5f, 0.2f,1,0.2f,1, 0,0 },
    // -X 蓝
    { -0.5f,-0.5f,-0.5f, 0.2f,0.4f,1,1, 0,1 }, { -0.5f,-0.5f, 0.5f, 0.2f,0.4f,1,1, 1,1 },
    { -0.5f, 0.5f, 0.5f, 0.2f,0.4f,1,1, 1,0 }, { -0.5f, 0.5f,-0.5f, 0.2f,0.4f,1,1, 0,0 },
    // +Y 黄
    { -0.5f, 0.5f, 0.5f, 1,1,0.2f,1, 0,1 }, {  0.5f, 0.5f, 0.5f, 1,1,0.2f,1, 1,1 },
    {  0.5f, 0.5f,-0.5f, 1,1,0.2f,1, 1,0 }, { -0.5f, 0.5f,-0.5f, 1,1,0.2f,1, 0,0 },
    // -Y 品红
    { -0.5f,-0.5f,-0.5f, 1,0.2f,1,1, 0,1 }, {  0.5f,-0.5f,-0.5f, 1,0.2f,1,1, 1,1 },
    {  0.5f,-0.5f, 0.5f, 1,0.2f,1,1, 1,0 }, { -0.5f,-0.5f, 0.5f, 1,0.2f,1,1, 0,0 },
};
static const uint16_t kCubeIndices[] = {
    0,1,2, 0,2,3,   4,5,6, 4,6,7,   8,9,10, 8,10,11,
    12,13,14, 12,14,15,   16,17,18, 16,18,19,   20,21,22, 20,22,23,
};

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    try {
        const wchar_t* kClass = L"ARenderFramework";
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);

        const int kWidth = 1280, kHeight = 720;
        RECT rc = { 0, 0, kWidth, kHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        HWND hwnd = CreateWindowExW(0, kClass, L"ARender Framework",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
        ShowWindow(hwnd, nCmdShow);

        DXContext ctx;
        if (!ctx.Initialize(hwnd, kWidth, kHeight)) return 1;
        ctx.CreateDepthBuffer();

        Pipeline pipeline;
        pipeline.Create(ctx, L"shaders/cube.hlsl");

        Mesh cube;
        cube.Create(ctx, kCubeVerts, 24, kCubeIndices, 36);

        // 帧常量缓冲：upload heap 常驻 Map，每帧写入转置后的 MVP
        ComPtr<ID3D12Resource> frameCB;
        void* cbMapped = nullptr;
        {
            D3D12_HEAP_PROPERTIES hp = {};
            hp.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = 256;
            rd.Height = 1;
            rd.DepthOrArraySize = 1;
            rd.MipLevels = 1;
            rd.SampleDesc.Count = 1;
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ThrowIfFailed(ctx.Device()->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frameCB)));
            ThrowIfFailed(frameCB->Map(0, nullptr, &cbMapped));
        }

        const bool smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
        const float clearColor[4] = { 0.10f, 0.20f, 0.40f, 1.0f };
        int frames = 0;
        float angle = 0.0f;
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                using namespace DirectX;
                angle += 0.02f;
                XMMATRIX world = XMMatrixRotationY(angle);
                XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 0, -3, 1),
                                                 XMVectorSet(0, 0, 0, 1),
                                                 XMVectorSet(0, 1, 0, 0));
                XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, kWidth / (float)kHeight, 0.1f, 100.0f);
                XMMATRIX mvp = XMMatrixTranspose(world * view * proj);
                memcpy(cbMapped, &mvp, sizeof(mvp));

                ctx.BeginFrame(clearColor);
                ID3D12GraphicsCommandList* cmd = ctx.CommandList();
                D3D12_VIEWPORT vp = { 0, 0, (float)kWidth, (float)kHeight, 0, 1 };
                D3D12_RECT sc = { 0, 0, kWidth, kHeight };
                cmd->RSSetViewports(1, &vp);
                cmd->RSSetScissorRects(1, &sc);
                pipeline.Bind(cmd);
                cmd->SetGraphicsRootConstantBufferView(0, frameCB->GetGPUVirtualAddress());
                cube.Draw(cmd);
                ctx.EndFrame();
                ++frames;
                if (smoke && frames >= 120) PostQuitMessage(0);
            }
        }
        ctx.Shutdown();
        return 0;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "ARender error", MB_ICONERROR);
        return 1;
    }
}
```

- [ ] **Step 6: 构建 + 冒烟验证**

Run:
```bash
cmake --build framework/build --config Debug
(cd framework/build/Debug && ./framework.exe --smoke); echo $?
```
Expected: 编译 0 error；输出 `0`；无 D3D12 ERROR。

- [ ] **Step 7: 肉眼验收**

Run: `framework/build/Debug/framework.exe`
Expected: 一个六面不同颜色的立方体绕 Y 轴旋转，无面穿插错误（深度正确）。

- [ ] **Step 8: Commit**

```bash
git add framework/
git commit -m "Stage0/Task4: constant buffer + matrices + depth buffer, rotating colored cube"
```

---

### Task 5: 纹理 + SRV + 静态采样器——旋转纹理立方体（阶段 0 交付物）

**Files:**
- Modify: `framework/src/dx_context.h`（追加 `ExecuteAndWait` 声明）
- Modify: `framework/src/dx_context.cpp`（追加 `ExecuteAndWait` 实现）
- Create: `framework/src/texture.h`
- Create: `framework/src/texture.cpp`
- Modify: `framework/src/pipeline.cpp`（root sig 加 SRV 表 + 静态采样器，整文件替换）
- Modify: `framework/shaders/cube.hlsl`（加纹理采样，整文件替换）
- Modify: `framework/src/main.cpp`（CBV/SRV heap、SRV、绑定，整文件替换）
- Modify: `framework/CMakeLists.txt`（add_executable 增加 `src/texture.cpp`）

**Interfaces:**
- Consumes: Task 4 全部接口。
- Produces:
  - `void DXContext::ExecuteAndWait(ID3D12GraphicsCommandList*)`（一次性上传用：close → execute → 新建 fence 等待 GPU 完成）
  - `bool Texture2D::CreateCheckerboard(DXContext&, uint32_t size)`
  - `ID3D12Resource* Texture2D::Resource() const`
  - 约定：root parameter 1 = descriptor table（SRV t0，PIXEL 可见）；静态采样器 s0；CBV/SRV heap 槽位 1 放纹理 SRV（槽位 0 预留未用——CBV 走 root descriptor）。

- [ ] **Step 1: dx_context.h——在 `bool CreateDepthBuffer();` 之后追加**

```cpp
    void ExecuteAndWait(ID3D12GraphicsCommandList* list);
```

- [ ] **Step 2: dx_context.cpp 文件末尾追加**

```cpp
void DXContext::ExecuteAndWait(ID3D12GraphicsCommandList* list) {
    ThrowIfFailed(list->Close());
    ID3D12CommandList* lists[] = { list };
    m_commandQueue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    ThrowIfFailed(m_commandQueue->Signal(fence.Get(), 1));
    HANDLE evt = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (fence->GetCompletedValue() < 1) {
        ThrowIfFailed(fence->SetEventOnCompletion(1, evt));
        WaitForSingleObject(evt, INFINITE);
    }
    CloseHandle(evt);
}
```

- [ ] **Step 3: 写 texture.h**

```cpp
#pragma once
#include "dx_context.h"

class Texture2D {
public:
    bool CreateCheckerboard(DXContext& ctx, uint32_t size);
    ID3D12Resource* Resource() const { return m_texture.Get(); }

private:
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12Resource> m_uploadBuffer;
};
```

- [ ] **Step 4: 写 texture.cpp（程序生成棋盘格；手动对齐行距上传，不用 d3dx12.h）**

```cpp
#include "texture.h"
#include "dx_util.h"
#include <vector>
#include <cstring>

bool Texture2D::CreateCheckerboard(DXContext& ctx, uint32_t size) {
    // 1) CPU 生成棋盘格像素（R8G8B8A8）
    std::vector<uint8_t> pixels(size_t(size) * size * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const uint8_t v = ((x / 32 + y / 32) % 2) ? 230 : 40;
            uint8_t* px = &pixels[(size_t(y) * size + x) * 4];
            px[0] = v; px[1] = v; px[2] = v; px[3] = 255;
        }
    }

    // 2) default heap 纹理，初始状态 COPY_DEST
    D3D12_HEAP_PROPERTIES defaultHp = {};
    defaultHp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = size;
    td.Height = size;
    td.DepthOrArraySize = 1;
    td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    ThrowIfFailed(ctx.Device()->CreateCommittedResource(&defaultHp, D3D12_HEAP_FLAG_NONE, &td,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture)));

    // 3) upload buffer，按 GPU 要求的 RowPitch 排布
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {};
    UINT numRows = 0;
    UINT64 rowSize = 0, totalSize = 0;
    ctx.Device()->GetCopyableFootprints(&td, 0, 1, 0, &fp, &numRows, &rowSize, &totalSize);

    D3D12_HEAP_PROPERTIES uploadHp = {};
    uploadHp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd = {};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bd.Width = totalSize;
    bd.Height = 1;
    bd.DepthOrArraySize = 1;
    bd.MipLevels = 1;
    bd.Format = DXGI_FORMAT_UNKNOWN;
    bd.SampleDesc.Count = 1;
    bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ThrowIfFailed(ctx.Device()->CreateCommittedResource(&uploadHp, D3D12_HEAP_FLAG_NONE, &bd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_uploadBuffer)));
    uint8_t* mapped = nullptr;
    ThrowIfFailed(m_uploadBuffer->Map(0, nullptr, (void**)&mapped));
    for (UINT y = 0; y < numRows; ++y)
        memcpy(mapped + fp.Offset + size_t(y) * fp.Footprint.RowPitch,
               pixels.data() + size_t(y) * rowSize, rowSize);
    m_uploadBuffer->Unmap(0, nullptr);

    // 4) 一次性命令列表：拷贝 + barrier 到 PIXEL_SHADER_RESOURCE，然后等 GPU 完成
    ComPtr<ID3D12CommandAllocator> alloc;
    ThrowIfFailed(ctx.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));
    ComPtr<ID3D12GraphicsCommandList> list;
    ThrowIfFailed(ctx.Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        alloc.Get(), nullptr, IID_PPV_ARGS(&list)));

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = fp;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = m_texture.Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &b);

    ctx.ExecuteAndWait(list.Get());
    return true;
}
```

- [ ] **Step 5: pipeline.cpp 整文件替换为（root sig：CBV + SRV 表 + 静态采样器）**

```cpp
#include "pipeline.h"
#include "dx_util.h"
#include <d3dcompiler.h>

static ComPtr<ID3DBlob> CompileShader(const wchar_t* path, const char* entry, const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompileFromFile(path, nullptr, nullptr, entry, target, flags, 0, &blob, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    return blob;
}

bool Pipeline::Create(DXContext& ctx, const wchar_t* shaderPath) {
    ComPtr<ID3DBlob> vs = CompileShader(shaderPath, "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileShader(shaderPath, "PSMain", "ps_5_1");

    // root param 0: CBV(b0)，VERTEX 可见
    D3D12_ROOT_PARAMETER params[2] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].Descriptor.RegisterSpace = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // root param 1: descriptor table，SRV(t0)，PIXEL 可见
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 静态采样器 s0
    D3D12_STATIC_SAMPLER_DESC samp = {};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.ShaderRegister = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samp.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_ROOT_SIGNATURE_DESC rsd = {};
    rsd.NumParameters = 2;
    rsd.pParameters = params;
    rsd.NumStaticSamplers = 1;
    rsd.pStaticSamplers = &samp;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ThrowIfFailed(hr);
    }
    ThrowIfFailed(ctx.Device()->CreateRootSignature(0, sig->GetBufferPointer(),
        sig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.InputLayout = { layout, _countof(layout) };
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = ctx.BackBufferFormat();
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    ThrowIfFailed(ctx.Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)));
    return true;
}

void Pipeline::Bind(ID3D12GraphicsCommandList* cmd) const {
    cmd->SetGraphicsRootSignature(m_rootSignature.Get());
    cmd->SetPipelineState(m_pso.Get());
}
```

- [ ] **Step 6: cube.hlsl 整文件替换为（纹理 × 顶点色）**

```hlsl
cbuffer FrameCB : register(b0) {
    float4x4 gMvp;
};

Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct VSIn {
    float3 pos : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), gMvp);
    o.color = i.color;
    o.uv = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
    return gTex.Sample(gSamp, i.uv) * i.color;
}
```

- [ ] **Step 7: CMakeLists.txt 的 add_executable 改为**

```cmake
add_executable(framework WIN32
    src/main.cpp
    src/dx_context.cpp
    src/mesh.cpp
    src/pipeline.cpp
    src/texture.cpp
)
```

- [ ] **Step 8: main.cpp 整文件替换为（SRV 创建与绑定）**

```cpp
#include <windows.h>
#include <cwchar>
#include <cstring>
#include <DirectXMath.h>
#include "dx_context.h"
#include "dx_util.h"
#include "mesh.h"
#include "pipeline.h"
#include "texture.h"

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

static const Vertex kCubeVerts[] = {
    // +Z 白
    { -0.5f,-0.5f, 0.5f, 1,1,1,1, 0,1 }, {  0.5f,-0.5f, 0.5f, 1,1,1,1, 1,1 },
    {  0.5f, 0.5f, 0.5f, 1,1,1,1, 1,0 }, { -0.5f, 0.5f, 0.5f, 1,1,1,1, 0,0 },
    // -Z 红
    {  0.5f,-0.5f,-0.5f, 1,0.2f,0.2f,1, 0,1 }, { -0.5f,-0.5f,-0.5f, 1,0.2f,0.2f,1, 1,1 },
    { -0.5f, 0.5f,-0.5f, 1,0.2f,0.2f,1, 1,0 }, {  0.5f, 0.5f,-0.5f, 1,0.2f,0.2f,1, 0,0 },
    // +X 绿
    {  0.5f,-0.5f, 0.5f, 0.2f,1,0.2f,1, 0,1 }, {  0.5f,-0.5f,-0.5f, 0.2f,1,0.2f,1, 1,1 },
    {  0.5f, 0.5f,-0.5f, 0.2f,1,0.2f,1, 1,0 }, {  0.5f, 0.5f, 0.5f, 0.2f,1,0.2f,1, 0,0 },
    // -X 蓝
    { -0.5f,-0.5f,-0.5f, 0.2f,0.4f,1,1, 0,1 }, { -0.5f,-0.5f, 0.5f, 0.2f,0.4f,1,1, 1,1 },
    { -0.5f, 0.5f, 0.5f, 0.2f,0.4f,1,1, 1,0 }, { -0.5f, 0.5f,-0.5f, 0.2f,0.4f,1,1, 0,0 },
    // +Y 黄
    { -0.5f, 0.5f, 0.5f, 1,1,0.2f,1, 0,1 }, {  0.5f, 0.5f, 0.5f, 1,1,0.2f,1, 1,1 },
    {  0.5f, 0.5f,-0.5f, 1,1,0.2f,1, 1,0 }, { -0.5f, 0.5f,-0.5f, 1,1,0.2f,1, 0,0 },
    // -Y 品红
    { -0.5f,-0.5f,-0.5f, 1,0.2f,1,1, 0,1 }, {  0.5f,-0.5f,-0.5f, 1,0.2f,1,1, 1,1 },
    {  0.5f,-0.5f, 0.5f, 1,0.2f,1,1, 1,0 }, { -0.5f,-0.5f, 0.5f, 1,0.2f,1,1, 0,0 },
};
static const uint16_t kCubeIndices[] = {
    0,1,2, 0,2,3,   4,5,6, 4,6,7,   8,9,10, 8,10,11,
    12,13,14, 12,14,15,   16,17,18, 16,18,19,   20,21,22, 20,22,23,
};

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    try {
        const wchar_t* kClass = L"ARenderFramework";
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClass;
        RegisterClassExW(&wc);

        const int kWidth = 1280, kHeight = 720;
        RECT rc = { 0, 0, kWidth, kHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        HWND hwnd = CreateWindowExW(0, kClass, L"ARender Framework",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
        ShowWindow(hwnd, nCmdShow);

        DXContext ctx;
        if (!ctx.Initialize(hwnd, kWidth, kHeight)) return 1;
        ctx.CreateDepthBuffer();

        Pipeline pipeline;
        pipeline.Create(ctx, L"shaders/cube.hlsl");

        Mesh cube;
        cube.Create(ctx, kCubeVerts, 24, kCubeIndices, 36);

        // 帧常量缓冲（root CBV 直接绑 GPU 地址）
        ComPtr<ID3D12Resource> frameCB;
        void* cbMapped = nullptr;
        {
            D3D12_HEAP_PROPERTIES hp = {};
            hp.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC rd = {};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = 256;
            rd.Height = 1;
            rd.DepthOrArraySize = 1;
            rd.MipLevels = 1;
            rd.SampleDesc.Count = 1;
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ThrowIfFailed(ctx.Device()->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frameCB)));
            ThrowIfFailed(frameCB->Map(0, nullptr, &cbMapped));
        }

        // CBV/SRV/UAV shader-visible heap：槽位 0 预留，槽位 1 放纹理 SRV
        ComPtr<ID3D12DescriptorHeap> cbvSrvHeap;
        {
            D3D12_DESCRIPTOR_HEAP_DESC hd = {};
            hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hd.NumDescriptors = 2;
            hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(ctx.Device()->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&cbvSrvHeap)));
        }
        const UINT cbvSrvIncr = ctx.Device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        Texture2D checker;
        checker.CreateCheckerboard(ctx, 256);
        {
            D3D12_CPU_DESCRIPTOR_HANDLE h = cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
            h.ptr += SIZE_T(1) * cbvSrvIncr;
            D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
            sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            sd.Texture2D.MipLevels = 1;
            ctx.Device()->CreateShaderResourceView(checker.Resource(), &sd, h);
        }

        const bool smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
        const float clearColor[4] = { 0.10f, 0.20f, 0.40f, 1.0f };
        int frames = 0;
        float angle = 0.0f;
        MSG msg = {};
        while (msg.message != WM_QUIT) {
            if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            } else {
                using namespace DirectX;
                angle += 0.02f;
                XMMATRIX world = XMMatrixRotationY(angle);
                XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 0, -3, 1),
                                                 XMVectorSet(0, 0, 0, 1),
                                                 XMVectorSet(0, 1, 0, 0));
                XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, kWidth / (float)kHeight, 0.1f, 100.0f);
                XMMATRIX mvp = XMMatrixTranspose(world * view * proj);
                memcpy(cbMapped, &mvp, sizeof(mvp));

                ctx.BeginFrame(clearColor);
                ID3D12GraphicsCommandList* cmd = ctx.CommandList();
                D3D12_VIEWPORT vp = { 0, 0, (float)kWidth, (float)kHeight, 0, 1 };
                D3D12_RECT sc = { 0, 0, kWidth, kHeight };
                cmd->RSSetViewports(1, &vp);
                cmd->RSSetScissorRects(1, &sc);
                ID3D12DescriptorHeap* heaps[] = { cbvSrvHeap.Get() };
                cmd->SetDescriptorHeaps(1, heaps);
                pipeline.Bind(cmd);
                cmd->SetGraphicsRootConstantBufferView(0, frameCB->GetGPUVirtualAddress());
                D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();
                srvHandle.ptr += UINT64(1) * cbvSrvIncr;
                cmd->SetGraphicsRootDescriptorTable(1, srvHandle);
                cube.Draw(cmd);
                ctx.EndFrame();
                ++frames;
                if (smoke && frames >= 120) PostQuitMessage(0);
            }
        }
        ctx.Shutdown();
        return 0;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "ARender error", MB_ICONERROR);
        return 1;
    }
}
```

- [ ] **Step 9: 构建 + 冒烟验证**

Run:
```bash
cmake --build framework/build --config Debug
(cd framework/build/Debug && ./framework.exe --smoke); echo $?
```
Expected: 编译 0 error；输出 `0`；无 D3D12 ERROR。

- [ ] **Step 10: 肉眼验收（阶段 0 交付物）**

Run: `framework/build/Debug/framework.exe`
Expected: 立方体绕 Y 轴旋转，表面为棋盘格纹理并被面颜色染色（白面是纯棋盘格），无拉伸错乱。

- [ ] **Step 11: Commit**

```bash
git add framework/
git commit -m "Stage0/Task5: checkerboard texture + SRV + static sampler, textured rotating cube"
```

---

### Task 6: 路线文档与阶段笔记骨架

**Files:**
- Create: `docs/roadmap.md`
- Create: `docs/notes/01-dx12-minimal.md`

**Interfaces:**
- Consumes: 无代码依赖。
- Produces: 后续阶段共用的路线图文档；阶段 0 笔记模板（学习者在回顾时填写）。

- [ ] **Step 1: 写 docs/roadmap.md**

```markdown
# 渲染学习路线（Roadmap）

| 阶段 | 主题 | 计划时长 | 状态 | 实际耗时 | 笔记 |
|---|---|---|---|---|---|
| 0 | DX12 最小可用框架 | ≤2 周 | ✅ | （回顾时填写） | [01-dx12-minimal](notes/01-dx12-minimal.md) |
| 1 | 现代渲染框架核心（延迟渲染、clustered、GPU-driven、render graph） | ~4 周 | ⬜ | | |
| 2 | PBR 与材质（BRDF/IBL） | ~3 周 | ⬜ | | |
| 3 | 阴影（CSM/PCSS/VSM） | ~3 周 | ⬜ | | |
| 4 | TAA 与上采样 | ~3 周 | ⬜ | | |
| 5 | GI 与几何管线（Lumen/Nanite 原理） | ~8 周 | ⬜ | | |
| 6 | 综合：UE5 一帧拆解 + 源码选读 + 总结文章 | ~4 周 | ⬜ | | |

- 设计文档：[2026-08-04-modern-rendering-learning-design.md](../superpowers/specs/2026-08-04-modern-rendering-learning-design.md)
- 调整原则：每完成一个阶段回顾实际耗时和卡点，必要时重排后续阶段。
```

- [ ] **Step 2: 写 docs/notes/01-dx12-minimal.md**

```markdown
# 阶段 0：DX12 最小可用

> 阶段完成后回顾填写。用自己的话，写错没关系，之后阶段再回来修正。

## 核心概念

- swap chain / 命令队列 / 命令列表 / 命令分配器：
- PSO（为什么把管线状态打包成一个对象）：
- root signature / descriptor / descriptor heap：
- upload heap vs default heap：
- resource barrier 为什么存在：
- fence 与双缓冲帧同步：

## 踩坑记录

-

## 待回头补（用到再学）

- （候选：多队列、内存分配器、bindless）
```

- [ ] **Step 3: Commit**

```bash
git add docs/roadmap.md docs/notes/01-dx12-minimal.md
git commit -m "Stage0/Task6: roadmap + stage-0 notes skeleton"
```

---

## 完成标准（对照 spec 阶段 0）

1. `framework/` 可一键构建，`--smoke` 退出码 0，肉眼看到旋转纹理立方体。
2. 六样最小集全部出现且只用这些：swap chain/命令队列（Task 2）、PSO（Task 3）、root signature 与 descriptor（Task 3/4/5）、vertex/index buffer 与 upload heap（Task 3）、depth buffer（Task 4）、基本 barrier（Task 2/5）。
3. `docs/roadmap.md` 与笔记骨架就位，阶段 0 打勾。
