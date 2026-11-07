# 2026-11-07 R4 Evidence Archive

状态：evidence bundle；candidate C evidence, not final acceptance

日期：2026-11-07

候选 `24f0a914f306cbc20e05b1dccc9f82dac59a6024` / tree
`0bc59434103671b403ffd3bebc77a3df2fee493a`，日期构建 `26.11.07-1`，SDK `0.9.1`。

`raw.zip` 收录原始 owning diagnostics、allocator contract logs、36个CPU正式样本及热身、不可变claims、
11行本地矩阵日志/manifest、所有PR/push CI记录、隔离读取原型构建/正确性/对象码/诊断、O2/F4草案和审查记录。
`manifest.json` 列出每个成员的字节数和SHA-256并绑定ZIP；`verify.py` 只读校验归档成员，重新解析36个CPU样本，
重算六项固定组结论、内存span去重、矩阵状态、诊断停止和hosted SHA，不启动Cuexis进程。

预期验证：`python -B verify.py`。memory/P8仍未准入，正式40K未运行；read_long正式超界保留；此附件不代表O2/O3 owner接受。
