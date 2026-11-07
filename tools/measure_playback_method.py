"""Convenience entry point for the playback baseline method tool."""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import runpy
import sys


_IMPLEMENTATION = Path(__file__).resolve().parent / "verification" / "playback_performance" / "baseline_method.py"
_REPOSITORY_ROOT = str(Path(__file__).resolve().parents[1])


def _load_implementation():
    if _REPOSITORY_ROOT not in sys.path:
        sys.path.insert(0, _REPOSITORY_ROOT)
    spec = spec_from_file_location("cuexis_playback_baseline_method_impl", _IMPLEMENTATION)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load playback baseline method tool: {_IMPLEMENTATION}")
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


if __name__ == "__main__":
    if _REPOSITORY_ROOT not in sys.path:
        sys.path.insert(0, _REPOSITORY_ROOT)
    runpy.run_path(str(_IMPLEMENTATION), run_name="__main__")
else:
    _module = _load_implementation()
    for _name, _value in vars(_module).items():
        if _name not in {"__name__", "__loader__", "__package__", "__spec__"}:
            globals()[_name] = _value
