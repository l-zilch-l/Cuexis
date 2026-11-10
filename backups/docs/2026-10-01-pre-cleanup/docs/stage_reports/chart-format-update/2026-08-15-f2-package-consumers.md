# Stage Chart Format Update CFU-F2 Package Consumers

状态：completed locally

快照日期：2026-08-15

实施基线：`codex/cfu-f` on commit `819064556e7f6eb0fe32a075cbcf7b13e141d8a4`
plus the uncommitted CFU-F1/CFU-F2 surface closed by this report

权威计划：[Stage Chart Format Update 实施计划](../../stage_plans/completed/chart-format-update/plan.md)

## 1. 结论

CFU-F2 已完成本地关闭。Playback-only external consumer 现在只 include 安装后的
`cuexis/playback/*.hpp`，并通过 public `Cuexis::Playback` target 覆盖 static/shared、
`add_subdirectory`/`find_package` 和 adapter-disabled headless。它真实 prepare filesystem project、
CXC file、CXC memory 与 owning typed project-document source，不再只构造 CXC/typed API。

static 安装包显式验证 `Cuexis::Playback` 的 `LINK_ONLY:Cuexis::InternalCxc` 闭包；consumer CMake
不得直接引用内部 target，安装后的 Playback headers 不得引用内部 CXC 头或 archive 实现。
shared 安装包继续验证内部 target、minizip 和内部运行库不泄漏。

该关闭不是 hosted 跨平台证据、canonical CXC 跨平台 byte parity、完整 CFU-F、公共 CXC package
API、完整 v4 动画 Playback、CFU-G 或 Stage 4。CFU-F3 为下一批次。

## 2. External Consumer

`tests/external/playback_consumer.cpp` 的 Cuexis include 被 CMake 门禁限制为：

```text
cuexis/playback/content_provider.hpp
cuexis/playback/frame_digest.hpp
cuexis/playback/playback_session.hpp
cuexis/playback/playback_source.hpp
cuexis/playback/presentation.hpp
```

clean consumer fixture 复制 CFU-F1 reference project/CXC、参数化 project、rational/weight Chart v4
和非空动画拒绝 CXC。正向路径覆盖：

```text
filesystem project / CXC file / CXC memory / typed project-document source
prepareLoad candidate identity and four-entry presentation manifest
presentation preflight and acquisition of every declared resource
commit, active identity, owning resource lifetime
0 ms, 625 ms, 250 ms backward seek, 1250 ms sampling
cross-source PreparedSemanticIdentity and FrameDigest v3 equality
```

固定 semantic identity 仍为：

```text
6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5
```

625 ms Stop sample 的固定 FrameDigest v3 value 仍为 `11596562486377158370`。

typed reference 使用 public `FilesystemContentProvider`、owning chart document table 和四个 public
`PlaybackAssetDescriptor`。它与 filesystem/CXC source 得到相同 identity 与四点 digest trace。

prepare options 不再在 prepare 前清空。number 参数 `layout.x=-4`、`layout.scale-y=2`、
`camera.fov=75` 经 filesystem project prepare 后冻结；调用方随后把原 options 改为 `layout.x=99`
不改变 committed frame。rational `2/2` 与 `1/1`、weight `0.5` 经 typed source 成功 prepare，两个
等价 rational 输入得到相同 semantic identity。

失败路径从 reference CXC active state reload 非空动画 CXC，并固定拒绝为：

```text
playback.capability.preflight_failed
playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1
playback.capability.unsupported@$/objects#cuexis.animation.layers.v1
```

拒绝后逐字段验证 active semantic identity、`PlaybackContentInfo`、active diagnostics 与 FrameDigest
version/value 未改变。

## 3. Package Closure

`cmake/VerifyExternalConsumer.cmake` 新增以下门禁：

```text
Playback-only consumer 的全部 Cuexis include 必须位于 cuexis/playback/
Playback-only consumer CMake 不得直接引用 Cuexis::Internal*、Core、Content、Audio 或 cuexis_cxc
安装后的 Playback headers 不得引用 cuexis/cxc、InternalCxc 或 cuexis_cxc
static CuexisTargets 必须包含 InternalCxc target
static Cuexis::Playback property block 必须包含 LINK_ONLY:Cuexis::InternalCxc
```

既有 package 门禁继续检查 static internal libraries 与 minizip license/dependency closure；shared
package 必须排除 `Cuexis::Internal*`、MINIZIP/minizip-ng、内部 DLL 和 adapter dependencies。consumer
始终只链接 `Cuexis::Playback`，因此 static 闭包由安装目标传递解析，shared 则由 Playback DLL
内部承载。

## 4. Local Verification

工具链：Visual Studio 2026 Community 18 / MSVC 19.51.36248.0，CMake 4.3.3，Windows x64。

```text
Debug static Playback-only add_subdirectory/find_package    passed (2/2)
Shared Debug Playback-only add_subdirectory/find_package    passed (2/2)
ctest --preset debug --no-tests=error                       368/368 passed
ctest --preset release --no-tests=error                     368/368 passed
ctest --preset shared-debug --no-tests=error                371/371 passed
ctest --preset headless-debug --no-tests=error              334/334 passed
shared export/import surface                                3/3 passed
cuexis_format_check                                         passed
tools/check_docs.py                                         128 Markdown / 20 JSON-CXT passed
tools/update_version.py --check                             passed (26.08.01-1)
git diff --check                                            passed
```

本报告没有 hosted Linux/Windows/MinGW、shared Release、headless shared、sanitizer、coverage、
clang-tidy 或跨平台 CXC byte parity 结果。这些仍属于 CFU-F3、CFU-F4 与 CFU-G。

## 5. Handoff

CFU-F3 下一步绑定最终实现 SHA，在 MSVC、MinGW 与 hosted GCC/Clang 上生成同一 canonical CXC，
比较 committed SHA-256、resolved semantic identity、迁移 golden 与 diagnostics 顺序。CFU-F2 不改变
public SDK API `0.6.0`，不公开 CXC archive 类型，也不解析或执行 v4 动画。
