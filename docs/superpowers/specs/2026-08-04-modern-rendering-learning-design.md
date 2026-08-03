# 现代渲染学习计划设计

- 日期：2026-08-04
- 学习者背景：有 DX9 时代渲染经验，理解经典渲染管线各环节；DX12/Vulkan 零基础；目标之后未跟进现代渲染
- 目标：看懂并理解 UE5/UE6 的大部分渲染管线和技术，且能动手复刻核心技术
- 时间预算：每周 5~10 小时，整体预计 8~10 个月

## 总原则

1. **理论 + 实践为主，API 为工具**：图形 API 不深入研究，DX12 只学最小可用集，用到再补。
2. **UE5 源码研究靠后**：前期不打 UE5 源码和抓帧对照——UE 整体架构的复杂性会带来大量非核心技术的干扰（RHI 抽象、RDG、引擎对象模型等）。等整体渲染框架学完（阶段 1~5 完成）再集中做 UE5 研究（阶段 6）。
3. **路线不是合同**：每完成一个阶段回顾一次，按实际耗时和卡点调整后续计划，在 `docs/roadmap.md` 维护"计划 vs 实际"。

## 学习路径：方案 C（专题驱动）

每个学习模块的核心闭环是三步：

1. **理论输入**（约 30% 时间）：读 1~2 份核心资料（书章节/教程/论文，不贪多）。每阶段开始时给出精选清单。
2. **复刻实践**（约 50% 时间）：在自家 DX12 框架里实现简化版。验收标准：能跑、能说清每个 pass 在做什么；不追求性能和边角 case。
3. **阶段笔记**（约 20% 时间）：产出一篇 markdown 笔记，记录核心概念、踩坑、疑问。是将来复习和写总结文章的原材料。

UE5 对照（RenderDoc 抓帧、源码阅读）**不是**每模块的固定环节，只在阶段 6 集中进行；学习过程中如个别时候想"看一眼工业界怎么做"，自行决定，不做要求。

## 阶段划分

### 阶段 0：DX12 最小可用（≤2 周，硬上限）

目标：搭出能画场景的 DX12 空壳框架，之后所有复刻在它上面生长。

只学六样：
- swap chain / 命令队列
- PSO
- root signature 与 descriptor（够用版）
- vertex/index buffer 与上传堆
- depth buffer
- 最基本的 resource barrier

明确不学：多队列并发、精细内存分配器、bindless——用到再补。

产出：`framework/` 空壳 + 一个旋转纹理立方体。超时即停，带着半成品的理解进入阶段 1，在实践中补。

### 阶段 1：现代渲染框架核心（约 4 周）

DX9 到今天的范式转移：前向 vs 延迟渲染、GBuffer、clustered/forward+ 光照、GPU-driven 渲染（indirect draw、compute 剔除）、render graph 思想。

复刻：把空壳扩成延迟渲染器 + clustered 光照。

### 阶段 2：PBR 与材质（约 3 周）

BRDF 理论（Cook-Torrance、GGX、Fresnel）、IBL（irradiance / prefiltered env map）、能量守恒。

复刻：PBR 材质 + IBL。

### 阶段 3：阴影（约 3 周）

Shadow map 家族：CSM、PCF/PCSS、VSM/EVSM，virtual shadow maps 的思想动机。

复刻：CSM + PCSS。

### 阶段 4：AA 与上采样（约 3 周）

TAA 原理（history、reprojection、ghosting 处理）、TAAU、TSR/DLSS/FSR 的共同骨架。

复刻：TAA。

### 阶段 5：GI 与几何管线（约 8 周，压轴）

- Lumen 线：GI 问题本质、辐射度缓存、SDF 追踪、screen probe → Lumen 架构
- Nanite 线：cluster/meshlet、LOD 思想、GPU culling、软件光栅 vs 硬件光栅 → Nanite 架构

复刻：简化版 probe GI + meshlet 剔除（mesh shader 或 compute 模拟）。本阶段不求复刻质量，求架构理解。

### 阶段 6：综合 —— UE5 研究（约 4 周）

此时整体渲染框架已学完，开始集中研究 UE5：

1. RenderDoc 完整拆解 UE5 一帧，逐 pass 标注
2. 选读 UE5 渲染源码模块
3. 输出《我理解的 UE5 渲染管线》总结文章，作为毕业验收

## 仓库结构

```
ARender/
├── docs/
│   ├── roadmap.md              # 总体路线（随实践调整，维护计划 vs 实际）
│   └── notes/                  # 每阶段笔记：01-dx12-minimal.md ...
├── framework/                  # 阶段 0 搭的 DX12 空壳，之后持续生长
├── projects/                   # 各阶段复刻项目
│   ├── 01-deferred-clustered/
│   ├── 02-pbr-ibl/
│   ├── 03-shadows/
│   ├── 04-taa/
│   └── 05-gi-meshlet/
└── capture/                    # 阶段 6 使用：RenderDoc 抓帧 + 分析笔记
```

原则：`framework/` 是长期资产、渐进生长；`projects/` 各自独立可运行，靠拷贝 framework 快照隔离，避免后期改动弄坏早期成果。

## 工具与资料基线

- DX12 入门：Microsoft 官方 DX12 教程 +《Introduction to 3D Game Programming with DirectX 12》（Frank Luna，只读需要的章节）
- 理论工具书：*Real-Time Rendering 4th*（按需查）、*Physically Based Rendering*（选读章节）
- 阶段 5 主力：GPU Pro / SIGGRAPH 课程笔记
- 阶段 6 工具：UE5 源码版（Epic GitHub 仓库，需绑定 GitHub 账号）+ RenderDoc

## 毕业验收

1. `projects/` 里 5 个可运行的复刻项目
2. `docs/notes/` 完整阶段笔记 + 《我理解的 UE5 渲染管线》总结文章
3. 打开 UE5 任意一帧的 RenderDoc 抓帧，能说清 80% 以上 pass 的目的

UE6 发布时，学习者已具备自学新特性的框架，无需新计划。
