# Tools 工具索引

本目录包含离线内容工具、工具侧共享实现、仓库检查脚本、验收驱动和研究代码。
工具已按实际职责与生命周期归入分类目录；目录整理不代表工具已获得发布或阶段验收。
当前阶段与交付状态以 [CURRENT_STATUS](../docs/CURRENT_STATUS.md) 为准。

## 分类依据

- **职责优先**：按 Chart、CXC、Gameplay、媒体、Shader、仓库维护等用途归类，不仅依据文件名或语言。
- **区分入口与实现**：可执行工具、共享库、生成器和验证驱动分别标明，避免把内部库当成独立命令。
- **保留生命周期边界**：候选格式工具、阶段合同检查、研究原型分别说明，不能由目录位置推断生产可用性。
- **迁移考虑调用关系**：CMake/CTest、CI、同目录导入、仓库根目录推导、构建对象路径和文档命令都属于路径依赖。

以下路径均相对于本目录。索引覆盖当前工具入口及共享组件；内部源文件由各组件目录管理。

## 离线内容工具与共享组件

| 分类 | 当前路径 | 用途与边界 |
| --- | --- | --- |
| Chart | [chart_validator/](chart/chart_validator/) | 校验 Chart，输出诊断。 |
| Chart | [chart_migrator/](chart/chart_migrator/) | 迁移 Chart 格式，支持目标版本 3/4，并输出迁移报告。 |
| Chart 候选工具 | [chart_candidate/](chart/chart_candidate/) | 展开 CXT V2、组装 packed Chart/CXC 内容并做 Playback prepare/commit 检查；候选开关开启时接入 Gameplay author 组装。 |
| CXC | [cxc_pack/](cxc/cxc_pack/) | 将项目内容打包为 CXC。 |
| CXC | [cxc_validate/](cxc/cxc_validate/) | 校验 CXC 包与内容闭包。 |
| CXC | [cxc_unpack/](cxc/cxc_unpack/) | 解包 CXC 内容。 |
| CXC 共享实现 | [cxc_common/](cxc/cxc_common/) | CXC 工具共用的打包、解包及候选处理实现；不是独立命令。 |
| Gameplay | [gameplay_assembler/](gameplay/gameplay_assembler/) | author 数据解析、lowering 与 packed capsule 组装共享库；`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 开启时另建 CLI。 |
| Gameplay Schema 生成 | [generate_author_schema.py](gameplay/gameplay_assembler/generate_author_schema.py) | 生成候选 Gameplay author Schema；与 assembler 一起维护。 |
| 媒体共享实现 | [media_import/](media/media_import/) | 图像、音频导入及缓存、身份和来源信息处理；不是独立命令。 |
| 媒体 CLI | [media_importer/](media/media_importer/) | 媒体导入入口、worker 进程调度和产物发布。 |
| Shader | [asset_importer/](shader/asset_importer/) | 编译 ShaderAsset GLSL，生成 Shader 缓存；名称不表示通用图像/音频导入器。 |
| 工具公共设施 | [asset_publish/](common/asset_publish/) | 内容工具共用的文件发布实现；不是独立命令。 |

### 构建关系

构建入口为 [CMakeLists.txt](CMakeLists.txt)：

| 开关或条件 | 加入的组件 |
| --- | --- |
| `CUEXIS_BUILD_DEVELOPER_TOOLS` | Gameplay assembler、CXC 共享实现与三个 CLI、三个 Chart 工具。 |
| 上述开关开启且存在 `cuexis_shader` target | Shader `asset_importer`。 |
| `CUEXIS_BUILD_MEDIA_TOOLS` | `media_import` 与 `media_importer`。 |
| 任一工具开关开启 | 公共 `asset_publish`。 |

媒体工具有独立构建开关，无需因此启用 Shader 工具链。
具体 target、依赖和测试以各组件的 `CMakeLists.txt` 为准；构建方法见
[构建指南](../docs/guides/BUILDING.md)。

## 仓库维护与门禁

版本管理、SDK 审批与推送前检查保留根级稳定入口；文档回归和 DLL 验证脚本归入 `verification/`。

| 分类 | 当前路径 | 用途 |
| --- | --- | --- |
| 文档检查 | [verification/check_docs.py](verification/check_docs.py) | 文档链接、索引、状态及候选示例一致性检查；`tools/check_docs.py` 保留兼容入口。 |
| 文档检查测试 | [check_docs_section_contract_tests.py](verification/check_docs_section_contract_tests.py)、[check_docs_status_contract_tests.py](verification/check_docs_status_contract_tests.py)、[check_docs_target_contract_tests.py](verification/check_docs_target_contract_tests.py) | 文档章节、状态与 target 合同的回归测试。 |
| 版本维护 | [update_version.py](update_version.py) | 同步规范版本与 vcpkg manifest；`--check` 为只读核验。 |
| 版本可信门禁 | [check_version_gate.py](check_version_gate.py)、[check_version_gate_tests.py](check_version_gate_tests.py) | 版本变更门禁及测试；路径同时受 CI trusted-root 清单和 CODEOWNERS 约束。 |
| 推送前检查 | [check_pre_push.ps1](check_pre_push.ps1) | 按 Quick/Linux/Full 模式组织本地检查。 |
| SDK 审批辅助 | [prepare_sdk_approval.cmd](prepare_sdk_approval.cmd)、[prepare_sdk_approval.ps1](prepare_sdk_approval.ps1) | Windows 入口及审批准备流程，包含 GitHub 交互；不是通用离线校验器。 |
| SDK 审批辅助测试 | [check_sdk_approval_assistant_tests.ps1](check_sdk_approval_assistant_tests.ps1) | SDK 审批辅助流程回归测试。 |
| Windows DLL 门禁 | [check_pe_export_gate.py](verification/check_pe_export_gate.py) | 用真实 DLL 和故意构造的非法导出表验证 PE 导出门禁。 |
| CI 设施 | [ci/gnu_asset_mirror.py](ci/gnu_asset_mirror.py)、[ci/gnu_asset_mirror_tests.py](ci/gnu_asset_mirror_tests.py) | GNU 资源镜像辅助及测试；保留 `ci/` 分类。 |

版本流程见 [VERSIONING](../docs/guides/VERSIONING.md)，文档规则见
[DOCUMENTATION_POLICY](../docs/DOCUMENTATION_POLICY.md)。

## 合同检查、验收与测量

| 分类 | 当前路径 | 用途与边界 |
| --- | --- | --- |
| 阶段合同检查 | [check_stage6_a2.py](verification/check_stage6_a2.py) | 对 S6-A2 Schema 与 fixture 合同做只读刻画检查。 |
| Gameplay 组装检查 | [check_gameplay_assembler_tests.py](verification/check_gameplay_assembler_tests.py) | Gameplay assembler 的命令行行为与产物回归检查。 |
| Gameplay host 检查 | [check_gameplay_host_tests.py](verification/check_gameplay_host_tests.py) | 运行已安装参考 host 的公开 Gameplay 命令，对照人工 golden。 |
| Gameplay 诊断门禁 | [check_gameplay_diagnostic_formalization.py](verification/check_gameplay_diagnostic_formalization.py) | 检查诊断映射迁移与规范化约束。 |
| 设备验收包检查 | [check_device_acceptance_chart.py](practice/check_device_acceptance_chart.py) | 独立检查设备包 ZIP、hash、author binding，并对照 host golden。 |
| RPA 验收矩阵 | [verification/run_rpa_acceptance_matrix.py](verification/run_rpa_acceptance_matrix.py) | 驱动全新本地 RPA 验收行并保留每次尝试；运行本身不宣告 F1/F3 验收。根目录保留兼容入口。 |
| PD-0 测量 | [verification/measure_pd0_method.py](verification/measure_pd0_method.py) | Windows static PD-0 方法校准及单独注册的验收；校准与验收证据分开。根目录保留兼容入口。 |

检查脚本与其检查对象一同归类，但不因此改名为内容生产工具。
部分驱动要求现有构建、fixture、host 或特定平台；运行前应查看对应参数与阶段文档。

## 练习素材与 fixture 辅助

| 当前路径 | 用途 |
| --- | --- |
| [make_entity_practice_chart.py](practice/make_entity_practice_chart.py) | 生成实体练习 Chart 及相关素材。 |
| [make_judgement_practice.py](practice/make_judgement_practice.py) | 为实体练习附加显式 RPA Gameplay V2 capsule。 |
| [make_device_acceptance_chart.py](practice/make_device_acceptance_chart.py) | 生成设备验收使用的内容包与输入材料。 |
| [compile_entity_practice.cpp](practice/compile_entity_practice.cpp) | 离线练习内容编译辅助源码。 |
| [build_entity_practice_compiler.py](practice/build_entity_practice_compiler.py) | 复用已有 Windows MSVC 配置与 Ninja 命令，构建上述辅助程序。 |

这些脚本用于离线素材准备，不是 Playback 自动执行入口。
设备验收用法见 [设备验收示例](../docs/examples/s7a78-device-acceptance.md)。

## 研究代码与生成物

- [research/gameplay_fold_spike/](research/gameplay_fold_spike/README.md)：Gameplay 候选规则的独立 C++20 实验，覆盖 fold、确定性、几何与连续性等研究。它未加入产品 CMake 构建，不能作为产品实现或阶段完成证据。
- `__pycache__/`：Python 运行缓存，不属于工具入口；本次整理不清理已有缓存或证据文件。

## 兼容入口

三个原先保留在根目录的工具实现现已按职责归入 `verification/`：

- [verification/measure_pd0_method.py](verification/measure_pd0_method.py)
- [verification/check_docs.py](verification/check_docs.py)
- [verification/run_rpa_acceptance_matrix.py](verification/run_rpa_acceptance_matrix.py)

根目录仍保留同名兼容包装：

- [measure_pd0_method.py](measure_pd0_method.py)
- [check_docs.py](check_docs.py)
- [run_rpa_acceptance_matrix.py](run_rpa_acceptance_matrix.py)

现有命令、CI 和历史文档入口继续可用；新代码和工具索引应引用 `verification/` 下的实现。

## 目录布局

```text
tools/
  chart/          Chart 校验、迁移与候选组装
  cxc/            CXC CLI 与共享实现
  gameplay/       Gameplay author 组装与候选 Schema 生成
  media/          媒体导入共享实现与 CLI
  shader/         Shader 编译与缓存工具
  common/         跨工具共享的产物发布实现
  verification/   文档、格式、Gameplay 与 DLL 合同检查
  practice/       练习内容生成、设备验收辅助与专用编译器
  ci/             CI 镜像辅助
  research/       独立研究原型
