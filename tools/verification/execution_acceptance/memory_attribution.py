"""Attribute an exact extended-observer partition to the canonical budget domains.

Every linked C++ allocation carries an allocation-origin tag; ``memory_domains.MAPPING`` turns the
origins into the budget domains declared by the reference profile. The attribution is only
meaningful when the partition is exact and has no untagged allocations: otherwise the observed
bytes would be a partial sum while the ceiling describes a whole domain.
"""
import argparse
import json
from pathlib import Path

from . import memory_domains

SCHEMA = "rpa.execution.memory.domain-comparison.v1"
SCOPE = ("Linked C++ new/delete only; direct malloc, SDL, driver, GPU and OS heaps are excluded, "
         "so a domain comparison never impersonates a process working-set or commit measurement.")


def domain_allocations(text, label, domain):
    """Read the optional per-allocation trace of one budget domain at one checkpoint.

    The trace is emitted only when ``CUEXIS_RPA_MEMORY_TRACE_DOMAIN`` names the domain, so a
    missing trace is an error rather than an empty result: silence must not look like "no
    allocations". The summary binds the parsed rows to the live count and byte total.
    """
    rows = [row for row in memory_domains.records(text, "RPA_MEMORY_DOMAIN_ALLOCATION ")
            if row.get("label") == label and row.get("domain") == domain]
    summaries = [row for row in memory_domains.records(
        text, "RPA_MEMORY_DOMAIN_ALLOCATION_SUMMARY ")
        if row.get("label") == label and row.get("domain") == domain]
    if len(summaries) != 1:
        raise ValueError(f"expected one domain allocation summary for {label}/{domain}")
    summary = summaries[0]
    emitted = memory_domains.unsigned(summary, "emitted")
    live = memory_domains.unsigned(summary, "live_count")
    total = memory_domains.unsigned(summary, "requested_cpp_bytes")
    if emitted != len(rows):
        raise ValueError("emitted count disagrees with the parsed allocation rows")
    if summary.get("truncated") is True:
        if live <= emitted:
            raise ValueError("truncated summary must report more live allocations than emitted")
    elif live != emitted:
        raise ValueError("untruncated summary must carry every live allocation")
    sizes = {}
    for row in rows:
        sizes[memory_domains.unsigned(row, "requested_cpp_bytes")] = \
            sizes.get(memory_domains.unsigned(row, "requested_cpp_bytes"), 0) + 1
    return {"schema": "rpa.execution.memory.domain-allocations.v1", "label": label,
            "domain": domain, "live_count": live, "requested_cpp_bytes": total,
            "emitted": emitted, "truncated": summary.get("truncated") is True,
            "sizes": dict(sorted(sizes.items()))}


def attribute(text, label):
    """Aggregate one exact checkpoint's domain cells by canonical budget domain."""
    partitions = [row for row in memory_domains.records(text, "RPA_MEMORY_PARTITION ")
                  if row.get("label") == label]
    if len(partitions) != 1:
        raise ValueError("expected exactly one partition at " + label)
    partition = partitions[0]
    if (partition.get("schema") != "rpa.execution.memory.partition.v1" or
            partition.get("exact") is not True or partition.get("exceeded") is not False or
            partition.get("untagged_live_count") != 0):
        raise ValueError("partition is not an exact tagged partition: " + label)
    domains, owners, cells = {}, {}, 0
    for row in memory_domains.records(text, "RPA_MEMORY_DOMAIN "):
        if row.get("label") != label:
            continue
        if row.get("schema") != "rpa.execution.memory.domain.v1":
            raise ValueError("unexpected domain schema at " + label)
        if row.get("owner") not in memory_domains.OWNERS:
            raise ValueError("unregistered owner: " + str(row.get("owner")))
        domain = memory_domains.MAPPING.get(row.get("domain"))
        if domain is None:
            raise ValueError("unmapped allocation origin: " + str(row.get("domain")))
        bucket = domains.setdefault(domain, {"requested_cpp_bytes": 0,
                                             "crt_usable_storage_bytes": 0, "live_count": 0})
        bucket["requested_cpp_bytes"] += memory_domains.unsigned(row, "requested_cpp_bytes")
        bucket["crt_usable_storage_bytes"] += memory_domains.unsigned(row, "crt_usable_storage_bytes")
        bucket["live_count"] += memory_domains.unsigned(row, "live_count")
        per_owner = owners.setdefault(domain, {})
        per_owner[row["owner"]] = (per_owner.get(row["owner"], 0) +
                                   memory_domains.unsigned(row, "requested_cpp_bytes"))
        cells += 1
    observed = sum(bucket["requested_cpp_bytes"] for bucket in domains.values())
    if observed != partition.get("sum_requested_cpp_bytes"):
        raise ValueError("domain cells do not add up to the exact partition")
    if sum(bucket["live_count"] for bucket in domains.values()) != partition.get("sum_live_count"):
        raise ValueError("domain counts do not add up to the exact partition")
    return {"label": label, "partition": partition, "cells": cells, "domains": domains,
            "owners": owners, "observed_requested_cpp_bytes": observed,
            "unclassified_requested_cpp_bytes":
                domains.get("unclassified", {}).get("requested_cpp_bytes", 0)}


def compare(attribution, profile):
    """Compare every declared domain with the observed bytes of the same canonical domain."""
    rows = []
    for domain in profile["domains"]:
        if domain.get("alias_of"):
            continue
        bucket = attribution["domains"].get(
            domain["id"], {"requested_cpp_bytes": 0, "crt_usable_storage_bytes": 0, "live_count": 0})
        ceiling = domain["requested_bytes"]
        if domain.get("device"):
            status = "device_domain_not_measured"
        elif bucket["requested_cpp_bytes"] <= ceiling:
            status = "within_declared"
        else:
            status = "over_declared"
        rows.append({"id": domain["id"], "ceiling_bytes": ceiling,
                     "observed_requested_cpp_bytes": bucket["requested_cpp_bytes"],
                     "observed_crt_usable_storage_bytes": bucket["crt_usable_storage_bytes"],
                     "observed_live_count": bucket["live_count"],
                     "ratio": (bucket["requested_cpp_bytes"] / ceiling) if ceiling else None,
                     "status": status, "lifetime": domain.get("lifetime"),
                     "owners": attribution["owners"].get(domain["id"], {})})
    return {"schema": SCHEMA, "profile": profile["id"], "label": attribution["label"],
            "observed_requested_cpp_bytes": attribution["observed_requested_cpp_bytes"],
            "unclassified_requested_cpp_bytes": attribution["unclassified_requested_cpp_bytes"],
            "domains": rows, "over_declared": [row for row in rows
                                               if row["status"] == "over_declared"],
            "partition": attribution["partition"], "scope": SCOPE,
            "acceptance": "comparison alone is not formal acceptance; reference design caps "
                          "require their exact O1 acceptance binding, workload/observer admission "
                          "and applicable lifecycle evidence; revised or production caps need "
                          "a separate owner record"}


def load_profile(path, profile_id):
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    return next(row for row in document["profiles"] if row["id"] == profile_id)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--label", default="active")
    parser.add_argument("--budget", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    document = compare(attribute(args.log.read_text(encoding="utf-8", errors="strict"), args.label),
                       load_profile(args.budget, args.profile))
    args.output.write_text(json.dumps(document, ensure_ascii=False, indent=1) + "\n",
                           encoding="utf-8")
    print(json.dumps({"profile": document["profile"],
                      "over_declared": [row["id"] for row in document["over_declared"]]},
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
