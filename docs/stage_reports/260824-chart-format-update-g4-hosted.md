# Stage Chart Format Update CFU-G4 Hosted Verification

状态：CFU-G4 complete；最终候选的同 SHA hosted 验证通过；CFU-G 仍 active，等待 G5 completion report 与 G6 owner acceptance

执行日期：2026-08-24

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md) §5.9、§11。

## 1. 验证对象

本次验证使用最终候选分支 `codex/cfu-g4` 的同一实现 SHA：

```text
implementation SHA: 4371fdcf04f4f89bfddf070cbb15e4c903810a53
branch: codex/cfu-g4
run attempt: 1 for every workflow and job
```

三套 workflow 均在该 SHA 上完成并成功。没有代码、Schema、fixture、CMake 或 workflow 修复；没有失败作业或 failed-job rerun。

## 2. Hosted workflow 结果

| Workflow | Run | Jobs | Attempt | First failure |
| --- | --- | --- | ---: | --- |
| Linux Quality | [32701829853](https://github.com/l-zilch-l/Cuexis/actions/runs/32701829853) | GCC Coverage; GCC Shared Release; GCC Release; Clang Shared Debug; Clang ASan + UBSan; clang-tidy | 1 | none |
| Windows MSVC | [32701829839](https://github.com/l-zilch-l/Cuexis/actions/runs/32701829839) | debug; release | 1 | none |
| Windows MinGW | [32701829856](https://github.com/l-zilch-l/Cuexis/actions/runs/32701829856) | debug; release | 1 | none |

每个表内 job 的 conclusion 均为 `success`。Linux Quality 的 Coverage、Shared Release、GCC Release、Clang Shared Debug、ASan/UBSan 和 clang-tidy 均完成；Windows 两套 workflow 的 Debug/Release 均完成。

## 3. F3 确定性产物

六个 F3 构建产物（Linux Release、Linux Clang Shared Debug、MSVC Debug/Release、MinGW Debug/Release）都包含相同的确定性输入和结果：

| 项目 | 跨平台结果 |
| --- | --- |
| CXC v1 bytes | `6905` |
| CXC package SHA-256 | `1cb2fcbf7a852a4db2ed9119359c68ff1cc4a06d1d41d18018fff89cf737d723` |
| Prepared semantic identity | `6d01494c126f3ae8fc9420259dc92873233022dec9dd6bf9caf04b217f100cc5` |
| Stop FrameDigest v3 | `11596562486377158370` |
| Canonical migration Chart SHA-256 | `1ca9f60feee215fdc4eca1f7cafbbea8704976eca18ed40e51333b6b2e7a5385` |
| Migration report SHA-256 | `81df14e422603ae411ea8a70f5de89fb49ae2a34b804af71146a8be3075824e0` |
| Diagnostics fingerprint SHA-256 | `9f7f98fc3bedf81588e62011fd3a49587014b3390aae3906404498e7b60176a0` |

The CXC binary, canonical migration JSON, and migration report are byte-identical across all six artifacts. The diagnostics fingerprint content is identical after normalizing platform line endings; LF/CRLF differences are limited to text packaging and do not alter the fingerprint fields.

The stable diagnostic signature is:

```text
playback.capability.unsupported@$/animationClips#cuexis.animation.clip.v1|playback.capability.unsupported@$/objects#cuexis.animation.layers.v1
```

This confirms that non-empty Chart v4 animation remains rejected before Stage 4 with the existing capability code. No animation execution capability was added.

## 4. F4 hosted evidence

The Linux Quality artifacts also completed the final safety, analysis, coverage and trend probes for this SHA:

| Evidence | Result |
| --- | --- |
| ASan/UBSan focused safety set | 9/9 tests passed; sanitizer log contains no failure |
| clang-tidy | 68/68 build steps completed; no diagnostic output |
| Coverage | chart aggregate: lines 80.3%, functions 93.7%, branches 42.4%; CXC/combined aggregate: lines 80.4%, functions 96.8%, branches 44.8% |
| Maximum-content trend | 67,113,275-byte content; CXC write 41.030 MiB/s; hash/load 61.544 MiB/s; prepare 343,901.812 us; warmed update/extract average 0.864 us |

Performance values are trend evidence only; no runner-specific hard threshold was added.

## 5. Exit-ledger transition

The hosted condition in the G1/G4 ledger is now satisfied. The current ledger is:

```text
17 PASS / 1 PENDING / 0 PRODUCT BLOCKER
```

The only remaining pending condition is the completion report and explicit project-owner acceptance. This report does not create `stage_chart_format_update_completion_report.md`, does not close CFU-G, and does not unblock Stage 4.

## 6. Boundaries retained

- `cuexis_cxc` remains an internal target; no public `Cuexis::Cxc` package component is introduced.
- Chart v4 static/parameterized prepare remains supported; non-empty animation execution remains Stage 4 work.
- No `AnimationSystem`, sampling, blending, property write-back, runtime scripts, callbacks, bytecode, new capability, FrameDigest version, ABI, or public C ABI was added.
- Stage 4 remains `future / blocked / not started` until the completion report is accepted by the project owner.

下一步按计划进入 G5：基于本报告和已有 C0-F、G1-G3 证据创建 completion report；G6 owner acceptance 之前不改变当前阶段状态。
