import sys
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.verification.playback_performance import measurement as m
from tools.verification.playback_performance import window_protocol as p
from tools.verification.playback_performance import owner_runner as runner


def registration():
    return dict(schema='pd5.windows.v1', accepted_profile=True, windows=3,
        pairs_per_window=3, gap_seconds=600, noise_limit=.15, regression_limit=1.10,
        units=[dict(id='release-legacy', kind='owner', config='release', metrics=['playback_only'])])


class WindowTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.serial=0

    def complete(self, reg, window, effect=1.05, slow=False):
        self.serial+=1;folder=self.root/str(self.serial);folder.mkdir();rows=[]
        for i,target in enumerate(p.schedule(reg,window)):
            cost=100*(effect if target['implementation']=='B' else 1)
            if slow and i==5:cost=200
            identity=dict(metrics_compile_enabled=False,allocator='normal',digest=42,
                replay_sha256='replay',snapshot_sha256='snapshot',packed_sha256='packed',profile='legacy')
            samples=[dict(kind=kind,group=g,active=False,operations=1,
                sample={'wall_us':200000 if kind=='read_long' else cost})
                for kind in ('validated_trace','playback_only','read_long') for g in range(8)]
            log=folder/f'{i}.log'
            log.write_text('PD_FORMAL_ID '+json.dumps(identity)+'\n'+'\n'.join(
                'PD_V2 '+json.dumps(s) for s in samples),encoding='utf-8')
            warm=folder/f'{i}-warmup.log';warm.write_text('PD_MACHINE_WARMUP {"seconds":30}\n',encoding='utf-8')
            path=folder/f'{i}.json'
            p.write_once(path,dict(data=m.collect(log,'owner'),commands={key:dict(path=str(value),
                sha256=p.file_hash(value),exit_code=0) for key,value in [('warmup',warm),('measure',log)]}))
            rows.append({**target,'exit_code':0,'execution_valid':True,'record':str(path),
                'record_sha256':p.file_hash(path),'raw_log_sha256':p.file_hash(log),
                'values':{'playback_only':cost},'mean_values':{}})
        return dict(window=window,runs=rows,status='collected')

    def claim_root(self):
        root=self.root/'registration';root.mkdir()
        pilot=self.root/'pilot.json';p.write_once(pilot,{'runs':[]})
        p.write_once(self.root/'preparation.json',{})
        reg={**registration(),'preparation':str(self.root),'pilot':str(pilot),
             'preparation_sha256':p.file_hash(self.root/'preparation.json'),
             'pilot_sha256':p.file_hash(pilot)}
        p.write_once(root/'registration.json',reg)
        p.write_once(self.root/'registration.claim.json',{'folder':str(root),'pilot':str(pilot),
                     'pilot_sha256':p.file_hash(pilot)})
        p.seal_registration(root)
        return root

    def save_first(self,root):
        p.claim(root,1,'2026-10-18T23:59:00+00:00',940)
        result=self.complete(registration(),1)
        result.update(registration_sha256=p.file_hash(root/'registration.json'),
            claim_sha256=p.file_hash(root/'window-1.claim.json'),
            ended_utc='2026-10-19T00:00:00+00:00',ended_monotonic=1000)
        p.write_once(root/'window-1.result.json',result);return result

    def test_balanced_fixed_order(self):
        self.assertEqual([x['implementation'] for x in p.schedule(registration(),1)],list('ABBAAB'))
        self.assertEqual([x['implementation'] for x in p.schedule(registration(),2)],list('BAABBA'))

    def test_claim_is_exclusive(self):
        root=self.claim_root();p.claim(root,1,'2026-10-19T00:00:00+00:00',1000)
        with self.assertRaises(FileExistsError):p.claim(root,1,'2026-10-19T00:00:01+00:00',1001)

    def test_registration_mutation_before_first_window_is_rejected(self):
        root=self.claim_root()
        reg=p.read(root/'registration.json');reg['units'][0]['metrics']=['read_long']
        (root/'registration.json').write_text(json.dumps(reg),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'changed after sealing'):
            p.claim(root,1,'2026-10-19T00:00:00+00:00',1000)
        self.assertFalse((root/'window-1.claim.json').exists())

    def test_unsealed_registration_is_rejected(self):
        root=self.claim_root();(self.root/'registration.seal.json').unlink()
        with self.assertRaises(FileNotFoundError):
            p.claim(root,1,'2026-10-19T00:00:00+00:00',1000)

    def test_summary_rejects_cross_window_raw_log_reuse(self):
        reg=registration();windows=[self.complete(reg,w) for w in (1,2,3)]
        old=windows[0]['runs'][0];row=windows[1]['runs'][1]
        old_record=p.read(old['record']);record=p.read(row['record'])
        record['commands']=old_record['commands']
        Path(row['record']).write_text(json.dumps(record),encoding='utf-8')
        row['record_sha256']=p.file_hash(row['record'])
        row['raw_log_sha256']=old['raw_log_sha256']
        with self.assertRaisesRegex(ValueError,'reused as independent evidence'):
            p.summarize(reg,windows)

    def test_independent_path_hardlink_does_not_create_another_sample(self):
        reg=registration();windows=[self.complete(reg,w) for w in (1,2,3)]
        first=p.read(windows[0]['runs'][0]['record'])
        row=windows[1]['runs'][1];record=p.read(row['record'])
        duplicate=Path(row['record']).with_suffix('.duplicate.log')
        os.link(first['commands']['measure']['path'],duplicate)
        record['commands']['measure']={**first['commands']['measure'],'path':str(duplicate)}
        Path(row['record']).write_text(json.dumps(record),encoding='utf-8')
        row['record_sha256']=p.file_hash(row['record'])
        row['raw_log_sha256']=p.file_hash(duplicate)
        with self.assertRaisesRegex(ValueError,'reused as independent evidence'):
            p.summarize(reg,windows)

    def test_formal_window_cannot_reuse_pilot_logs(self):
        root=self.claim_root();result=self.save_first(root)
        pilot=Path(p.read(root/'registration.json')['pilot'])
        pilot.write_text(json.dumps({'runs':[{'path':result['runs'][0]['record']}]}),encoding='utf-8')
        # Create a separate valid registration that freezes this pilot, rather than tamper its seal.
        reg=p.read(root/'registration.json');reg['pilot_sha256']=p.file_hash(pilot)
        (root/'registration.json').write_text(json.dumps(reg),encoding='utf-8')
        claim=p.read(self.root/'registration.claim.json');claim['pilot_sha256']=p.file_hash(pilot)
        (self.root/'registration.claim.json').write_text(json.dumps(claim),encoding='utf-8')
        (self.root/'registration.seal.json').unlink();p.seal_registration(root)
        first=p.read(root/'window-1.claim.json');first['registration_sha256']=p.file_hash(root/'registration.json')
        (root/'window-1.claim.json').write_text(json.dumps(first),encoding='utf-8')
        result['registration_sha256']=first['registration_sha256']
        result['claim_sha256']=p.file_hash(root/'window-1.claim.json')
        (root/'window-1.result.json').write_text(json.dumps(result),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'reused as independent evidence'):
            p.load_windows(root,1)

    def test_standalone_summary_cannot_reuse_pilot_logs(self):
        reg=registration();windows=[self.complete(reg,w) for w in (1,2,3)]
        pilot=self.root/'summary-pilot.json'
        p.write_once(pilot,{'runs':[{'path':windows[0]['runs'][0]['record']}]})
        reg.update(pilot=str(pilot),pilot_sha256=p.file_hash(pilot))
        with self.assertRaisesRegex(ValueError,'reused as independent evidence'):
            p.summarize(reg,windows)

    def test_gap_requires_both_clocks_and_complete_predecessor(self):
        root=self.claim_root()
        with self.assertRaises(ValueError):p.claim(root,2,'2026-10-19T00:20:00+00:00',2000)
        self.save_first(root)
        for utc,mono in [('2026-10-19T00:09:59+00:00',2000),('2026-10-19T00:20:00+00:00',1599),
                         ('2026-10-19T00:20:00+00:00',10)]:
            with self.assertRaises(ValueError):p.claim(root,2,utc,mono)
        p.claim(root,2,'2026-10-19T00:10:00+00:00',1600)

    def test_stopped_registration_cannot_advance(self):
        root=self.claim_root();p.write_once(root/'stopped.json',{})
        with self.assertRaises(ValueError):p.claim(root,1,'2026-10-19T00:00:00+00:00',1000)

    def test_complete_fixed_group_passes_without_pd_exit(self):
        reg=registration();r=p.summarize(reg,[self.complete(reg,w) for w in (1,2,3)])
        self.assertEqual(r['status'],'pass');self.assertFalse(r['pd5_complete'])

    def test_missing_reordered_failed_samples_rejected(self):
        for mutate in (lambda r:r['runs'].pop(),lambda r:r['runs'].reverse(),
                       lambda r:r['runs'][0].update(exit_code=1),lambda r:r['runs'][0].update(execution_valid=False)):
            reg=registration();rows=[self.complete(reg,w) for w in (1,2,3)];mutate(rows[0])
            with self.assertRaises(ValueError):p.summarize(reg,rows)

    def test_same_metric_three_windows_required_for_split(self):
        reg=registration()
        self.assertTrue(p.summarize(reg,[self.complete(reg,w,1.2) for w in (1,2,3)])['results'][0]['split_design_trigger'])
        rows=[self.complete(reg,w,1.2 if w!=2 else 1) for w in (1,2,3)]
        self.assertFalse(p.summarize(reg,rows)['results'][0]['split_design_trigger'])

    def test_slow_sample_retained_as_inconclusive(self):
        reg=registration();r=p.summarize(reg,[self.complete(reg,w,slow=w==3) for w in (1,2,3)])
        self.assertEqual(r['status'],'inconclusive');self.assertEqual(r['results'][0]['process_values']['B'][2][-1],200)

    def test_values_records_logs_must_match(self):
        for fault in ('values','record','raw','delete','warmup'):
            reg=registration();result=self.complete(reg,1);row=result['runs'][0];record=p.read(row['record'])
            if fault=='values':row['values']['playback_only']=1
            elif fault=='record':Path(row['record']).write_text('{}',encoding='utf-8')
            elif fault=='delete':Path(record['commands']['measure']['path']).unlink()
            else:Path(record['commands']['warmup' if fault=='warmup' else 'measure']['path']).write_text('changed',encoding='utf-8')
            with self.assertRaises((ValueError,FileNotFoundError)):p.validate_window(reg,{'window':1},result)

    def test_claim_rechecks_prior_raw_evidence_and_chain(self):
        root=self.claim_root();result=self.save_first(root);path=root/'window-1.claim.json';original=path.read_bytes()
        path.write_bytes(original+b' ')
        with self.assertRaises(ValueError):p.claim(root,2,'2026-10-19T00:10:00+00:00',1600)
        path.write_bytes(original);Path(p.read(result['runs'][0]['record'])['commands']['measure']['path']).unlink()
        with self.assertRaises(FileNotFoundError):p.claim(root,2,'2026-10-19T00:10:00+00:00',1600)

    def test_same_pilot_cannot_register_again(self):
        pilot=self.root/'pilot.json';p.write_once(self.root/'preparation.json',{})
        p.write_once(pilot,dict(status='pilot_complete',preparation_sha256=p.file_hash(self.root/'preparation.json'),
            selected_mask=4,read_counts={},environment={},expected_identities={},expected_scheduling={}))
        args=SimpleNamespace(preparation=self.root,pilot=pilot,environment_declaration='stable')
        with patch.object(runner,'verify'),patch.object(runner,'environment'),patch.object(runner,'same_environment'),patch.object(runner,'verify_pilot',return_value=p.read(pilot)):
            runner.register(args)
            with self.assertRaises(FileExistsError):runner.register(args)


if __name__=='__main__':unittest.main()
