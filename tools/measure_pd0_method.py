"""Compatibility entry point for the PD-0 verification tool."""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import runpy
import sys


_IMPLEMENTATION = Path(__file__).resolve().parent / "verification" / "measure_pd0_method.py"
_IMPLEMENTATION_DIR = str(_IMPLEMENTATION.parent)


def _load_implementation():
    if _IMPLEMENTATION_DIR not in sys.path:
        sys.path.insert(0, _IMPLEMENTATION_DIR)
    spec = spec_from_file_location("cuexis_measure_pd0_method_impl", _IMPLEMENTATION)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load PD-0 verification tool: {_IMPLEMENTATION}")
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


if __name__ == "__main__":
    if _IMPLEMENTATION_DIR not in sys.path:
        sys.path.insert(0, _IMPLEMENTATION_DIR)
    runpy.run_path(str(_IMPLEMENTATION), run_name="__main__")
else:
    _module = _load_implementation()
    for _name, _value in vars(_module).items():
        if _name not in {"__name__", "__loader__", "__package__", "__spec__"}:
            globals()[_name] = _value
