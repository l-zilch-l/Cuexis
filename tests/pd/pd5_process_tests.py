import ctypes as ct
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.verification.playback_performance import process_control as p


@unittest.skipUnless(os.name=='nt','Windows Job integration')
class ProcessTests(unittest.TestCase):
    def mask(self):
        k=ct.WinDLL('kernel32',use_last_error=True);k.GetCurrentProcess.restype=ct.c_void_p
        k.GetProcessAffinityMask.argtypes=[ct.c_void_p,ct.POINTER(ct.c_size_t),ct.POINTER(ct.c_size_t)]
        a,b=ct.c_size_t(),ct.c_size_t();self.assertTrue(k.GetProcessAffinityMask(k.GetCurrentProcess(),ct.byref(a),ct.byref(b)))
        return a.value & -a.value

    def test_normal_exit_and_failure_code(self):
        with tempfile.TemporaryDirectory() as tmp:
            for code in (0,7):
                path=Path(tmp)/str(code)
                with path.open('wb') as out:
                    r=p.run([sys.executable,'-c',f'print("child-output");raise SystemExit({code})'],self.mask(),
                        cwd=tmp,env=os.environ.copy(),output=out,timeout=10)
                self.assertEqual(r.returncode,code);self.assertIn('child-output',path.read_text())

    def test_timeout_terminates_native_child_and_grandchild(self):
        with tempfile.TemporaryDirectory() as tmp:
            script=Path(tmp)/'child.py';pidfile=Path(tmp)/'pids.json'
            script.write_text('import os,sys,subprocess,json,time\n'
                'child=subprocess.Popen([sys.executable,"-c","import time;time.sleep(60)"])\n'
                'open(sys.argv[1],"w").write(json.dumps([os.getpid(),child.pid]))\n'
                'time.sleep(60)\n',encoding='utf-8')
            with (Path(tmp)/'timeout.log').open('wb') as out,self.assertRaises(subprocess.TimeoutExpired):
                p.run([sys.executable,str(script),str(pidfile)],self.mask(),cwd=tmp,
                    env=os.environ.copy(),output=out,timeout=2)
            k=ct.WinDLL('kernel32',use_last_error=True);k.OpenProcess.restype=ct.c_void_p
            k.OpenProcess.argtypes=[ct.c_ulong,ct.c_int,ct.c_ulong];k.WaitForSingleObject.argtypes=[ct.c_void_p,ct.c_ulong]
            k.CloseHandle.argtypes=[ct.c_void_p]
            for pid in json.loads(pidfile.read_text()):
                handle=k.OpenProcess(0x100000,False,pid)
                if handle:
                    try:self.assertEqual(k.WaitForSingleObject(handle,5000),0)
                    finally:k.CloseHandle(handle)


if __name__=='__main__':unittest.main()
