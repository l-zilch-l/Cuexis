# S6-E3 资源身份、缓存与包原子发布

状态：in_progress（本地实现与 `debug-media-tools` 全量验证；同 SHA hosted 四平台证据待补。不是批次
退出、Stage 6 关闭、PR 合并或 owner acceptance）

日期：2026-09-25

本报告记录 S6-E3（资源身份、缓存、索引与 CXC 原子发布）的实现与本地证据。合同来自
[stage-06 plan.md](../../../stage_plans/active/stage-06/plan.md) §S6-E3 与 ADR 0042 §S6-D06；
本页不重新裁定 profile，不把本地结果当作 release 或 owner acceptance 证据。

## 1. 批次基线

| 项 | 事实 |
| --- | --- |
| 工作区 | `C:/Users/Zilch/.codex/worktrees/7596/Cuexis` |
| 分支 | `stage-06-workspace` |
| PR | #29，`OPEN`，未合并 |
| 起始顶端 SHA | `730d0d6`（E1/E2 收尾后的 green 顶端） |
| 显示版本 | 起始 `26.09.24-1`；收尾时按 Version Gate 日期滚动推进到 `26.09.26-1`（见 6.2） |
| SDK API | `0.7.0`（本批次没有改 SDK minor） |

本批次没有扩大 R5 §10.1 的允许消费清单，没有引入运行时脚本入口，没有让 `engine/animation/`
解析 JSON/CXC/CXT，没有改写任何 A2 golden，没有给任何产物加 epsilon、容差或平台分支，也没有把
媒体工具链入 Playback 或 Player。Playback 与 Player 既不链接媒体导入库，也不链接发布事务库。

## 2. 计划项对照

计划 §S6-E3 的四条要求与实现落点：

1. **分离四类身份**：`rawSourceIdentity`（原始编码字节 SHA-256）、`profileIdentity`
   （`mediaProfileIdentity()`，覆盖 profile 版本、解码器构建与输出相关构建选项）、
   `artifactIdentity`（canonical 产物 SHA-256）与资源 `assetId` 在 provenance 记录中各自独立，
   并有测试断言原始输入身份不等于产物身份。原始资源保留在作者侧：provenance 只记录
   locator、字节数与身份，不把原始编码字节或调试来源写进运行时资源闭包。
2. **缓存键包含影响结果的全部输入**：`MediaCacheKeyInput` 由原始输入身份、profile 身份、
   decoder 家族标识与输出相关构建策略（`minimp3-no-simd;fp-precise;contract-off`）构成，
   键是 domain `cuexis.media.cache.key.v1` 上长度前缀 preimage 的 SHA-256。命中时记录与请求
   逐字段复验、产物按内容地址重新哈希；损坏、被编辑或引用了不存在产物的条目一律以
   `media.cache.corrupt` 拒绝，`--rebuild` 才做显式离线重建。Playback 不链接该库，也没有任何
   静默再导入路径。
3. **单一发布点**：新增内部工具库 `cuexis_asset_publish`，把「先在 staging 生成并验证，再以
   一个明确发布点切换可见产物」实现为事务：不可变、内容寻址的 `generations/<identity>` 目录，
   staging 写入 + 独占创建 + `fsync`，marker 落盘后重新读取并逐条重算摘要，`fsync` 目录，最后
   用**一次** `rename` 切换；任何失败路径都清理 staging。并发 writer 由操作系统级独占锁拒绝
   （Windows `LockFileEx`，POSIX `flock`），锁在进程崩溃时由内核释放，因此重启恢复不需要
   stale-lock 超时；锁文件本身保留（存在与否不代表持锁）。
4. **同一批产物建立 v4 与 candidate 闭包**：`publishPackagePair` 从同一批条目构建两个包
   （candidate 闭包 = 共享闭包 + candidate 专属条目），两者都在触碰任何目标之前构建并通过
   生产加载器自校验；替换时先写独占临时文件、用 `CxcPackageLoader::loadFile` 复验字节并比对
   包身份，再备份/替换/必要时回滚，失败不覆盖上一有效包。

