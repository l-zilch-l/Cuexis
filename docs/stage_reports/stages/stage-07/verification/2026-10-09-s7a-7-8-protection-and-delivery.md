# S7A-7/8保护配置与受限交付收口

状态：dated verification；受限交付，S7A-8.4保留来源门禁

证据日期：2026-10-09（Asia/Shanghai）

## 授权与交付边界

owner要求完成7/8剩余工作及提交推送，随后明确授权一并处理保护配置。
这覆盖本次保护配置修改，不授权合并、发行、迁移仓库、购买服务或接受生产阈值。
基线为stage-7、PR #32、HEAD `c4b1f68db0d809b9a9c0c27b37d53538423417ff`，
master为 `3076948ce403ea0da6fbe389835b6a4c9defbcd3`。现有未提交文档与审批工具完整纳入交付。

本轮产品代码、公共API和版本输入没有变化；交付Stage 7-RPA分册归属、九行余项核对、
审批助手和保护证据。审批工具固定7890代理、直接发布并回读、语义去重、POST不盲重试、
保留错误日志；CMD窗口在失败后持续保留。架构/正式命名/设备同步仅规划，未实施。

## 已生效的保护

创建ruleset `24799746`，名称 `Cuexis master PR and version integrity`，
仅作用于 `refs/heads/master`，enforcement为active，bypass_actors为空。
API回读 `current_user_can_bypass=never`，有效分支规则包括：

| 规则 | 配置 |
| --- | --- |
| pull_request | 必须通过PR更新master；解决review thread；正式review数量为0，兼容同作者owner评论审批 |
| required_status_checks | `Version advancement (pre-merge)`，integration_id15368，strict=true |
| non_fast_forward | 禁止强推 |
| deletion | 禁止删除 |

既有classic protection的strict、enforce_admins及App绑定保留。没有新增绕过者或移除必需检查。
通过GET ruleset、有效master规则和rulesets列表三项回读确认配置已生效；没有为测试而推送master。

## S7A-8.4来源门禁为什么仍未退出

仓库 `l-zilch-l/Cuexis` 为public个人仓库，owner.type为User；当前账号有admin权限，
但没有组织。按官方REST schema构造指定 `.github/workflows/version-gate.yml` 来源的workflows规则，
分别使用ref+sha和仅ref，两次POST均返回HTTP 422；没有创建任何workflows规则。

```json
{"message":"Validation Failed","errors":["Invalid rule 'workflows': Invalid parameter workflows: Workflow error at index 0: "],"status":"422"}
```

官方文档将ruleset workflows配置定位于组织/企业级；普通必需status check不区分workflow、
matrix或event来源。因此当前ruleset能保证PR和App/context条件，不能保证不可伪造的workflow来源。
官方来源：[workflows规则](https://docs.github.com/enterprise-cloud@latest/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets#require-workflows-to-pass-before-merging)、
[status checks来源边界](https://docs.github.com/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/troubleshooting-rules#troubleshooting-required-status-checks)。

owner明确选择“保持个人仓库，暂保留S7A-8.4未退出”。这不是接受来源例外，
也不将App/context绑定等同于可信workflow。后续若启用组织级required workflow或独立可信App，
仍需专门任务、真实配置及最终SHA证据；当前不迁移或新增外部服务。

## I78-5证据与交付

7.1–7.5、8.1–8.3的C78/R78合同、实现、fixture、人工golden、独立oracle、故障注入、
完整结果及平台命令继续由[10-08联合报告](../implementation/2026-10-08-s7a-7-8-entry-assembler-progress.md)
逐行维护；[10-09当前SHA核对](2026-10-09-s7a-7-8-current-gate-audit.md)绑定c4b1f68的三平台PR与
Version Gate成功。它们不证明本报告所属新提交已经通过hosted。
本报告所属提交是本轮最终文档/工具交付，最终SHA及推送后审批/CI留在桌面接手和
`out/s7a78-closeout-20261009/final-delivery.json`，不为回填自指SHA再追加报告提交。

SDK0.7.1兼容差异证明保持；执行 `tools/update_version.py 26.10.09-2` 后版本输入无差异，
因此没有新的version-change fresh/clean-first构建。此前版本变化的真实构建证据继续按其SHA保留。
推送后新SHA的审批及hosted核对独立记录，旧成功不代替新提交检查。

## 验证和未执行项

本轮运行Windows PowerShell5.1审批助手离线正负例、原生CMD持久窗口/带空格路径故障例，
docs、status/section/target、version gate与版本一致性检查，原始命令与输出见
[证据ZIP](2026-10-09-s7a-7-8-protection-and-delivery-evidence.zip)。
保护请求/原始422/成功ruleset和三个回读均在该归档；未保存认证token。

未执行新的C++ Debug/Release、ON/OFF、headless、安装static/shared、MinGW/Linux或设备矩阵；
本轮未更改这些输入。新SHA hosted推送后待回填。GPU/window/audio/真实键盘与端到端同步
归7-RPA继续保留旧失败或未执行，不能借本轮治理配置标记通过。
S7A-8.4未退出；S7A-9只保留计数/测量/handoff准备，不接受预算或关闭Stage 7A。
