# PD-3-c 宿主细化与 SDK 差异决策

日期：2026-10-19。状态：owner 已在本对话接受新增只读 API 与 SDK 0.8.1；内部实施及适用本地验证完成；新建无上下文 GPT-6-Luna 独立审核发现的 P2 测试覆盖缺口已补正，最终复核 PASS；PD-3-c 完成。
用户本次授权实施 PD-3-c；本记录不把授权扩展到 PD-4、合并、推送或发行。
当前实施状态唯一见 [CURRENT_STATUS](../../../../CURRENT_STATUS.md)。

## 1. 已授权内部范围

落实[宿主合同 §8.1](../../../../api/realtime-host-boundary.md)及
[PD 细则 §4.4、§11](../../../../stage_plans/active/stage-07/presentation-decoupling-design.md)：
来源与确认使用许可分离；Pause/Resume/纯 EOF 换段保留 projectionScope、状态来源和激活；
只从实际最后成功呈现画面生成新 attempt；保留重绘不重新求值、不消费 drain 的新 Fact 或 lifetime。
Seek/Reset/Stop/内容或表现替换撤销旧画面再次提交资格。完整 HostContext、资源与呈现端核对保留。

消费者注册、交付额度、反馈 optional expiry、载荷/ledger 回收和压力驱逐不在本批。
原 PD-3-b 源码及证据保存在 `out/pd3c-verification-20261019/pre-pd3c-workspace.zip`、
`pre-pd3c-state.json`；已核验其原 evidence manifest，未覆盖旧基线。

## 2. shared 边界与具体候选

PD-3-b 的提交描述由 Playback 私有 State 唯一保存。独立 static `execution_host_support`
在 shared 配置只消费安装 Playback API；不能链接 hidden `detail::PresentationAccess`。
`executionOwnerState().generation` 包含纯控制变化，不是 resultVersion；当前 frame/cursor
也不能用新 query 的较新 drain 前缀替换旧来源。复制一套 Worker 修订状态机或导出私有符号都不采用。

候选为一个兼容新增的只读 API：`PlaybackSession::presentationSource()` 返回 owning
`PresentationSourceInfo`。它只查询既有当前提交描述，不构帧、不推进任何状态。

| 新类型字段 | 表示 |
| --- | --- |
| sessionScope/resultVersion/contentRevision/layoutRevision/worldRevision/updateOrdinal | u64 当前画面来源修订；与 HostContext 的控制代数独立 |
| source | 新 enum：Prepared / RuntimeUpdate / Restore / Mutation |
| frame | optional 既有 Playback RuntimeFrame |
| presentationTick/workTick | optional i64 |
| factCursor/controlCursor/activationRevision | optional u64；不伪造初始来源 |
| hasFold | bool，实际来源存在 Fold 时为 true |

Runtime-only 可查询，GameplayOnly/Empty 按既有 `playback.session.empty` 拒绝；owner/reentry
沿既有错误边界。新名字不增加旧方法重载，不改 FrameSnapshot、HostContext、FeedbackObservation、
旧返回值、类对象布局或虚表；不暴露 Runtime/World 或新控制凭据。

## 3. 版本候选与门禁

按 [VERSIONING 的 0.x preview 规则和更新流程](../../../../guides/VERSIONING.md)，
上述经审查确认兼容的新增名字 API/类型，候选 SDK 从当前 `0.8.0` 升至 `0.8.1`。
这是本次实际最小消费者版本，不为 PD-4 或任何未来批次预留版本。
owner 已明确答复“接受新增只读 API 与 0.8.1，继续实施”。该接受只覆盖本批 API 与版本决策。

接受后必须 fresh/clean 构建 static Debug、Release、shared、candidate OFF；验证公共头 ASCII、
shared export/import、clean installed consumer、旧源码重建、最低 patch 请求以及过高 patch、
跨 minor/major 拒绝。复用旧 Snapshot/Digest golden，并新增 Pause drain、Resume、EOF、旧控制帧、
迟到 receipt、较旧最后成功画面、resize、资源换代、控制未知/Closing 和无保留画面的定向证据。
这些门禁是待执行事项，不是本记录的已通过结论。

## 4. 接受前的独立准备与实测（历史）

已完成但尚未接入宿主运行路径的两项内部 helper：

- `FrameObservationLedger::rebind`：保留同一 scope 的既有反馈、source tick 前沿和旧 attempt
  记录；只允许同源内容/资源/呈现端下的前向已确认控制，或严格的纯 clock segment 换段。
  迟到的旧 control receipt 仍匹配原 attempt，不作为新控制证明。
- `rematerializeRetainedSnapshot`：只复制已有 owning Snapshot 并按新 viewport 重算 projection；
  不读取当前 Runtime，不改变 object/view matrix/材质/visible 等原值。