### 2.1 与既有实现的关系

- `CxcWriter` 只在内存中构建（没有临时文件、rename 或 fsync），`CxcPackageLoader::loadFile`
  要求 `.cxc` 后缀，包身份是整段 canonical 归档字节流的 SHA-256。发布事务因此建立在
  `cuexis_cxc` 之上而不是重复实现打包。
- 仓库里此前唯一带 fsync 的原子写是 `engine/project` 与 `engine/chart` 的私有实现，唯一进程间锁
  是 `engine/player_support` 用户偏好里的匿名命名空间 `ExclusiveFileLock`（从不删除、也不被任何
  发布路径使用）。E3 没有复用私有实现，而是把发布事务集中到工具库里，避免在引擎模块之间建立
  新的私有耦合。
- `engine/filesystem` 是只读模块，工具库不依赖它；`tools/asset_publish` 只依赖 `cuexis::core`
  与 `cuexis_cxc`。

## 3. 实现清单

新增：

- `tools/asset_publish/` — `include/cuexis/tools/asset_publish.hpp`、`src/asset_publish.cpp`、
  `src/publish_fs_internal.{hpp,cpp}`、`CMakeLists.txt`（目标 `cuexis_asset_publish`，别名
  `cuexis::asset_publish`，仅在 `CUEXIS_BUILD_MEDIA_TOOLS=ON` 时构建）。
- `tools/media_import/include/cuexis/media_import/media_provenance.hpp`、
  `src/media_provenance.cpp` — 四类身份与 provenance 记录（canonical 单行 JSON、固定键序）。
- `tools/media_import/include/cuexis/media_import/media_cache.hpp`、`src/media_cache.cpp` —
  身份复验缓存。
- `tools/media_import/src/media_json_internal.{hpp,cpp}` — provenance 与缓存记录共用的严格
  canonical JSON 读写器（未知键、重复键、畸形转义、非规范数字、尾部内容全部拒绝）。
- `tests/asset_publish/` — `asset_publish_tests.cpp`（15 个 TEST_CASE）、`CMakeLists.txt`。

改动：

- `tools/media_importer/src/main.cpp` — 新增 `--asset-id`、`--provenance-dir`、`--cache-dir`、
  `--rebuild`、`--generation-dir`、`--generation-id`；导入 → 身份 → provenance → 发布 →
  写缓存成为一条路径，worker 与 `--in-process` 仍产出相同字节与相同 info。
- `tools/media_import/CMakeLists.txt`、`tools/media_importer/CMakeLists.txt`、
  `tools/CMakeLists.txt`、`tests/CMakeLists.txt`、根 `CMakeLists.txt`
  （`CUEXIS_ACTIVE_TARGETS` 与 allowlist）。
- `cmake/VerifyMediaImporter.cmake` — 追加 E3 端到端门禁（见 4.2）。
- `tests/media_import/media_import_tests.cpp` — 追加 5 个 E3 TEST_CASE（provenance 四身份、
  严格编解码、缓存命中复验、损坏缓存拒绝、缺失原始资源与缺失缓存条目的区别）。

发布事务的 marker 是行格式而不是 JSON：它只由该库读写，行格式让「重新读取并逐条重算摘要」的
自校验不必再引入第三个 JSON 解析路径。generation 身份是 domain `cuexis.generation.closure.v1`
上 label、运行时闭包列表与 provenance 列表的长度前缀摘要；provenance 记录存放在
`provenance/` 子目录，不进入运行时闭包列表。

失败注入沿用仓库既有先例（`CUEXIS_CXC_PACK_FAIL_*` 风格）：`CUEXIS_ASSET_PUBLISH_FAIL_*`
只在 `CUEXIS_BUILD_TESTS=ON` 的构建里编译进库，用于覆盖 staging 失败、发布前失败、替换前失败与
指定目标替换失败。

## 4. 本地验证

### 4.1 测试

