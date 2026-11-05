"""Attribute extended execution observer records without changing the historical PD5 parser."""
import json
from pathlib import Path
import re

from .budgets import REQUIRED_DOMAINS


OWNERS = {"probe", "main", "playback-worker", "renderer-owner", "audio-owner", "audio-worker"}
MAPPING_SOURCE = Path(__file__).resolve().parents[3] / "engine/core/internal/cuexis_internal/execution_memory_domains.def"
MAPPING = {origin: budget for origin, budget in re.findall(
    r'CUEXIS_MEMORY_DOMAIN\("([^"\n]+)", "([^"\n]+)", \d+\)',
    MAPPING_SOURCE.read_text(encoding="ascii"))}
if len(MAPPING) != 30:
    raise ValueError("incomplete extended observer domain mapping")



def records(text, prefix):
    return [json.loads(line[len(prefix):]) for line in text.splitlines() if line.startswith(prefix)]


def unsigned(row, key):
    value = row.get(key)
    if type(value) is not int or value < 0:
        raise ValueError("non-integer or negative observer field: " + key)
    return value


def collect(text):
    global_rows = records(text, "RPA_MEMORY ")
    partitions = records(text, "RPA_MEMORY_PARTITION ")
    cells = records(text, "RPA_MEMORY_DOMAIN ")
    aggregates = records(text, "RPA_MEMORY_AGGREGATE ")
    products = records(text, "RPA_MEMORY_PRODUCT ")
    if not global_rows or not partitions:
        raise ValueError("missing memory checkpoint or exact partition")
    labels = [row.get("label") for row in partitions]
    if any(not isinstance(label, str) or not label for label in labels) or len(labels) != len(set(labels)):
        raise ValueError("missing or duplicate checkpoint label")
    globals_by_label = {row["label"]: row for row in global_rows}
    if len(globals_by_label) != len(global_rows) or set(globals_by_label) != set(labels):
        raise ValueError("global and partition checkpoints differ")
    result = []
    for partition in partitions:
        label = partition["label"]
        global_row = globals_by_label[label]
        if (partition.get("schema") != "rpa.execution.memory.partition.v1" or
                global_row.get("schema") != "rpa.execution.memory.v1" or
                partition.get("exact") is not True or global_row.get("exceeded") is not False):
            raise ValueError("observer range or exactness invalid")
        selected = [row for row in cells if row.get("label") == label]
        keys = [(row.get("owner"), row.get("domain")) for row in selected]
        if len(set(keys)) != len(keys) or partition.get("cells") != len(selected):
            raise ValueError("duplicate or missing domain cell")
        for row in selected:
            if row.get("schema") != "rpa.execution.memory.domain.v1":
                raise ValueError("unexpected domain schema")
            owner, domain = row.get("owner"), row.get("domain")
            if row.get("cell") != f"{owner}.{domain}":
                raise ValueError("owner/domain identity mismatch")
            if owner == "unknown" or owner not in OWNERS:
                raise ValueError("unknown allocation owner")
            if owner == "probe":
                row["budget_domain"] = "fixture"
            elif domain in MAPPING and MAPPING[domain] != "unclassified":
                row["budget_domain"] = MAPPING[domain]
            else:
                raise ValueError("unrated product allocation domain: " + str(domain))
            for live, peak in (("live_count", "peak_live_count"),
                               ("requested_cpp_bytes", "peak_requested_cpp_bytes"),
                               ("crt_usable_storage_bytes", "peak_crt_usable_storage_bytes")):
                if unsigned(row, live) > unsigned(row, peak):
                    raise ValueError("live value exceeds observed peak")
            if unsigned(row, "requested_cpp_bytes") > unsigned(row, "crt_usable_storage_bytes"):
                raise ValueError("requested exceeds actual storage charge")
            unsigned(row, "oldest_live_age_us")
        for local, global_key, sum_key in (
                ("live_count", "global_live_count", "sum_live_count"),
                ("requested_cpp_bytes", "global_requested_cpp_bytes", "sum_requested_cpp_bytes"),
                ("crt_usable_storage_bytes", "global_crt_usable_storage_bytes", "sum_crt_usable_storage_bytes")):
            total = sum(unsigned(row, local) for row in selected)
            if (total != unsigned(partition, sum_key) or total != unsigned(partition, global_key) or
                    total != unsigned(global_row, local)):
                raise ValueError("domain/global books disagree")
        if unsigned(partition, "untagged_live_count"):
            raise ValueError("untagged live allocations")
        grouped = {}
        for row in selected:
            grouped.setdefault(row["budget_domain"], []).append(row)
        aggregate_rows = [row for row in aggregates if row.get("label") == label]
        product_rows = [row for row in products if row.get("label") == label]
        if len(product_rows) != 1 or len({row.get("domain") for row in aggregate_rows}) != len(aggregate_rows):
            raise ValueError("missing product book or duplicate aggregate")
        if {row.get("domain") for row in aggregate_rows} != set(grouped):
            raise ValueError("aggregate domain coverage differs from owner/domain books")
        for row in aggregate_rows:
            if row.get("schema") != "rpa.execution.memory.aggregate.v1":
                raise ValueError("unexpected aggregate schema")
            for key in ("live_count", "requested_cpp_bytes", "crt_usable_storage_bytes"):
                if unsigned(row, key) != sum(unsigned(cell, key) for cell in grouped[row["domain"]]):
                    raise ValueError("aggregate and owner/domain totals disagree")
            for key in ("requested_cpp_bytes", "crt_usable_storage_bytes"):
                if unsigned(row, "peak_" + key) < row[key]:
                    raise ValueError("aggregate live value exceeds simultaneous peak")
        product = product_rows[0]
        if product.get("schema") != "rpa.execution.memory.product.v1":
            raise ValueError("unexpected product book schema")
        for key in ("requested_cpp_bytes", "crt_usable_storage_bytes"):
            total = sum(unsigned(row, key) for row in aggregate_rows if row["domain"] != "fixture")
            if unsigned(product, key) != total or unsigned(product, "peak_" + key) < total:
                raise ValueError("product aggregate is not exact")
        result.append({"label": label, "global": global_row, "cells": selected,
                       "aggregates": aggregate_rows, "product": product})
    if any(row.get("label") not in labels for row in cells):
        raise ValueError("orphan memory domain")
    return {"schema": "rpa.execution.memory.domains.v1", "checkpoints": result}


