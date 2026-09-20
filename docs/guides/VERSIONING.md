# Cuexis Versioning

状态：已接受

更新日期：2026-09-20

## 格式

规范版本：

```text
yy.mm.dd-v
```

日期使用 UTC。`v` 是同一 UTC 日期内从 1 开始人工递增的构建号。

构建后缀：

```text
正式      无后缀
开发      -dev
测试      -test
内部      -internal
实验      -exp.<name>
```

## 单一来源

`cmake/CuexisVersion.cmake` 保存年、月、日和 build。其他 CMakeLists 不重复手写版本。
`tools/update_version.py` 是人工更新入口，负责在一次事务中同步该文件与 `vcpkg.json`。

CMake `project(VERSION)` 使用无前导零的三段数字，例如 `26.8.1`。完整显示版本单独生成，例如 `26.08.01-1-dev`。

`vcpkg.json` 的 `version-string` 保存不带构建类型后缀的规范版本，例如 `26.08.01-1`，因为同一源码可以同时构建 Debug 和 Release。

## 更新流程

```text
取得目标 UTC yy/mm/dd
-> 若日期变化，将 build 设为 1
-> 若同一日期再次发布构建，人工递增 build
-> 运行 python -B tools/update_version.py yy.mm.dd-v
-> configure 生成 version.hpp
-> 运行版本一致性测试
```

只检查当前文件而不写入：

```powershell
python -B tools/update_version.py --check
```

## 生成头

`${binaryDir}/generated/cuexis/version.hpp` 至少提供完整字符串、三段日期、build 和 suffix。
旧的公开 `cuexis::version::hour` 为保持 0.5.x 源码兼容暂时保留，并固定为 `0`；它不再属于
构建身份。生成头不提交源码控制。

## 验证

测试检查：

```text
CMake 三段版本与版本配置一致
月份、日期和 build 合法
vcpkg version-string 与规范版本一致
suffix 只来自允许集合或 exp.<name>
窗口标题和启动日志使用完整显示版本
```

版本错误必须使 configure 或测试失败，不能只产生警告。

## SDK、ABI 与内容版本

项目显示版本不替代独立的兼容性版本：

```text
Cuexis display version     仓库/发行构建身份
C++ SDK API version        CMake package 与源码兼容版本；当前 static/shared preview 为 0.7.0
C ABI version              稳定 shared-library 二进制契约，阶段 12 首次冻结
Chart/Project/Asset format 各自持久化 format + version
ReplayData format          阶段 11 冻结的独立持久化版本
simulationVersion          粒子等确定性算法版本
```

当前 `CUEXIS_SDK_API_VERSION` 是 CMake package version 的唯一来源，生成头通过
`cuexis::version::sdkApi` 暴露，package config 同时设置 `Cuexis_VERSION` 和
`Cuexis_API_VERSION`。`Cuexis_VERSION_DISPLAY` 继续保存完整构建身份。preview 使用
`SameMinorVersion`：`0.7.x` 内允许满足不高于已安装版本的请求，minor 或 major 变化必须由
consumer 显式接受。日期构建号变化不改变 `find_package` 兼容性。

阶段 1D 的 SourceClockSample、RuntimeTimeline、Prepared Playback 和 Audio package components
曾将 preview 提升到 `0.2.0`。阶段 1E 的 PlaybackSource 构造边界、FrameDigest 与 static/shared
package linkage 契约进一步把 `CUEXIS_SDK_API_VERSION`、package version、生成头和 external
consumer 同步提升到 `0.3.0`；后续不兼容修改必须再次提升 minor，禁止只修改其中一个版本来源。

ADR 0033 已将同工具链 C++ shared preview 纳入阶段 1E。`0.3.0` 已实现 Playback 的 source
构造边界与 package linkage 契约；static/shared 使用同一 API minor 和公共 target 名，但不能在
一个 install prefix 混装。`vcpkg.json` 继续只记录日期构建版本，不复制 `0.3.0`。

阶段 2 的 `FrameSnapshot` Visibility/Material 字段、FrameDigest version 2、capability 查询和
Chart v3 可观察合同是不兼容的公共结构变化，因此 ADR 0036 将 static/shared preview 提升到
`0.4.0`。`CUEXIS_SDK_API_VERSION`、生成头、package version 与 external consumer 必须保持
同步；`vcpkg.json` 仍不复制 SDK API 版本。

