import copy
import json
import unittest

from tools.verification.execution_acceptance.memory_domains import collect


def captured():
    cell = dict(schema="rpa.execution.memory.domain.v1", label="active", owner="playback-worker",
                domain="decode", cell="playback-worker.decode", live_count=1,
                requested_cpp_bytes=100, crt_usable_storage_bytes=160, peak_live_count=2,
                peak_requested_cpp_bytes=200, peak_crt_usable_storage_bytes=320,
                oldest_live_age_us=10)
    total = dict(schema="rpa.execution.memory.v1", label="active", exceeded=False,
                 live_count=1, requested_cpp_bytes=100, crt_usable_storage_bytes=160)
    partition = dict(schema="rpa.execution.memory.partition.v1", label="active", exact=True,
                     cells=1, untagged_live_count=0, sum_live_count=1, global_live_count=1,
                     sum_requested_cpp_bytes=100, global_requested_cpp_bytes=100,
                     sum_crt_usable_storage_bytes=160, global_crt_usable_storage_bytes=160)
    return total, partition, cell


def render(rows):
    total, partition, cell = rows
    text = "\n".join(prefix + json.dumps(row) for prefix, row in zip(
        ("RPA_MEMORY ", "RPA_MEMORY_PARTITION ", "RPA_MEMORY_DOMAIN "), rows))
    text += "\nRPA_MEMORY_AGGREGATE " + json.dumps(dict(
        schema="rpa.execution.memory.aggregate.v1", label="active", domain="content.decoded",
        live_count=1, requested_cpp_bytes=100, crt_usable_storage_bytes=160,
        peak_requested_cpp_bytes=200, peak_crt_usable_storage_bytes=320))
    text += "\nRPA_MEMORY_PRODUCT " + json.dumps(dict(
        schema="rpa.execution.memory.product.v1", label="active",
        requested_cpp_bytes=100, crt_usable_storage_bytes=160,
        peak_requested_cpp_bytes=200, peak_crt_usable_storage_bytes=320))
    return text



class MemoryDomainTests(unittest.TestCase):
    def test_decoded_product_is_rated_as_product_not_excluded_fixture(self):
        document = collect(render(captured()))
        self.assertEqual(document["checkpoints"][0]["cells"][0]["budget_domain"], "content.decoded")

    def test_unknown_unrated_mismatched_and_overrange_records_fail_closed(self):
        changes = [lambda rows: rows[0].update(exceeded=True),
                   lambda rows: rows[1].update(exact=False),
                   lambda rows: rows[1].update(sum_requested_cpp_bytes=101),
                   lambda rows: rows[1].update(cells=2),
                   lambda rows: rows[2].update(domain="other", cell="playback-worker.other"),
                   lambda rows: rows[2].update(owner="unknown", cell="unknown.decode"),
                   lambda rows: rows[2].update(live_count=True),
                   lambda rows: rows[2].update(peak_requested_cpp_bytes=99),
                   lambda rows: rows[2].update(oldest_live_age_us=-1)]
        for change in changes:
            rows = copy.deepcopy(captured())
            change(rows)
            with self.subTest(change=change), self.assertRaises(ValueError):
                collect(render(rows))

    def test_duplicate_or_orphan_records_are_rejected(self):
        rows = captured()
        text = render(rows)
        with self.assertRaises(ValueError):
            collect(text + "\nRPA_MEMORY_DOMAIN " + json.dumps(rows[2]))
        orphan = dict(rows[2], label="orphan")
        with self.assertRaises(ValueError):
            collect(text + "\nRPA_MEMORY_DOMAIN " + json.dumps(orphan))


if __name__ == "__main__":
    unittest.main()
