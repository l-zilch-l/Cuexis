# Stage Chart Format Update CFU-C3 CXC Report

状态：completed implementation checkpoint

快照日期：2026-08-12

基线分支：`stage-ChartFormatUpdate`

基线提交：`c1b3a29 Fix GCC 13 reader diagnostics`

权威计划：[Stage Chart Format Update 实施计划](../stage_plans/stage_chart_format_update_implementation_plan.md)

## 1. 结论

CFU-C3 已完成内部 `cuexis_cxc` archive/package 基线，并完成本地可执行的 CXC Reader/Writer、
closure、fixture 和静态分析门禁。当前状态保持为：

```text
CFU-C0/C1/C2/C3 complete; CFU-C4 next
```

这里的“complete”只表示 CFU-C3 实现检查点关闭，不表示完整 CXC 产品支持、公共 CXC package
API、Playback 接入或 Stage Chart Format Update 完成。CXC pack/validate/unpack 工具仍属于
CFU-C4，未在本检查点实现。

## 2. 交付范围

### 2.1 内部 target 与边界

- 新增内部静态 target `cuexis_cxc`，导出名为 `InternalCxc`。
- 依赖方向限定为 Core、Content、Filesystem、Project、Chart、JSON support 和 minizip-ng。
- CXC 不依赖 Playback、Runtime、World、Render、Audio、SDL、OpenGL 或 host SDK。
- CXC 头文件、archive 类型、JSON DOM、ZIP offset 和文件句柄不进入公共安装组件。
- CXC 的 static/shared 安装和符号检查规则已写入 CMake/package verification；shared package
  不应暴露 `Cuexis::Cxc`、`InternalCxc` 或 `MINIZIP::`。

### 2.2 ZIP32 Stored envelope

Reader 在 minizip-ng extraction/CRC 检查前执行有界 envelope validation。Writer 固定输出：

```text
manifest-first
single-disk ZIP32
Stored method (0)
flags 0
DOS epoch 1980-01-01 00:00:00
version needed 10
version made by 0x000A
no extra fields, comments, padding or trailing bytes
```

Reader/Writer 对 local header、central header、EOCD、entry range 和总长度执行 checked arithmetic
和一致性检查。拒绝规则覆盖：

```text
ZIP64 sentinel / ZIP64 EOCD / locator / extra field
compression, encryption, data descriptor and multi-disk
directory, symlink and special-file entries
local/central metadata mismatch
overlapping ranges, invalid EOCD span and trailing bytes
```

### 2.3 Manifest、路径与项目闭包

- Manifest v1 的 format、version、project、entries、requiredExtensions 和 extensions 均按
  CXC contract 验证。
- Entry path 使用 portable ASCII `/` 路径；拒绝 parent、absolute、backslash、保留名称、
  尾随点/空格、重复路径、ASCII case-fold 冲突和 file/descendant 前缀冲突。
- 校验 entry size、CRC32、manifest SHA-256、manifest order、listed-byte budget 和 archive
  entry budget。
- 读取并验证 ProjectConfig、所有声明 Asset Index、Asset source/dependency、入口 Chart、
  Chart 引用的 CXT 以及资源闭包。
- Asset Index 的声明记录全部保留；不会因入口 Chart 当前未引用而裁剪 Asset Index。
- 隐藏 payload、缺失依赖、root overlap、Asset Index/Chart/CXT source alias 和 project-document
  closure 冲突均拒绝。

### 2.4 Owning package 与内容域

- `CxcPackageLoader::loadFile` 和 `loadMemory` 均在验证后发布 owning package。
- memory loader 接管输入 bytes；调用方释放原始 buffer 后，package 仍可读取内容。
- file loader 限制 `.cxc` locator，并对文件大小、regular-file、root containment、读取期间变化
  和完整读取执行检查。
- `CxcContentProvider` 只提供 manifest/Asset Index 声明的 Asset source bytes。
- 入口 Chart 与 CXT 通过独立的 `projectDocuments()` 表进入 typed prepare 边界，不伪装成 AssetId
  或通过任意 archive path 裸读。
- `CxcPackageIdentity` 定义为精确 `.cxc` bytes 的 SHA-256；它与跨 source 的 semantic identity
  和 provider revision 分离。

### 2.5 Writer 与异常边界

- Writer 对 entry path、entry count、entry size、listed bytes、manifest 和 extensions 先做预算
  与结构检查。
- Writer 生成 canonical manifest-first archive 后立即重新加载并完整验证，再向调用方返回 bytes。
- 非 OOM 异常转换为 `cxc.internal.failure` 诊断；`std::bad_alloc` 继续传播。
- 生成、验证和 package publication 失败时不发布部分 package state。

## 3. Fixture 与测试覆盖

### 3.1 Committed fixtures

生产 Writer 生成并提交：

```text
golden/cxc_v1_v4_cxt.cxc
golden/cxc_v1_v4_cxt.manifest.json
binary/valid_noncanonical_metadata.cxc
```

