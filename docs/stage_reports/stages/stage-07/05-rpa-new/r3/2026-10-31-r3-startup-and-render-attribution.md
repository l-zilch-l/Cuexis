# R3 窗口启动响应、JSON 准备及渲染归因

状态：有限诊断与未选用候选归档；仅测试探针响应修复进入根树，R3 未退出。

日期：2026-10-31（部分实验始于 2026-10-30）。返回 [New RPA 索引](../README.md)。
当前接受状态只见 [CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 现场反馈与实际修改

owner 报告 40K 测试窗口创建后长时间无响应。本批立即停止控制器，再终止最后一个实际子进程；
原 `gl-state-measure-v3` 保留 23/24 个完成进程、1,380 帧及末进程日志，不补完、不改判。
此前四帧重叠 RGB 换色检查的 baseline/candidate 像素和摘要一致，只证明该有限场景。

探针原顺序是创建可见窗口后，在同一主线程同步生成夹具、执行 Playback prepare、首帧更新和提取，
直到稳态循环才 pump。40K 静态准备实际约 18.9 秒，足以解释窗口长时间没有消息泵。
根树修改仅限 `tests/player/gpu_throughput_probe.cpp`：CPU 夹具和 Playback prepare 在建窗前完成，
各启动阶段独立记录 begin/end 和耗时；创建窗口后以及首帧/正常循环消费 SDL 事件，
窗口关闭、Quit 或 Escape 抛出受测试框架记录的取消，不再继续运行整批。
session / PreparedPlayback 全程留在创建线程，不跨线程搬运 owner-bound 状态。

窗口创建时机改变不构成加载提速。`gl-startup-final-check-v1` 使用格式化后根源码的实际重编二进制，
绑定匹配的 Release DLL；1K 正常路径 21 assertions 通过，实际投递 WM_CLOSE 后退出 42，
日志明确包含取消原因。旧格式探针的独立 40K 启动诊断也通过功能检查；窗口不在 CPU prepare
阶段出现。消息确认观察仍有 100 ms 超时，主要在冷图形阶段，未宣称所有建窗后操作实时响应。
产品 Player 的同步加载路径并未随探针修改而修复。

## 运行库身份修正

本机 `.buildenv.ps1` 把全局 Debug DLL 路径置于 Release 之前。早期 GL overlay 没有复制本地 DLL，
因此虽编译为 Release，未取得精确已加载模块清单，不能作为已核验 Release 设备结论。
这影响本批早期状态缓存、实例化以及初轮启动归因的完整运行身份；原结果和限制原位保留。

`json-sax-check-v2` 中原 parser 的 schema 测试先发生 0xC0000005，candidate 尚未运行。
新 `json-sax-runtime-v3` 复制完全相同的 exe，再加入与头文件/import library 同树的 app-local
Release DLL，schema 与其余预期测试通过。旧失败不解释为 SAX 缺陷。
之后正式的有限 CPU prepare 对照、最终探针响应检查和渲染归因均显式绑定这些 Release DLL。
没有重写旧目录、回填旧测量或把新身份套给旧组。

GL overlay 的共同库来自 `product-sequence-build-v1`，双方都含私有分页活跃索引原型；
不是根树产品 kernel。该夹具不执行 Gameplay，但仍明确报告来源，不冒称最终根源码矩阵。
实际恢复测试源码绑定为 `product-sequence-source-v1-resume-v2.json`，旧 probe binding
指向 pre-resume manifest 的问题由补充身份说明纠正，不追改冻结回执。

## 独立分页活跃索引取舍

独立产品原型不含 R3c、预算或 Fact batch。此前四套件 420 cases 与架构/依赖门通过。
固定 writer 组的 1K、40K、六波中位数变化为 +0.10%、-0.80%、-0.23%，数值保护与重复性通过。
固定 Seek 组数值保护通过，但 1K H902 的 candidate/baseline 相对范围为 25.64%/15.30%，
整组未过。40K 无实质收益，维护取舍为不选用；不会靠追加样本制造明显收益。

## 状态缓存与实例化原型

状态缓存只在单次 unlit pass 局部复用状态，每对象 world uniform 及绘制仍执行。
其 24 进程组因 owner 的无响应反馈在最后进程中断，不能生成完整接受结论。

实例化 v2 仅合并相邻、不透明、同 mesh/texture/cull/alpha 状态的 unlit draws，
不排序合批，透明及 parameterized 路径保留。矩阵和颜色为逐实例属性，临时 buffer
在 pass 末释放；显式移除 mesh VAO 对临时 buffer 的引用，避免仅 disable 属性仍保留资源。
四帧 RGB 像素/摘要一致。固定 8 进程 render-only 对照结果如下：

| 实体数 | 完整 cycle 中位数 baseline → candidate | submit 改善 | 重复性 |
| --- | --- | --- | --- |
| 1K | 1.268 → 0.824 ms | 35.09% | 通过本地 15% screen |
| 40K | 54.069 → 40.711 ms | 24.02% | 通过本地 15% screen |

预设 40K submit 至少改善 30% 未达到，原型不选用。且这些早期进程存在上述 DLL 身份缺口，
不外推 Release 设备接受或 240 FPS；不追加样本改判。

## 长准备的归因及 JSON 候选

单独插桩进程将约 18.1 秒 prepare 分为 Chart load 8.71 秒、canonical Writer 8.99 秒，
Runtime prepare 224 ms；继续插桩发现两次 nlohmann DOM parse 各约 8.3 秒，
Cuexis DOM 转换各约 216–221 ms、typed objects 约 270 ms。
这些属于带输出探针的归因，不与普通组混作同一基线。

本机依赖的 `json_sax_dom_callback_parser::end_object` 在每次对象完成后扫描父容器寻找
discarded 值；本 callback 始终保留值，宽对象数组仍反复扫描，形成平方成本。
独立 SAX 候选直接构建 Cuexis Value，保留同一 lexer、整数/浮点分类、解码字符串限制和深度限制。
普通 parse 继续推迟报告 duplicate，使之后的语法、深度或字符串错误保持原优先顺序；
不会套用 `parseBounded` 提前拒绝的不同合同。首个重复键在 key 回调记录。
旧空字符串 sentinel 会误接受重复空键；v2 用 optional 记录并正确拒绝，这是明确的合同修复。

29 JSON cases / 6,454 assertions 在正确 Release DLL 下通过；旧版只有新增空重复键反例失败。
26 项数字、malformed 与嵌套语料的完整结果/错误 code、message、context 逐行一致。
只读代码质量审查未发现阻塞，仍不替代全量源码验证。

固定普通 CPU 组：1K/10K/40K，各三对 AB/BA，共 18 个新进程，每进程一次 timed prepare。
mask 4、BelowNormal、执行节流 OFF 逐进程核验，普通分配器、无 GPU 窗口；夹具在计时外，
commit/update/FrameDigest 在计时后。三规模 Prepared SHA 和 FrameDigest 均一致。

| 候选 | 1K prepare | 10K prepare | 40K prepare | 整组结果 |
| --- | --- | --- | --- | --- |
| SAX v2 | 76.720 → 57.330 ms | 1.677 → 0.700 s | 17.678 → 2.737 s | 重复性通过；1K 改善 25.27%，未达预设各规模 ≥30% |
| SAX + legacy parse-once | 79.385 → 53.456 ms | 1.677 → 0.509 s | 17.555 → 2.028 s | 数值通过；1K 双方、10K baseline 重复性未过 |

组合版以私有 const-view typed Reader 读取同一源树，再调用原 canonical Writer，
保留原二参数 dispatch symbol，不改变任何公开类型、wire 或 canonical 身份规则。
但 writer 现在早于 caller 的 diagnostics.append；若 canonicalization 抛异常，可能丢失先前
局部 loader 警告，普通 Result 失败仍由 Playback 检查并拒绝。canonical 输出参数仅在成功
legacy 路径赋值，不能只凭 dispatch.hasValue 推断成功。此异常诊断限制单独保留。
两候选均未进入根树产品，未放宽或补样；2.028 秒仅 prepare，不能当进程到首画面 ≤2 秒通过。

## 实例化之后的剩余 CPU 成本

`render-attribution-measure-v2` 使用一个 40K render-only 进程，实例化 v2 + SAX v2，
正确 Release DLL，带阶段输出。末段观察中 neutral builder 约 30–32 ms，
其中命令构造约 7–8 ms、排序约 5–6 ms、复制约 4 ms、digest 约 11–12 ms；
GPU handle resolution 约 4–5 ms、GL draws 约 1.7–2.4 ms。
时间段包含同步 CPU、输出及驱动交互，不等于 GPU busy，完整组是诊断而非普通性能。
说明下一步应处理命令/字符串重复物化及摘要成本，而不只继续压缩逐对象 GL 状态调用。

## 证据与剩余边界

原始材料在 `out/verification-20261030/r3-next-v1`，成功与失败目录都保留。
[附件索引](2026-10-31-startup-evidence/index.md) 及 [manifest](2026-10-31-startup-evidence/manifest.json)
保存复制字节、哈希、源码、控制器和未选用补丁；
[身份与取舍补充](2026-10-31-startup-evidence/identity-and-selection-supplement.json)
记录 DLL 缺口、来源恢复绑定、审查限制和各失败门。
命令行准备失败、错误命名空间/读取编码、缺 runtime DLL 的 schema 崩溃、
render timer 初版作用域编译失败均独立保留，不把工具错误解释为产品性能失败。

已验证的探针修改通过实际 GPU 功能与关闭反例、单文件 clang-format 和 diff 检查；
文档及架构检查另记本次命令日志。没有运行最终全配置矩阵或宣告所有任务完成。
普通帧 240 FPS、完整 load、Seek/R3c 取舍、完整播放内存/保留上界、最终源码矩阵、
R4 正式、设备/生产接受与 Stage 7 关闭仍开放。
