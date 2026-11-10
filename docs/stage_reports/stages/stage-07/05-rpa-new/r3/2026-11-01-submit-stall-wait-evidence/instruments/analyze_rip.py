"""Analyse an instruction pointer sample stream.

Inputs are the two files sample_rip.py writes: the sample stream and the module snapshot. Outputs
module shares per thread, the hottest instruction addresses of the thread that runs the measured
loop, and the instruction addresses that dominate multi-millisecond blocks. Page fault counters
captured during sampling are used to test whether a block coincides with a page fault burst.

Names for addresses inside system modules come from their export tables, read through
llvm-readobj; an address inside a DLL without a matching export keeps its raw module offset.
"""

import argparse
import bisect
import json
import os
import subprocess
import sys

LLVM_READOBJ = r"C:\Program Files\LLVM\bin\llvm-readobj.exe"


def load_modules(path):
    modules = []
    seen = set()
    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ", 3)
            base = int(parts[0], 16)
            size = int(parts[1])
            name = parts[2]
            image = parts[3] if len(parts) > 3 else ""
            key = (base, size, name)
            if key in seen:
                continue
            seen.add(key)
            modules.append({"base": base, "size": size, "name": name, "path": image})
    modules.sort(key=lambda item: item["base"])
    return modules


def load_samples(path):
    rows = []
    with open(path, "r", encoding="utf-8") as stream:
        for line in stream:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) != 5:
                continue
            rows.append((int(parts[0]), int(parts[1]), int(parts[2], 16),
                         int(parts[3]), int(parts[4])))
    return rows


def export_table(path, cache):
    """Name/address pairs from a module export table, computed once per module."""
    key = os.path.basename(path).lower()
    if key in cache:
        return cache[key]
    names = []
    if path and os.path.exists(path):
        try:
            completed = subprocess.run([LLVM_READOBJ, "--coff-exports", path],
                                       capture_output=True, text=True, timeout=180)
        except (OSError, subprocess.SubprocessError):
            completed = None
        if completed is not None and completed.returncode == 0:
            pending = None
            for line in completed.stdout.splitlines():
                line = line.strip()
                if line.startswith("Name: "):
                    pending = line.split(": ", 1)[1]
                elif line.startswith("RVA: 0x") and pending:
                    names.append((int(line.split("0x", 1)[1], 16), pending))
                    pending = None
            names.sort()
    cache[key] = names
    return names