Malformed binary fixtures 覆盖：

```text
invalid_zip64.cxc
invalid_zip64_eocd.cxc
invalid_zip64_locator.cxc
invalid_zip64_extra_field.cxc
invalid_path_prefix.cxc
invalid_data_descriptor.cxc
invalid_compression.cxc
invalid_multi_disk.cxc
invalid_extra_field.cxc
invalid_archive_comment.cxc
invalid_directory.cxc
invalid_symlink.cxc
invalid_header_mismatch.cxc
invalid_overlap.cxc
invalid_trailing_bytes.cxc
invalid_crc.cxc
invalid_manifest_hash.cxc
invalid_manifest_size.cxc
invalid_case_conflict.cxc
invalid_parent_path.cxc
```

普通测试只读 committed bytes；只有显式设置 `CUEXIS_UPDATE_CXC_FIXTURES=1` 才允许更新 fixture。
`.cxc` 文件已通过 `.gitattributes` 标记为 binary，避免文本换行转换改变 golden bytes。

### 3.2 本地验证矩阵

以下结果来自 2026-08-12 的 MSVC x64 工作流记录和本轮复核：

| 检查 | 结果 | 说明 |
| --- | --- | --- |
| Debug fresh configure | passed | `cmake --preset debug --fresh` |
| Debug clean build | passed | 包含 CXC target 和测试 |
| Debug 全量 CTest | passed | `341/341`；1 个既有 Windows symlink 能力测试跳过 |
| Release fresh configure | passed | `cmake --preset release --fresh` |
| Release clean build | passed | warnings-as-errors |
| Release 全量 CTest | passed | `341/341`；1 个既有 Windows symlink 能力测试跳过 |
| CXC Debug executable | passed | `405 assertions / 19 test cases` |
| CXC Release executable | passed | `405 assertions / 19 test cases` |
| CXC fixture update/read-only gate | passed | committed bytes 与生产 Writer 一致 |
| `clang-tidy` | passed | 6/6 CXC 核心 translation units，无诊断 |
| `cuexis_format_check` | passed | 使用仓库 `.clang-format` |
| `check_docs.py` | passed | 116 Markdown、20 candidate JSON/CXT |
| `update_version.py --check` | passed | canonical version `26.08.01-1` |
| `git diff --check` | passed | 仅报告既有 LF/CRLF 转换警告 |

### 3.3 未形成有效证据的检查

以下检查不在本报告中宣称通过：

- Hosted Linux Quality、ASan/UBSan、coverage、MinGW 和远端 workflow 尚未运行。
- 本轮 shared Debug fresh configure 在 Ninja database recompaction 阶段遇到本机
  `failed recompaction: Permission denied`。
- 本轮 Release external-consumer 过滤测试无法完成配置，失败原因为本机 Visual Studio 环境
  找不到 Windows SDK import library：`LNK1104: 无法打开文件“kernel32.lib”`。

这些是本地工具链/环境阻塞，不构成 CXC 代码通过或失败的替代证据。最终 static/shared、安装
consumer 和跨平台闭环仍须在可用的目标环境及 hosted workflow 中取得精确 commit SHA 证据。

## 4. 安全与确定性审查结论

Raw envelope 不依赖“archive 库能否打开文件”作为唯一安全边界；结构、范围、路径和预算在
解包前完成。所有 diagnostics 使用稳定 code、field path 和 package-relative path，不输出宿主
绝对路径。Writer 的 metadata、entry order、manifest bytes 和 package identity 由 binary golden
锁定。

Malformed archive、manifest mismatch、资源闭包缺失和超预算输入均在部分 package publication
之前失败；未观察到越界、整数溢出或 partial publication 诊断。

## 5. 明确未包含内容

本检查点不包含：

```text
cuexis_cxc_pack / cuexis_cxc_validate / cuexis_cxc_unpack
pack/unpack 的 atomic staging、no-overwrite 和 exit-code contract
public Cuexis::Cxc component 或 installed CXC headers
PlaybackSource / prepare / reload 对 CXC package 的接入
Chart migration to v4
Stage 4 animation evaluation
runtime scripts or per-frame script callbacks
完整 CXC 产品发布和最终跨平台验收
```

运行时脚本和逐帧脚本回调仍无限期延后；本阶段没有为它们预留 Chart/CXT/CXC 字段、extension、
capability、bytecode、ABI 或 Playback 执行入口。

## 6. 交接

CFU-C3 关闭后，下一批次是 CFU-C4：

```text
cuexis_cxc_pack
cuexis_cxc_validate
cuexis_cxc_unpack
exit code 0/1/2
bounded source snapshot
sibling staging and atomic commit
no-overwrite and cleanup guarantees
pack -> validate -> unpack -> repack binary round-trip golden
```

CFU-C4 完成前，不进入 Chart migration、Playback prepare 接入或 Stage 4 动画实现。本报告不把
CFU-C4 的计划文字当作已交付证据。