def rate(document, profile):
    caps = {row["id"]: row for row in profile["domains"]}
    if set(caps) != REQUIRED_DOMAINS:
        raise ValueError("incomplete accepted profile")
    failures = []
    for checkpoint in document["checkpoints"]:
        for row in checkpoint["aggregates"]:
            domain = row["domain"]
            limit = (profile["fixture_requested_ceiling"] if domain == "fixture"
                     else caps[domain]["requested_bytes"])
            if limit is None:
                raise ValueError("allocation assigned to an uncharged alias")
            if row["peak_requested_cpp_bytes"] > limit:
                failures.append({"label": checkpoint["label"], "domain": domain,
                                 "simultaneous_peak_bytes": row["peak_requested_cpp_bytes"],
                                 "limit_bytes": limit, "status": "allocation_origin_ceiling_exceeded"})
        for field, cap in (("peak_requested_cpp_bytes", "cpu_requested_envelope"),
                           ("peak_crt_usable_storage_bytes", "cpu_storage_envelope")):
            if checkpoint["product"][field] > profile[cap]:
                failures.append({"label": checkpoint["label"], "domain": field,
                                 "status": "over_budget"})
        process = checkpoint["global"]
        for field, cap in (("private_commit_bytes", "process_private_commit_ceiling"),
                           ("working_set_bytes", "process_working_set_ceiling")):
            if unsigned(process, field) > profile[cap]:
                failures.append({"label": checkpoint["label"], "domain": field,
                                 "status": "machine_guard_exceeded"})
    return {"schema": "rpa.execution.memory.rating.v1", "failures": failures,
            "status": "partial" if not failures else "unrated",
            "formal_acceptance": False,
            "interpretation": "Allocation-origin totals are exact; semantic ownership and lifetime closure must be independently admitted before product-domain verdicts.",
            "missing_evidence": ["owner references, legal handles and retained lifetime attribution",
                                 "GPU backend bytes and independent driver delta",
                                 "final payload/attempt/device release and bounded steady domains"]}
