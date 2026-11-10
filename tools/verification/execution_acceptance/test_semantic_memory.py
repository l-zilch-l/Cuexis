import copy
import json
import unittest

from .semantic_memory import collect, rate


class SemanticMemoryTests(unittest.TestCase):
    def setUp(self):
        self.proof = {"schema": "rpa.execution.memory.world-proof.v1", "census_id": 1,
                      "root_id": 7, "phase": "active", "stamp_us": 100,
                      "domain": "state.world", "bound_kind": "instantaneous_unique_lower_bound",
                      "complete_world_graph": False, "valid": True, "live_count": 1,
                      "lower_bound_requested_cpp_bytes": 150, "backing_requested_cpp_bytes": 150,
                      "backing_crt_usable_storage_bytes": 214,
                      "oldest_live_age_us": 10, "census_stack_bytes": 1024}
        self.block = {"schema": "rpa.execution.memory.world-allocation.v1", "census_id": 1,
                      "allocation_id": 42, "root_id": 7, "requested_cpp_bytes": 150,
                      "crt_usable_storage_bytes": 214, "charged_cpp_bytes": 150,
                      "age_us": 10, "origin_cell": 80,
                      "membership": "actual_component_page"}

    def log(self, proof=None, blocks=None, spans=None):
        proof = self.proof if proof is None else proof
        blocks = [self.block] if blocks is None else blocks
        if spans is None:
            spans = [{"schema": "rpa.execution.memory.world-span.v1", "census_id": row["census_id"],
                      "allocation_id": row["allocation_id"], "offset": 0,
                      "bytes": row["charged_cpp_bytes"]} for row in blocks]
        return ("RPA_MEMORY_WORLD_PROOF " + json.dumps(proof) + "\n" + "\n".join(
            "RPA_MEMORY_WORLD_ALLOCATION " + json.dumps(row) for row in blocks) + "\n" +
            "\n".join("RPA_MEMORY_WORLD_SPAN " + json.dumps(row) for row in spans))

    def profile(self, limit):
        return {"domains": [{"id": "state.world", "requested_bytes": limit}]}

    def test_lower_bound_proves_failure_only(self):
        document = collect(self.log())
        self.assertEqual(rate(document, self.profile(100))["status"], "proven_over_budget")
        result = rate(document, self.profile(150))
        self.assertEqual(result["status"], "unrated")
        self.assertFalse(result["formal_acceptance"])

    def test_prepared_is_not_active_failure(self):
        self.proof["phase"] = "prepared"
        self.assertEqual(rate(collect(self.log()), self.profile(1))["status"], "unrated")

    def test_missing_duplicate_and_orphan_blocks_rejected(self):
        for blocks in ([], [self.block, self.block], [dict(self.block, census_id=2)]):
            with self.subTest(blocks=blocks), self.assertRaises(ValueError):
                collect(self.log(blocks=blocks))

    def test_invalid_observation_and_totals_rejected(self):
        for changes in ({"valid": False}, {"complete_world_graph": True},
                        {"lower_bound_requested_cpp_bytes": 151}, {"oldest_live_age_us": 11},
                        {"live_count": True}, {"census_id": 0}, {"phase": "retiring"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                collect(self.log(dict(self.proof, **changes)))

    def test_id_cannot_change_after_transfer(self):
        later = copy.deepcopy(self.proof)
        later["census_id"] = 2
        later["root_id"] = 8
        block = dict(self.block, census_id=2, root_id=8, origin_cell=81)
        with self.assertRaises(ValueError):
            collect(self.log() + "\n" + self.log(later, [block]))

    def test_late_sticky_invalidity_rejects_earlier_proof(self):
        for suffix in ('RPA_MEMORY {"exceeded":true}',
                       'RPA_MEMORY_PARTITION {"exact":false}'):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                collect(self.log() + "\n" + suffix)

    def test_overlapping_and_disjoint_spans_use_union_without_prefix_charge(self):
        spans = [{"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                  "allocation_id": 42, "offset": start, "bytes": size}
                 for start, size in ((16, 32), (32, 32), (100, 10), (16, 32))]
        proof = dict(self.proof, lower_bound_requested_cpp_bytes=58)
        block = dict(self.block, charged_cpp_bytes=58)
        self.assertEqual(collect(self.log(proof, [block], spans))["censuses"][0][
            "proof"]["lower_bound_requested_cpp_bytes"], 58)

    def test_missing_or_out_of_range_span_rejected(self):
        for spans in ([], [{"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                           "allocation_id": 42, "offset": 149, "bytes": 2}]):
            with self.subTest(spans=spans), self.assertRaises(ValueError):
                collect(self.log(spans=spans))

    def test_typed_owned_spans_require_known_categories(self):
        block = dict(self.block, membership="actual_owned_span")
        span = {"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                "allocation_id": 42, "offset": 0, "bytes": 150, "kind": "material_string"}
        document = collect(self.log(blocks=[block], spans=[span]))
        self.assertEqual(document["censuses"][0]["spans"][0]["kind"], "material_string")
        for invalid in ({key: value for key, value in span.items() if key != "kind"},
                        dict(span, kind="unvisited_buffer")):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                collect(self.log(blocks=[block], spans=[invalid]))

    def test_typed_span_aliases_do_not_add_to_stable_backing(self):
        block = dict(self.block, membership="stable_registry_subset")
        spans = [{"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                  "allocation_id": 42, "offset": 16, "bytes": 80, "kind": kind}
                 for kind in ("transform_page", "component_entities")]
        document = collect(self.log(blocks=[block], spans=spans))
        self.assertEqual(document["censuses"][0]["proof"]["lower_bound_requested_cpp_bytes"], 150)

    def test_swapped_buffer_disappears_without_root_adoption(self):
        block = dict(self.block, membership="actual_owned_span")
        span = {"schema": "rpa.execution.memory.world-span.v1", "census_id": 1,
                "allocation_id": 42, "offset": 0, "bytes": 150, "kind": "material_string"}
        later = dict(self.proof, census_id=2, live_count=0, lower_bound_requested_cpp_bytes=0,
                     backing_requested_cpp_bytes=0, backing_crt_usable_storage_bytes=0,
                     oldest_live_age_us=0)
        document = collect(self.log(blocks=[block], spans=[span]) + "\n" +
                           self.log(later, blocks=[], spans=[]))
        self.assertEqual([row["proof"]["live_count"] for row in document["censuses"]], [1, 0])


if __name__ == "__main__":
    unittest.main()
