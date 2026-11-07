import sys
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import os

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.verification.playback_performance import prepare_profiles as p
from tools.verification.playback_performance import profile_runner as runner


class Pd5PrepareTests(unittest.TestCase):
    def profile(self):
        suite=p.load(p.SUITE)
        return {**suite['defaults'],**suite['profiles'][0]}

    def test_all_finite_tool_profiles(self):
        suite=p.load(p.SUITE)
        for item in suite['profiles']:
            profile=p.expanded_profile(suite,item)
            if item.get('harness')=='presentation-matrix':
                self.assertIn('no Gameplay or Replay',profile['input'])
                self.assertEqual(profile['capacity']['global_bytes'],8388608)

    def test_wrong_harness_mode_rejected(self):
        profile=self.profile();profile['mode']='capture-1'
        with self.assertRaises(ValueError):p.validate_profile(profile)

    def test_profile_runner_rejects_shortened_controls_before_pilot(self):
        profile=self.profile();profile['mode']='controls'
        identity={key:profile[key] for key in ('mode','frames','warmup_frames','cycles','rate')}
        identity.update(smoke=False,audio=False,source_step_ticks=7000,source_tick_rate=1000000,
                        counts=p.expected_counts(profile))
        data={'identity':identity,'statistics':{}}
        runner.validate_identity({'profile':profile},data)
        for metric in ('pause','resume','reset_paused','reset_running','resize'):
            changed=copy.deepcopy(data);changed['identity']['counts'][metric]-=1
            with self.subTest(metric=metric),self.assertRaisesRegex(ValueError,'sample counts'):
                runner.validate_identity({'profile':profile},changed)

    def test_missing_or_infinite_capacity_rejected(self):
        for field in self.profile()['capacity']:
            for bad in (0,-1,None,True,float('inf')):
                profile=copy.deepcopy(self.profile());profile['capacity'][field]=bad
                with self.subTest(field=field,bad=bad),self.assertRaises(ValueError):
                    p.validate_profile(profile)

    def test_short_retention_coverage_and_warmup_rejected(self):
        for key,value in [('frames',9999),('warmup_frames',2399),('rate',100000),('cycles',101)]:
            profile=copy.deepcopy(self.profile());profile[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):p.validate_profile(profile)
        profile=copy.deepcopy(self.profile());profile['capacity']['retention_us']=100000000
        with self.assertRaises(ValueError):p.validate_profile(profile)

    def test_production_and_audio_not_silently_claimed(self):
        for key,value in [('audio',True),('metrics_compile_enabled',True),('allocator','probe'),('etw',True)]:
            profile=copy.deepcopy(self.profile());profile[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):p.validate_profile(profile)
        for key,value in [('viewport',[0,0]),('rate',300),('cycles',9)]:
            profile=copy.deepcopy(self.profile());profile[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):p.validate_profile(profile)

    def test_historical_builds_and_manifest_replacement_rejected(self):
        with self.assertRaises(ValueError):p.build_binding(p.ROOT/'out/build/pd4-release','x.exe',[])
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'manifest.json';p.save_new(path,{'a':1})
            with self.assertRaises(FileExistsError):p.save_new(path,{'a':2})
            self.assertEqual(p.load(path),{'a':1})

    def test_transitive_implementation_staleness_and_relink_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);build=root/'out/build/tool';build.mkdir(parents=True)
            for name in ('CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','implementation.cpp','a.hpp'):
                (root/name).write_text('input',encoding='utf-8')
            for folder in ('engine','bin','lib'):(build/folder).mkdir()
            (build/'CMakeCache.txt').write_text('CMAKE_MAKE_PROGRAM:FILEPATH=ninja\n',encoding='utf-8')
            (build/'compile_commands.json').write_text(json.dumps([{'directory':str(build),
                'file':str(root/'implementation.cpp'),'command':'cl /Foengine/a.obj'}]),encoding='utf-8')
            (build/'CMakeFiles').mkdir()
            for path in (build/'build.ninja',build/'CMakeFiles/rules.ninja'):
                path.write_text('rules',encoding='utf-8')
            obj=build/'engine/a.obj';lib=build/'lib/a.lib';exe=build/'bin/probe.exe'
            for file in (obj,lib,exe):file.write_bytes(b'compiled')
            stamp=max(x.stat().st_mtime_ns for x in root.iterdir() if x.is_file())+1000000
            for file in (build/'CMakeCache.txt',build/'compile_commands.json',build/'build.ninja',build/'CMakeFiles/rules.ninja'):
                os.utime(file,ns=(stamp-1000000,stamp-1000000))
            for file in (obj,lib):os.utime(file,ns=(stamp,stamp))
            os.utime(exe,ns=(stamp+1000000,stamp+1000000))
            def query(args,**kwargs):
                if args[0]=='rg':return 'CMakeLists.txt\n'
                if 'commands' in args:return b'cl /Foengine/a.obj\nlink lib/a.lib\n'
                return ('engine/a.obj: #deps 1, deps mtime 1 (VALID)\n    '+str(root/'a.hpp')+'\n\n').encode()
            with patch.object(p,'ROOT',root),patch.object(p.subprocess,'check_output',side_effect=query):
                self.assertEqual(p.require_target_clean(build,'probe.exe')['objects_checked'],1)
                os.utime(root/'implementation.cpp',ns=(stamp+2000000,stamp+2000000))
                with self.assertRaisesRegex(ValueError,'object is stale'):p.require_target_clean(build,'probe.exe')
                os.utime(root/'implementation.cpp',ns=(stamp-1000000,stamp-1000000))
                os.utime(lib,ns=(stamp+2000000,stamp+2000000))
                with self.assertRaisesRegex(ValueError,'Executable predates'):p.require_target_clean(build,'probe.exe')
                os.utime(lib,ns=(stamp,stamp))
                os.utime(build/'compile_commands.json',ns=(stamp+2000000,stamp+2000000))
                with self.assertRaisesRegex(ValueError,'command/configuration newer'):p.require_target_clean(build,'probe.exe')


if __name__=='__main__':
    unittest.main()
