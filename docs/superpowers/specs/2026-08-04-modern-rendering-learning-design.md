# 现代渲染学习计划设计

- 日期：2026-08-04
- 修订：2026-09-02
- 学习者背景：有 DX9 时代渲染经验，理解经典渲染管线各环节；DX12/Vulkan 零基础；之后未持续跟进现代渲染
- 目标：理解 UE5/UE6 大部分渲染管线与核心技术，并能亲手复刻其简化版本
- 时间预算：每周 5~10 小时，整体预计 8~10 个月；每阶段结束后按实际情况调整
- 开发环境：Windows、Visual Studio 2026、Windows SDK、DirectX 12

## 总原则

1. **理论 + 实践为主，API 为工具**：DX12 只学习支撑当前专题所需的部分，不把图形 API 本身扩展成主线。
2. **单一工程持续演进**：使用一个 Visual Studio solution 和一个主渲染器项目逐步加入能力，不为每个阶段复制框架。
3. **Git 保存阶段快照**：每个任务完成后 commit，每个阶段完成后打 tag。需要回看或运行旧成果时切换 tag，而不是维护重复代码。
4. **一个阶段只设一个主问题和主交付物**：可选扩展不进入最低完成标准，避免同时铺开多个大型专题。
5. **先遇到问题，再引入抽象**：先手工组织少量 pass；资源依赖和状态管理真正产生重复后，再引入教学性质的简化 Render Graph。
6. **UE 源码研究靠后**：阶段 0~5 不系统阅读 UE 源码，避免 RHI、RDG 和对象模型干扰核心概念；阶段 6 再集中对照。
7. **路线不是合同**：在 `docs/roadmap.md` 记录计划时间、实际时间、完成状态和调整原因。

## 工程与版本策略

主线使用 Visual Studio 2026 原生 C++ 工程，不引入 CMake。原因是当前项目固定为 Windows + DX12 + Visual Studio、没有外部依赖，构建系统不应成为额外学习主题。

工程遵循以下规则：

- `ARender.sln` 中默认只有一个持续演进的主渲染器项目。
- 源码、shader 与资源的构建设置由 `.vcxproj` 管理并提交 Git；`.vs/`、中间文件和编译输出不提交。
- 新增 `.cpp/.h` 或 shader 时直接通过 Visual Studio 加入项目。
- 只有当两个实验必须同时运行或长期对照时，才在同一 solution 中增加独立 sample project；sample 共享已有基础代码，不复制整套框架。
- 每个阶段完成后创建 Git tag，例如 `stage-0-dx12-minimal`、`stage-1-deferred`。
- 若未来出现跨平台、CI、多生成器或复杂依赖管理需求，再单独评估迁移到 CMake，不提前建设。

## 学习闭环

每个阶段都完成以下闭环：

1. **理论输入（约 30%）**：精读 1~2 份核心资料，只覆盖当前主问题。
2. **复刻实践（约 50%）**：在主渲染器中实现简化版本；要求能运行、能解释每个 pass 的输入输出，不追求产品级性能和边角 case。
3. **调试验证**：保留可视化中间结果，使用 D3D12 Debug Layer；从阶段 1 开始逐步使用 PIX 或 RenderDoc 检查自己的渲染器。
4. **阶段笔记（约 20%）**：记录核心概念、数据流、踩坑、未解决问题和下一阶段调整。
5. **版本冻结**：完成验收后 commit 并打阶段 tag。

## 阶段划分

### 阶段 0：DX12 最小可用（目标 2 周）

**主问题：** CPU 如何通过 DX12 驱动 GPU 绘制一个有深度和纹理的物体？

只学习：

- swap chain、命令队列、命令列表与 allocator
- PSO
- root signature 与 descriptor（够用版）
- vertex/index buffer 与 upload heap
- depth buffer
- 最基本的 resource barrier 与 fence

明确不学：多队列并发、自定义内存分配器、bindless、Render Graph。HLSL 暂用 Shader Model 5.1 和 `D3DCompileFromFile`，减少环境变量。

**主交付物：** 主工程能够稳定显示旋转纹理立方体；Debug Layer 无 ERROR；完成阶段笔记。

两周时必须复盘，但 15 小时只作为估算，不作为失败标准。非核心细节卡住超过 2 小时先记录；fence、barrier、资源生命周期等核心概念若仍不清楚，则先补清楚再进入下一阶段。

### 阶段 1：多 Pass 与延迟渲染（约 4 周）

**主问题：** 一个渲染器如何组织多个 pass 及其资源依赖？

先实现 GBuffer、基础 deferred lighting 和少量点光源。开始时手工管理 2~3 个 pass；当资源创建、barrier 和执行顺序出现明显重复后，再实现教学性质的简化 Render Graph。

本阶段不做 clustered lighting、GPU-driven 和通用引擎架构。

**主交付物：** 可切换观察 GBuffer 各通道的延迟渲染场景，并能在 PIX 或 RenderDoc 中解释各 pass 的输入输出。

