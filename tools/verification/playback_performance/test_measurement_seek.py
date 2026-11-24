"""The GPU seek parser must reject anything that is not the registered class request."""

import unittest

from tools.verification.playback_performance import measurement


def log(class_name="natural", seeks=2, records=129, mismatched=0, verified=None, schema="pd5.seek.v1"):
    verified = seeks if verified is None else verified
    lines = [
        f'PD5_SEEK_ID {{"schema":"{schema}","records":{records},"seeks":{seeks},'
        f'"seek_class":"{class_name}","hints":1,"certified_records":1}}'
    ]
    for index in range(seeks):
        lines.append(
            f'PD5_SEEK {{"records":{records},"seek_class":"{class_name}","index":{index},'
            f'"horizon":{1103 + index * 7000},"request_to_ready_us":{300 + index},'
            f'"request_to_present_us":{450 + index},"state_matches_oracle":true}}')
    lines.append(f'PD5_SEEK_VERIFY {{"records":{records},"seek_class":"{class_name}",'
                 f'"verified":{verified},"mismatched":{mismatched}}}')
    return "\n".join(lines) + "\n"


class ParseSeekTests(unittest.TestCase):
    def test_valid_log_yields_identity_and_both_metrics(self):
        parsed = measurement.parse_seek(log())
        self.assertEqual(parsed["identity"]["seek_class"], "natural")
        self.assertEqual(parsed["statistics"]["request_to_present_us"]["n"], 2)
        self.assertEqual(parsed["verify"]["mismatched"], 0)

    def test_oracle_mismatch_rejects_the_log(self):
        with self.assertRaises(ValueError):
            measurement.parse_seek(log(mismatched=1, verified=1))

    def test_missing_requests_reject_the_log(self):
        text = log().replace('PD5_SEEK {"records":129,"seek_class":"natural","index":1',
                             'PD5_SEEK_X {"records":129,"seek_class":"natural","index":1')
        with self.assertRaises(ValueError):
            measurement.parse_seek(text)

    def test_failed_process_rejects_the_log(self):
        with self.assertRaises(ValueError):
            measurement.parse_seek(log(), exit_code=1)

    def test_wrong_schema_rejects_the_log(self):
        with self.assertRaises(ValueError):
            measurement.parse_seek(log(schema="pd5.other.v1"))

    def test_sample_from_another_class_rejects_the_log(self):
        text = log().replace('"seek_class":"natural","index":1',
                             '"seek_class":"external","index":1')
        with self.assertRaises(ValueError):
            measurement.parse_seek(text)


if __name__ == "__main__":
    unittest.main()
