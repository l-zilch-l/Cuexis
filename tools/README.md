# Tools 工具索引

本目录包含离线内容工具、工具侧共享实现、仓库检查脚本、验收驱动和研究代码。
本索引按实际职责与生命周期分类；分类名称不代表目录已经迁移，也不代表工具已获得发布或阶段验收。
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
| Chart | [chart_validator/](chart_validator/) | 校验 Chart，输出诊断。 |
| Chart | [chart_migrator/](chart_migrator/) | 迁移 Chart 格式，支持目标版本 3/4，并输出迁移报告。 |
| Chart 候选工具 | [chart_candidate/](chart_candidate/) | 展开 CXT V2、组装 packed Chart/CXC 内容并做 Playback prepare/commit 检查；候选开关开启时接入 Gameplay author 组装。 |
| CXC | [cxc_pack/](cxc_pack/) | 将项目内容打包为 CXC。 |
| CXC | [cxc_validate/](cxc_validate/) | 校验 CXC 包与内容闭包。 |
| CXC | [cxc_unpack/](cxc_unpack/) | 解包 CXC 内容。 |
| CXC 共享实现 | [cxc_common/](cxc_common/) | CXC 工具共用的打包、解包及候选处理实现；不是独立命令。 |
| Gameplay | [gameplay_assembler/](gameplay_assembler/) | author 数据解析、lowering 与 packed capsule 组装共享库；`CUEXIS_ENABLE_CHART_V5_CANDIDATE` 开启时另建 CLI。 |
| Gameplay Schema 生成 | [generate_author_schema.py](gameplay_assembler/generate_author_schema.py) | 生成候选 Gameplay author Schema；与 assembler 一起维护。 |
| 媒体共享实现 | [media_import/](media_import/) | 图像、音频导入及缓存、身份和来源信息处理；不是独立命令。 |
| 媒体 CLI | [media_importer/](media_importer/) | 媒体导入入口、worker 进程调度和产物发布。 |
| Shader | [asset_importer/](asset_importer/) | 编译 ShaderAsset GLSL，生成 Shader 缓存；名称不表示通用图像/音频导入器。 |
| 工具公共设施 | [asset_publish/](asset_publish/) | 内容工具共用的文件发布实现；不是独立命令。 |

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

这些脚本的当前根级路径是日常命令或自动化入口，优先保留。

| 分类 | 当前路径 | 用途 |
| --- | --- | --- |
| 文档检查 | [check_docs.py](check_docs.py) | 文档链接、索引、状态及候选示例一致性检查；受保护入口。 |
| 文档检查测试 | [check_docs_section_contract_tests.py](check_docs_section_contract_tests.py)、[check_docs_status_contract_tests.py](check_docs_status_contract_tests.py)、[check_docs_target_contract_tests.py](check_docs_target_contract_tests.py) | 文档章节、状态与 target 合同的回归测试。 |
| 版本维护 | [update_version.py](update_version.py) | 同步规范版本与 vcpkg manifest；`--check` 为只读核验。 |
| 版本可信门禁 | [check_version_gate.py](check_version_gate.py)、[check_version_gate_tests.py](check_version_gate_tests.py) | 版本变更门禁及测试；路径同时受 CI trusted-root 清单和 CODEOWNERS 约束。 |
| 推送前检查 | [check_pre_push.ps1](check_pre_push.ps1) | 按 Quick/Linux/Full 模式组织本地检查。 |
| SDK 审批辅助 | [prepare_sdk_approval.cmd](prepare_sdk_approval.cmd)、[prepare_sdk_approval.ps1](prepare_sdk_approval.ps1) | Windows 入口及审批准备流程，包含 GitHub 交互；不是通用离线校验器。 |
| SDK 审批辅助测试 | [check_sdk_approval_assistant_tests.ps1](check_sdk_approval_assistant_tests.ps1) | SDK 审批辅助流程回归测试。 |
| Windows DLL 门禁 | [check_pe_export_gate.py](check_pe_export_gate.py) | 用真实 DLL 和故意构造的非法导出表验证 PE 导出门禁。 |
| CI 设施 | [ci/gnu_asset_mirror.py](ci/gnu_asset_mirror.py)、[ci/gnu_asset_mirror_tests.py](ci/gnu_asset_mirror_tests.py) | GNU 资源镜像辅助及测试；保留 `ci/` 分类。 |

版本流程见 [VERSIONING](../docs/guides/VERSIONING.md)，文档规则见
[DOCUMENTATION_POLICY](../docs/DOCUMENTATION_POLICY.md)。

## 合同检查、验收与测量

