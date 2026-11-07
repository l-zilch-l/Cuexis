# R3 中立路径 CPU 成本与派生摘要

状态：保留局部实现；普通物理 GPU 有限对照通过，完整最终矩阵、容量阶梯与本阶段接受仍开放。

日期：2026-11-01。返回 [New RPA 索引](../README.md)。
当前接受状态只见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 问题与取舍

前批[中立绘制命令转移](2026-10-31-r3-neutral-command-transfer.md)把 neutral builder 的近有序检测和
命令转移成本降下来后，提交前的中立路径仍在逐帧做两件与画面无关的事：计算整份绘制摘要的 64 位 digest，
以及在 OpenGL 适配层把每条 `DrawCommand` 复制进逐帧临时数组。本组的基线臂（**不含**本轮改动）在
1800 实体 render-only 上实测完整 cycle 中位数 2 721.9 us，即 4.166667 ms 预算的 65%，因此这两项都在
计时路径上。

40K 压力测试启动阶段只差 149 ms，owner 已裁定不为达标做大规模重构（typed Reader / SAX 复合改造不做）。
因此本轮只取最便宜、最低风险的两处：让摘要 digest 改为按需派生、不再逐帧存储；让适配层引用已构造的
命令而不是复制。两者都不改算法、不改输出、不改 SDK 边界。

## 改动与语义

共 7 个文件。核心只有两处语义删除：

- `DrawSummary` 不再持有 `digest` 字段。`summaryDigest(const DrawSummary&)` 成为外部链接的派生函数，
  算法、域分隔串 `cuexis.validation.summary.v1` 与逐字段顺序全部保持原实现；`buildPresentationCommands`
  不再在返回前为 summary 盖戳。逐帧路径因此不再付出这条串行 FNV 链（约 4 周期/字节）。
- OpenGL 适配层的 `PreparedDraw` 改为持有 `const DrawCommand*`，指向 `preparedSummary` 中已就位的命令；
  `presentFrame` 不再构造本地 `std::vector<PreparedDraw>` 副本。所有权不变：`preparedSummary` 比所有
  绘制调用和随后的 summary 移动都活得久，`*summary = std::move(preparedSummary)` 转移的是向量缓冲，
  元素地址在移动后仍然有效。帧末显式清空两个暂存向量，使"非拥有指针只在帧内有效"这一不变量对提前返回
  和探针路径也成立。

`app/player/src/player_smoke.cpp` 与两个测试改为调用派生函数；`tests/render/open_gl_presentation_tests.cpp`
新增源码级反例：适配层源文件不得出现 `summaryDigest(`，`presentFrame` 不得持有本地
`std::vector<PreparedDraw>`，且必须经由 `resolveGpuDraw(` 取命令。这些断言让"下次有人把副本或逐帧
digest 加回来"直接失败，而不是靠注释约束。

摘要值本身必须不变：digest 是 Validation Sink 的同一份 portable summary 身份，改值就是破坏契约。
smoke 的 golden 因此没有重录。

## 功能与反例

候选构建的 smoke 全部通过，且逐帧 digest 与既有 golden 逐字相同（`player_smoke.cpp` 中的
`presentationSmokeDigest` 常量未被修改）：

- `Frame 0 summary=4536622714229612252`、`Frame 4 summary=4536622714229612252`
- `Frame 1 summary=18316288860163381829`、`Frame 5 summary=18316288860163381829`（等于 golden）
- `Restore kept digest 4536622714229612252 at drawable 1280x720`
- `Debug summary parity held for omitted, empty, and populated scenes`、`Completed frames: 6`

摘要只在 smoke 的每帧日志、restore 处和测试中被派生，正常播放不再计算它。`engine/` 内
`summaryDigest` 只有三处文本出现（声明、定义、`buildPresentationCommands` 的说明注释），引擎内部没有
任何逐帧调用点；其余调用全在 smoke 与两个测试里，`tests/presentation/validation_sink.cpp` 中的同名函数
是 Validation Sink 自己的另一份重载，与本函数无关。

## 普通物理 GPU 固定组

`neutral-lever-measure-v1` 预登记两个门格：1800 与 2600 实体、render-only、每格每臂 3 个全新进程、
每进程 600 计时帧 / 20 秒、1080p。臂序按对交替（baseline-first、candidate-first、baseline-first），
另加 40000 实体每臂一个诊断进程，报告在外、永不并入门格。

