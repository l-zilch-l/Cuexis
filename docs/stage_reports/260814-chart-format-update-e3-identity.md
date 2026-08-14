# Stage Chart Format Update CFU-E3 Prepared Semantic Identity

状态：completed

快照日期：2026-08-14

后续关闭：[CFU-E4 报告](260814-chart-format-update-e4-gates.md) 已取得本地全量门禁数字。[CFU-E 关闭报告](260814-chart-format-update-e-close.md) 已由项目所有者接受整包 CFU-E。

实施基线：`stage-ChartFormatUpdate` worktree on commit
`1af1606faf301ac5f534ff5ce6bd98403046ebb6`

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

API 冻结：[CFU-E0 API 评审](260814-chart-format-update-e0-api.md)

Prepare 基线：[CFU-E2 报告](260814-chart-format-update-e2-prepare.md)

Combiner 合同：[CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md) §3

## 1. 结论

CFU-E3 已关闭。成功 prepare 现在会在资源获取与 typed 校验之后组装
`PreparedSemanticIdentity`。candidate 与 committed session 都可以观察该值；失败 reload 不改
active identity 或已抽出的帧。同一规范内容加同一冻结参数，在 chart-text、typed host、memory、
filesystem 与 CXC file/memory source 上得到同一 identity。

该关闭不包含 CFU-E4 全量门禁、CFU-D3、动画采样或混合、完整 CXC 产品支持、公共 package API 或
Stage 4。E3 不宣称 CFU-E 完成。

## 2. Combiner 与公共观察面

`cuexis_chart` 实现 combiner，继续使用内部 `CanonicalContentIdentity`，不依赖 Playback 公共类型。
最终 digest 是 [CHART_V4_FORMAT.md](../CHART_V4_FORMAT.md) 冻结的域分隔 SHA-256：

```text
domain            "cuexis.prepared-semantic.v1\0"
chart             32-byte canonical Chart identity
cxtCount          uint32 LE + sorted CXT identities
resourceCount     uint32 LE + sorted fetched resource identities
parameter         32-byte parameter identity
```

资源条目是实际获取并 typed 校验后的内容 identity，不是 C2 的 `resourceRequirements`。Mesh /
Texture2D / UnlitMaterial 复用 `PresentationContentIdentity`；MainMusic 使用
`SHA-256("cuexis.prepared-audio.v1\0" + exact fetched bytes)`。`CxcPackageIdentity`、archive
metadata、source path 和 Provider revision 不进入该编码。

安装 Playback 头按 E0 原样增加：

```text
PreparedSemanticIdentity
PreparedPlayback::semanticIdentity() -> optional
PlaybackSession::semanticIdentity() -> Result
```

empty / moved-from candidate 返回 `nullopt`；未 commit 的 session 复用 `playback.session.empty`。
公共头没有 `hex()`，也不暴露 Chart、CXC、JSON DOM 或 archive 类型。

## 3. Prepare 事务

v4 路径保留完整 `ChartV4ResolvedArtifact`，不再只抽取 `document.chart` 与 `animationProgram`。
v1/v2/v3 成功 prepare 使用 canonical Writer bytes 的 Chart identity、空 CXT 列表、空 parameter
identity，以及同样在 fetch 之后组装的资源 manifest。

identity 只在 presentation prepare 与 reload 采样成功之后发布。`commit()` 把 candidate identity
拷到 active session；`unload()` 清掉。parse、parameter、capability、资源或 identity 失败只写
`lastOperationDiagnostics`，不替换 active content。owner / generation / candidate token 未改。

## 4. Local verification

工具链：Visual Studio 2026 Community 18.7.3 / MSVC 19.51.36248.0，CMake 4.2.1，Windows x64。

```text
Debug configure                                              passed
combiner [chart][identity][cfu-e3]                           7 assertions / 1 case passed
identity [playback][identity]                                152 assertions / 6 cases passed
cuexis_chart_tests                                           714 assertions / 103 cases passed
cuexis_playback_tests                                        1913 assertions / 47 cases passed
cuexis_cxc_tests                                             410 assertions / 19 cases passed
git diff --check                                             passed
```

覆盖点：跨 source 同一 static identity、默认与 override 不同、约分 rational 相同、失败动画 reload
保持 identity 与帧、成功 options reload 改变 identity 与 FrameDigest v3、empty / moved-from /
unload 无 identity、v1/v2/v3 成功 prepare 都有 identity。历史 `frame_digest_tests.cpp` golden 未改。

E3 不拥有 Debug/Release 全量 CTest、七个 consumer 或 shared export/import；那些数字属于 CFU-E4。

## 5. Handoff

CFU-E4 补齐 empty static v4 CXC fixture、consumer/export 观察、参数化 FrameDigest 与计划 5.7 退出
门禁。CFU-D3 继续等待整包 CFU-E 关闭；不得从 identity 已可观察推断 FrameSnapshot / FrameDigest
v3 / seek-stop 运行时等价已经关闭。