### 阶段 2：PBR 与材质（约 3 周）

**主问题：** 材质参数如何通过 BRDF 形成物理上连贯的直接光与环境光？

学习 Cook-Torrance、GGX、Fresnel、能量守恒和 IBL；在已有 deferred renderer 上加入 PBR 材质。材质系统只做支撑实验所需的最小结构。

**主交付物：** PBR 材质球场景 + IBL，能说明 roughness、metallic 和 Fresnel 对结果的影响。

### 阶段 3：阴影（约 3 周）

**主问题：** 阴影如何作为独立 pass 接入现有渲染流程，并处理精度与过滤问题？

从单方向光 shadow map 开始，再实现 CSM + PCF。重点理解 light space、深度偏移、级联划分、稳定性和过滤。

PCSS、VSM/EVSM 与 Virtual Shadow Maps 作为原理阅读或可选扩展，不进入最低验收。

**主交付物：** 稳定的 CSM + PCF，可视化各 cascade，并记录 bias 与接缝问题。

### 阶段 4：时域抗锯齿（约 4 周）

**主问题：** 如何利用历史帧、运动矢量与重投影复用时间信息？

实现 jitter、motion vector、history reprojection、history rejection/clamping 和基础 TAA。TAAU、TSR、DLSS、FSR 只分析共同骨架，不要求复刻。

**主交付物：** 可开关对比的 TAA，能构造并解释 ghosting、disocclusion 和闪烁案例。

### 阶段 5A：简化全局光照（约 5 周）

**主问题：** 实时渲染如何近似、缓存并更新间接光？

学习 GI 问题本质、screen-space 信息、probe/radiance cache、时空累积和遮挡泄漏。实现一条范围受控的简化 probe GI；SDF tracing 与 screen probe 作为理解 Lumen 的延伸。

**主交付物：** 可视化 probe 状态和间接光贡献的简化 GI，能说明它与 Lumen 的相似点及关键差距。

### 阶段 5B：GPU-Driven 与几何管线（约 5 周）

**主问题：** 如何把可见性判断与绘制提交从 CPU 转移到 GPU？

学习 compute culling、indirect draw、GPU buffer 数据组织、LOD 与 cluster/meshlet 思想。此阶段前将 shader 工具链升级到 DXC + Shader Model 6。

最低实现为 compute culling + indirect draw；meshlet 和 mesh shader 为进阶项，软件光栅只做原理研究。

**主交付物：** 大量实例的 GPU culling + indirect draw，并能用计数器或可视化验证剔除结果。

### 阶段 6：UE5 综合研究（约 4 周）

此时再集中研究 UE5：

1. 使用 RenderDoc 或 PIX 拆解 UE5 一帧并逐 pass 标注。
2. 按已学专题定向阅读 UE5 渲染源码，重点对照 RHI、RDG、材质、阴影、时域、Lumen 与 Nanite 的工程化取舍。
3. 输出《我理解的 UE5 渲染管线》总结文章。

## 仓库结构

```text
ARender/
├── ARender.sln                 # Visual Studio 2026 solution
├── renderer/                   # 唯一主项目，随阶段持续演进
│   ├── ARender.vcxproj
│   ├── src/
│   ├── shaders/
│   └── assets/
├── docs/
│   ├── roadmap.md              # 计划、实际进度与调整原因
│   └── notes/                  # 各阶段笔记
└── captures/                   # 自研渲染器与阶段 6 的抓帧分析
```

仓库不预建 `projects/` 副本。需要并列实验时，在 solution 中按需增加 `samples/` 项目；历史成果默认由 Git tag 保存。

## 工具与资料基线

- 开发与调试：Visual Studio 2026、Windows SDK、D3D12 Debug Layer
- DX12 入门：Microsoft 官方 D3D12 Hello World 示例 +《Introduction to 3D Game Programming with DirectX 12》（Frank Luna，按需阅读）
- 理论工具书：*Real-Time Rendering 4th*、*Physically Based Rendering*（均按需查阅）
- GPU 调试：阶段 1 起使用 PIX 或 RenderDoc 检查自研渲染器；UE5 系统抓帧留到阶段 6
- 阶段 5 资料：GPU Pro、SIGGRAPH 课程笔记与相关论文
- 阶段 6：UE5 源码版（Epic GitHub 仓库，需绑定 GitHub 账号）

## 毕业验收

1. 主渲染器在最终版本可运行，并能通过 Git tag 还原各阶段交付状态。
2. `docs/notes/` 包含完整阶段笔记和《我理解的 UE5 渲染管线》。
3. 打开 UE5 任意一帧的 RenderDoc/PIX 抓帧，能说清 80% 以上主要 pass 的目的、关键资源和前后依赖。
4. 能明确解释自研简化实现与 UE5 工业实现之间的差距，而不以复刻画质或性能作为毕业标准。

UE6 发布或 UE 渲染架构变化时，以已有概念框架定向补充，不重做整条路线。
