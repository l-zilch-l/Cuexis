"""Derived analysis of the wait-handle samples: which kernel object a stalled thread waits on.

Input is the pair a `sample_wait.py` run leaves behind: the sample stream (`qpc_us,tid,rip,faults,
working_set,r10`) and the handle table (`value type name`). Nothing here measures anything new; it
re-reads recorded samples, so it can be re-run at any time without touching the evidence.

Key ideas:
  * The ntdll export table turns an instruction pointer into `syscall + offset`. An offset of 19..21
    is at or after the `syscall` instruction, so the thread was inside the kernel call; offsets 16
    to 18 are the last user-mode instructions before it.
  * R10 holds the first syscall argument for the whole call, so for a `NtWaitForSingleObject` sample
    it is the wait target handle, and the handle table names that object.
  * A stall cluster is a run of samples that never leaves one address bucket for at least
    `--cluster-min-us`; the cluster shares are the thing to compare between configurations.
"""

import argparse
import bisect
import json
import sys
from pathlib import Path

import pe_bytes

NTDLL_NAMES = {"ntdll.dll"}


def load_modules(path):
    """(base, name) list; later phases only add modules, so the first match wins per name."""
    rows = []
    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split()
            if len(parts) < 3:
                continue
            rows.append((int(parts[0], 16), parts[2]))
    rows.sort()
    return rows


class Modules:
    def __init__(self, rows):
        self.rows = rows
        self.bases = [row[0] for row in rows]
        self.by_name = {}
        for base, name in rows:
            self.by_name.setdefault(name.lower(), []).append(base)

    def locate(self, rip):
        index = bisect.bisect_right(self.bases, rip) - 1
        if index < 0:
            return "unmapped", rip
        base, name = self.rows[index]
        return name, rip - base

    def base_of(self, name):
        found = self.by_name.get(name.lower())
        return found[0] if found else None


def load_handles(path):
    table = {}
    if not path or not Path(path).exists():
        return table
    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split(None, 3)
            value = int(parts[0], 16)
            kind = parts[1] if len(parts) > 1 else "?"
            name = parts[2] if len(parts) > 2 else ""
            note = parts[3].strip() if len(parts) > 3 else ""
            table[value] = {"type": kind, "name": "" if name == "-" else name, "note": note}
    return table


PSEUDO = {
    0xFFFFFFFFFFFFFFFF: "pseudo-handle current process",
    0xFFFFFFFFFFFFFFFE: "pseudo-handle current thread",
}