新测试位于 `tests/pd/control_binding_tests.cpp`，编入既有 `cuexis_host_protocol_tests`。
`out/build/pd3c-debug` 的该目标构建通过；三例专项共 56 assertions，全协议测试共
38 cases / 908 assertions，通过。格式与 `git diff --check` 通过；文档检查为
455 Markdown / 20 candidate JSON/CXT。第一次全构建因新增测试的 include 路径错误中断，
修正为仓库相对路径后重建该目标成功；没有将这次目标构建描述为完整四配置验收。

以上为接受前的历史准备结果；owner 接受后已实施 §5，最终验证和独立审核见 §6。


## 5. 已接入的行为与维护边界

公共查询只映射 PD-3-b 的现有描述；没有新增另一套修订状态机或私有导出。新 API 返回的
sessionScope 命名软件来源，不替代 HostContext 的控制证明。内部 FrameEnvelope 的 T、frame、
Fold cursor 使用 optional；普通 Runtime-only 帧不再带伪造的零 Fold，Prepared 查询保留缺省。

D5 在调用设备前保留本次 owner 签发的不可变 attempt；成功后以无分配的 shared_ptr 移交
记录实际最后成功画面，随后才传 receipt。修改返回给调用者的帧副本不能改变实际提交内容。
每个当前 pending attempt 在设备调用前消费，重入、重复 present 和旧控制帧不能再次执行。
这只是现有单呈现端的本地保护，不是 PD-4 的消费者注册或额度机制。

冻结重绘由不可伪造的 RetainedPresentation handle 进入 D3，只复制原来源、成员和资源引用。
同 viewport 通过 aliasing shared_ptr 共享原 Snapshot；resize 从原 owning 值复制并重算 projection。
该分支没有 session.update、sampleGameplayPresentation、当前 World extract、新 membership 或 expiry
推进。允许较旧来源，但不会倒退正常采样的时间前沿。普通帧的历史 Snapshot 仍留在固定 ledger
中，普通交付仍有现有值复制；去除这些保留和额度管理属于 PD-4，不能据此宣称已解决容量耗尽。

Pause/Resume 的完整软件 fence 释放和确认后，ledger 同 scope rebind；EOF 只有合法换段证明后
更新绑定。Seek/其他表现身份重建控制轮换 scope 并撤销保留资格，Close 在清理前撤销。scope 容量
在有副作用之前检查。设备 instance/generation 与激活状态在新采样和设备调用前核对；无保留画面
使用既有内部 context_stale 拒绝，不伪造初始 RuntimeFrame。未知控制、busy、Suspended、Closing
禁止新 present。晚到旧 receipt 只匹配其不可变原记录；不会确认新控制或重开旧反馈义务。

新用例在 tests/pd 下，既有 RPA 测试可执行文件只负责注册。tests/pd/host_delivery_probe.cpp
仅为新的 optional 来源字段做必要编译适配：Runtime-only 期望 absent Fold，实际 RuntimeFrame
显式解引用；时延采样算法、次数、gate 不变。改动前源码与 SHA-256 已保存在本批 evidence 根，
冻结的旧 PD-0 注册、样本、旧构建与 PD-3-b evidence 不修改。本批不运行或重新接受正式性能窗口。

## 6. 最终验证与独立审核

最终 58 个实施/测试/构建输入指纹为
`abdf37111c38826b803d3e6d4ec36c561861d2f94d53d8247ea6844b1e980ed8`。
`out/pd3c-verification-20261019/implementation-inputs.zip` 保存输入；
`implementation-build-binding.json` 保存实际 Ninja 依赖、对象与最终链接产物的 hash/时间。
五个构建树的 generated SDK version 均已核验为 0.8.1。关键对象依赖非空且为 VALID，源码和
仓库头均早于对应对象；新控制、只读查询和 Player 调用点都参与实际编译。

| 最终配置 | 普通 CTest 选中 | 失败 | 明确跳过 | 暂存消费者/导入导出 |
| --- | ---: | ---: | ---: | --- |
| candidate static Debug | 1088 | 0 | 1 | 3/3 |
| candidate static Release / WX ON | 1088 | 0 | 1 | 3/3 |
| candidate shared Debug | 1073 | 0 | 5 | 6/6 |
| candidate OFF static Debug | 962 | 0 | 1 | 本批不单列 OFF 包消费者 |

普通回归排除了另行执行的 package/import gates 与独立性能脚本；不把此表描述为裸跑所有
注册 CTest。static 的一项 skip 为 Windows symlink 环境限制；shared 另有一项内部 parse
计数不可用和三项跨 DLL allocation 注入不可用的既有 skip。完整命令、统计和跳过名称见
`verification-summary.json` 及对应 `*-regression.log`。

