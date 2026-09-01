# 260829 Full Review 整改第 0 批

状态：completed；2026-08-29 本地整改与门禁完成

本报告记录 Full Review 第 0 批（B0）在分支 `260829-full-review` 的实现证据。原始审查报告
`260829-full-review.md` 与 `CURRENT_STATUS.md` 未修改；本报告不改变 Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`eef3f4702b13d2942171f8d340024620dd10cfeb`
- 实现分支：`260829-full-review`
- Finding：CH-01、PB-08、CM-03、AP-01、AP-02、CM-01、AP-06、RT-02（文档侧）、AP-19
- D1-D6 决策门：均无独立 owner/spec acceptance；D1-D5 保持 unresolved，identity 迁移不实施。

## 实现

- Chart v4 非法空/超长 mask property 产生稳定 `chart.animation.mask_conflict` diagnostics，并整体拒绝 Layer/Instance；合法顺序与 identity/digest/canonical 不变。
- Playback 五个 owning-copy 方法捕获 `bad_alloc`、`length_error` 及其他异常，转换为既有 Playback 错误；HostOverride 失败不注册半个 token，active state 与下一帧保持不变。
- HostClock 公共头冻结 single-owner、非线程安全及 snapshot 并发读取限制；完整跨线程同步仍延期至后续批次。
- BUILDING、AGENTS、RUNTIME_SESSION 文档事实修正；删除空 `tests/particles/.gitkeep`。
- 删除无消费者的 `CUEXIS_PLAYBACK_EXPORT_TARGETS` 死变量；安装导出逻辑未变。

## 验证

- Debug fresh configure：通过（MSVC 2026 x64）。
- Debug clean build：通过。
- 聚焦测试：Chart `[ch-01]` 18 assertions、Playback `[pb08]` 36 assertions（四个 owning-copy 查询）、Audio HostClock 5 assertions，全部通过。
- CTest 语义聚焦：12/12 通过；架构与 external consumer 门禁：8/8 通过。
- `cuexis_format_check`：通过。
- `tools/check_docs.py`：通过（172 Markdown、20 candidate JSON/CXT）。
- `tools/update_version.py --check`：通过（`26.08.01-1`）。
- `git diff --check`：通过。

## 保留合同与残余风险

FrameDigest v1-v3、canonical bytes/order、identity、Playback 公共观察面、安装边界、公共头 ASCII、
candidate/active rollback、owner-thread 合同及 runtime-script 无限期延后边界均保持不变。
RT-02 代码侧零分配优化、HostClock 跨线程同步、identity 迁移及其他批次任务仍按计划暂缓。`acquireHostOverride`
的 fault-injection 全分配点探针在 MSVC Debug 下不稳定并未纳入测试套件；实现已覆盖其 reserve、映射、ownerId
拷贝和注册路径的异常转换，后续应补充稳定的定向探针。

实现 SHA 以本地提交为准；本批未执行 push。Hosted Linux/MinGW 门禁需在相应 hosted 环境重验。