def label(table, value):
    if value in PSEUDO:
        return PSEUDO[value]
    entry = table.get(value)
    if entry is None:
        return "unlabelled"
    kind = entry["type"]
    name = entry["name"]
    if name:
        return "{} {}".format(kind, name)
    return kind


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", required=True)
    parser.add_argument("--modules", required=True)
    parser.add_argument("--handles", default=None)
    parser.add_argument("--ntdll", default=r"C:\Windows\System32\ntdll.dll")
    parser.add_argument("--out", default=None)
    parser.add_argument("--top", type=int, default=12)
    parser.add_argument("--cluster-gap-us", type=int, default=1000)
    parser.add_argument("--cluster-min-us", type=int, default=1500)
    parser.add_argument("--cluster-min-samples", type=int, default=3)
    parser.add_argument("--kernel-offset", type=int, default=19,
                        help="first stub offset that is at or after the syscall instruction")
    args = parser.parse_args(argv)

    modules = Modules(load_modules(args.modules))
    handles = load_handles(args.handles)
    stubs = pe_bytes.stub_table(args.ntdll)
    ntdll_base = modules.base_of("ntdll.dll")

    threads = {}
    order = []
    rows = []
    with open(args.samples, "r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.strip().split(",")
            if len(parts) < 6:
                continue
            when = int(parts[0])
            tid = int(parts[1])
            rip = int(parts[2], 16)
            faults = int(parts[3])
            r10 = int(parts[5], 16)
            module, rva = modules.locate(rip)
            call = None
            if module.lower() in NTDLL_NAMES:
                found = pe_bytes.resolve(stubs, rva)
                if found:
                    call = {"name": found[0], "ssn": found[1], "offset": found[2]}
            rows.append({"us": when, "tid": tid, "rip": rip, "faults": faults, "r10": r10,
                         "module": module, "rva": rva, "call": call})
            if tid not in threads:
                threads[tid] = {"tid": tid, "samples": 0, "modules": {}, "calls": {},
                                "waits": {}, "kernel_calls": 0}
                order.append(tid)
            threads[tid]["samples"] += 1

    if not rows:
        raise SystemExit("no samples")

    def tally(bucket, key, amount=1):
        bucket[key] = bucket.get(key, 0) + amount

    for row in rows:
        thread = threads[row["tid"]]
        tally(thread["modules"], row["module"])
        if row["call"]:
            key = "{}ssn{}".format(row["call"]["name"], row["call"]["ssn"])
            tally(thread["calls"], key)
            if row["call"]["offset"] >= args.kernel_offset:
                thread["kernel_calls"] += 1
                tally(thread["waits"], label(handles, row["r10"]))

    # Main thread: the one that executes the most image code.
    def exe_samples(thread):
        return sum(count for name, count in thread["modules"].items() if name.endswith(".exe"))

    main_tid = max(order, key=lambda tid: exe_samples(threads[tid]))
    main = threads[main_tid]

    def share(bucket, total=None):
        total = total or (main["samples"] or 1)
        return {key: count / total for key, count in
                sorted(bucket.items(), key=lambda item: -item[1])}

    # Stall clusters on the main thread: one address bucket held for at least the minimum span.
    main_rows = [row for row in rows if row["tid"] == main_tid]
    clusters = []
    current = []
    for row in main_rows:
        if current and row["us"] - current[-1]["us"] > args.cluster_gap_us:
            clusters.append(current)
            current = []
        current.append(row)
    if current:
        clusters.append(current)
    stalled = []
    for cluster in clusters:
        span = cluster[-1]["us"] - cluster[0]["us"]
        buckets = {}
        for row in cluster:
            key = (row["module"], row["rva"] // 16 * 16)
            buckets[key] = buckets.get(key, 0) + 1
        bucket, count = max(buckets.items(), key=lambda item: item[1])
        if (span >= args.cluster_min_us and len(cluster) >= args.cluster_min_samples
                and count >= 0.5 * len(cluster)):
            stalled.append({"start_us": cluster[0]["us"], "span_us": span,
                            "samples": len(cluster), "bucket": bucket, "share": count / len(
                                cluster), "rows": cluster})

    def describe_bucket(bucket):
        module, rva = bucket
        text = "{} +{:#x}".format(module, rva)
        if module.lower() in NTDLL_NAMES:
            found = pe_bytes.resolve(stubs, rva)
            if found:
                text += " [{} ssn{} +{}]".format(found[0], found[1], found[2])
        return text

    stalled_samples = sum(item["samples"] for item in stalled)
    stalled_cover = sum(item["span_us"] for item in stalled)
    cluster_calls, cluster_waits, cluster_buckets = {}, {}, {}
    for item in stalled:
        for row in item["rows"]:
            tally(cluster_buckets, describe_bucket((row["module"], row["rva"] // 16 * 16)))
            if row["call"]:
                tally(cluster_calls, "{}ssn{}+{}".format(
                    row["call"]["name"], row["call"]["ssn"], row["call"]["offset"]))
                if row["call"]["offset"] >= args.kernel_offset:
                    tally(cluster_waits, label(handles, row["r10"]))

    # Page-fault rate inside and outside the stalled intervals, from the deduplicated series.
    series = []
    for row in main_rows:
        if series and series[-1][1] == row["faults"]:
            continue
        series.append((row["us"], row["faults"]))
    fault_series = [item[0] for item in series]

    def faults_between(low, high):
        left = bisect.bisect_left(fault_series, low)
        right = bisect.bisect_right(fault_series, high)
        if right - left < 1:
            return 0
        return max(0, series[min(right, len(series) - 1)][1] - series[left][1])

    inside_faults = 0
    inside_us = 0
    for item in stalled:
        inside_faults += faults_between(item["start_us"], item["start_us"] + item["span_us"])
        inside_us += item["span_us"]
    total_us = main_rows[-1]["us"] - main_rows[0]["us"]
    total_faults = series[-1][1] - series[0][1] if len(series) > 1 else 0
    outside_us = max(1, total_us - inside_us)
    outside_faults = max(0, total_faults - inside_faults)

    def per_ms(count, micro):
        return count / (micro / 1000.0) if micro else 0.0

    result = {
        "samples": {"rows": len(rows), "threads": len(threads)},
        "duration_us": total_us,
        "main_thread": main_tid,
        "main_thread_samples": main["samples"],
        "threads": [{"tid": tid, "samples": threads[tid]["samples"],
                     "module_share": share(threads[tid]["modules"], threads[tid]["samples"]),
                     "top_calls": dict(sorted(threads[tid]["calls"].items(),
                                              key=lambda item: -item[1])[:args.top]),
                     "kernel_call_share": threads[tid]["kernel_calls"] / threads[tid]["samples"]
                     if threads[tid]["samples"] else 0.0}
                    for tid in order],
        "main_module_share": share(main["modules"]),
        "main_calls": share(main["calls"]),
        "main_kernel_call_share": main["kernel_calls"] / (main["samples"] or 1),
        "main_waits": share(main["waits"]),
        "clusters": {
            "gap_us": args.cluster_gap_us,
            "min_span_us": args.cluster_min_us,
            "min_samples": args.cluster_min_samples,
            "count": len(stalled),
            "samples": stalled_samples,
            "covered_us": stalled_cover,
            "covered_share": stalled_cover / total_us if total_us else 0.0,
            "buckets": dict(sorted(cluster_buckets.items(), key=lambda item: -item[1])),
            "calls": dict(sorted(cluster_calls.items(), key=lambda item: -item[1])),
            "waits": dict(sorted(cluster_waits.items(), key=lambda item: -item[1])),
            "longest": [{"start_us": item["start_us"], "span_us": item["span_us"],
                         "samples": item["samples"], "bucket": describe_bucket(item["bucket"])}
                        for item in sorted(stalled, key=lambda item: -item["span_us"])[:10]],
        },
        "faults": {
            "total": total_faults,
            "inside_clusters": inside_faults,
            "outside_clusters": outside_faults,
            "inside_per_ms": per_ms(inside_faults, inside_us),
            "outside_per_ms": per_ms(outside_faults, outside_us),
            "cluster_time_share": inside_us / total_us if total_us else 0.0,
            "cluster_fault_share": inside_faults / total_faults if total_faults else 0.0,
        },
    }
    result["faults"]["ratio"] = (result["faults"]["inside_per_ms"]
                                 / result["faults"]["outside_per_ms"]) \
        if result["faults"]["outside_per_ms"] else None

    text = json.dumps(result, indent=2, ensure_ascii=False)
    if args.out:
        Path(args.out).write_text(text + "\n", encoding="utf-8", newline="\n")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
