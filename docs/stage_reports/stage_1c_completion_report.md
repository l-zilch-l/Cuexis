# Cuexis 阶段 1C 完成报告

状态：代码实现与本地验收门禁完成；Windows/MSVC 托管 CI 已配置，等待首次远程运行  
报告日期：2026-07-22  
完成版本：`26.07.18.18-1`  
阶段目标：完成 typed Behavior Track、绝对时间采样、RuntimeFrame、拥有型 FrameSnapshot、
headless Playback 与 Player 单一路径闭环。

## 1. 完成结论

阶段 1C 的代码范围已经完成。Chart Behavior 不再是 opaque 数据；
`behavior.transform.keyframe` version 1 可以驱动位置、旋转、缩放和相机 `fovY`。
Chart Compiler 在 prepare 前完成 Beat 到 `chartTimeMs` 的有界转换，BehaviorSystem 按绝对时间
采样，Transform 与 FOV Resolver 从初始基线构建候选值并整体提交，从而保证 Seek、不同帧率和
不同到达路径得到相同结果。

`PlaybackSession` 已成为宿主和 Player 的播放门面。Player 不再持有私有 RuntimeSession/World
路径，每帧固定执行 `PlaybackSession::update()`、`extractFrame()`、RenderScene adapter 和
OpenGL backend。FrameSnapshot 拥有对象与相机数据，后续 update、reload 或 unload 不会使旧
Snapshot 悬空。

本地 Debug/Release 均完成 fresh configure、104 步 clean build 和 `167/167` 项 CTest；
架构扫描、全仓 clang-format 门禁和 6 组 GPU smoke 全部通过。新增 GitHub Actions 工作流覆盖
非 GPU 的 Windows/MSVC 门禁，但本次实现会话没有远程运行记录，因此不将托管 CI 声明为已通过。

## 2. ADR 0028/0029 落地结果

阶段实现遵循 [ADR 0028](../adr/0028-camera-projection-and-events.md) 的相机归属和 Snapshot
契约，并通过 [ADR 0029](../adr/0029-behavior-keyframe-and-runtime-frame.md) 冻结 1C 的
Behavior、时间和 Playback 语义。

| 领域 | 已冻结并实现的契约 |
| --- | --- |
| 相机归属 | `CameraComponent` 属于 `cuexis_render`；Transform 继续属于 `cuexis_world` |
| 相机数据 | `camera.fovY` 范围为 `(0, 179)` 度，视口宽高由宿主在提取 Snapshot 时提交 |
| v1 Property | `transform.position.x/y/z`、`transform.rotation`、`transform.scale`、`camera.fovY` |
| Easing | `linear`、`in_cubic`、`out_cubic`、`in_out_cubic` |
| 插值 | Scalar/Vec3 分量线性插值；Quaternion shortest-path slerp 后重新归一化 |
| 边界行为 | 单 Key 为常量；首 Key 前和末 Key后钳制；目标 Key 的 easing 控制入段 |
| RuntimeFrame | 绝对 `chartTimeMs`、非负 delta、只比较变化的 discontinuity ID |
| 更新事务 | Behavior evaluate -> PropertyWriteBuffer -> Transform/FOV 整体校验与提交 -> world transform |
| Snapshot | 拥有型对象、相机、view/projection 和 viewport 数据，不暴露后端或 ECS 类型 |

## 3. 实际交付范围

| 模块 | 完成内容 |
| --- | --- |
| `cuexis_chart` | typed Track/Key/Property/Easing；canonical 与 Simple Behavior 子树严格字段校验 |
| Chart Compiler | Track/Key 稳定排序、重复 Property/Beat 检测、Beat 到毫秒 IR、总量预算 |
| `cuexis_world` | `PropertyWriteBuffer`、Transform baseline 与事务式 Transform resolver |
| `cuexis_behavior` | STATIC 实现、Scalar/Vec3/Quaternion 绝对时间 Sampler、二分 Key 定位 |
| `cuexis_runtime` | BehaviorProgram/Binding prepare、FOV baseline/resolver、`RuntimeFrame` 更新与事务 reload |
| `cuexis_playback` | load/update/extract/reload/unload 门面、生命周期校验、拥有型 FrameSnapshot |
| Player | GPU 初始化前 preflight；只经 PlaybackSession 驱动和提取；保留 canonical/simple 回归入口 |
| Demo | 独立 `stage1c_project`，3 objects、3 behaviors，覆盖 position/rotation/scale/FOV |
| 文档 | Chart、Simple Chart、RuntimeSession、Project Guide、1C 计划、SDK transition plan、ADR 0029 |
| CI | `.github/workflows/windows-msvc.yml`，Debug/Release 非 GPU 矩阵 |

### 3.1 Chart 与 Behavior

Track 输入顺序没有运行语义。Compiler 先按 RationalBeat 排序，再一次性调用 TimingMap 生成
`chartTimeMs` IR；逐帧求值不解析 JSON、Beat 或 Property 字符串。空 Track、未知 Property/Easing、
错误 value shape、非有限值、非归一 Quaternion、重复 Beat、重复 Property 和越界 FOV 均稳定失败。

