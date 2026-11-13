"""Capacity evidence rejects backend substitution, incomplete GPU release and missing telemetry."""
import copy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from . import capacity_evidence as evidence
from . import prepare_capacity
from .binding import artifact, read, write_once
from .test_memory_domains import captured, render


def workflow_log(*, retained=False, released=True, world=True):
    lines = []
    labels = ("before-load", "after-content-prepare", "after-host-load-prepare", "active",
              "after-close", "after-fixture-release")
    for label in labels:
        empty = label == "after-fixture-release" and not retained
        for line in render(captured()).splitlines():
            prefix, raw = line.split(" ", 1)
            row = json.loads(raw)
            row["label"] = label
            if empty:
                for key in ("live_count", "requested_cpp_bytes", "crt_usable_storage_bytes",
                            "sum_live_count", "global_live_count", "sum_requested_cpp_bytes",
                            "global_requested_cpp_bytes", "sum_crt_usable_storage_bytes",
                            "global_crt_usable_storage_bytes", "oldest_live_age_us"):
                    if key in row:
                        row[key] = 0
            if prefix == "RPA_MEMORY":
                row.update(private_commit_bytes=1000, working_set_bytes=1000)
            if prefix == "RPA_MEMORY_PARTITION":
                row["exceeded"] = False
            if prefix == "RPA_MEMORY_PRODUCT":
                row.update(observer_header_size=64, observer_header_bytes=64,
                           observer_static_bytes=128, observer_checkpoint_stack_bytes=64)
            lines.append(prefix + " " + json.dumps(row))
    for label in ("active", "after-close", "after-fixture-release"):
        active = label == "active"
        root = {"label": label, "root_id": 7, "valid": True, "live_count": int(active),
                "requested_cpp_bytes": 100 if active else 0,
                "crt_usable_storage_bytes": 160 if active else 0,
                "phase": "active" if active or not released else "released",
                "retired_us": 100, "released_us": 200}
        lines.append("RPA_MEMORY_SEMANTIC_ROOT " + json.dumps(root))
    lines.append("RPA_MEMORY_SEMANTIC_ALLOCATION " + json.dumps({
        "label": "active", "root_id": 7, "allocation_id": 42,
        "requested_cpp_bytes": 100, "crt_usable_storage_bytes": 160}))
    if world:
        proof = {"schema": "rpa.execution.memory.world-proof.v1", "census_id": 1,
                 "root_id": 7, "phase": "active", "stamp_us": 100, "domain": "state.world",
                 "bound_kind": "instantaneous_unique_lower_bound", "complete_world_graph": False,
                 "valid": True, "live_count": 1, "lower_bound_requested_cpp_bytes": 100,
                 "backing_requested_cpp_bytes": 100, "backing_crt_usable_storage_bytes": 160,
                 "oldest_live_age_us": 10, "census_stack_bytes": 1024}
        block = {"schema": "rpa.execution.memory.world-allocation.v1", "census_id": 1,
                 "allocation_id": 42, "root_id": 7, "requested_cpp_bytes": 100,
                 "crt_usable_storage_bytes": 160, "charged_cpp_bytes": 100,
                 "age_us": 10, "origin_cell": 80, "membership": "actual_component_page"}
        span = {"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                "allocation_id": 42, "offset": 0, "bytes": 100}
        for prefix, row in (("RPA_MEMORY_WORLD_PROOF", proof),
                            ("RPA_MEMORY_WORLD_ALLOCATION", block), ("RPA_MEMORY_WORLD_SPAN", span)):
            lines.append(prefix + " " + json.dumps(row))
    return "\n".join(lines)


class CapacityEvidenceTests(unittest.TestCase):
    def test_capacity_requires_the_declared_backend_and_exact_result(self):
        identity = {"schema": "pd5.capacity.v1", "entities": 2048, "requirements": 2048,
                    "reuse": "high", "gpu": True, "packed_bytes": 100,
                    "snapshot_bytes": 200, "frame_byte_bound": 300,
                    "packed_sha256": "a" * 64, "snapshot_sha256": "b" * 64,
                    **{key: True for key in ("snapshot_valid", "decoded_snapshot_valid",
                                             "advance_executed", "replay_evidence_valid",
                                             "replay_equal", "closed")}}
        text = "PD5_CAPACITY " + json.dumps(identity)
        self.assertEqual(evidence.capacity(text, 2048, 2048, "high", True), identity)
        for changes in ({"gpu": False}, {"replay_equal": False}, {"snapshot_sha256": ""},
                        {"requirements": 64}, {"closed": 1}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                evidence.capacity("PD5_CAPACITY " + json.dumps(dict(identity, **changes)),
                                  2048, 2048, "high", True)
        with self.assertRaises(ValueError):
            evidence.capacity(text + "\n" + text, 2048, 2048, "high", True)

    def test_gpu_release_and_tool_range_are_independent_from_budget(self):
        row = {"label": "after-device-release", "live_count": 0, "peak_count": 2,
               "limit_count": 8192, "logical_storage_bytes": 0,
               "peak_logical_storage_bytes": 100, "limit_logical_storage_bytes": 256,
               "gpu_drained": True, "exceeded": False}
        text = "PD5_GPU_MEMORY " + json.dumps(row)
        self.assertEqual(evidence.gpu_storage(text, 64)["status"], "over_reference_ceiling")
        for changed in ({"live_count": 1}, {"gpu_drained": False}, {"exceeded": True},
                        {"peak_logical_storage_bytes": 300}):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                evidence.gpu_storage("PD5_GPU_MEMORY " + json.dumps(dict(row, **changed)), 64)
        with self.assertRaises(ValueError):
            evidence.gpu_storage("", 64)

    def test_residency_uses_qpc_intervals_and_does_not_turn_missing_into_zero(self):
        phases = [{"label": label, "pid": 123, "begin_qpc": 100 + index * 100,
                   "end_qpc": 180 + index * 100, "frequency": 1000}
                  for index, label in enumerate(evidence.PHASES)]
        text = "\n".join("PD5_CAPACITY_PHASE " + json.dumps(row) for row in phases)
        samples = [{"pid": 123, "qpc": 110 + index * 100, "qpc_frequency": 1000,
                    "bytes": {"Dedicated Usage": 20 + index * 30, "Shared Usage": 10}}
                   for index in range(len(phases))]
        result = evidence.residency(text, samples, 64)
        self.assertEqual(result["active_delta_bytes"], 30)
        self.assertEqual(result["status"], "within_sampled_reference_ceiling")
        # A transient already sampled during load must not disappear behind a low active value.
        spike = {"pid": 123, "qpc": 295, "qpc_frequency": 1000,
                 "bytes": {"Dedicated Usage": 300, "Shared Usage": 10}}
        spiked = sorted([*samples, spike], key=lambda value: value["qpc"])
        self.assertEqual(evidence.residency(text, spiked, 64)["status"], "over_reference_ceiling")
        changed = copy.deepcopy(samples)
        changed[1]["bytes"]["Dedicated Usage"] = None
        self.assertEqual(evidence.residency(text, changed, 64)["status"], "unavailable")
        changed = copy.deepcopy(samples)
        changed[2]["pid"] = 456
        self.assertIsNone(evidence.residency(text, changed, 64)["active_delta_bytes"])
        with self.assertRaises(ValueError):
            evidence.residency("", samples, 64)

    def test_observer_pairs_cannot_reuse_one_invocation(self):
        with self.assertRaisesRegex(ValueError, "six independent"):
            prepare_capacity.qualification([Path("normal/summary.json")] * 3,
                                           [Path("memory/summary.json")] * 3, {}, False)

    def test_collect_rejects_retained_product_unreleased_roots_and_missing_world_subset(self):
        profile = next(row for row in read(Path(__file__).with_name("budget-proposal.v1.json"))["profiles"]
                       if row["id"] == "r4-reference-n64-v1")
        for changes, error in (({"retained": True}, "product allocations remain"),
                               ({"released": False}, "did not retire after Close"),
                               ({"world": False}, "missing instantaneous World proof")):
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, error):
                evidence.collect(workflow_log(**changes), profile)
        result = evidence.collect(workflow_log(), profile)
        self.assertFalse(result["formal_acceptance"])
        self.assertFalse(result["semantic_workflow"]["rating"]["complete_semantic_graph"])
        self.assertEqual(result["semantic_workflow"]["rating"]["status"], "unrated")

    def test_qualification_rejects_formal_invocations(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            normal = [root / f"normal-{index}" / "summary.json" for index in range(3)]
            memory = [root / f"memory-{index}" / "summary.json" for index in range(3)]
            normal[0].parent.mkdir()
            write_once(normal[0].parent / "declaration.json", {
                "source_sha": "a" * 40, "purpose": "observer-qualification",
                "formal_sampling": True, "device": False, "mode": "normal", "entities": 64,
                "rows": 64, "reuse": "high", "phase_hold_ms": 0})
            with self.assertRaisesRegex(ValueError, "qualification identity"):
                prepare_capacity.qualification(normal, memory, {"sha": "a" * 40}, False)

    def test_preparation_outputs_qualification_and_blocked_record_without_formal_proofs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "cuexis_pd_host_probe.exe"
            binary.write_bytes(b"unit-test binary identity; never executed")
            metadata_path = root / "build.json"
            write_once(metadata_path, {"binary": artifact(binary), "config": "Release",
                                       "toolchain_signature": {"compiler": "test"}})
            observed = {"raw_bindings": [artifact(binary)], "wall_cost_ratios": [1, 1, 1],
                        "scale": (64, 64, "high"), "cpu_environment": {},
                        "toolchain_signature": {"compiler": "test"}}
            for entities, rows, reuse in ((64, 64, "high"), (2048, 2048, "low"),
                                          (40000, 4096, "high")):
                output = root / f"output-{entities}"
                args = SimpleNamespace(source=root, output=output, build_binding=metadata_path,
                                       qualification_normal=[], qualification_memory=[], device=False,
                                       entities=entities, rows=rows, reuse=reuse)
                with self.subTest(entities=entities), \
                        patch.object(prepare_capacity, "clean_source", return_value={"sha": "a" * 40}), \
                        patch.object(prepare_capacity.build_binding, "verify"), \
                        patch.object(prepare_capacity, "qualification", return_value=observed), \
                        patch("builtins.print"):
                    prepare_capacity.run(args)
                    self.assertEqual({path.name for path in output.iterdir()},
                                     {"qualification.json", "blocked.json"})
                    qualification = read(output / "qualification.json")
                    self.assertFalse(qualification["formal_sampling"])
                    self.assertFalse(qualification["formal_acceptance"])
                    self.assertEqual(qualification["applicability"]["target_workload_matches"],
                                     entities == 64 and rows == 64 and reuse == "high")
                    blocked = read(output / "blocked.json")
                    self.assertFalse(blocked["formal_registration"])
                    self.assertFalse(blocked["formal_acceptance"])
                    self.assertIn("complete semantic ownership/lifetime graph validator", blocked["reason"])
                    self.assertEqual(blocked["qualification_binding"], artifact(output / "qualification.json"))

    def test_normal_child_environment_removes_inherited_observers(self):
        from .environment import process_environment
        parent = {"PATH": "tools", "CUEXIS_PD5_GPU_TIMER": "1",
                  "CUEXIS_PROCESS_GPU_MEMORY_LOG": "old.jsonl",
                  "CUEXIS_ALLOCATION_OBSERVER_PROCESS": "1", "CUEXIS_RPA_MEMORY_TRACE_DOMAIN": "world"}
        self.assertEqual(process_environment(parent), {"PATH": "tools"})
        self.assertEqual(len(parent), 5)


if __name__ == "__main__":
    unittest.main()
