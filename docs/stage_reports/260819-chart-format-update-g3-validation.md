# Stage Chart Format Update CFU-G3 Final Candidate Validation

状态：CFU-G3 未关闭；本地候选门禁通过，候选发布与同 SHA hosted 验证受执行环境阻断

执行日期：2026-08-19

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)
§5.9。

## 1. 结论

G0-G2 内容已冻结为本地临时候选提交
`9d9444f72d673672458a71bc08b0c25f96680cd5`（父提交
`913639ca6049ce9c974a6d8fe210cd2d77ec4dd7`）。冻结时逐字节比较了 625 个 tracked 文件，当前
验证输入与该提交无差异；本报告和状态同步是验证完成后追加的文档证据，不属于该临时候选。本地
Debug、Release、headless Release、format、architecture、安装包、公共头 ASCII、license/NOTICE、
version、文档和 diff 门禁均已通过。

该 SHA 只存在于可写临时 clone，尚未成为仓库或远端分支上的可观察提交。当前权限不允许写入主
worktree 的 Git metadata，受限网络也不允许 push。因此 Linux Quality、Windows MSVC 和 Windows
MinGW 没有在该 SHA 上运行。G3 仍未关闭，不能创建 completion report、请求项目所有者接受、把
Stage Chart Format Update 标为 completed，或解锁 Stage 4。

## 2. 候选身份

| 项目 | 结果 |
| --- | --- |
| 远端分支最后可观察 SHA | `913639ca6049ce9c974a6d8fe210cd2d77ec4dd7` |
| 本地临时候选 SHA | `9d9444f72d673672458a71bc08b0c25f96680cd5` |
| 候选内容比较 | 625 个 tracked 文件，0 mismatch |
| 主 worktree commit | 未创建；`D:/Cuexis/.git/worktrees/Cuexis3/index.lock` 写入被拒绝 |
| 远端 push | 未执行成功；当前网络无法连接 GitHub，GitHub CLI 配置也不可读 |
| hosted final-SHA run | 未产生 |

`913639c` 在 2026-08-16 的 Linux Quality、Windows MSVC 和 Windows MinGW workflow 均成功，但
它不包含 G0-G2 文档与防回退门禁，不能作为 G3 证据：

- [Linux Quality run 31935414060](https://github.com/l-zilch-l/Cuexis/actions/runs/31935414060)
- [Windows MSVC run 31935413971](https://github.com/l-zilch-l/Cuexis/actions/runs/31935413971)
- [Windows MinGW run 31935413943](https://github.com/l-zilch-l/Cuexis/actions/runs/31935413943)

## 3. 本地验证

本地编译器为 MinGW GNU 16.1.0。由于全局 `D:/vcpkg` 在当前权限下只读，fresh configure 先把
buildtrees/packages 重定向到 worktree，再用已安装依赖和 `VCPKG_MANIFEST_MODE=OFF` 运行 nested
external consumers。该调整只改变本地验证环境，不改变候选源码或生产 CMake 合同。

| 门禁 | 结果 |
| --- | --- |
| Debug fresh configure + clean build | 通过；204 个构建步骤 |
| Debug CTest | 等效 `378 passed / 1 skipped`；并发双套件中的一次 CXC unpack staging 提交失败单独重跑通过 |
| Release fresh configure + clean build | 通过；204 个构建步骤 |
| Release CTest | `378 passed / 1 skipped` |
| Headless Release fresh configure + clean build | 通过；162 个构建步骤 |
| Headless Release CTest | `343 passed / 1 skipped` |
| Debug/Release external consumers | 各 `7/7` 通过 |
| architecture | Debug、Release、Headless Release 各 `1/1` 通过 |
| format | `cuexis_format_check` 通过 |
| installed public-header ASCII | 通过；由 clean find_package package scan 覆盖 |
| license/NOTICE manifest | 通过；由 clean find_package package scan 精确校验 |
| version | `26.08.01-1` 一致 |
| documentation | 通过；最终报告加入后由 `tools/check_docs.py` 重跑 |
| whitespace | `git diff --check` 通过 |

唯一 skip 为 `Secure file rejects physical containment escapes through symlinks`，原因是当前 Windows
环境没有创建该测试所需 symlink 的能力；它不是失败。

## 4. Shared 与 MSVC 观察

同内容 worktree 的 MinGW `shared-release` 和 `headless-shared-release` 构建成功，但各有 5 个 CTest
失败：4 个 package 测试按 Windows MSVC 约定检查 `.lib`，而 MinGW 生成 `.dll.a`；export surface
测试选择的 LLVM `llvm-nm` 不能读取该 PE DLL 的 dynamic symbol table。ADR 0033 将 MinGW shared
列为 experimental，hosted Windows MinGW 也只执行 static Debug/Release，因此这些结果不是受支持
shared 产品回归。

当前 shell 没有可直接使用的 MSVC 环境；调用 Visual Studio 环境后又会进入受限 vcpkg/network
路径。本地 MSVC 没有形成候选证据，也不能替代要求的 hosted Windows MSVC run。

## 5. 阻断与下一动作

G3 目前只有外部执行阻断，没有新的产品代码、Schema、fixture、package 或工具阻断。恢复 Git
metadata 写权限与 GitHub 网络后，必须：

1. 将 G0-G2 内容提交到仓库分支，确认远端 SHA；
2. 在该 SHA 上运行 Linux Quality、Windows MSVC 和 Windows MinGW；
3. 记录 run URL、job 和第一失败步骤，任何实现/CMake/workflow 修复都重新冻结候选；
4. 全部成功后创建 `stage_chart_format_update_completion_report.md` 并等待项目所有者接受。

在这些动作完成前，CFU-G 保持 active，Stage 4 保持 blocked / not started。不得宣称完整 CXC 公共
产品支持、公共 CXC package API、完整 v4 动画 Playback 或 CFU-G 完成。