仪器为共享 `gpu_runner.py` 模板（`424e1b61…`）加三处文本补丁，`runner_sha256` 与模板同值：
两臂都开启逐帧像素回读（`PIXEL_READBACK=1`），GPU timer 关闭，`CUEXIS_GPU_BARRIER=1`，
预热至少 8 帧与 2000 ms，初末像素证明在计时外；mask 65535、BelowNormal、执行节流 OFF 逐进程核验。
回读成本落在 `present_us` 内，而本组的判据是 CPU 侧 `submit_us`，因此两臂同开回读既保守又同时证明了
逐帧像素输出仍在。

两臂二进制在同一构建目录按同一编译器/标志重建：基线为 `1474316` 的干净工作树（
`04092C8F…`，5 730 816 字节），候选为本轮 7 文件改动的同一树（`9AAA34F7…`，5 726 720 字节），
DLL 逐字节相同。绑定、计划与仪器哈希见附件。

## 固定组结果

14 个进程全部 `valid`、退出 0、按预登记顺序启动，无替换、无补样。每格每臂取 3 个进程内中位数的中位数：

| 门格 | submit baseline → candidate | 变化 | 完整 cycle 中位数 | 变化 | cycle p99 中位数 | 变化 |
| --- | --- | --- | --- | --- | --- | --- |
| 1800 render-only | 2 239.4 → 1 478.5 us | -33.98% | 2 721.9 → 1 930.7 us | -29.07% | 3 496.3 → 2 624.7 us | -24.93% |
| 2600 render-only | 3 213.2 → 2 051.2 us | -36.16% | 3 864.9 → 2 526.6 us | -34.63% | 4 685.4 → 3 282.0 us | -29.95% |

预登记门全部满足：submit 两格改善都超过 10%；cycle 与 cycle p99 都没有退化（均为下降）；
各格各臂的进程相对范围：submit 最大 8.29%（2600 基线），cycle 最大 6.20%（2600 基线），
cycle p99 最大 2.82%（2600 基线），其余读数更小，
都在 15% 屏幕门内；每进程像素为 255,204,51,255，实际 draw 数与入镜中心数都等于实体数，两臂一致。

同组的墙钟原位读数（非门，仅说明成本去了哪）：1800 367.39 → 517.96 FPS，最短 1 秒窗 353 → 488 帧；
2600 258.74 → 395.79 FPS，最短窗 237 → 379 帧。

门外的独立归因组（插桩探针，不作为本组数据）给出同一方向：1800 的中立路径自 893 us 降到 294 us，
其中 digest 自 513–588 us 降到 0，resolve 自 155–165 us 降到 69–74 us；40000 的 digest 自
11 531–11 559 us 降到 0，`gl.pre` 自 1 793–1 804 次降到 1 次，resolve 自 4 316–4 399 us 降到
1 354–1 385 us。两次是不同仪器、不同构建，只作方向核对，不相减、不相加。

## 容量含义与不拼接边界

本组自身给出一个直接观察：2600 render-only 上，基线 3 个进程全部不满足 240 FPS 预算
（cycle p99 4 583.7–4 715.8 us，均在 4.166667 ms 之上，最短 1 秒窗 237–244 帧），候选 3 个进程全部满足
（p99 3 280.1–3 336.2 us，最短窗 379–381 帧）；1800 上两臂都满足。

这与已冻结的容量前沿证据（`8e275a5`，二进制 `daa0c3cf…`）记录的 render-only 括区 `[2400, 2600)`
方向一致，但那是另一份源码、另一份二进制上的阶梯：本组不修改它、不与之拼接，也不把"候选通过 2600"
当成新的前沿点。结论只有一个——新源码的括区必须在完整矩阵那一批里用同一份源码重新推导，
不能沿用旧阶梯，也不能把本组读数写进任何阶梯表。

## 40000 诊断

同一组内每臂一个 40000 实体 render-only 进程，只用于确认杠杆在压力规模上仍然生效：
submit 44 083.2 → 27 476.0 us（-37.68%），完整 cycle 44 204.1 → 27 595.0 us（-37.57%），
cycle p99 52 501.1 → 31 994.2 us（-39.06%），墙钟 22.62 → 36.24 FPS。它既不是容量格，
也不涉及 R3 的初次 load ≤2000 ms 门（那条要另用启动探针测），因此不参与任何判定。

