# B2 存储与寿命实施证据

状态：implementation evidence；B2 隔离存储/寿命设施已实施并通过定向验证，RPA 产品协议仍未完成

证据日期：2026-10-11（Asia/Shanghai）。本记录只覆盖 `tests/rpa/` 的 B2 test-only
设施和定向证据，不把它写成生产 archive、Recovery rev3、RPA profile 或 SDK API 的实施。

## 实施范围

本批次在既有 B1 fixture 上增加 `rpa_b2_fixture.hpp/.cpp` 和 `rpa_b2_tests.cpp`，继续使用
隔离的 `cuexis_rpa_tests` target。设施包括：

- `ChunkedAppendLog` 的等价模型：active tail 追加、固定 chunk 容量、发布时复制成功前缀，发布后的新追加不会改变旧结果。
- 自足 `ReplayEvaluation`：保存 archive identity、profile identity、Fact semantic、event codec、state schema、cut、horizon、event count、全部 replay records、完整 projection 和可选 fault。
- `FaultPrefix`：保存 stage、code、category、severity、faulted 和实际 record prefix；比较使用完整对象相等，不使用 score/hash 摘要替代。
- checkpoint key：同时绑定 archive number、log cut、horizon 和 profile identity；同 H 不同 cut、同 cut 不同 H 均产生不同键。
- generation/handle：active session 关闭后，旧 handle 被拒绝；此前返回的 owning result 仍可独立读取和 restore。
- 预留失败注入：append 在变更前失败，记录 stage、位置、rollback 和 retry 状态；下一次显式重试可成功。
- 独立寿命计数：append、publish、read、restore、复制记录数、active 峰值、最终 release 次数和 release 尾部操作数。计数状态由共享 owning token 保活，不借用已关闭的 store。

## 实际验证

工作树继续保留既有未提交改动。本批次新增代码只位于 `tests/rpa/`，并把 B2 源文件加入
`cuexis_rpa_tests`；没有修改 Playback、Session、World、设备或安装公共头。

使用现有 `mingw-headless-debug` 构建目录执行：

```text
cmake --build out/build/mingw-headless-debug --target cuexis_rpa_tests PASS
ctest --test-dir out/build/mingw-headless-debug -R '^B2 ' --output-on-failure PASS (11/11)
```

实际通过的 11 项：

| 测试 | 实际结果 |
| --- | --- |
| active tail 跨发布 | 首次结果保留 2 条记录；之后追加第 3 条不改变旧结果；复制计数为 2 |
| 已存在 active tail 的 cut 发布 | 3 条 active 记录按 `cut=2` 发布只包含前 2 条，active tail 仍保留 |
| 完整 ReplayEvaluation/fault prefix | 完整字段相等通过；修改 admission frontier 后比较失败，证明不是摘要比较 |
| checkpoint/archive/cut/H/profile | 同 archive 下不同 cut 或不同 H 的 key 均不相等，profile identity 保留 |
| 预留失败与重试 | append 位置 1 失败，`rolledBack=true`，active 记录仍为 1；显式重试成功 |
| publish 预留失败 | publish 前预留失败，记录 `publish` 阶段，active 记录不变 |
| 分配失败 | publish 前注入 allocation failure，记录失败位置与 `allocation` 阶段，active 记录不变 |
| Close 与旧 handle | Close 后旧 handle 返回 `closed`；owning result 仍有效，restore 成功 |
| read/restore | read 和 restore 各计数 1，恢复结果与原 owning evaluation 完整相等 |
| store 销毁后的结果寿命 | store 析构后 owning result 仍有效，archive identity 和全部记录可读 |
| 长历史峰值、读恢复与释放 | 128 条记录、chunk=16；peak=128，publish copy=128，长历史 read/restore 各 1，release=1，release tail operations=136 |

失败位置、重试/回退和释放数据均来自 fixture 的实际计数，不由测试名称或预设阈值推导。

## 边界和未执行项

本批次验证的是可控同线程 test-only 模型；未执行或不能由本批次证明的内容保持明确：

- 未实现 production chunk codec、Recovery envelope rev3、schema 2 wire reader/writer 或新 SDK minor。
- 未接入真实 PlaybackSession/World、RPA D2/D3 worker、audio owner、GPU/present 或实体设备。
- 未执行跨线程关闭、真实时钟 drift、EOF tail、队列容量预算、hosted CI、MSVC/Release 全量矩阵。
- 本批次没有把 test-only profile、复制量或 release 尾部操作数转成生产接受阈值；`releaseTailOperations` 是确定性操作计数，不是墙钟释放尾延迟。
- B3 时间/执行协议、B4 宿主资源提交、B5 部署设备和 F1–F4 收尾仍未执行。

本批次未提交、未推送、未创建 PR。
