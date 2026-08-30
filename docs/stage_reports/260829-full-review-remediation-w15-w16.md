# 260829 Full Review 整改 W15/W16

状态：completed；2026-08-29 W15 shader pipeline 集成与 W16 本地 gate 完成

本报告记录 Full Review 第 1 批 W15/W16 的实现和验证证据。原始审查报告
`260829-full-review.md`、`docs/CURRENT_STATUS.md` 和整改计划均未修改；本报告不改变
Stage 4/5 状态。

## 基线与范围

- 基线 SHA：`857d0bdf5e4b384fd70510eadfda46fcaeec4d92`
- 实现 SHA：`92dc195b4c0a76db591b6c500f915d62b3927c80`
- 实现分支：`260829-full-review`
- W15 W1：`AG-A3-PIPELINE-I`
- W15 W2：`AG-D1-RELOAD-T` 复核
- W16 W1：`AG-C2-TARGET-EXPORT` 复核
- W16 W2：`AG-D2-RELOAD-I` 复核

## 实现与行为

- `engine/render_opengl/src/open_gl_presentation.cpp` 在启用 shader-tools 且提供 cache
  directory 时，构造统一 `ShaderCompileRequest` 和 `ShaderCacheKeyInput`，通过
  `ShaderPipelineCache::prepareCandidate()` 获取 artifact；compile-disabled 仍只读 cache，
  cache 缺失返回 `shader.cache.missing`。
- 保留 shader-tools + 空 cache directory 的直接编译行为且不落盘；无 shader-tools 构建继续
  使用 cache-only 兼容路径。
- 保持 Playback candidate/active、commit/discard、presentation `activate()` 的事务顺序；
  编译、cache 读取或 adapter prepare 失败均不污染 active presentation。
- `app/player/src/player_app.cpp` 的 adapter failure smoke 场景改用非法
  `portableProfileVersion = 2`，确保测试确实覆盖 prepare 失败回滚。
- C2 active-target 导出（`70d8b7c`）与 D1/D2 reload 单点提交修复（`cbce029`）在本轮以新
  基线重新验证；未新增格式、身份或 API 决策。

## 合同保持

FrameDigest v1-v3、PreparedSemanticIdentity、canonical bytes/order、golden、Playback 公共
观察面、candidate/active rollback、owner-thread 合同、公共头 ASCII/安装边界和 runtime-script
无限期延后边界均保持不变。未修改 identity、digest、序列化字段、公共头或 SDK 版本。

## 验证

- Debug fresh configure、`--clean-first` 构建：通过。
- `cuexis_format_check`：通过；shader cache 源按当前 clang-format 规范化后无语义差异。
- Debug 全量 CTest：除 Windows symlink 条件测试 1 项跳过外，其余基础测试通过；7 个
  external-consumer 测试在 VS Developer 环境中重跑 7/7 通过。首次普通 PowerShell 运行的
  7 个失败均为缺少 `kernel32.lib` 的环境问题。
- Debug shader-tools fresh configure、`--clean-first` 构建：通过；shader/OpenGL/runtime/
  reload 聚焦测试 54/54 通过。
- GPU Player `--smoke-test`：退出码 0，6 帧完成；失败 reload、adapter prepare 失败、legacy
  candidate rejection、outstanding candidate 和成功 activate 均保持预期事务语义。
- Release fresh configure、`--clean-first` 构建：通过；Runtime/Playback reload 聚焦测试
  10/10 通过。
- C2/status 合同测试：`check_docs_target_contract_tests.py` 2/2、
  `check_docs_status_contract_tests.py` 3/3 通过。
- `python -B tools/check_docs.py`：通过（181 Markdown、20 candidate JSON/CXT）。
- `python -B tools/update_version.py --check`、`git diff --check`：通过。
- architecture、static/shared external-consumer、Playback-only package gates：由 Debug
  CTest 和 external-consumer 7/7 覆盖并通过。

## 残余风险

M01 的 D1-D6 owner/spec 决策门仍按既有记录处理；本轮没有自行选择 identity 迁移方案。
Hosted Linux/MinGW 与 owner acceptance 仍需在相应环境完成。本地未执行 push。
