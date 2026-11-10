import copy
import json
import unittest

from gameplay_admission_evidence import EXPECTED_40K, STAGES, validate


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        size, decoded, section, digest = EXPECTED_40K["low"]
        self.records = [
            ("PD5_C_STAGE", {"stage": stage, "state": state})
            for stage in STAGES for state in ("begin", "succeeded")
        ] + [
            ("PD5_C_ID", dict(source_sha="a" * 40, entities=40000, requirements=40000,
                              revision=4, reuse="low", lifetime_observer=True, formal_measurement=False)),
            ("PD5_C_PACKED", dict(entities=40000, requirements=40000, revision=4, file_bytes=size,
                                  decoded_bytes=decoded, max_section_bytes=section, strings=40048,
                                  references=13, semantic_digest=digest, semantic_bytes=99588531,
                                  scratch_peak=7145909, four_hash_limits_accepted=True,
                                  budgets_relaxed=False, ix_limit=1000000, key_limit=4096, sha_computations=1)),
            ("PD5_CAPACITY", dict(entities=40000, requirements=40000, reuse="low", packed_bytes=size,
                                  snapshot_valid=True, decoded_snapshot_valid=True, advance_executed=True,
                                  replay_evidence_valid=True, replay_equal=True, closed=True,
                                  formal_acceptance=False, seek_start_us=10, seek_confirmed_us=30,
                                  seek_present_us=40, seek_us=30)),
        ] + [("PD5_MEMORY", dict(label=label, allocator="lifetime-observer", exceeded=False))
             for label in ("before-load", "active", "after-close")]

    def check(self, records):
        text = "\n".join(prefix + " " + json.dumps(row) for prefix, row in records)
        return validate(text + "\nAll tests passed (100 assertions in 1 test case)",
                        source_sha="a" * 40, reuse="low", entities=40000)

    def test_complete_evidence(self):
        self.assertEqual(self.check(self.records)["stages_completed"], list(STAGES))

    def test_missing_stage_or_checkpoint(self):
        for index in (0, len(self.records) - 1):
            rows = copy.deepcopy(self.records)
            del rows[index]
            with self.assertRaises(ValueError):
                self.check(rows)

    def test_rejects_false_success_identity_counts_and_relaxed_budgets(self):
        mutations = [("PD5_C_STAGE", "state", "failed"),
                     ("PD5_C_ID", "source_sha", "b" * 40),
                     ("PD5_C_PACKED", "requirements", 39999),
                     ("PD5_C_PACKED", "semantic_digest", "0" * 64),
                     ("PD5_C_PACKED", "scratch_peak", 8388609),
                     ("PD5_C_PACKED", "key_limit", 8192),
                     ("PD5_C_PACKED", "budgets_relaxed", True),
                     ("PD5_CAPACITY", "replay_equal", False),
                     ("PD5_CAPACITY", "seek_us", 1)]
        for prefix, key, value in mutations:
            with self.subTest(prefix=prefix, key=key):
                rows = copy.deepcopy(self.records)
                next(row for name, row in rows if name == prefix)[key] = value
                with self.assertRaises(ValueError):
                    self.check(rows)

    def test_unfinished_stage_cannot_claim_final_success(self):
        rows = [row for row in self.records
                if row[0] != "PD5_C_STAGE" or row[1]["stage"] != "after-close-memory"]
        with self.assertRaises(ValueError):
            self.check(rows)


if __name__ == "__main__":
    unittest.main()