阶段 3B 新增 Portable Presentation Profile v1 public values、candidate manifest/token 和 owning
resource acquisition，因此 preview 已提升到 `0.5.0`。阶段 3C 已在同一 public minor 中补齐
FrameSnapshot resource refs 与 FrameDigest v3，没有重复提升版本。

Stage Chart Format Update 的 CFU-E0 接受了 owning project-document source、CXC file/memory factory、
prepare options 和 semantic identity observation 的 additive API 方向。CFU-E1 首次落下 public source
type/factory 并将 preview、生成头、package config、version rejection 与 static/shared consumer 同步
提升到 `0.6.0`；E2/E3 将在同一已批准 minor 中完成其余冻结 surface。Stage 5 S5-C 新增公开
Presentation 类型 `Shader` / `ParameterizedMaterial`，因此 preview 提升到 `0.7.0`。

shared `0.x` 只支持使用匹配 SDK minor、编译器工具链、标准库、运行时、架构和 Debug/Release
配置重新构建的 consumer。CMake `SameMinorVersion` 继续表达源码/package 请求兼容，绝不表示
可以替换一个已部署 shared binary 而不重新编译 host。C ABI version 仍不存在，直至阶段 12 在
Judgement/Replay 和完整生命周期证据基础上冻结。

升级项目显示版本不得隐式升级 SDK API、内容格式或 ABI。安装包必须提供可查询的显示版本、
SDK API 版本和已启用组件；稳定 C ABI 在阶段 12 建立后再提供独立可查询版本。不兼容 API/ABI、
未来内容版本和不支持的 ReplayData 版本必须稳定失败。

## SDK 版本递增规范

本节是 SDK 版本选择和更新流程的统一规范；阶段计划引用本节，不为后续阶段预留版本号。
SDK 使用 `major.minor.patch` 三个非负整数，不是小数；例如
`0.9.0 -> 0.10.0 -> 0.11.0`，minor 和 patch 不设一位数字上限。
SDK 版本与 Stage 编号、日期构建号及内容格式版本均不绑定。一个阶段可以不升级 SDK，
也可以包含多次已批准的 SDK 发行；不得为了等待 Stage 12 而压低 minor。

### 0.x preview 的选择规则

| 变化 | SDK 版本处理 |
|---|---|
| 仅文档、内部重构、测试或构建身份变化，公开合同未变 | 通常不改 SDK 版本；显示版本仍遵守其自身规则 |
| 恢复既有合同的兼容修复，或经审查确认兼容的新增名字 API/类型 | 可递增 patch；必须保持旧 consumer 的源码及可观察合同兼容 |
| 不兼容的公开布局、签名、虚表、枚举语义、默认入口/行为或 capability 合同变化 | 必须递增 minor，patch 归零；先批准合同和迁移决策，再实现/发行 |
| 一组经批准的公共合同发行，即使全部为 additive | 可递增 minor，patch 归零；记录发行边界与理由，不强制所有新增 API 都升 minor |
| 正式稳定 SDK | 仅在 Stage 12 稳定性与 ABI 验收后进入 `1.0.0` |

以上是本项目 **0.x preview** 政策，不宣称 additive API 使用 patch 符合稳定 1.x 的版本规则。
“additive”不能只按名字或字段数量判断：必须验证重载解析、聚合初始化、继承实现、
默认行为和旧 consumer 的可观察结果；无法证明兼容时不得用 patch 掩盖风险。
已经发行的版本号及其合同不得重新分配，不得回退版本，也不要求每次提交都增加 SDK 版本。
同一已批准发行的分批实现可以共用目标版本，但不得修改已经发行的该版本合同。

`SameMinorVersion` 只检查 package 请求，不检测功能是否存在。例如使用 `0.7.1` 新 API 的
consumer 必须请求至少 `0.7.1`，不能仅以 `find_package(Cuexis 0.7 ...)` 声称满足最低要求。
内容能力仍须按 capability/格式合同检查；experimental 构建标记不代表正式支持。
preview 的源码兼容不代表 shared binary 可直接替换，旧 host 仍须按受支持配置重新构建。

### SDK 更新流程与验收

1. 以实际集成基线和最近已发行合同盘点公共差异，包含期间其他阶段交付；列出最低 consumer
   版本、兼容性结论和迁移影响。不能从旧计划中的历史版本直接推算目标。
2. 在对应阶段版本决策中记录具体目标、patch/minor 理由和验收矩阵，经 owner 接受；
   不兼容变更须先接受相应 ADR。若偏离已冻结目标，先重开该决策并同步依赖计划。
