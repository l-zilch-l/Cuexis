# Stage Chart Format Update CFU-C0 Baseline

状态：completed baseline report

快照日期：2026-08-11

后续关闭证据：[实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md) 和最终
Stage Chart Format Update completion report。

## 接受与实施基线

项目所有者于 2026-08-11 明确授权开始 Stage Chart Format Update 实施。该指令接受
[ADR 0038](../adr/0038-cxc-v1-and-chart-v4-boundary.md) 的完整十一项门禁，并允许按 CFU-C0 至
CFU-G 顺序修改生产 Schema、Reader、Writer、构建图、迁移和 Playback 接口。它不授权 Stage 4
动画求值实现。

```text
branch                  stage-ChartFormatUpdate
base commit             b092df7d39d16373a9dcc425453a24f2427a8b31
accepted docs date      2026-08-11
SDK API baseline        0.5.0
build version baseline  26.08.01-1
FrameDigest             v3, unchanged
```

实施开始时，CFU-B 修订文档、检查器和候选示例位于同一未提交工作树中；它们是本分支的接受输入，
不是 `b092df7` 已提交能力。后续证据必须绑定实际实现 commit，不得只引用本基线。

## Archive 依赖

选择 vcpkg baseline `40f3c709db80acf154ac4b17a1f83c564ebd022e` 中的 `minizip-ng` 4.1.0：

```text
license                 zlib
vcpkg features          all default features disabled
owner                    internal cuexis_cxc target
public headers          none
public CMake component  none
compression/encryption  disabled; CXC v1 is Stored only
```

minizip-ng 负责成熟 ZIP 读写。Cuexis 的窄 envelope validator 仍先验证 local/central metadata、EOCD、
ZIP64 sentinel、extra/comment、offset/range、overlap 和 trailing bytes。任何 archive library 私有错误
文本都映射为稳定 Cuexis diagnostic。若 C3 技术测试证明库无法在窄 validator 后可靠处理合法 Stored
archive，则保持 `cuexis_cxc` 接口并重新评估 libzip；不得放宽 CXC 合同或自研压缩算法。

依赖记录已进入 `vcpkg.json`、`docs/DEPENDENCY_POLICY.md` 和 `THIRD_PARTY_NOTICES.md`。C3 必须把
minizip-ng copyright 加入适用的静态安装许可证清单，并验证 shared Playback 不产生未记录 DLL。

## Fixture Promotion

```text
Chart/CXT valid candidate       copy to tests/fixtures/chart_format_update/valid
Chart/CXT invalid candidate     copy to tests/fixtures/chart_format_update/invalid
CXC manifest valid/invalid      copy to valid/invalid and pair with typed manifest diagnostics
strict ZIP violations           generate binary fixtures in C3; cannot be represented by JSON alone
canonical CXC package bytes     generate committed golden only after C3 writer is deterministic
runtime-script rejection CXT    promote as a permanent negative fixture
```

`docs/examples/chart_format_update` remains the review-facing copy. Production tests use only the promoted
fixtures and record one primary diagnostic for each invalid input.

## Target And API Boundary

Candidate target graph frozen for implementation:

```text
cuexis_chart -> core + json_support
cuexis_cxc   -> core + content + filesystem + project + chart + json_support + private minizip-ng
cuexis_playback -> existing dependencies + private cuexis_cxc
```

`cuexis_cxc` uses export name `InternalCxc`, participates in active-target/dependency verification, and enters
the static implementation closure only when C3 exists. It has no installed header or public alias.

CFU-E0 may add owning project-document source, `ChartParameterSet`/prepare options, CXC file/memory factories
and semantic identity observation. Existing `TypedPlaybackProject` aggregate layout and no-options overloads
remain unchanged. `0.6.0` is only the candidate SDK API target until E0 approval.

## C0 Exit

- ADR/spec status and date are aligned.
- Implementation branch and base commit are recorded.
- Archive dependency, license, feature set, containment and exit path are recorded.
- Fixture promotion, target topology and API sketch are frozen.
- C1 may start; CXC archive production claims remain forbidden until C3-C4 close.