| 分类 | 当前路径 | 用途与边界 |
| --- | --- | --- |
| 阶段合同检查 | [check_stage6_a2.py](check_stage6_a2.py) | 对 S6-A2 Schema 与 fixture 合同做只读刻画检查。 |
| Gameplay 组装检查 | [check_gameplay_assembler_tests.py](check_gameplay_assembler_tests.py) | Gameplay assembler 的命令行行为与产物回归检查。 |
| Gameplay host 检查 | [check_gameplay_host_tests.py](check_gameplay_host_tests.py) | 运行已安装参考 host 的公开 Gameplay 命令，对照人工 golden。 |
| Gameplay 诊断门禁 | [check_gameplay_diagnostic_formalization.py](check_gameplay_diagnostic_formalization.py) | 检查诊断映射迁移与规范化约束。 |
| 设备验收包检查 | [check_device_acceptance_chart.py](check_device_acceptance_chart.py) | 独立检查设备包 ZIP、hash、author binding，并对照 host golden。 |
| RPA 验收矩阵 | [run_rpa_acceptance_matrix.py](run_rpa_acceptance_matrix.py) | 驱动全新本地 RPA 验收行并保留每次尝试；运行本身不宣告 F1/F3 验收。受保护入口。 |
| PD-0 测量 | [measure_pd0_method.py](measure_pd0_method.py) | Windows static PD-0 方法校准及单独注册的验收；校准与验收证据分开。受保护入口。 |

检查脚本与其检查对象一同归类，但不因此改名为内容生产工具。
部分驱动要求现有构建、fixture、host 或特定平台；运行前应查看对应参数与阶段文档。

## 练习素材与 fixture 辅助

| 当前路径 | 用途 |
| --- | --- |
| [make_entity_practice_chart.py](make_entity_practice_chart.py) | 生成实体练习 Chart 及相关素材。 |
| [make_judgement_practice.py](make_judgement_practice.py) | 为实体练习附加显式 RPA Gameplay V2 capsule。 |
| [make_device_acceptance_chart.py](make_device_acceptance_chart.py) | 生成设备验收使用的内容包与输入材料。 |
| [compile_entity_practice.cpp](compile_entity_practice.cpp) | 离线练习内容编译辅助源码。 |
| [build_entity_practice_compiler.py](build_entity_practice_compiler.py) | 复用已有 Windows MSVC 配置与 Ninja 命令，构建上述辅助程序。 |

这些脚本用于离线素材准备，不是 Playback 自动执行入口。
设备验收用法见 [设备验收示例](../docs/examples/s7a78-device-acceptance.md)。

## 研究代码与生成物

- [research/gameplay_fold_spike/](research/gameplay_fold_spike/README.md)：Gameplay 候选规则的独立 C++20 实验，覆盖 fold、确定性、几何与连续性等研究。它未加入产品 CMake 构建，不能作为产品实现或阶段完成证据。
- `__pycache__/`：Python 运行缓存，不属于工具入口；本次整理不清理已有缓存或证据文件。

## 受保护文件

以下三个文件按用户要求保留原位，禁止在本次工具整理中移动或修改：

- [measure_pd0_method.py](measure_pd0_method.py)
- [check_docs.py](check_docs.py)
- [run_rpa_acceptance_matrix.py](run_rpa_acceptance_matrix.py)

依赖这些入口的调用方也必须继续可用。例如，文档合同测试使用同目录导入或
`Path(__file__).with_name('check_docs.py')`，不能单独移动测试后假定仍然兼容。

## 后续整理的实施顺序

本轮仅新增本索引。下面是后续实施建议，目标目录是方案，不是当前可用路径。

1. **建立导航**：先保留当前路径，用本文件提供职责索引，并明确三个受保护入口。
2. **记录依赖**：为每批候选迁移列出旧路径、目标路径、调用方与验证项；区分当前操作指南和历史证据引用。
3. **先整理独立内容工具**：评估将 Chart、CXC 工具分别归入 `chart/`、`cxc/`，Shader 工具归入 `shader/`；保留现有 target 名称。每批同时更新 CMake、CTest、`Verify*.cmake`、脚本及当前文档引用，再验证。
4. **再整理共享组件及耦合工具**：评估 `gameplay/`、`media/` 与 `common/`，按依赖成组迁移。Gameplay 练习编译辅助硬编码源码与 Ninja 对象目录，须先消除或同步这些路径依赖。
5. **最后评估根级脚本**：按维护、验证、素材生成职责继续细分导航；版本门禁、用户稳定入口和三个受保护文件优先保留根级。只有明确兼容方案后才考虑其他脚本的物理迁移。

迁移检查应覆盖以下位置：

- [本目录 CMake](CMakeLists.txt)、[根 CMake](../CMakeLists.txt)、[测试注册](../tests/CMakeLists.txt)、`cmake/Verify*.cmake`。
- `.github/workflows/` 的直接调用、版本门禁 trusted-root 清单及 `.github/CODEOWNERS`。
- Python 的同目录导入、`__file__` 推导仓库根目录，以及 PowerShell/CMD 的相邻脚本路径。
- 当前指南、示例和可复制命令；带日期的历史报告保留原始执行路径，必要时通过索引或引用说明提供新旧路径对应关系，不批量改写历史证据。

每批迁移按实际影响运行对应工具测试与构建检查。本次索引文档新增使用以下检查：

```powershell
python -B tools/check_docs.py
git diff --check
```