Simple v1 继续只提供既有标量 position 简写，但其 Behavior/Track/Key 子树和 canonical 一样对
未知字段报错；Simple 其他未知字段仍保留原有 warning/保留语义。

### 3.2 Runtime 与事务

Resolver 每帧从实例化时捕获的 Transform 和 Camera FOV 基线重新构建候选状态，不使用上一帧
结果作为下一帧基线。稀疏写入仅替换目标字段；Transform 与 FOV 全部候选校验完成后才统一提交，
任一写入冲突或非法结果都不会发布半帧状态。

相同 discontinuity ID 下的时间回退作为未声明 Seek 拒绝。ID 改变时要求首帧 delta 为 0，所有
Behavior 直接从目标绝对时间采样。Reload 接收显式目标 RuntimeFrame 和策略；替换候选先以
`delta=0` 采样目标帧，成功后才原子替换活动 World、Program、baseline 与映射。

### 3.3 Playback 与 Player

Playback 公共头只暴露 Cuexis-owned 类型和 `core::Result`/Diagnostics，不包含 RuntimeSession、
World、EnTT、SDL、OpenGL、GLAD 或 JSON DOM。Headless 测试覆盖 load/update/seek/reload/extract/
unload、多 Session 独立性、非法生命周期、相机视口投影和旧 Snapshot 生命周期。

Player 在窗口/OpenGL 创建前完成 Project、Asset Index、Chart 与 Behavior preflight。默认 1C
项目不含 Renderable 资产；它以 3 个对象生成 9 条 DebugLine，分别展示 position + rotation、
scale 和 camera FOV 动画。阶段 1A canonical/simple fixture 继续走同一 Playback 路径。

## 4. 默认安全预算

```text
Behavior definitions            10000
v1 tracks / Behavior            6
Property path                   128 bytes
Keys / Track                    65536
Total Behavior keys / Chart     262144
Behavior-bound objects          100000
Property writes / frame         600000
Diagnostics                     1024
```

这些值是代码级输入耗尽防护，不属于 ProjectConfig、UserPreferences 或 DeviceProfile。
逐帧路径不执行资源 I/O，也不保存无界诊断历史。

## 5. 测试与验证

构建环境为 Visual Studio 2026 Developer Command Prompt、MSVC `19.51.36248.0`、Ninja、CMake
Preset 和 vcpkg manifest mode。`VCPKG_ROOT` 指向 `D:\vcpkg`。最终本地门禁使用 Visual Studio
自带的 Ninja 和 clang-format 显式路径，避免 WindowsApps/WinGet shim 的沙箱权限歧义。

```powershell
cmake --preset debug --fresh
cmake --build --preset debug --clean-first
ctest --preset debug --no-tests=error
cmake --build --preset debug --target cuexis_format_check

cmake --preset release --fresh
cmake --build --preset release --clean-first
ctest --preset release --no-tests=error
```

| 验证项 | 结果 |
| --- | --- |
| Debug fresh configure | 通过 |
| Debug clean build | 通过，`104/104` 步成功 |
| Debug CTest | `167/167` 通过，0 失败 |
| Release fresh configure | 通过 |
| Release clean build | 通过，`104/104` 步成功 |
| Release CTest | `167/167` 通过，0 失败 |
| Architecture scan | Debug/Release 均通过；同时包含在完整 CTest 中 |
| clang-format dry-run | 通过，0 条格式诊断 |
| GPU smoke | Debug/Release 各 3 组，`6/6` 通过 |

CTest 由 13 个 Catch2 测试可执行文件、159 个自动发现的 Catch2 test case、1 个 CMake 架构
扫描和 7 个 Player CLI/失败路径测试组成，共 167 项。

### 5.1 1C 专项测试

| 测试目标 | Test case | Assertions | 结果 |
| --- | ---: | ---: | --- |
| `cuexis_chart_tests` | 42 | 281 | 全部通过 |
| `cuexis_behavior_tests` | 4 | 23 | 全部通过 |
| `cuexis_world_tests` | 7 | 33 | 全部通过 |
| `cuexis_runtime_tests` | 20 | 145 | 全部通过 |
| `cuexis_playback_tests` | 4 | 103 | 全部通过 |

专项覆盖 typed Reader/Compiler、四种 easing、Scalar/Vec3/Quaternion Sampler、baseline 重建、
失败不部分提交、RuntimeFrame/Seek、reload 回滚、确定性、拥有型 Snapshot、多 Session 和完整 1C
fixture。`Stage 1C project samples all demo properties deterministically` 对默认项目的目标时间
采样值进行数值断言。

### 5.2 GPU smoke

| 构建 | 输入 | Objects / Behaviors | Debug commands | Frames | 结果 |
| --- | --- | --- | ---: | ---: | --- |
| Debug | 默认 `stage1c_project` | 3 / 3 | 9 | 3 | 通过 |
| Debug | 阶段 1A canonical | 3 / 0 | 9 | 3 | 通过 |
| Debug | 阶段 1A simple | 3 / 0 | 9 | 3 | 通过 |
| Release | 默认 `stage1c_project` | 3 / 3 | 9 | 3 | 通过 |
| Release | 阶段 1A canonical | 3 / 0 | 9 | 3 | 通过 |
| Release | 阶段 1A simple | 3 / 0 | 9 | 3 | 通过 |

