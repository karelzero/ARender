# 阶段 0：DX12 最小可用框架 实施计划

> **本计划不含任何实现代码（项目约定，见 CLAUDE.md）。** 代码由学习者亲手编写；卡顿时查阅指定资料或讨论思路。Steps 用 checkbox（`- [ ]`）跟踪进度。

**Goal:** 搭出一个能画场景的 DX12 最小渲染器，最终交付一个旋转纹理立方体，作为后续所有专题持续演进的地基。

**Architecture（目标形态，自行实现）:** 单 Visual Studio 2026 solution、单原生 C++ project、WIN32 可执行文件。建议拆成四个职责单一的组件：DXContext（设备/命令队列/swap chain/RTV/DSV/fence 与帧同步）、Mesh（顶点/索引缓冲与绘制）、Pipeline（shader 编译、root signature、PSO）、Texture2D（纹理创建与上传），由 main.cpp 驱动窗口和帧循环。

**Tech Stack:** C++20、DirectX 12（Windows SDK 自带，含 DirectXMath）、HLSL 5.1（运行时用 `D3DCompileFromFile` 编译）、Visual Studio 2026。

## Global Constraints

- 只用 DX12 最小集：swap chain/命令队列、PSO、root signature 与 descriptor、vertex/index buffer 与 upload heap、depth buffer、基本 barrier。**不要引入**：多队列、自定义内存分配器、bindless、DirectXTK、d3dx12.h、vcpkg 等外部依赖。
- 环境：Windows + Visual Studio 2026（Desktop development with C++）+ Windows SDK。
- 验收统一两种方式：① 程序支持 `--smoke` 参数（跑 120 帧后自动退出、返回码 0）；② Debug 配置下 D3D12 debug layer 打开，调试输出无 ERROR。每个任务另附肉眼验收项。
- 目标周期 2 周（约 15 小时），两周时必须复盘；15 小时是估算，不是失败标准。单个非核心问题卡住超过 2 小时：记录到笔记"待回头补"，继续推进或提出讨论。fence、barrier、资源生命周期等核心概念未理解时，先补清楚再进入阶段 1。
- 主工程在后续阶段持续演进，不复制工程。每完成一个任务 commit 一次；阶段完成后打 tag `stage-0-dx12-minimal`。
- Visual Studio 的输出目录统一设置为 `bin/$(Platform)/$(Configuration)/`，中间目录设置为 `build/$(Platform)/$(Configuration)/`；两者均不提交 Git。
- shader 从 Task 3 起通过项目的 Post-Build Event 复制到 exe 旁边；调试器 Working Directory 设为 `$(TargetDir)`，保证命令行运行和 F5 调试使用相同资源路径。

## 通用参考资料（整阶段有效，各任务再标注重点）

- **官方示例集**：GitHub `microsoft/DirectX-Graphics-Samples` 中的 `Samples/Desktop/D3D12HelloWorld` 系列——本阶段四个任务与 `HelloWindow`、`HelloTriangle`、`HelloConstBuffers`、`HelloTexture` 四个示例一一对应，是首要参照。
- **书**：《Introduction to 3D Game Programming with DirectX 12》（Frank Luna）——按需读 Direct3D 初始化、渲染管线、绘制、常量缓冲、纹理相关章节，不要通读。
- **文档**：Microsoft Learn「Direct3D 12 programming guide」，重点小节会在各任务标注。

---

### Task 1: VS2026 工程 + Win32 窗口 + 冒烟模式

**目标：** 能构建出一个显示空白窗口的 exe，支持 `--smoke` 参数自动退出。

