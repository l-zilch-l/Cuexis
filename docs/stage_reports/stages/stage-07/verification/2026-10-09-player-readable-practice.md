# Player readable four-lane device practice

状态：受限 CPU 与定向 SDL/GPU 窗口验证通过；实体键盘、听音、校准与新 SHA hosted 待回填

证据日期：2026-10-09（Asia/Shanghai）

## 问题、合同与实施

父 HEAD `e2055763e4bb49fa69b1458d3487ecef596adb00`，stage-7 / existing PR32。
旧六方块只显示 phase 反馈，没有音符、判定线或开始等待；要求用户看 H 日志才能操作，
不适合直接试玩。本轮改为 D/F/J/K 四轨、下落短音符、K Hold 长条、判定线、实际分数和
Hit/Miss；启动 Ready 等 Space。四个 Tap 后接一个 Hold，不再要求故意漏 J。

所属合同：[Playback Player guide v1](../../../../api/playback-session.md)，
决策：[plan-a §3.4.17](../../../../stage_plans/active/stage-07/plan-a.md)，
操作：[设备示例](../../../../examples/s7a78-device-acceptance.md)。
guide 是显式可选 testOnly 的 Player 私有显示输入，不安装新 API、Kernel/Recovery 图或依赖。
精确整数目标与实际 H 决定位置；命中/失败读取实际 FactBinding snapshot visibility，
分数读取公共 query。显示不生成输入、Fact 或 token，不改变判定四分量或原逐转换 H/T 桥。
Seek/reload/Stop 从当前 snapshot/query 重画，没有 outcome 缓存。R 重置 Gameplay H，
既有 reload 保留音频/Chart 时间的政策不变，因此重来不声明音乐同步。

guide 与 configuration 共用显式 maxBytes/maxContainerElements 限额；拒绝不完整记录、
非整数/负 Tick、错误数量/尾字段、重复 cue、未映射按键、当前 snapshot 缺失 cue。
合同/Spec v2、Capsule revision3、guide v1；SDK0.7.1、build26.10.09-2。
没有新增公共 API 差异或版本输入变化；本轮沿前次 fresh/clean-first 配置增量编译，
不声称重复执行版本变更全量构建。推送前通过 updater 按 master build1+1 保持候选2。

## 人工 golden 与独立核对

新 fixture 为五个 requirement、七个 phase、十四个 Hit/Miss 标记。
Tap 目标60/120/180/240，Hold head300/body420/tail440，H480 后核对最终结果。
人工全命中：`score=18 combo=7 hits=7 misses=0`，分数 `2+2+2+2+2+3+5`，
每个 Hit phase 均 incrementsCombo=true。零输入为 `score=-7 combo=0 hits=0 misses=7`。
初次 checker 错把五个 requirement 当作 combo5，实际公开路径得到 combo7；经逐项
核对既有 scoreRules 修正人工期望，未改 Fold。该失败尝试及订正保留在原始证据中。

标准库 checker 不导入 generator 的 EVENTS、不调用生产推进来生成期望：独立读取
ASCII guide、typed author phaseTargets/atomBinding、完整 requirement identity、FactBinding
phase/outcome/target/refs、实际实体和计分配置，并核对 ZIP 完整 bytes/SHA、Graph/Packed
identity 与 PCM。实际 Host 四个键盘进程分别覆盖 Graph/Packed 的输入/零输入，完整
ReplayEvaluation 比较均 same；两次 audio/ChartClock 明确拒绝是预期负例。
生产 assembler 两包均 actualPrepareValidated=true、productionBudgetAccepted=false。

## 命令、结果与证据

实现 SHA：aec09b98273278b3cf31cb7af98e8cc6d03688ff；后续绑定提交仅修改文档，不改变受测代码/构建输入。受测工作区 diff、二进制/fixture 哈希与原始命令输出归
[原始证据 ZIP](2026-10-09-player-readable-practice-evidence.zip)。本轮自生成素材，无外部下载。

| 验证 | 本轮结果 |
| --- | --- |
| MSVC Debug/Release candidate ON STATIC Player 与 Controller | 各34 cases/739 assertions通过，包含exact >2^53、负/浮点/缺失/重复/预算/映射及snapshot拒绝 |
| MSVC Debug/Release candidate OFF STATIC | 各28 cases/483 assertions通过，guide参数明确candidate.disabled；普通播放控制兼容 |
| Debug/Release Gameplay 公共路径 | 各25 cases/1467 assertions通过，完整结果比较及既有故障注入保持 |
| 两轴 architecture / installed Playback / installed Gameplay consumer | 各3/3通过，包含headless安装树消费者 |
| guide/source/binding独立核对、ZIP/PCM与六个Host进程 | 通过；四个golden/Replay、两个明确时钟拒绝 |
| 独立oracle故障注入、生产assembler复装配 | 交换Hit/Miss cue后明确拒绝；两包与author/configuration/guide重复生成逐字节相同 |
| 定向 Win32→实际 SDL poll→Player submit/advance/query→GPU | Ready/长条/完成窗口已查看；DFJK与Hold命中，H480 score18/combo7/hits7/misses0、Replay=same、exit0 |
| docs/status/section/target、诊断码、version、格式与diff | 394 Markdown/20 JSON，4/4、5/5、2/2；Version Gate回归26 tests、2环境跳过，其余通过；格式/diff通过 |

窗口测试只向本轮启动的 Player HWND 投递转换，未全局注入键盘；截图只截该窗口 client。
这验证实际 SDL/GPU 路径与可读提示，不能称为实体键盘、人工听音或生产校准验收。
初次本地增量测试出现崩溃：修改私有 profile 头后 Ninja 未重编译旧 Controller test object；
旧object时间18:00早于头19:03，应用/测试源重编译后 Debug/Release 全通过。
这是本机构建依赖/语言前缀问题的受限处置，不声称完成所有构建环境排查。

核心复现命令：

```powershell
python -B tools/make_device_acceptance_chart.py --tool out/build/candidate-debug/bin/cuexis_chart_candidate.exe --fixture out/build/candidate-debug/s7a78-public-fixture --output out/device-practice-v2
python -B tools/check_device_acceptance_chart.py --directory out/device-practice-v2 --host out/build/candidate-debug/ec/host/host-build/cuexis_reference_host.exe
python -B out/practice_final_validation.py
powershell -NoProfile -ExecutionPolicy Bypass -File out/verify_practice_window.ps1
```

out helper 与每条实际编译/test命令随 ZIP 保存；不是承诺安装这些内部测试文件。
独立只读 review 核对没有第二判定路径，并要求追加 guide↔source/binding检查与拒绝用例，已落实。
桌面原两cmd继续使用，同步新包/config/guide/脚本/说明/二进制哈希；旧runs与旧包备份保留。

## 保留门禁

本轮没有重新执行 MinGW/Linux C++、独立 shared、sanitize/coverage 全矩阵。
Version Gate 回归的2个环境跳过保留在原始日志，不记为实际shell路径已执行。
新 SHA hosted/owner approval 待回填；旧 SHA 成功不认证本轮显示。
新四轨版实体 DFJK、focus/control、实际音频听感、输出设备丢失/恢复、刷新率/输入延迟与
wall-clock/audio 校准仍待逐项人工证据；不把此前六方块启动观察迁为新谱面通过。
音频点击声仍只用于输出/控制观察，不作为 H 目标的音乐节拍。
不接受生产阈值、不退出 S7A-8.4，不开展 S7A-9 最终关闭或 Stage7A关闭/Stage8发行。
