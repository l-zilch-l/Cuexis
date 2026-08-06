# Cuexis 阶段 2 完成报告

状态：实现与 Windows 本地验收完成；最终跨平台验收待 hosted Linux CI
报告日期：2026-08-06
完成版本：`26.07.18.18-1`（Debug：`26.07.18.18-1-dev`）
SDK preview API：`0.4.0`
FrameDigest algorithm：`2`

## 1. 完成结论

阶段 2 已完成 Chart v3、Beat 域 Tempo/Stop、Behavior Event、Visibility/Material 表现、
Playback capability、内部调试快照和显式 v1/v2 -> v3 迁移闭环。阶段 1C 的
`behavior.transform.keyframe` version 1 读取与采样路径保持不变；迁移不会自动写回源文件。

Windows/MSVC 本地 static/shared Debug/Release、headless、external consumer、format、
architecture、工具、零分配与 GPU smoke 门禁均已通过；Windows MinGW headless Release 也已
通过。当前工作树未提交或推送，`gh run list --commit` 没有返回 run，因此 hosted Linux CI 不能
声明通过，阶段 2 的最终跨平台验收仍待关闭。

## 2. Chart v3 与 TimingMap

- `schemas/cuexis.chart.v3.schema.json`、typed Reader、版本白名单和最小 v3 fixture 已交付。
- Tempo Event 直接对 BPM 执行 cubic Hermite 插值，BPM 范围为 `[1,65536]`。
- 每个 Tempo Event 最多预编译 16 个几何子段，每段使用固定 16 点 Gauss-Legendre 积分。
- `chartTimeMsToBeat` 和内部几何边界定位均使用固定 64 次二分，不依赖容差提前退出。
- 每张 Chart 最多包含 4096 个 Tempo Event 和 4096 个 Stop。
- Stop、负 Beat、零持续 Tempo Event、同 Beat 顺序和半开边界均有正向/逆向测试。

## 3. Behavior 与可观察输出

- 连续 Event 支持 Position、Rotation、Scale、Camera FOV、Material Opacity 和 Tint。
- Step Event 支持 `render.visible` 与 `render.material`，并使用 Beat 边界后的保持语义。
- `groupId`、同属性冲突、零持续 typed equality、Quaternion shortest-path slerp 和事件预算在
  Reader/Compiler 前置阶段验证。
- Runtime 每帧只计算一次 BeatSample，Stop、Seek、Reload 和 discontinuity 均执行绝对重采样。
- `ObjectSnapshot` 输出 `visible`、`materialAssetId`、`materialOpacity` 和 `materialTint`，并继续
  拥有自身数据。
- FrameDigest v2 覆盖新增字段。内部测试 golden 为 `7850652359432829177`，external consumer
  golden 为 `605411979409056204`。

## 4. Capability、调试与迁移

Playback capability set version 1 固定包含：

```text
cuexis.chart.v3
cuexis.behavior.event.v1
cuexis.render.visibility.v1
cuexis.material.snapshot.v1
```

preflight 在资源获取和 World 发布前拒绝缺失 capability。内部调试快照按固定容量记录初始基准、
命中事件、进度、Behavior 输出和最终值；关闭时不进入普通 FrameSnapshot。

`cuexis_chart_migrator` 显式接收输入、v3 输出和报告路径。迁移覆盖 v1/v2 scalar、Vec3、
Quaternion、FOV、共享 Behavior、模板展开、未绑定 Behavior、单 Key 与 `in_out_cubic` 精确中点
拆分。失败不会修改已有目标，输入与输出路径冲突稳定失败。`cuexis_chart_validator` 同时执行 typed
load 和 compile 校验。

## 5. Windows/MSVC 验收结果

所有配置使用 MSVC 19.51、Ninja 和已安装的 `x64-windows` 依赖树。Release 配置启用
`CUEXIS_WARNINGS_AS_ERRORS=ON`。

| 配置 | Linkage | CTest | 结果 |
| --- | --- | ---: | --- |
| full Debug | STATIC | 249 | 通过 |
| full Release `/WX` | STATIC | 249 | 通过 |
| headless Debug | STATIC | 216 | 通过 |
| headless Release `/WX` | STATIC | 216 | 通过 |
| full Debug | SHARED | 251 | 通过 |
| full Release `/WX` | SHARED | 251 | 通过 |

每套 CTest 均有 1 个 Windows 环境不支持的 symlink containment 用例按既有条件跳过，没有失败。
shared 配置额外通过 export surface、consumer imports、版本化 DLL、Debug/Release 配置不匹配和
MSVC runtime 不匹配拒绝门禁。五种 external consumer 模式覆盖 add_subdirectory、基础/核心
find_package 与 AudioSDL，并验证安装公共头为纯 ASCII。独立 find_package consumer 使用显式
`VCPKG_INSTALLED_DIR` 和 triplet prefix，并强制 `VCPKG_MANIFEST_MODE=OFF`；Cuexis package 构建
本身仍保留 manifest 校验。

Windows MinGW headless Release 使用 `-Werror` 构建，并在排除该配置未提供的 external package
consumer 后通过 `211/211`；1 个 symlink containment 用例按 Windows 条件跳过。

## 6. 性能与工具证据

- MSVC Debug：`FrameSnapshot` 240 bytes，`ObjectSnapshot` 176 bytes。
- MSVC Release：`FrameSnapshot` 232 bytes，`ObjectSnapshot` 160 bytes。
- Debug/Release warmup 后连续 64 次 `PlaybackSession::update()` 与可复用 extract：0 次动态分配。
- `cuexis_format_check`：通过。
- `git diff --check`：通过。
- `cuexis_chart_validator --input assets/charts/stage2_example.cuexis.chart.json`：通过。
- v1 migration fixture 的 CLI 输出与 committed v3/report golden：一致。
- 迁移后的 v3 与 v1 scalar/Vec3/Quaternion/FOV Runtime 采样：通过冻结误差预算。
- 两份 Stage 2 golden 的行尾扫描分别为 `11 LF/0 CRLF` 与 `270 LF/0 CRLF`。
- Debug/Release GPU smoke：NVIDIA GeForce RTX 4060 Laptop GPU、OpenGL 3.3，普通场景均完成 3 帧。
- Debug/Release 全不可见 GPU smoke：`Objects: 4, debug commands: 0`，均完成 3 帧。

## 7. 待补最终验收

- hosted Linux CI：本任务未提交或推送分支，未触发 GitHub Actions，不能用 Windows 结果替代。

hosted Linux GCC/Clang、sanitizer 和 package consumer jobs 补齐前，本文证明 Stage 2 实现与
Windows 本地矩阵完成，不证明最终受支持平台矩阵全部通过。

## 8. 明确延期

ParentBinding、局部 Beat、`startBeat`、`repeat`/`pingPong`、多 Clip、priority、weight 和
Override/Additive 混合不进入 Stage 2。Chart v3 Schema 和 typed Reader 不预留这些字段；阶段 3
也不得在没有新 ADR、Schema 版本和迁移合同的情况下改变既有 v3 语义。
