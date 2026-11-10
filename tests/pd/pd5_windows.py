"""Compatibility entry; implementation moved to tools.verification.playback_performance.window_protocol."""
import importlib
from pathlib import Path
import runpy
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
_MODULE = "tools.verification.playback_performance.window_protocol"
if __name__ == '__main__':
    runpy.run_module(_MODULE, run_name='__main__')
else:
    sys.modules[__name__] = importlib.import_module(_MODULE)