```text
cmake --build --preset debug-media-tools
ctest --preset debug-media-tools --no-tests=error
```

- `cuexis_asset_publish_tests`：15 个 TEST_CASE、176 条断言全部通过，无编译警告。覆盖：generation
  发布/幂等/内容寻址/标签参与身份、非法批次不落盘、注入失败后 staging 清理且旧 generation 完好、
  被篡改的已发布 generation 以 `asset.publish.validation_failed` 拒绝且不覆盖、重启恢复清理
  staging 与临时文件、显式 adopt 不改写工程入口、同进程与**跨进程**发布锁 busy、包发布与替换、
  替换失败保留上一有效包、缺失目标目录 fail closed、同一批次的 v4+candidate 双闭包、candidate
  闭包非法时两个目标都不动、candidate 替换失败时 v4 回滚。
- `cuexis_media_import_tests`：23 个 TEST_CASE、615 条断言全部通过（含 5 个新增 E3 用例）。
- `cuexis_media_importer_tool_tests`：CLI 门禁通过，含 4.2 的 E3 用例。

跨进程锁用例的形态：父用例以 `[.publish-lock-child]` 重新启动同一个测试二进制，子进程持有锁并
写 ready 文件，父进程在同一路径上取锁必须得到 `asset.publish.busy`；子进程退出后锁立即可用。
这条用例证明的是真实的进程间互斥，而不是同进程内的计数。

### 4.2 CLI 端到端门禁（E3 部分）

`cmake/VerifyMediaImporter.cmake` 追加的断言：

- `--asset-id` + `--provenance-dir`：sidecar 文件名是 `textures%2Fchecker.provenance.json`
  （AssetId 单段化，不是路径），记录里的 `rawSourceIdentity` 等于输入 fixture 的 SHA-256、
  `profileIdentity` 等于 golden 里的 profile、`artifactIdentity` 等于 golden 的产物摘要，
  且原始输入身份不等于产物身份。
- `--generation-dir` + `--generation-id`：`generations/<64 位十六进制>/` 下运行时条目
  `textures/textures%2Fchecker.texture.bin` 的 SHA-256 等于 golden 产物（完整、可用），
  `provenance/` 下有 sidecar，`cuexis.generation` marker 存在，`staging/` 不留残余；重复发布
  同一批次不新增 generation。
- `--cache-dir`：第一次运行写入一条记录；第二次运行报告 `"cached":true` 且服务的产物身份正确；
  记录被损坏后运行以 `media.cache.corrupt` 和非零退出码拒绝，已发布产物不被改写；记录里的
  profile 身份被替换为旧值后同样以 `media.cache.corrupt` 拒绝（旧 profile 不复用、不静默重建）；
  `--rebuild` 显式重建后恢复为一条有效记录。
- 缺失原始资源：非零退出且不发布任何文件。
- 重启恢复：generation 根目录预先放入一个空的 `.cuexis.publish.lock` 和一个被遗弃的
  `staging/` 子目录，发布仍然成功，且产出的 generation 身份与串行运行完全相同。

### 4.3 其他本地检查

- `clang-format --dry-run --Werror`：新增与改动的源文件全部通过。
- `python -B tools/check_docs.py` 与 `git diff --check` 通过。
- shader-tools OFF：`debug-media-tools` 在 `CUEXIS_BUILD_SHADER_TOOLS=OFF` 下配置、构建、测试
  全部通过；`developer-tools` OFF 的默认 `debug` preset 不构建媒体工具，Playback 安装消费者
  不依赖任何媒体解码库（`cuexis_asset_publish` 与 `cuexis_media_import` 都不在安装闭包里）。

## 5. 实现过程中发现并修复的缺陷

- **临时文件必须保留扩展名**：`uniqueSibling` 最初把后缀追加在文件名之后，得到
  `static.cxc.cuexis-publish.tmp.<token>`，被 `CxcPackageLoader::loadFile` 以
  `cxc.archive.invalid: CXC file locator must use .cxc` 拒绝。现在改为
  `<stem>.<role>.tmp.<token><ext>`，临时文件仍能被生产加载器接受。
