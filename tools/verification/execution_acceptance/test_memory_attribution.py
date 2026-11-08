import json
import tempfile
import unittest
from pathlib import Path

from tools.verification.execution_acceptance import memory_attribution


def partition(label="active", **overrides):
    row = {"schema": "rpa.execution.memory.partition.v1", "label": label, "exact": True,
           "sum_live_count": 30, "global_live_count": 30, "sum_requested_cpp_bytes": 3000,
           "global_requested_cpp_bytes": 3000, "sum_crt_usable_storage_bytes": 3200,
           "global_crt_usable_storage_bytes": 3200, "cells": 3, "untagged_live_count": 0,
           "exceeded": False, "coverage": "linked C++ new/delete only"}
    row.update(overrides)
    return "RPA_MEMORY_PARTITION " + json.dumps(row)


def cell(owner, origin, requested, storage, count=10, label="active"):
    return "RPA_MEMORY_DOMAIN " + json.dumps({
        "schema": "rpa.execution.memory.domain.v1", "label": label, "owner": owner,
        "domain": origin, "cell": owner + "." + origin, "requested_cpp_bytes": requested,
        "crt_usable_storage_bytes": storage, "live_count": count})


def log(*rows):
    return "\n".join(rows) + "\n"


def profile(domains):
    return {"id": "r4-reference-n64-v1", "domains": domains}


class AttributionTests(unittest.TestCase):
    def test_canonical_domains_merge_origins_and_owners(self):
        text = log(partition(),
                   cell("playback-worker", "world", 1000, 1100, 10),
                   cell("main", "world", 500, 550, 10),
                   cell("main", "command-session", 1500, 1550, 10))
        value = memory_attribution.attribute(text, "active")
        self.assertEqual(value["domains"]["state.world"]["requested_cpp_bytes"], 1500)
        self.assertEqual(value["owners"]["state.world"],
                         {"playback-worker": 1000, "main": 500})
        self.assertEqual(value["observed_requested_cpp_bytes"], 3000)
        self.assertEqual(value["cells"], 3)

    def test_comparison_marks_over_and_device_domains(self):
        text = log(partition(sum_live_count=20, global_live_count=20), cell("main", "world", 1000, 1100),
                   cell("main", "command-session", 2000, 2100))
        attribution = memory_attribution.attribute(text, "active")
        document = memory_attribution.compare(attribution, profile([
            {"id": "state.world", "requested_bytes": 500},
            {"id": "command.session", "requested_bytes": 4096},
            {"id": "gpu", "requested_bytes": 1024, "device": True},
            {"id": "delivery.ready", "requested_bytes": None, "alias_of": "delivery.payload"}]))
        statuses = {row["id"]: row["status"] for row in document["domains"]}
        self.assertEqual(statuses["state.world"], "over_declared")
        self.assertEqual(statuses["command.session"], "within_declared")
        self.assertEqual(statuses["gpu"], "device_domain_not_measured")
        self.assertNotIn("delivery.ready", statuses)
        self.assertEqual([row["id"] for row in document["over_declared"]], ["state.world"])

    def test_an_inexact_or_untagged_partition_is_rejected(self):
        for broken in (partition(exact=False), partition(untagged_live_count=2),
                       partition(exceeded=True), partition(schema="other")):
            with self.subTest(broken=broken), self.assertRaises(ValueError):
                memory_attribution.attribute(log(broken, cell("main", "world", 3000, 3200)),
                                             "active")

    def test_a_missing_or_unknown_cell_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unmapped allocation origin"):
            memory_attribution.attribute(log(partition(), cell("main", "nonsense", 3000, 3200)),
                                         "active")
        with self.assertRaisesRegex(ValueError, "do not add up"):
            memory_attribution.attribute(log(partition(), cell("main", "world", 10, 10)), "active")
        with self.assertRaisesRegex(ValueError, "unregistered owner"):
            memory_attribution.attribute(log(partition(), cell("ghost", "world", 3000, 3200)),
                                         "active")

    def test_cli_writes_the_comparison(self):
        with tempfile.TemporaryDirectory() as root:
            log_path, budget_path = Path(root) / "log.txt", Path(root) / "budget.json"
            log_path.write_text(log(partition(sum_live_count=10, global_live_count=10,
                                              sum_requested_cpp_bytes=1000,
                                              global_requested_cpp_bytes=1000,
                                              sum_crt_usable_storage_bytes=1100,
                                              global_crt_usable_storage_bytes=1100,
                                              cells=1),
                                    cell("main", "world", 1000, 1100)), encoding="utf-8")
            budget_path.write_text(json.dumps({"profiles": [profile([
                {"id": "state.world", "requested_bytes": 500}])]}), encoding="utf-8")
            output = Path(root) / "out.json"
            import sys
            argv = sys.argv
            sys.argv = ["memory_attribution", "--log", str(log_path), "--budget",
                        str(budget_path), "--profile", "r4-reference-n64-v1",
                        "--output", str(output)]
            try:
                memory_attribution.main()
            finally:
                sys.argv = argv
            document = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(document["over_declared"][0]["id"], "state.world")


if __name__ == "__main__":
    unittest.main()
