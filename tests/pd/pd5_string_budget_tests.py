import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import string_budget_audit as audit


def observations():
    stages=[dict(stage=label,strings=100,references=50) for label in
            ('global','requirements','definitions','coordination')]
    return dict(stages=stages,fixture=dict(entities=40000,requirements=40000,reuse='low'),
        dictionary=dict(entities=40000,strings=160000,references=120000,utf8_bytes=20000000,
            str0_bytes=20640012,strings_limit=100000,references_limit=100000,
            section_limit=16777216,reference_kinds={'11':40000,'12':40000,'13':40000}),
        result=dict(wire_accepted=False,code='packed.budget.strings',performance_acceptance=False))


def text(rows):
    return '\n'.join(['AUDIT_STAGE '+json.dumps(r) for r in rows['stages']]+[
        tag+json.dumps(rows[key]) for tag,key in
        (('AUDIT_FIXTURE ','fixture'),('AUDIT_DICT ','dictionary'),('AUDIT_RESULT ','result'))])


class StringBudgetTests(unittest.TestCase):
    def test_expected_rejection_is_not_capacity_or_performance_acceptance(self):
        result=audit.parse_log(text(observations()),40000,'low')
        self.assertFalse(result['result']['wire_accepted'])
        self.assertFalse(result['result']['performance_acceptance'])

    def test_missing_counts_changed_budgets_and_false_acceptance_rejected(self):
        changes=(lambda r:r['stages'].pop(),
                 lambda r:r['fixture'].update(requirements=64),
                 lambda r:r['dictionary'].update(strings_limit=200000),
                 lambda r:r['dictionary']['reference_kinds'].update({'13':1}),
                 lambda r:r['dictionary'].update(str0_bytes=1),
                 lambda r:r['result'].update(wire_accepted=True))
        for change in changes:
            rows=copy.deepcopy(observations());change(rows)
            with self.subTest(change=change),self.assertRaises(ValueError):
                audit.parse_log(text(rows),40000,'low')


if __name__ == '__main__':
    unittest.main()