3. 在 API 实现及门禁落地时更新 `cmake/CuexisVersion.cmake` 的
   `CUEXIS_SDK_API_VERSION`，同步 API 文档、consumer 最低版本和版本拒绝测试。
   package config 和版本头从单一来源生成，不另设手写版本副本。
   `tools/update_version.py` 只更新日期构建身份；`vcpkg.json` 不填写 SDK API 版本。
4. 版本变更使用 fresh configure 和 clean build；验证生成头、安装 package version 一致，
   static/shared clean staged consumer 通过，并覆盖旧源码重建、最低版本请求、
   过高 patch 和跨 minor/major 请求拒绝。breaking minor 的旧 consumer 按批准的迁移合同验收，
   不伪造“无需迁移”的兼容承诺。
5. 发行记录写明实际 SDK 版本、显示版本、对应 SHA、兼容/迁移结论和验证证据；
   更新当前状态及后续阶段接手基线。不能仅因版本号已改就宣称阶段或 ABI 已完成。

以上是规范与验收要求，不表示全部已由自动化强制执行。
Stage 12 必须另行冻结 `1.x` 的 API/ABI 兼容、弃用和版本递增政策及其测试；
在此之前，`0.x` minor 可以持续增长，不能从 `0.9.x` 自动进位到 `1.0.0`。

## Stage 6 已冻结的版本方向（尚未实施）

[ADR 0042](../adr/0042-stage-6-productization-boundaries.md) 将 Stage 6 SDK 目标冻结为
`0.7.1`，仅用于新名字的源兼容增量 API；不得改变已有公开布局、签名、虚表、枚举含义或
默认入口语义。现有实现仍为 `0.7.0`，版本代码在对应 API 实现及 consumer 门禁落地时同步更新。
旧 consumer 需要重新构建，patch 号不承诺 shared binary 可替换。需要不兼容变更时必须重新
裁定 minor 和 Stage 8 接手版本，不能在 `0.7.1` 内偷偷破坏契约。`0.7.1` 是以当前
`0.7.0` 基线及上述兼容条件为前提的目标，不是可以覆盖其他已发行版本的固定配额。
Stage 8 不预留 `0.8.0`；正式 v5 发行版本按届时实际基线（包括 Stage 7A）与公共合同差异
重新裁定，并在发行前落实为具体版本、consumer 要求和验收记录。

experimental candidate 是独立构建/安装标识，不是 SDK minor 或正式内容能力。
生产与实验安装树不能混用；具体 opt-in 和 consumer 配置规则见 ADR 0042。
同一 ADR 固定 Stage 6 的显示版本门禁方向：受保护 `master` 最新 SHA、UTC 日期、
同日 build 精确加一、跨日归一、docs-only 不豁免和历史 SHA 复验分离。
S6-B1 已落下 `tools/check_version_gate.py`、独立 focused tests 和
`.github/workflows/version-gate.yml`。比较器负责规范四元组、trusted UTC、祖先关系、
manifest/CMake 一致性和 SDK API 不变性；workflow 在 PR、merge queue、合并后 push 与历史复验
中从 trusted baseline materialize checker，不从候选树回退使用 checker。首次装配若基线缺少
checker/test/workflow 文件会明确失败并记录 `version.bootstrap.required`，必须由 owner 审查
bootstrap 例外及保护规则后才能建立受保护基线。

### S6-B1 合并与发行 checklist

本地只证明脚本合同和当前文件一致，不能替代 hosted 保护证据：

```powershell
python -B tools/check_version_gate_tests.py
python -B tools/check_version_gate.py --check-current
python -B tools/update_version.py --check
```

受保护 `master` 的 PR/merge queue 检查使用事件提供的目标分支基线和 `github.sha` 候选最终树，
要求完整历史、trusted baseline checker 和同日精确递增；合并后 push 只作防漏审计。docs-only
也必须更新日期版本。历史复验必须传入已记录的 baseline、candidate 和 UTC 日期，只证明历史门禁，
不产生新的发行版本。

发布前还必须记录对应 SHA、显示版本、SDK API 版本、静态/共享安装 consumer 和 fresh
configure/clean build 证据；版本变化不隐式升级 SDK API、内容格式或 ABI。仓库保护未启用时，
只能报告“脚本完成、门禁未启用”，不能把本地通过写成 B1 退出证据。
