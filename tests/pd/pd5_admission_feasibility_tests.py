from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.verification.playback_performance import admission_feasibility as audit
import json


def observation(n):
    sections=[dict(code=code,bytes=20,records=1,serialized_validation=n<=64)
              for code in ('META','TIME','STR0','REF0','IDN0','ARCH','ENT0','REQ0','CNS0','GPH0','GPR0','GPD0','GRC0')]
    dictionaries=[dict(layout=layout,strings=strings,references=refs,utf8_bytes=10,str0_bytes=22+4*strings)
                  for layout,strings,refs in [('legacy3',4*n+48,3*n+13),('proposed4',n+48,13)]]
    counts=dict(entities=n,requirements=n,claims=n,occupying_claims=n,logical_derived_projections=3*n,
        header_bytes=96,directory_bytes=32*len(sections),decoded_bytes=20*len(sections),
        proposed_file_bytes=96+52*len(sections),proposed_physical_fit=True,semantic_preimage_bytes=50,
        semantic_digest='a'*64,hash_output_buffer_bytes=0,proposed_wire_accepted=False,performance_acceptance=False)
    result=dict(legacy_wire_accepted=n<=64,legacy_code='' if n<=64 else 'packed.budget.strings',
        proposed_wire_accepted=False,host_reached=False,small_sizing_and_hash_verified=n<=64)
    return dict(ADMISSION_DICT=dictionaries,ADMISSION_COUNTS=[counts],ADMISSION_RESULT=[result],
        ADMISSION_SECTION=sections,ADMISSION_HASH_STAGE=[dict(stage=name,bytes=10) for name in
        ('coordination','definitions','global','requirements','static_prefix')],ADMISSION_EXPANSION=[])


def text(rows):
    return '\n'.join(tag+' '+json.dumps(row) for tag,records in rows.items() for row in records)


class AdmissionContractTests(unittest.TestCase):
    def test_original_rejection_and_proposed_count_are_distinct(self):
        for n in (1,64,40000):
            self.assertFalse(audit.parse_log(text(observation(n)),n)['counts']['proposed_wire_accepted'])

    def test_missing_sections_or_hash_stage_cannot_pass(self):
        for tag in ('ADMISSION_SECTION','ADMISSION_HASH_STAGE','ADMISSION_COUNTS','ADMISSION_RESULT'):
            rows=observation(1);rows[tag].pop()
            with self.subTest(tag=tag),self.assertRaises(ValueError):audit.parse_log(text(rows),1)

    def test_fictitious_acceptance_and_changed_input_rejected(self):
        for tag,key,value in [('ADMISSION_COUNTS','proposed_wire_accepted',True),
                              ('ADMISSION_RESULT','host_reached',True),
                              ('ADMISSION_RESULT','legacy_wire_accepted',True),
                              ('ADMISSION_COUNTS','requirements',39999),
                              ('ADMISSION_COUNTS','logical_derived_projections',0),
                              ('ADMISSION_COUNTS','hash_output_buffer_bytes',1)]:
            rows=observation(40000);rows[tag][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):audit.parse_log(text(rows),40000)

    def test_accounting_disagreements_rejected(self):
        for tag,key in [('ADMISSION_COUNTS','directory_bytes'),('ADMISSION_COUNTS','proposed_file_bytes'),
                        ('ADMISSION_COUNTS','semantic_preimage_bytes'),('ADMISSION_DICT','str0_bytes'),
                        ('ADMISSION_DICT','references')]:
            rows=observation(64);rows[tag][0][key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):audit.parse_log(text(rows),64)


if __name__=='__main__':unittest.main()