```

根级保留 `CMakeLists.txt`、本索引、三个兼容包装，以及版本、SDK 审批和推送前检查入口及其配套测试。
`ci/`、`research/` 和已有缓存保持原有位置。

## 旧路径对应关系

下表路径均相对于 `tools/`。旧路径供阅读历史报告时查阅；除明确保留的根级入口外，执行命令请使用上方索引中的当前路径。
组件内部的文件后缀路径保持不变；CMake target、二进制名称和 `cuexis/...` 头文件 include 名称保持不变。

| 旧路径 | 当前路径 |
| --- | --- |
| `chart_candidate/`、`chart_validator/`、`chart_migrator/` | `chart/` 下的同名目录 |
| `cxc_common/`、`cxc_pack/`、`cxc_unpack/`、`cxc_validate/` | `cxc/` 下的同名目录 |
| `gameplay_assembler/` | `gameplay/gameplay_assembler/` |
| `media_import/`、`media_importer/` | `media/` 下的同名目录 |
| `asset_importer/` | `shader/asset_importer/` |
| `asset_publish/` | `common/asset_publish/` |
| `check_docs_section_contract_tests.py`、`check_docs_status_contract_tests.py`、`check_docs_target_contract_tests.py` | `verification/` 下的同名文件 |
| `check_stage6_a2.py`、`check_pe_export_gate.py` | `verification/` 下的同名文件 |
| `check_gameplay_assembler_tests.py`、`check_gameplay_host_tests.py`、`check_gameplay_diagnostic_formalization.py` | `verification/` 下的同名文件 |
| `make_entity_practice_chart.py`、`make_judgement_practice.py`、`make_device_acceptance_chart.py` | `practice/` 下的同名文件 |
| `check_device_acceptance_chart.py`、`build_entity_practice_compiler.py`、`compile_entity_practice.cpp` | `practice/` 下的同名文件 |
| `measure_pd0_method.py`、`check_docs.py`、`run_rpa_acceptance_matrix.py` | `verification/` 下的实现；根目录保留兼容包装 |

历史报告保留原始命令、源码路径和行号；仅将真实 Markdown 链接的目标修正到当前文件，保留原有显示文字。
当前指南与示例使用新路径。已有构建目录需重新运行 CMake configure，再继续构建和测试。

## 维护与验证

PD 测量的当前无编译插桩入口是 [formal_baseline.py](../tests/pd/formal_baseline.py)，
配置及适用范围见 [PD 测试索引](../tests/pd/README.md)。
`verification/measure_pd0_method.py` 的插桩测量入口保留用于历史复现，其身份与 child 运行函数
仍被新入口依赖，不能作为过时工具删除。根级兼容包装也仍在迁移保留期。
一次性本机构建脚本与临时产物不提升为仓库工具；归档、删除明细见
[PD 复测与归档报告](../docs/stage_reports/stages/stage-07/pd/2026-10-17-pd0-supplemental-retests-and-archive.md)。

后续目录变更需同步检查以下位置：

- [本目录 CMake](CMakeLists.txt)、[根 CMake](../CMakeLists.txt)、[测试注册](../tests/CMakeLists.txt)、`cmake/Verify*.cmake`。
- `.github/workflows/` 的直接调用、版本门禁 trusted-root 清单及 `.github/CODEOWNERS`。
- Python 的同目录导入、`__file__` 推导仓库根目录，以及 PowerShell/CMD 的相邻脚本路径。
- 当前指南、示例和可复制命令；带日期的历史报告保留原始执行路径，必要时通过索引或引用说明提供新旧路径对应关系，不批量改写历史证据。

每批迁移按实际影响运行对应工具测试与构建检查。文档及合同检查入口示例：

```powershell
python -B tools/check_docs.py
python -B tools/verification/check_docs_section_contract_tests.py
python -B tools/verification/check_docs_status_contract_tests.py
python -B tools/verification/check_docs_target_contract_tests.py
python -B tools/verification/check_stage6_a2.py
git diff --check
```
