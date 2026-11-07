"""Fill the measured tables of the MinGW allocation-probe report from the recorded JSON.

The report body is authored prose with four placeholders; every number in those placeholders comes
from a file the recorders wrote, so the report cannot drift from the archive it links to:

    @@VARIANT_TABLE@@    variants.json                  (control matrix over the as-run header)
    @@TRACE_FACTS@@      gdb/trace-sessions.json        (the two debugger sessions)
    @@ISOLATION_TABLE@@  isolation/isolation-matrix.json (no-framework cross)
    @@POSTFIX_TABLE@@    post-fix/summary.json          (the repaired suites)

`--report` defaults to the authored copy. `--out` writes the filled document elsewhere so the
authored original keeps its placeholders and the fill stays repeatable. Every placeholder refuses to
publish when its record contains a mismatch, so a failed check cannot be smoothed over by prose.

Usage:
    python -B fill_probe_report.py [--out <path>] [--commit <sha>]
"""

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPORT = HERE / "authored" / "report" / "2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md"
# materialize_docs.py copies `<staging>/<report name>`, so the filled document has to sit at the set
# root next to the archive; the authored copy keeps its placeholders.
OUTPUT = HERE / "2026-11-01-r3-run2-and-mingw-allocation-probe-limit.md"
VARIANTS = HERE / "variants.json"
TRACE = HERE / "gdb" / "trace-sessions.json"
ISOLATION = HERE / "isolation" / "isolation-matrix.json"
POSTFIX = HERE / "post-fix" / "summary.json"

VARIANT_NOTES = {
    "dynamic-runtime": "偏移载荷 + DLL 内 C++ 运行时",
    "static-runtime": "偏移载荷 + 静态 C++ 运行时",
    "resource-only": "替换编译掉，只留显式 PMR 上游",
    "passthrough-operators": "替换在块首，无偏移",
    "current-header-dynamic": "今天的头文件",
}

ISOLATION_NOTES = {
    ("dynamic-runtime", "roundtrip"): "DLL 运行时，全部释放都回到替换",
    ("dynamic-runtime", "mismatch"): "DLL 运行时，多一次 `new` + `std::free`",
    ("static-runtime", "roundtrip"): "静态运行时，全部释放都回到替换",
    ("static-runtime", "mismatch"): "静态运行时，多一次 `new` + `std::free`",
}

MODES = {"roundtrip": "roundtrip", "mismatch": "mismatch"}


def load(path):
    if not path.is_file():
        raise SystemExit("missing record: {}".format(path))
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def header_labels(payload):
    labels = {"none": "无探针头文件"}
    for key, label in (("as_run", "as-run"), ("current", "current")):
        record = (payload.get("headers") or {}).get(key)
        if record:
            labels[key.replace("_", "-")] = "{} `{}`（{} B）".format(
                label, record["sha256"][:8], record["bytes"])
    return labels


def variant_table(payload):
    if payload.get("mismatches"):
        raise SystemExit("variants.json reports {} mismatches: refusing to publish".format(
            payload["mismatches"]))
    labels = header_labels(payload)
    rows = ["| 变体 | 头文件 | 附加开关 | 预期 | 实测 | 判定 |", "| --- | --- | --- | --- | --- | --- |"]
    for record in payload["variants"]:
        expected = record.get("expected_exit") or "（无预测）"
        actual = record.get("exit") or "编译失败"
        if record.get("expected_exit") is None:
            verdict = "仅记录"
        else:
            verdict = "符合" if record.get("matches_expectation") else "**不符**"
        flags = record.get("extra_flags")
        if flags is None:
            flags = []
            for token in ("-static-libstdc++", "-static-libgcc",
                          "-DCUEXIS_PD_ALLOCATION_PROBE_RESOURCE_ONLY=1"):
                joined = " ".join(record.get("command") or [])
                if token in joined:
                    flags.append(token)
        note = VARIANT_NOTES.get(record["name"], "")
        rows.append("| `{}`{} | {} | {} | `{}` | `{}` | {} |".format(
            record["name"], "（{}）".format(note) if note else "",
            labels.get(record.get("header"), record.get("header") or "—"),
            " ".join("`{}`".format(flag) for flag in flags) or "—",
            expected, actual, verdict))
    return "\n".join(rows)


