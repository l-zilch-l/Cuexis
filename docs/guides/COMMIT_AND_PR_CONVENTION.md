# 提交与 PR 标题约定

状态：active

更新日期：2026-11-18

<!-- cuexis-meta
id: GUIDE-COMMIT_AND_PR_CONVENTION
status: active
authority: owns
-->

本页定义 **commit subject** 与 **PR title** 的模板。两者是不同字段：commit 每次提交都有，
PR title 整个 PR 只有一个；`git push` 本身没有标题。

## Commit subject

```text
<type>(<scope>): <summary>
```

- `type`：`feat`、`fix`、`docs`、`test`、`build`、`ci`、`refactor`、`perf`、`chore`、`revert`；
- `scope`：可选，写受影响区域，如 `chart`、`playback`、`docs`、`tools`、`ci`；
- `summary`：祈使句、小写开头、不超过 72 字符、结尾不加句号；
- 正文写“为什么”，不重复 diff；引用 issue/ADR 放在 footer。

示例：

```text
docs(handoff): add plan document split plan
ci: fix shader-tools target allowlist
feat(playback): expose candidate seek API
```

模板文件：[.gitmessage](../../.gitmessage)。启用方式：

```powershell
git config commit.template .gitmessage
```

## PR title

```text
<Area>: <summary>
```

- `Area`：阶段名（`Stage 7-RR`、`Stage 7.5`）、子系统（`playback`、`chart`、`docs`、`tools`）
  或 `repo`；
- `summary`：祈使句、不超过 72 字符、结尾不加句号；
- 与 commit subject 不同：PR title 描述整个 PR，不要求带 `type(scope)`。

示例：

```text
Stage 7-RR: documentation governance, review register, and tools audit
repo: retire legacy renderFrame entry point
docs: add chart representation map
```

GitHub 没有原生的“标题模板”机制：正文模板
[.github/pull_request_template.md](../../.github/pull_request_template.md) 只生成正文，
标题必须在创建/编辑 PR 时手动按上述格式填写。若使用 squash merge，GitHub 默认用 PR title
作为 squash commit subject，因此 PR title 也要满足 commit subject 的长度与语气要求。

## 检查方式

目前是**约定**，不强制校验。若后续需要，可加一个只读 PR title 正则检查的 workflow；
commit-msg hook 也可选，但不默认启用，避免影响本地提交。