- [ ] **Step 1:** 在仓库根目录创建 `ARender.sln`，添加名为 `ARender` 的 C++ Empty Project，项目文件放在 `renderer/`。只保留或启用 x64 平台，后续不再为各阶段创建工程副本。
- [ ] **Step 2:** 在项目属性中统一配置 Debug/Release：C++20、Unicode、Windows 子系统、输出目录、中间目录；链接 `d3d12.lib`、`dxgi.lib`、`d3dcompiler.lib`、`dxguid.lib`。确认 `ARender` 是启动项目。
- [ ] **Step 3:** 创建 `renderer/src/main.cpp` 并通过 Visual Studio 加入项目：注册窗口类、创建 1280x720 窗口、使用 PeekMessage 消息循环；解析命令行，含 `--smoke` 时在 120 次无消息循环更新后退出；返回值 0 表示成功。
- [ ] **Step 4:** 在 Visual Studio 中选择 `Debug | x64`，执行 Build Solution。Expected：编译链接 0 error，目标文件生成在约定的 `bin/x64/Debug/`。
- [ ] **Step 5:** 在 exe 输出目录运行 `ARender.exe --smoke` 并检查退出码。Expected：窗口闪现后自动关闭，退出码为 0。
- [ ] **Step 6:** 不带参数按 F5 调试。Expected：出现空白窗口且可正常关闭；断点、调用栈和变量查看正常。
- [ ] **Step 7:** Commit（`Stage0/Task1: VS2026 project + Win32 window with smoke mode`）

---

### Task 2: DXContext——设备、命令队列、swap chain、RTV、fence，清屏并 Present

**目标：** 窗口每帧清成深蓝色并正常 Present，双缓冲 + fence 帧同步正确。

**要做的：**
- 新建 `dx_util.h`：HRESULT 检查助手（失败抛异常）。
- 新建 `dx_context.h/.cpp`：Debug 配置开启 D3D12 debug layer；枚举硬件 adapter 建 device（失败退回 WARP）；direct 命令队列；flip model 双缓冲 swap chain；RTV descriptor heap + 两个 back buffer 的 RTV；每帧一个 command allocator + 一个命令列表；fence + event 做帧同步（建议参照官方示例的 `MoveToNextFrame` 模式）。
- 对外接口保持简单：`Initialize / Shutdown / BeginFrame(清屏色) / EndFrame`，以及暴露 device 和命令列表的访问器。BeginFrame 里做 PRESENT→RENDER_TARGET barrier、绑 RTV、清屏；EndFrame 做反向 barrier、关闭并提交列表、Present、推进帧。
- main.cpp 接入：帧循环中 BeginFrame/EndFrame，设好 viewport 和 scissor。

**重点阅读：**
- 官方示例 `D3D12HelloWindow`（整个流程几乎就是这个任务）。
- Microsoft Learn「Managing Object Lifetime」与「Resource Barriers」小节——理解为什么需要 barrier 和 fence。

- [ ] **Step 1:** 阅读 `D3D12HelloWindow` 示例源码，用自己的话在纸上画出对象创建顺序图。
- [ ] **Step 2:** 实现 dx_util.h 与 dx_context.h/.cpp。
- [ ] **Step 3:** main.cpp 接入 DXContext，帧循环改为清屏 + Present。
- [ ] **Step 4:** 在 Visual Studio 中构建并运行 `--smoke`；从本任务起按完成 Present 的帧数计数，120 帧后退出码为 0。检查 VS 调试输出无 D3D12 ERROR（有 ERROR 优先修，多半是 barrier 或同步问题）。
- [ ] **Step 5:** 肉眼验收：窗口稳定深蓝色（如 0.1/0.2/0.4），无闪烁。
- [ ] **Step 6:** Commit（`Stage0/Task2: DXContext with device/swapchain/RTV/fence, clear to blue`）

---

### Task 3: Shader + Root Signature + PSO + Vertex Buffer——彩色三角形

**目标：** 画出一个 RGB 顶点色三角形，理解从"清屏"到"绘制"跨越了哪些对象。

**要做的：**
- 新建 `mesh.h/.cpp`：定义含位置/颜色/UV 的顶点结构体（UV 本任务不用，但格式一次定好）；用 upload heap 建顶点/索引缓冲（常驻 GENERIC_READ，无需 barrier），提供 `Create` 和 `Draw`。
- 新建 `pipeline.h/.cpp`：运行时编译 `shaders/cube.hlsl`（VSMain/PSMain，vs_5_1/ps_5_1，编译错误输出到调试窗口）；本任务 root signature 为空参数；创建 PSO（手写填各状态结构，cull 先关掉避免绕序问题）。
- 新建 `shaders/cube.hlsl`：顶点色直出。
- main.cpp：创建三角形 mesh，帧循环中 Bind pipeline 后 Draw。
- 新增源文件通过 Visual Studio 加入现有 `ARender` project；配置 Post-Build Event，把 shader 目录复制到 `$(TargetDir)/shaders/`。

