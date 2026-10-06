# Stage 7 CI runtime optimization

状态：implemented；语法、覆盖保持和三工具链本地缓存验证通过，Windows media 全量991项复验通过；hosted 尚未测量；owner 已授权提交/推送且不等待 CI 完成

日期：2026-10-05

## 1. 授权和测量基线

owner 已授权在 S7A-3/4 任务完成后、推送前优化远端 CI。
本报告采用 GitHub Actions API 中同一 HEAD
`6b11d102f6dcac4d9782cd03d0bae15add54af28` 的六个成功 runs；它们只用于耗时基线，
不证明本次未提交实现已通过 hosted。原始 run/jobs JSON 位于 `out/s7a-ci-runs-baseline.json`
与 `out/s7a-ci-jobs-<run-id>.json`。

| workflow / event | workflow 墙钟 | 最长 job 等待启动 | 主要 job 实际运行 |
| --- | --- | --- | --- |
| [MSVC push](https://github.com/l-zilch-l/Cuexis/actions/runs/37211595084) | 3135 s | 3 s | Debug2564 s / Release3131 s |
| [MSVC PR](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597458) | 4206 s | 1012 s | Debug2439 s / Release3193 s |
| [MinGW push](https://github.com/l-zilch-l/Cuexis/actions/runs/37211595061) | 3202 s | 3 s | Debug3198 s / Release1941 s |
| [MinGW PR](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597321) | 3578 s | 446 s | Debug3219 s / Release1935 s |
| [Linux push](https://github.com/l-zilch-l/Cuexis/actions/runs/37211595051) | 1640 s | 450 s | shader ASan1077 s / shader coverage1008 s |
| [Linux PR](https://github.com/l-zilch-l/Cuexis/actions/runs/37211597307) | 3479 s | 1668 s | shader ASan1927 s / shader coverage1021 s |

workflow 墙钟采用 updated_at-created_at；job 执行采用 completed_at-started_at；
等待采用 job.started_at-run.created_at。三者不混用，最大等待不一定来自最长 job。
六个 samples 是单个 SHA 的观察，不能当所有 runner 的长期分布。

## 2. 已定位重复和瓶颈

Windows Debug/Release job 各自执行相同的 `debug-media-tools` fresh configure、完整 build 和
完整 CTest。它们使用同一个 Debug media preset、相同编译器和 triplet；没有独立的
Release media preset 覆盖。每个 workflow/event 实际重复这个组合两次。

MSVC PR 两次 media step 是1163/1168 s；push 是1219/1151 s。
MinGW PR 是1448/850 s；push 是1465/855 s。
同一 SHA 六个 workflow runs 内，这个重复部分按所选执行位置合计可消除5232 s（87.20 job-minutes）；
这是实际重复 step 时间之和，不是预计 workflow 墙钟减少87分钟。

Linux shader ASan PR Configure716 s、shader coverage Configure692 s；push 对应442/682 s。
基线 workflows 没有持久化依赖二进制 archives 的 cache step。保留 pinned vcpkg 后，
跨 run 的二进制缓存可作为优化方向；实际可恢复包数与冷/热耗时仍需测量。

## 3. 已实施方案和取舍

1. 每个 Windows workflow 保留两个现有 Debug/Release full build/test job，
   media 全量检查仅执行一次。根据上述关键路径，MSVC 放在 Debug job，MinGW 放在 Release job。
   不增加第三个并行 job，避免在当前显著排队的情况下增加 job 数；两工具链 media 全量组合保留。
2. 增加统一的 vcpkg binary archive 缓存，按 OS/arch、编译器指纹、pinned vcpkg、manifest 与
   profile 区分 key；恢复前创建真正的绝对目录。缓存不包含 installed tree、SDK build outputs
   或测试结果。准备日志记录配置和 archive count；GitHub cache miss 仍执行正常 configure/build。
3. 保留当前 push/PR/dispatch 触发、所有 compiler/preset、full CTest、external consumers、
   format/architecture/diagnostic/version、sanitize/coverage 和 evidence 上传。
   不引入全局 parallel CTest；已观测到资源 fixture 的共享临时目录冲突。

以 PR sample 为纯算术参照，MSVC 去掉 Release 的重复 media 后，最长 job 从3193 s 变为
max(2439,3193-1168)=2439 s；MinGW 去掉 Debug 的重复 media 后，从3219 s 变为
max(3219-1448,1935)=1935 s。对应无额外队列变化时的理想关键路径减少754/1284 s。
这些是按旧 hosted sample 推算的上限参照，优化已实施但尚无优化后 hosted 耗时。缓存收益不计入上述推算。

master 当前 required-status API 只登记 Version advancement (pre-merge)；本次不修改保护规则。
新媒体执行位置仍属于现有 job，任一检查失败使对应 workflow 失败。

缓存机制依据 [vcpkg binary caching](https://learn.microsoft.com/en-us/vcpkg/reference/binarycaching)
及 [GitHub dependency cache](https://docs.github.com/en/actions/reference/workflows-and-actions/dependency-caching)。
文件 provider 使用 `files,<absolute-directory>,readwrite`；key/restore-key 与 vcpkg 自身 ABI
校验分别承担缓存集合查找和二进制兼容性判定。不能把恢复到 archives 等同实际命中全部 packages。

## 4. 实施与本地验证

S7A-3/4 的 GCC/Clang 完整矩阵各853项通过后，才修改三个 workflows。
新 action 为 `.github/actions/cache-vcpkg/action.yml` 和 ASCII Python helper `prepare.py`。
三个 workflows 共9个 job definitions 调用 action；展开矩阵后17个 vcpkg job 使用缓存，
parser-only job 不安装依赖。Windows media 的唯一位置条件为 MSVC Debug、MinGW Release；
media 三条完整 configure/build/test 命令逐字保持。
MinGW 的 Locate build tools 用 `command -v` / `cygpath -m` 导出真实 g++ 路径，
交给 PowerShell composite helper 做指纹；没有假设编译器固定安装目录。

缓存目录从 RUNNER_TEMP 解析、mkdir 后写入 GITHUB_ENV / GITHUB_OUTPUT。
`VCPKG_DEFAULT_BINARY_CACHE` 的值是绝对目录；`VCPKG_BINARY_SOURCES` 是
`clear;files,<absolute-directory>,readwrite`。两者职责不混用。
编译器指纹包含 executable bytes、实际 version output、OS/arch、runner image、MSVC toolset/SDK；
key 再含 pinned vcpkg、manifest/Presets/triplets 和 profile。
restore-keys 只在相同 OS/arch、编译器指纹和 pinned vcpkg 内复用 archives 集合，
vcpkg 自身 ABI 决定具体可恢复包。action 记录 exact-key hit 与恢复后的 archive count/bytes。

| 本地验证 | 结果 / 证据 |
| --- | --- |
| actionlint1.7.12 | 三个修改后的 workflows exit0；`out/s7a-ci-actionlint-final.log`；release ZIP 已核对官方 SHA256 |
| 覆盖保持 | YAML 结构与 HEAD baseline 比较：仅新增 cache steps、两个 media 条件、MinGW 编译器路径导出；所有原 job/matrix/trigger/permissions/验证命令/上传不变；`out/s7a-ci-coverage-invariants.json` |
| version gate | 文件文本与 HEAD 一致，未修改日期/版本规则或 required-status |
| composite / scripts | PyYAML 实际解析、Python AST、全部 Linux/MinGW bash run scripts 的 `bash -n` 通过；11个 PowerShell run scripts 原生 Parser 0 errors |
| cache helper | Windows cl、MinGW UCRT g++、Linux GCC 实际 executable 探测通过；目录均存在，MSYS 继承 provider 字符串无路径改写 |
| Windows media | fresh configure / 完整 build 和991项 CTest exit0；0 failed、1 Windows symlink skip，571.63 s；`out/s7a-ci-media-verify.log` / `.xml` |
| 输入锚点 | 五个 CI 文件 manifest=`out/s7a-ci-inputs-final.json`；SHA256=`9ad06ff770991c38415b003b01aa3bf0044a968252e1bb757e3c0e6638682f6f`；65个功能行为输入指纹保持不变 |

### 4.1 pinned vcpkg 实际冷/热恢复

探针使用同一 pinned commit `40f3c709db80acf154ac4b17a1f83c564ebd022e`，
两个业务 ports `entt` / `tl-expected` 加两个 vcpkg CMake helper，共4包。
冷缓存目录为空且只启用本次 files provider；成功生成4个 ZIP。
将 ZIP archives 复制到另一个 runner 目录，重新运行同一 helper；热轮使用新的 installed、
buildtrees、packages 根。三个环境均实际报告 `Restored 4 package(s)`，无 source build，
不是仅验证目录变量或复用 installed tree。

| 环境 / triplet | 冷轮进程墙钟 | 热轮进程墙钟 | archives / 恢复 |
| --- | --- | --- | --- |
| MSVC / x64-windows | 22.511 s | 8.531 s | 4 / 4 |
| Linux GCC / x64-linux | 93.154 s | 37.954 s | 4 / 4 |
| 本地 MinGW UCRT / x64-mingw-static | 37.101 s | 12.028 s | 4 / 4 |

原始日志为 `out/s7a-ci-cache-windows.log`、`out/s7a-ci-cache-linux.log`、
`out/s7a-ci-cache-mingw.log`；Windows 两种 installed 根各自 install.log/measurements.json
位于 `out/ci-draft/probe-*`，Linux 原始探针根在 `/tmp/cuexis-cache-probe-final`。
这些是一次本地小型 provider 实验，包含 compiler detection，不能外推 shader/media 全 manifest 的
缓存速度或 hosted Actions cache 网络/排队收益。CI MinGW 仍用原 MINGW64/x64-mingw-dynamic；
本机 UCRT/static 的验证不冒充同 runner/triplet 全量证据。

shellcheck/pyflakes 未安装，actionlint 明确禁用它们；bash -n、PowerShell Parser 与 Python AST
只证明语法，功能性证据来自上述真实 provider 恢复和完整 CTest，二者分开记录。

## 5. 推送前与 hosted 边界

owner 随后于2026-10-05明确授权“提交并推送”，并要求推送后不等待 CI 跑完。
该授权覆盖本次实现、相关设计/验收文档、CI 优化和必要日期 build 更新，不关闭 Stage7A 或授权 merge/release。
优化后的 hosted runner、MINGW64/dynamic media、cache service 命中、排队和 workflow 墙钟仍未测得；
旧六个 runs 只作基线。包含本报告的实际 Git commit 作为本次推送锚点。

为满足 trusted UTC 2026-10-05 的 live 门禁，本轮通过 `tools/update_version.py` 将日期 build
推进为26.10.05-1，SDK 保持0.7.0。20个 version-gate tests 通过（2个 Windows skip）；
本地 working-tree 与 origin/master 的 live comparison 通过，基线日期26.09.29-1。
版本文件一致性与日期推进分别验证，未跳过 stale-date 门禁。
日期更新后 Debug/Release 均执行 fresh configure / clean-first build；
Debug187项聚焦检查通过（37.52 s），Release完整943项通过（0 failed、1 symlink skip，667.02 s），结果同步记入功能验收报告。
其余四工具链旧日期功能矩阵、provider 实验和 hosted 性能边界保持原记录，不改记为本日期或新SHA实测。