- **pair 请求的闭包形态**：v4 包不能包含只有 candidate 扩展声明的条目，否则 v4 包自身就以
  `cxc.entry.unlisted` 失败。`PackagePairRequest` 因此区分共享闭包 `entries` 与 candidate 专属的
  `candidateEntries`，candidate 闭包是两者的并集。
- **缺失目标目录要在取锁之前判定**：原实现先取发布锁再检查父目录，父目录不存在时锁的创建失败会
  被报成 `asset.publish.io_failed`。现在先校验父目录，返回 `asset.publish.invalid_request`。
- **worker 路径的重复发布**：bounded worker 路径自己会把临时文件提升为已发布产物，父进程随后
  又用（当时未读回的）空字节再次发布，触发 `media.publish.immutable_conflict`。现在
  `ImportOutcome::published` 记录产物是否已发布，只有需要时才读回字节。
- **provenance 目录前缀重复**：发布库已经为 provenance 记录加上 `provenance/` 前缀，CLI 又加了
  一次，导致记录落到 `provenance/provenance/`。现在记录路径相对 provenance 目录给出。
- **portable 路径字符集**：运行时条目路径需要允许 `%`（AssetId 单段化结果），同时不应允许空格
  （需要引用的路径不算可移植）。`isPortableRelativePath` 相应调整。
- **`REQUIRE_MESSAGE` 在 Catch2 v3.15.2 不可用**：测试改用 `requireOk` 辅助函数，在失败时输出库
  诊断而不是只输出布尔值。
- **MSVC 弃用警告**：`getenv` 在库与测试里都改为 `_dupenv_s`（POSIX 保留 `getenv`），
  `debug-media-tools` 保持零警告。

## 6. Hosted 四平台结果

### 6.1 四平台矩阵

待补：本批次提交后需要与 E1/E2 相同的四平台证据（Linux Quality GCC media-tools、Linux Quality
Clang ASan+UBSan media-tools、Windows MSVC、Windows MinGW），并确认新增目标在四个平台都编译、
注册与运行。本页在拿到该证据之前只声明本地结果。

### 6.2 Version Gate 日期滚动

本批次第一次推送时 `Version Gate` 失败，与 E3 实现无关，是日期滚动：可信 UTC 日期已进入
`2026-09-26`，而候选版本仍是 E1/E2 收尾时滚动的 `26.09.24-1`，检查器报
`version.release_date.stale: candidate date 26.09.24-1 is before trusted UTC date 2026-09-26`。
处置方式与 E1/E2 相同：用 `tools/update_version.py 26.09.26-1` 推进日期版本
（`cmake/CuexisVersion.cmake` 与 `vcpkg.json` 同步），并在本机用与 CI 相同的参数复跑
`tools/check_version_gate.py` 确认通过。这次推进只改日期构建身份，`CUEXIS_SDK_API_VERSION` 仍为
`0.7.0`，不影响媒体 profile identity、canonical golden 或发布事务的 identity 计算。

## 7. 未完成项

- hosted 四平台证据（6）。
- E3 验收里「完整产物可由 clean staging Player 与宿主消费」在本批次由
  `cuexis_asset_publish_tests` 的 v4+candidate 双闭包用例与既有 CXC/Player 门禁覆盖；把
  importer 产出的媒体资源接进一个真实工程的端到端用例仍属 C4。
- 次要清理项（E1/E2 报告已记录）：`tools/media_import/CMakeLists.txt` 里
  `${MINIMP3_INCLUDE_DIRS}` 是未定义变量的空展开。

## 8. 边界

本页是 S6-E3 的实现证据，不是批次退出、不是 Stage 6 关闭、不是 PR 合并，也不构成 owner
acceptance。`cuexis_asset_publish` 与 `cuexis_media_import` 都是默认 `OFF` 的内部工具库，不进入
SDK 安装闭包；Playback 与 Player 不链接它们，也不在运行时启动导入或发布路径。