## 回归与检查

在候选构建树 `out/build/rpa-candidate-release` 上：格式目标 `cuexis_format_check` 退出 0，覆盖 651 个
文件，7 个改动文件全在列表内且无格式差异；`VerifyArchitecture.cmake` 退出 0；`git diff --check` 无输出；
`check_docs.py` 通过。归档的自检把这四项各自重跑一遍并把输出写进 `validation.json`，不引用本节结论。

## 全量回归

同一棵候选 Release 树跑完整 CTest：1418 个测试全部启动、1418 个 `Test Passed.`、0 个 `Test Failed.`、
进程退出 0，`Total Test time (real) = 783.54 sec`（逐项 `Test time` 之和 779.73 s）。第 77 项
`Secure file rejects physical containment escapes through symlinks` 按仓库设置跳过，只出现在控制台末尾的
"did not run" 列表里，不计入失败。

控制台流在作业缓冲里只留到第 335 项之后的段落，这一段按原样归档、不做补写；完整的 1418 个测试块另行
以确定性 gzip 归档（其时间戳是本地码页字节，非 UTF-8，故按不透明字节保存，不做文本归一）。归档自检
从 journal 重新数出 1418/1418、0 失败与逐项时间之和，从控制台末段重新读出 `100%`、783.54 s 与退出码，
再与 `selection.json` 逐项比对。

这不等于新源码的完整矩阵：矩阵还要求 Debug/Release × STATIC/SHARED、安装树与外部消费者、版本门在同一
份源码上一起跑，属于下一步，本节只记录一次候选树上的全量回归。

## 保留、撤回与开放

首次启动本组即因包装器缺陷退出（把 arms 目录当作映射索引，`TypeError`），未启动任何进程；
该目录按撤回归档保留，不删除、不当作测量。此后从零重建，14 个进程各跑一次。

本轮没有重开被 owner 否决的方向：不做 typed Reader / SAX 复合重构，不做跨帧缓存，不改预算，
不放宽任何原始门。仍开放：D2/D3/Snapshot/排队 p99、request→ready 与 GPU render p99、
Pause/Resume/resize、Reset/Close、适用 Seek ≤20 ms、正式读取相对门（配对对数比 Student-t 双侧 95%
置信区间上界 ≤1.10）、正式重复性，以及 40K 的初次 load 门。这些都没有在本轮源码上测过，
不能由本组的局部对照替代。

## 证据与后续

原目录 `out/verification-20261101/r3-neutral-path-v1` 原位冻结。
[附件索引](2026-11-01-neutral-path-lever-evidence/index.md)、
[manifest](2026-11-01-neutral-path-lever-evidence/manifest.json) 与
[选用绑定](2026-11-01-neutral-path-lever-evidence/selection.json) 保存双臂清单、仪器源码、逐进程日志、
预注册门与判定、被撤回的首次启动，以及组外检查的转录（完整 CTest journal、控制台末段、格式目标与其
文件清单）；建库自检在归档内的 `validation.json`。逐进程日志以确定性 gzip 归档，其余记录按 CRLF 原文
保存并三向绑定（归档字节、来源字节、LF 归一内容）。

定稿前有四次被撤回的草稿，全部原名保留在
`out/verification-20261101/evidence-staging/r3-neutral/withdrawn-build-attempt-1..4`，没有删除。前三次
是建库脚本自身的缺陷：仓库装配目录被当成映射索引、把整条 gzip 流（含 10 字节头与 8 字节尾）交给裸
deflate 解压、按 `(entities, mode)` 取键而计数器按 `(entities, variant)` 存键。第四次写出了清单与自检，
但自检 19 项失败：gzip 条目把 CRLF 内容哈希当成 LF 归一内容哈希、混合行尾的 `smoke-B.log` 无法用两向
重建规则回推、`check_docs.py` 在本报告还指向尚未生成的 `validation.json` 时先跑。三次是半成品，第四次
是被自己的检查否掉的完整草稿；四者都不被引用。

下一步按既定顺序：以新鲜、可重复的完整矩阵判断是否达到相应阶段门槛，并在同一份源码上重新推导
240 FPS 前沿。R3/R3c 技术接受、设备/生产与 Stage 7 结论仍由 owner 裁定。