实际 GPU 环境：Windows SDL video driver、NVIDIA GeForce RTX 4060 Laptop GPU，OpenGL
`3.3.0 NVIDIA 596.36`。Debug 版本为 `26.07.18.18-1-dev`，Release 版本为
`26.07.18.18-1`。

GPU smoke 是窗口/后端/帧循环的功能门禁，检查对象数、Debug command 数和完成帧数；Behavior
确定性、Seek 一致性和相机矩阵由 headless 数值测试负责，不使用 GPU 像素 golden baseline。

## 6. 托管 CI 状态

新增 [Windows/MSVC 工作流](../../.github/workflows/windows-msvc.yml)，触发条件为 push、
pull request 和手动运行。工作流使用 Debug/Release 矩阵，显式建立 x64 MSVC 环境并定位托管
vcpkg、Ninja 和 clang-format，然后执行：

```text
fresh configure
clean build
complete CTest
explicit architecture test
clang-format check（Debug job）
```

GPU smoke 不进入托管 CI。工作流文件已经落地，但本次会话未推送仓库，也没有 GitHub Actions
运行记录。因此当前准确状态是“本地门禁全部通过、远程 CI 待首次运行”，不是“远程 CI 已通过”。

## 7. 验收标准对照

| 阶段 1C 验收标准 | 证据 | 结论 |
| --- | --- | --- |
| chartTimeMs 驱动位置、旋转、缩放和 FOV | Behavior/Runtime/Playback 数值测试与默认 demo | 通过 |
| 结果不依赖历史、帧率、输入顺序或 ECS 遍历 | absolute sampler、baseline、排序和多路径测试 | 通过 |
| Seek/discontinuity 等价于直接目标采样 | RuntimeFrame 与 Playback seek/reload 测试 | 通过 |
| 非法输入无资源副作用或部分 World 更新 | Reader/prepare/resolver/reload 失败测试 | 通过 |
| 依赖方向不反转 | architecture scan 和 target allowlist | 通过 |
| 宿主只经 Playback 获取拥有型帧 | 公共边界扫描和 Playback 生命周期测试 | 通过 |
| Player 不保留私有 Runtime 路径 | Player 源码依赖与 6 组 smoke | 通过 |
| Debug/Release/CTest/format/architecture/GPU | 本报告第 5 节 | 通过 |
| Windows/MSVC 托管 CI | 工作流已配置，尚无远程运行记录 | 待首次运行 |

## 8. 残余风险与后续边界

当前没有已知 P0/P1 实现问题。以下事项不改变 1C 已完成的运行时契约：

| 优先级 | 事项 | 后续处理 |
| --- | --- | --- |
| P2 | Windows/MSVC workflow 尚未产生远程运行证据 | 首次 push/PR 后检查两项矩阵 job，并将结果补入后续报告 |
| P2 | GPU smoke 没有像素或 GPU frame hash golden，只验证后端和帧循环 | 正式 Renderable/材质路径出现后建立可移植图像或命令流基线 |
| P2 | 当前 headless 单元路径不依赖 SDL/OpenGL，但仓库尚无面向安装包的 adapter-disabled configure preset | 阶段 1E 与 component install/export、外部 consumer gate 一并建立 |
| P3 | 多相机仍采用第一个相机对象；正交投影与切换策略未冻结 | 需要真实多相机消费者时单独形成 ADR，不静默改变 v1 |

明确留给后续阶段的范围：

```text
阶段 1D：HostClock/CuexisAudio 双模式、AudioTransport 和 SDL 音频 adapter
阶段 1E：ContentProvider 注入、install/export/find_package、仓库外 consumer 门禁
阶段 2：通用 Curve、更多 Property、循环和 BehaviorClip 组合
阶段 11：Judgement、InputEvent、ReplayData 和确定性判定契约
```

阶段 1D 只能替换 RuntimeFrame 的时间来源，不得绕过 PlaybackSession，也不得修改本阶段已冻结
的 Behavior、Seek、Resolver 和 FrameSnapshot 语义。

## 9. 相关文档

- [阶段 1C 实施计划](../stage_plans/stage_1c_implementation_plan.md)
- [SDK 转型方案](../stage_plans/cuexis_sdk_transition_plan.md)
- [ADR 0027：Playback SDK 产品边界](../adr/0027-playback-sdk-product-boundary.md)
- [ADR 0028：相机投影与事件](../adr/0028-camera-projection-and-events.md)
- [ADR 0029：Behavior Keyframe 与 RuntimeFrame](../adr/0029-behavior-keyframe-and-runtime-frame.md)
- [Chart 格式](../CHART_FORMAT.md)
- [Simple Chart 格式](../SIMPLE_CHART_FORMAT.md)
- [RuntimeSession 规范](../RUNTIME_SESSION.md)
- [项目技术指南](../PROJECT_GUIDE.md)