**重点阅读：**
- 官方示例 `D3D12HelloTriangle`。
- Microsoft Learn「Pipeline State Objects」与「Root Signatures」小节——理解这两个 DX9 没有的对象为什么存在（这是 DX12 的核心心智转变，值得在笔记里写一段）。
- Frank Luna 书渲染管线一章，回顾 VS/PS 之外还有哪些固定功能阶段。

- [ ] **Step 1:** 阅读 `D3D12HelloTriangle`，对照 Task 2 的 HelloWindow 找出"多了哪些对象"。
- [ ] **Step 2:** 实现 mesh.h/.cpp。
- [ ] **Step 3:** 实现 pipeline.h/.cpp 与 cube.hlsl。
- [ ] **Step 4:** 将新增源文件和 shader 加入现有 VS project，配置 shader 构建后复制，并更新 main.cpp。
- [ ] **Step 5:** 构建 + 冒烟验证 → 退出码 0，无 D3D12 ERROR。
- [ ] **Step 6:** 肉眼验收：深蓝背景中央 RGB 渐变三角形。
- [ ] **Step 7:** Commit（`Stage0/Task3: shader+root signature+PSO+vertex buffer, colored triangle`）

---

### Task 4: Constant Buffer + 矩阵 + 深度缓冲——旋转彩色立方体

**目标：** 三角形变成立方体，绕 Y 轴旋转，深度正确。

**要做的：**
- DXContext 增加深度支持：`CreateDepthBuffer()`（D32_FLOAT、default heap、DSV heap），BeginFrame 在有深度缓冲时同时绑 DSV 并清深度。
- pipeline：root signature 加一个 CBV root descriptor 参数（b0，VERTEX 可见）；PSO 开启深度测试（LESS）并设置 DSV 格式。
- shader：加 cbuffer（b0）放 MVP 矩阵，VS 中变换顶点。注意行主序/列主序——C++ 侧上传前转置，或 shader 侧调整乘法顺序，选一种并在笔记里记一句原因。
- main.cpp：upload heap 常量缓冲（256 字节对齐，常驻 Map），每帧用 DirectXMath 计算 world/view/proj 合成 MVP 写入；`SetGraphicsRootConstantBufferView` 绑定。三角形数据换成 24 顶点 36 索引立方体（每面 4 个独立顶点，各面不同颜色便于观察）。
- 顶点/索引 buffer 代码不用改——Create 接口设计得够通用就能直接复用，验证一下你的抽象。

**重点阅读：**
- 官方示例 `D3D12HelloConstBuffers`。
- Microsoft Learn「Constant Buffer View」与上传堆相关小节——理解为什么 CB 适合 upload heap 常驻 Map，而纹理不适合。
- DirectXMath 文档中 `XMMatrixLookAtLH / XMMatrixPerspectiveFovLH / XMMatrixTranspose` 的说明。

- [ ] **Step 1:** 阅读 `D3D12HelloConstBuffers`，注意它如何对齐 CB 尺寸、如何每帧更新。
- [ ] **Step 2:** DXContext 增加深度缓冲支持。
- [ ] **Step 3:** 更新 pipeline（root sig + PSO）与 cube.hlsl（cbuffer + MVP）。
- [ ] **Step 4:** main.cpp：常量缓冲、立方体数据、每帧矩阵更新与绑定。
- [ ] **Step 5:** 构建 + 冒烟验证 → 退出码 0，无 D3D12 ERROR。
- [ ] **Step 6:** 肉眼验收：六面不同颜色的立方体绕 Y 轴旋转，无面穿插（深度正确）。如果看到面互相穿透，先查深度是否真的开启，再查 DSV 是否每帧被清。
- [ ] **Step 7:** Commit（`Stage0/Task4: constant buffer + matrices + depth buffer, rotating colored cube`）

---

### Task 5: 纹理 + SRV + 静态采样器——旋转纹理立方体（阶段 0 交付物）

**目标：** 立方体贴上程序生成的棋盘格纹理。这是纹理上传这条 DX12 经典长流程的唯一一次完整演练。