新专项在三个 candidate 配置均为 public source query 1 case / 74 assertions、实际宿主保留
控制 4 / 204、ledger/resize 3 / 71；OFF 的 source query 为 1 / 44。覆盖真实 override 与 reload 对应的公开 Mutation/Restore 枚举、普通 Runtime 更新后
hasFold=false 仍保留已有激活身份、Pause/Resume 不推进修订、旧成功画面落后于 World、修改返回
副本不改 owner-issued payload、重复/旧控制 attempt 拒绝、resize 撤销旧 viewport pending
attempt、NoRetained、EOF、Seek、设备未知/换代与 Close 后 owning CPU 值存活。

Player 在独立 candidate Debug / WX ON 树实际编译链接通过，包括 player_execution.cpp；
没有用 headless 测试替代该调用点检查。未运行本批真实 GPU/window/audio smoke 或物理设备验收。

清洁 installed Playback-only consumer 请求最低 0.8.1 并实际调用新查询；既有 Gameplay consumer
继续请求 0.8.0 并重建运行成功。新包验证过高 patch 0.8.2、跨 minor 0.7/0.9 和 major 1.0
拒绝；额外直接核验冻结的真实 0.8.0 ConfigVersion 拒绝新查询所需的 0.8.1 请求。shared
导出/导入与 installed public header ASCII 检查通过，未增加私有导出。

复用冻结 PD-3-b 的公开消费者 probe，13 个 Snapshot 与 FrameDigest v3 的全部序列化结果
逐字节一致：7,878 bytes，SHA-256 为
`f4303f95f2b2de54993946f3f5ba6a3a91e28b7a8bb333427bc43029ea1c60c0`。
`source-query` 不改变同步 Snapshot 字段；原 probe 未重写 golden。125 个 PD-3-b evidence
manifest 文件均重新核验未改动。48 个 C++ 输入的 clang-format dry run、版本一致性、文档
检查（455 Markdown / 20 candidate JSON/CXT）和 git diff --check 通过。

开发期的 include、FrameDigest 比较、资源 enum、测试时钟回退及 installed consumer 局部变量
重名错误均已修复；消费者首轮失败与后续成功日志分别保存。接受前的较早编译日志曾被开发
runner 覆盖，原错误仍在对话工具记录；当前 runner 在重跑前将已有日志归档，不能把首轮失败
写成通过。另修正了新查询用 hasFold 判断激活身份缺省的错误，新增普通更新后保持身份的
public API 回归，并在最终源码下重新构建/运行矩阵。本批没有重试正式性能窗口。

新建无上下文 GPT-6-Luna 首轮独立审核为 FAIL：P2 指出 public source query 仅断言 Prepared/RuntimeUpdate，未验证 Restore/Mutation 的公开枚举映射。已通过实际 acquireHostOverride 与 reload 补上两者的公开断言；candidate 三配置现为 74 assertions，OFF 为 44。首轮 232 个证据文件及其 manifest 保存在 evidence 根的 review-round1/，该归档内的 manifest 路径以归档目录为根；当前工作证据将更新为补正后的版本。初审记录为 luna-initial-audit.md。

该新建代理以 fork_turns=none、model=gpt-6-luna 独立复核后给出最终 PASS，未发现新的可行动问题。代理逐项核验 58 个 live 输入、36 个差异文件、输入 ZIP、47 个对象/产物 hash 及全部 255 项 round2 证据；与首轮相比只有上述测试输入变化，生产输入保持一致。审核使用保存日志，没有另行重跑测试。最终记录为 luna-final-audit.md；review-round2/ 保存审核时的完整证据与 manifest（以该归档为根）。审核后的改动仅为完成状态与本段报告，实施指纹不变。PD-3-c 内部实施、本地适用门禁和独立审核完成。

## 7. 维护性与退出边界

协调沿既有 main/D3/D5 owner 与确认回执；没有增加线程或独立求值器。软件来源从唯一已提交
描述复制，交付绑定从完整控制证明派生；保留 handle 只由实际呈现成功的 owner 产生。
这使状态同步责任分别落在 SDK 提交描述、现有 fence 与 D5 成功调用上。CPU backing 共享，
Close/Seek/resize 的资格撤销集中在宿主入口，外部 owning 值可继续存活。设备未知、容量和
分配失败按既有错误/挂起路径报告，不能据超时制造新许可。

普通交付仍有值复制，固定 ledger 仍可能在容量耗尽时拒绝；消费者注册、紧凑历史、真实回收、
反馈 optional expiry 与压力撤销/恢复属于 PD-4。本批不接受生产预算、Release 性能收益、
多 Renderer、RPA 物理设备或 Stage 7 关闭；不授权提交、推送、PR、合并或发行。
