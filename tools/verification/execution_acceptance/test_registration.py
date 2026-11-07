import copy
import unittest

from tools.verification.execution_acceptance.registration import formal_blockers, validate_draft, validate_samples


def draft():
    return dict(schema="rpa.r4.registration-draft.v1", formal_registration=False,
                windows=3, processes_per_window=3, gap_seconds=600,
                orders=["AB/BA/AB", "BA/AB/BA", "AB/BA/AB"],
                regression_limit=1.10, noise_limit=.15,
                units=[dict(id="seek-cold", workload="seek", seek_class="cold",
                            comparison="B-only", cold_process=True,
                            requests_per_process=1, completion_endpoint="seek-to-target-present",
                            profile="cold-reference-v1")])


class RegistrationTests(unittest.TestCase):
    def test_draft_lists_required_admission_material(self):
        self.assertEqual(len(formal_blockers(draft())), 9)

    def test_protocol_and_identity_changes_fail_closed(self):
        mutations = [
            lambda d: d.update(gap_seconds=599),
            lambda d: d.update(windows=2),
            lambda d: d.update(orders=["AB/BA/AB"] * 3),
            lambda d: d.update(regression_limit=1.15),
            lambda d: d.update(formal_registration=True),
            lambda d: d["units"][0].update(requests_per_process=16),
            lambda d: d["units"][0].update(comparison="A/B", baseline="R3"),
            lambda d: d["units"][0].update(comparison="A/B", baseline="pre-PD",
                                             cold_process=False, requests_per_process=3),
            lambda d: d["units"][0].update(comparison="P8-single-sided", invocations=2),
            lambda d: d["units"].append(copy.deepcopy(d["units"][0])),
        ]
        for mutate in mutations:
            document = draft()
            mutate(document)
            with self.subTest(mutation=mutate), self.assertRaises(ValueError):
                validate_draft(document)

    def test_invalid_and_incomplete_samples_are_rejected(self):
        for values in ([1, 2], [1, 2, float("nan")], [1, 2, float("inf")], [1, 2, 0],
                       [1, 2, True]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                validate_samples(values, 3)
        validate_samples([1, 2, 3], 3)

    def test_seek_classes_enforce_the_full_fixed_request_protocol(self):
        for seek_class in ("natural", "hot", "external", "fallback"):
            document = draft()
            document["units"][0].update(seek_class=seek_class, cold_process=False,
                                        comparison="A/B", baseline="pre-PD",
                                        requests_per_process=16)
            validate_draft(document)
            for count in (1, 3, 15, 17):
                document["units"][0]["requests_per_process"] = count
                with self.subTest(seek_class=seek_class, count=count), self.assertRaises(ValueError):
                    validate_draft(document)

    def test_non_seek_comparison_keeps_its_own_request_count(self):
        document = draft()
        document["units"][0].update(id="steady", workload="steady", comparison="A/B",
                                    baseline="pre-PD", cold_process=False, requests_per_process=8)
        validate_draft(document)


if __name__ == "__main__":
    unittest.main()