def nearest_export(names, rva):
    if not names:
        return None, None
    values = [item[0] for item in names]
    index = bisect.bisect_right(values, rva) - 1
    if index < 0:
        return None, None
    return names[index][1], rva - names[index][0]


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", required=True)
    parser.add_argument("--modules", required=True)
    parser.add_argument("--out", default=None)
    parser.add_argument("--cluster-gap-us", type=int, default=1000)
    parser.add_argument("--cluster-min-us", type=int, default=1500)
    parser.add_argument("--cluster-min-samples", type=int, default=3)
    parser.add_argument("--top", type=int, default=12)
    args = parser.parse_args(argv)

    modules = load_modules(args.modules)
    rows = load_samples(args.samples)
    if not rows:
        raise SystemExit("no samples")
    bases = [item["base"] for item in modules]
    spans = [item["base"] + item["size"] for item in modules]

    def locate(rip):
        index = bisect.bisect_right(bases, rip) - 1
        if index >= 0 and rip < spans[index]:
            module = modules[index]
            return module["name"].lower(), rip - module["base"], module
        return None, None, None

    per_thread = {}
    for entry in rows:
        stamp, tid, rip, faults, working = entry
        name, rva, module = locate(rip)
        bucket = per_thread.setdefault(tid, {"samples": 0, "modules": {}, "addresses": {},
                                             "rows": [], "faults_last": 0})
        bucket["samples"] += 1
        key = name if name else "unmapped"
        bucket["modules"][key] = bucket["modules"].get(key, 0) + 1
        address = (key, rva if rva is not None else rip)
        bucket["addresses"][address] = bucket["addresses"].get(address, 0) + 1
        bucket["rows"].append((stamp, key, rva, faults, working))
        bucket["faults_last"] = faults

    exe_name = None
    for module in modules:
        if module["name"].lower().endswith(".exe"):
            exe_name = module["name"].lower()
            break
    main_tid = None
    best = -1
    for tid, bucket in per_thread.items():
        count = bucket["modules"].get(exe_name, 0)
        if count > best:
            best = count
            main_tid = tid

    summary = {
        "samples": len(rows),
        "duration_us": rows[-1][0] - rows[0][0],
        "modules_loaded": len(modules),
        "exe": exe_name,
        "threads": {},
    }
    for tid, bucket in sorted(per_thread.items(), key=lambda pair: -pair[1]["samples"]):
        summary["threads"][str(tid)] = {
            "samples": bucket["samples"],
            "module_share": {key: round(value / bucket["samples"], 6)
                             for key, value in sorted(bucket["modules"].items(),
                                                      key=lambda pair: -pair[1])},
        }

    cache = {}
    thread_rows = per_thread[main_tid]["rows"]
    thread_rows.sort()
    addresses = per_thread[main_tid]["addresses"]
    total = per_thread[main_tid]["samples"]
    hottest = sorted(addresses.items(), key=lambda pair: -pair[1])[:args.top]
    hot = []
    for (name, rva), count in hottest:
        module = next((item for item in modules if item["name"].lower() == name), None)
        symbol, offset = None, None
        if module is not None and name and not name.endswith(".exe"):
            symbol, offset = nearest_export(export_table(module["path"], cache), rva)
        hot.append({
            "module": name,
            "rva": rva,
            "samples": count,
            "share": round(count / total, 6),
            "base": module["base"] if module else None,
            "symbol": symbol,
            "symbol_offset": offset,
        })

    clusters = []
    current = None
    for stamp, name, rva, faults, working in thread_rows:
        signature = (name, (rva // 64 * 64) if rva is not None else None)
        if current is not None and signature == current["signature"] \
                and stamp - current["last"] <= args.cluster_gap_us:
            current["last"] = stamp
            current["samples"] += 1
            current["members"].append((stamp, name, rva))
            continue
        if current is not None:
            clusters.append(current)
        current = {"signature": signature, "first": stamp, "last": stamp, "samples": 1,
                   "members": [(stamp, name, rva)]}
    if current is not None:
        clusters.append(current)

    stalled = [item for item in clusters
               if item["last"] - item["first"] >= args.cluster_min_us
               and item["samples"] >= args.cluster_min_samples]
    stalled.sort(key=lambda item: -(item["last"] - item["first"]))
    covered = sum(item["last"] - item["first"] for item in stalled)
    module_totals = {}
    for item in stalled:
        name = item["signature"][0]
        module_totals[name] = module_totals.get(name, 0) + (item["last"] - item["first"])
    member_modules = {}
    member_addresses = {}
    member_count = 0
    for item in stalled:
        for _stamp, name, rva in item["members"]:
            member_count += 1
            member_modules[name] = member_modules.get(name, 0) + 1
            key = (name, rva // 64 * 64 if rva is not None else None)
            member_addresses[key] = member_addresses.get(key, 0) + 1
    background_modules = {}
    for _stamp, name, _rva, _f, _w in thread_rows:
        background_modules[name] = background_modules.get(name, 0) + 1
    member_top = []
    for (name, bucket), count in sorted(member_addresses.items(),
                                        key=lambda pair: -pair[1])[:args.top]:
        module = next((entry for entry in modules if entry["name"].lower() == name), None)
        symbol, offset = None, None
        if module is not None and bucket is not None and name and not name.endswith(".exe"):
            symbol, offset = nearest_export(export_table(module["path"], cache), bucket)
        member_top.append({
            "module": name,
            "rva": bucket,
            "symbol": symbol,
            "symbol_offset": offset,
            "samples": count,
            "share_of_cluster_samples": round(count / member_count, 6) if member_count else None,
        })
    top_clusters = []
    for item in stalled[:args.top]:
        name, bucket_rva = item["signature"]
        module = next((entry for entry in modules if entry["name"].lower() == name), None)
        symbol, offset = None, None
        if module is not None and bucket_rva is not None and name and not name.endswith(".exe"):
            symbol, offset = nearest_export(export_table(module["path"], cache), bucket_rva)
        top_clusters.append({
            "module": name,
            "rva": bucket_rva,
            "symbol": symbol,
            "symbol_offset": offset,
            "span_us": item["last"] - item["first"],
            "samples": item["samples"],
        })

    fault_series = []
    for stamp, _n, _r, faults, _w in thread_rows:
        if fault_series and fault_series[-1][1] == faults:
            continue
        fault_series.append((stamp, faults))
    fault_summary = None
    if len(fault_series) > 1:
        fault_times = [item[0] for item in fault_series]

        def faults_between(start, end):
            left = max(0, bisect.bisect_right(fault_times, start) - 1)
            right = min(len(fault_series) - 1, bisect.bisect_left(fault_times, end))
            return max(0, fault_series[right][1] - fault_series[left][1])

        rates = []
        for index in range(1, len(fault_series)):
            span = fault_series[index][0] - fault_series[index - 1][0]
            if span < 200:
                continue
            rates.append((fault_series[index][1] - fault_series[index - 1][1]) / (span / 1000.0))
        rates.sort()
        cluster_rates = []
        cluster_faults = 0
        for item in stalled:
            span = item["last"] - item["first"]
            if span <= 0:
                continue
            inside = faults_between(item["first"], item["last"])
            cluster_faults += inside
            cluster_rates.append(inside / (span / 1000.0))
        cluster_rates.sort()
        total_faults = fault_series[-1][1] - fault_series[0][1]
        fault_summary = {
            "snapshots": len(fault_series),
            "total_faults": total_faults,
            "median_per_ms": rates[len(rates) // 2] if rates else None,
            "p95_per_ms": rates[int(0.95 * (len(rates) - 1))] if rates else None,
            "max_per_ms": rates[-1] if rates else None,
            "intervals": len(rates),
            "cluster_faults": cluster_faults,
            "cluster_fault_share": round(cluster_faults / total_faults, 6) if total_faults else None,
            "cluster_time_share": round(covered / (summary["duration_us"] or 1), 6),
            "cluster_median_per_ms": cluster_rates[len(cluster_rates) // 2] if cluster_rates else None,
            "cluster_max_per_ms": cluster_rates[-1] if cluster_rates else None,
            "cluster_ratio": (round(cluster_rates[len(cluster_rates) // 2]
                                    / (rates[len(rates) // 2] or 1.0), 3)
                              if cluster_rates and rates and rates[len(rates) // 2] else None),
        }

    summary["main_thread"] = main_tid
    summary["main_thread_samples"] = total
    summary["hot_addresses"] = hot
    summary["clusters"] = {
        "gap_us": args.cluster_gap_us,
        "min_span_us": args.cluster_min_us,
        "min_samples": args.cluster_min_samples,
        "count": len(clusters),
        "stalled_count": len(stalled),
        "stalled_covered_us": covered,
        "stalled_covered_share": round(covered / (summary["duration_us"] or 1), 6),
        "stalled_module_us": module_totals,
        "background_module_share": {key: round(value / total, 6)
                                    for key, value in sorted(background_modules.items(),
                                                             key=lambda pair: -pair[1])},
        "cluster_module_share": {key: round(value / member_count, 6)
                                 for key, value in sorted(member_modules.items(),
                                                          key=lambda pair: -pair[1])}
        if member_count else {},
        "cluster_samples": member_count,
        "top": top_clusters,
        "cluster_top_addresses": member_top,
    }
    summary["faults"] = fault_summary
    text = json.dumps(summary, indent=2)
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(text + "\n")
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