def trace_facts(payload):
    if payload.get("mismatches"):
        raise SystemExit("trace-sessions.json reports {} mismatches: refusing to publish".format(
            payload["mismatches"]))
    header = payload["header"]
    source = payload["source"].replace("\\", "/")
    lines = [
        "调试器为 `{}`。两次运行都用 `{}`（sha256 `{}`），编译时用的是 as-run 头文件"
        "（sha256 `{}`，{} 字节）。断点打在 CRT 导出的 `free` 上——在这个 CRT 里 `_free_base` 不是一个"
        "符号，而且在共享库加载之前下断点只会解析到 exe 里一个还没被填充的死 thunk，所以断点是在"
        " `main` 之后下的。命中条件只读内存：".format(
            payload["gdb"], source, payload["source_sha256"], header["sha256"],
            header["bytes"]),
        "",
        "```text",
        payload["predicate"],
        "```",
        "",
        "也就是“被释放的指针前面 16 字节，正好是这个头文件写给自己的 `{base, size}` 自指对”。"
        "这样命中就只能是替换实现的载荷，不可能是某个块的真实块首。",
        "",
    ]
    for run in payload["runs"]:
        expectation = "应当出现" if run["expected_payload_free_hits"] else "应当为零"
        entry = ("- `{binary}`（sha256 `{sha}`，{bytes} 字节）：`{bp}` 断点命中 **{hits}** 次"
                 "（{expectation}）；不带条件的同一个断点也命中 {coverage} 次，说明它确实解析到了"
                 "实现而不是死 thunk；`libstdc++-6.dll` {dll}模块表；不挂调试器直接运行退出 "
                 "`{exit}`。记录：`{transcript}`。").format(
                     binary=Path(run["binary"]).name, sha=run["binary_sha256"],
                     bytes=run["binary_bytes"], bp=payload["breakpoint"].split(" (")[0],
                     hits=run["payload_free_hits"], expectation=expectation,
                     coverage=run["free_breakpoint_coverage"],
                     dll="在" if run["libstdcxx_loaded"] else "不在", exit=run["exit_code_unattended"],
                     transcript=run["transcript"].replace("\\", "/"))
        lines.append(entry)
        for hit in run["payload_free_lines"]:
            lines.append("  - 断点输出：`{}`".format(hit))
        for site, code in zip(run["call_site_symbols"], run["call_site_code"]):
            lines.append("  - 返回地址属于：`{}`".format(site.split(" in section ")[0]))
            lines.append("  - 该处的指令：")
            lines.append("")
            lines.append("```text")
            lines.extend(code)
            lines.append("```")
            lines.append("")
        if run.get("crash_in_transcript"):
            lines.append("  同一份记录里，进程在这次释放之后以 `Critical error detected c0000374` 结束，"
                         "即崩溃发生在这条释放路径之后，而不是断言失败。")
            lines.append("")
    return "\n".join(lines).rstrip()


def isolation_table(payload):
    if payload.get("mismatches"):
        raise SystemExit("isolation-matrix.json reports {} mismatches: refusing to publish".format(
            payload["mismatches"]))
    rows = ["| 运行时 | 模式 | 场景 | 预期 | 实测 | 判定 |", "| --- | --- | --- | --- | --- | --- |"]
    for run in payload["runs"]:
        note = ISOLATION_NOTES.get((run["build"], run["mode"]), "")
        verdict = "符合" if run.get("matches_expectation") else "**不符**"
        rows.append("| `{}` | `{}` | {} | `{}` | `{}` | {} |".format(
            run["build"], MODES.get(run["mode"], run["mode"]), note or "—",
            run["expected_exit"], run["exit_status"], verdict))
    return "\n".join(rows)


def postfix_table(payload):
    if payload.get("mismatches"):
        raise SystemExit("post-fix/summary.json reports {} mismatches: refusing to publish".format(
            payload["mismatches"]))
    rows = ["| 目标 | 构建目录 | 退出码 | 断言 | 用例 | 判定 |",
            "| --- | --- | ---: | ---: | ---: | --- |"]
    for record in payload["runs"]:
        verdict = "符合" if record.get("matches_expectation") else "**不符**"
        rows.append("| `{}` | `{}` | {} | {} | {} | {} |".format(
            record["name"], record.get("build") or record.get("build_dir") or "—",
            record.get("exit"), record.get("assertions"), record.get("cases"), verdict))
    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=REPORT)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--commit", default=None,
                        help="commit sha that carries the six measurement repairs")
    arguments = parser.parse_args()
    text = arguments.report.read_text(encoding="utf-8")
    replacements = {
        "@@VARIANT_TABLE@@": variant_table(load(VARIANTS)),
        "@@TRACE_FACTS@@": trace_facts(load(TRACE)),
        "@@ISOLATION_TABLE@@": isolation_table(load(ISOLATION)),
        "@@POSTFIX_TABLE@@": postfix_table(load(POSTFIX)),
    }
    for marker, value in replacements.items():
        if marker not in text:
            raise SystemExit("report does not contain {}".format(marker))
        text = text.replace(marker, value)
    if arguments.commit:
        if "@@COMMIT@@" not in text:
            raise SystemExit("report does not contain @@COMMIT@@")
        text = text.replace("@@COMMIT@@", "`{}`".format(arguments.commit))
    elif "@@COMMIT@@" in text:
        print("warning: @@COMMIT@@ left in place (pass --commit <sha>)")
    target = arguments.out or OUTPUT
    if target.resolve() == REPORT.resolve():
        raise SystemExit("refusing to overwrite the authored report; pass --out <path>")
    target.parent.mkdir(parents=True, exist_ok=True)
    with open(target, "w", encoding="utf-8", newline="\r\n") as handle:
        handle.write(text)
    print("wrote {} ({} bytes)".format(target, target.stat().st_size))
    return 0


if __name__ == "__main__":
    sys.exit(main())