**要做的：**
- DXContext 增加 `ExecuteAndWait`：关闭并提交一个一次性命令列表，用独立 fence 等 GPU 完成（纹理上传期间要用）。
- 新建 `texture.h/.cpp`：CPU 生成 256x256 棋盘格像素（R8G8B8A8）；建 default heap 纹理（初始 COPY_DEST）；用 `GetCopyableFootprints` 算出带行距对齐的上传布局，建 upload buffer 按 RowPitch 逐行拷贝；一次性命令列表执行 `CopyTextureRegion` + barrier 到 PIXEL_SHADER_RESOURCE；`ExecuteAndWait` 等完成。**不用** d3dx12.h 的 UpdateSubresources——手写一遍对齐逻辑，这是本任务的主要学习点。
- pipeline：root signature 加 descriptor table（一个 SRV，t0，PIXEL 可见）+ 静态采样器（s0，线性过滤、wrap）。
- shader：`Texture2D`（t0）+ `SamplerState`（s0），PS 中采样并与顶点色相乘。
- main.cpp：建 CBV/SRV/UAV shader-visible heap（槽位 1 放纹理 SRV，槽位 0 可预留）；帧循环中 `SetDescriptorHeaps` + `SetGraphicsRootDescriptorTable` 绑定。

**重点阅读：**
- 官方示例 `D3D12HelloTexture`。
- Microsoft Learn「Uploading Texture Data through Buffers」——行距对齐（256 字节）的原因。
- 「Descriptor Heaps」小节——shader-visible 与非 shader-visible heap 的区别。

- [ ] **Step 1:** 阅读 `D3D12HelloTexture`，画出"像素数据从内存到 shader"的完整路径图（upload buffer → CopyTextureRegion → barrier → SRV → Sample）。
- [ ] **Step 2:** DXContext 增加 ExecuteAndWait。
- [ ] **Step 3:** 实现 texture.h/.cpp。
- [ ] **Step 4:** 更新 pipeline（table + 静态采样器）与 cube.hlsl（采样）。
- [ ] **Step 5:** main.cpp：heap、SRV、绑定。
- [ ] **Step 6:** 构建 + 冒烟验证 → 退出码 0，无 D3D12 ERROR。
- [ ] **Step 7:** 肉眼验收（阶段 0 交付物）：旋转立方体表面为棋盘格纹理、被面色染色（白面是纯棋盘格），纹理不拉伸错乱。如果纹理全黑/全白，优先查 SRV 绑的槽位和 table 偏移是否一致。
- [ ] **Step 8:** Commit（`Stage0/Task5: checkerboard texture + SRV + static sampler, textured rotating cube`）

---

### Task 6: 路线文档与阶段笔记

**目标：** 把本阶段沉淀成可复习的资产，并把路线图落到仓库里。

- [ ] **Step 1:** 创建 `docs/roadmap.md`：阶段 0、1、2、3、4、5A、5B、6 表格（主题/计划时长/状态/实际耗时/笔记链接），阶段 0 打勾；注明调整原则（每阶段回顾后可重排后续）。
- [ ] **Step 2:** 创建 `docs/notes/01-dx12-minimal.md` 并**认真填写**（这是阶段验收的一部分）：
  - 核心概念（用自己的话）：swap chain/命令队列/命令列表/allocator、PSO 为什么存在、root signature 与 descriptor、upload vs default heap、barrier 为什么存在、fence 与双缓冲同步
  - 踩坑记录：实际遇到的问题和解法
  - 待回头补：多队列、内存分配器、bindless 等留到用到再学
- [ ] **Step 3:** Commit（`Stage0/Task6: roadmap + stage-0 notes`），随后创建 tag `stage-0-dx12-minimal`。

---

## 完成标准（对照 spec 阶段 0）

1. `ARender.sln` 可在 Visual Studio 2026 中以 `Debug | x64` 一键构建和 F5 调试，`--smoke` 退出码 0，肉眼看到旋转纹理立方体。
2. 六样最小集全部亲手实现且只用了这些：swap chain/命令队列、PSO、root signature 与 descriptor、vertex/index buffer 与 upload heap、depth buffer、基本 barrier。
3. shader 在 F5 调试和从输出目录直接启动时都能稳定找到；Debug Layer 无 ERROR。
4. `docs/roadmap.md` 与阶段 0 笔记填写完成，Git tag `stage-0-dx12-minimal` 可还原本阶段成果。
