# tools/practice

状态：current

更新日期：2026-11-17

维护者手工脚本，**不属于 CI / CTest / 预推送门禁**。用途：生成与校验 v5 练习 fixture、
RPA Gameplay V2 capsule 和设备验收包。产物默认写入 `out/` 或参数指定的目录，不提交。

| 脚本 | 用途 | 主要参数 |
| --- | --- | --- |
| `make_entity_practice_chart.py` | 生成自包含的 v5 Behavior candidate entity practice | `--output`、`--compiler`、`--fixture` |
| `make_judgement_practice.py` | 给已生成的 v5 practice 附加显式 RPA Gameplay V2 capsule | `--directory`、`--fixture`、`--assembler` |
| `compile_entity_practice.cpp` | 离线 fixture 编译 helper 的源码 | 由 `build_entity_practice_compiler.py` 编译 |
| `build_entity_practice_compiler.py` | 在已有 Windows MSVC 配置上构建上述 helper | `--build`、`--output`、`--vcvars` |
| `make_device_acceptance_chart.py` | 用真实 candidate assembler 生成 test-only 设备 fixture（音频本地合成，无网络资产） | `--tool`、`--fixture`、`--output` |
| `check_device_acceptance_chart.py` | 用独立 ZIP/hash 校验与手工 host golden 检查设备包 | `--directory`、`--host` |

典型顺序：

```text
make_entity_practice_chart.py -> make_judgement_practice.py
build_entity_practice_compiler.py + compile_entity_practice.cpp
make_device_acceptance_chart.py -> check_device_acceptance_chart.py
```

这些脚本的产物只用于人工练习/验收复现，不构成发布证据；需要进入证据链的结论必须按
`docs/templates/report.md` 写入 `docs/stage_reports/`。
