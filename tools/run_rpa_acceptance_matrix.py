"""Compatibility entry point for the RPA acceptance matrix runner."""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import runpy


_IMPLEMENTATION = Path(__file__).resolve().parent / "verification" / "run_rpa_acceptance_matrix.py"


def _load_implementation():
    spec = spec_from_file_location("cuexis_run_rpa_acceptance_matrix_impl", _IMPLEMENTATION)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load RPA acceptance matrix runner: {_IMPLEMENTATION}")
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


if __name__ == "__main__":
    runpy.run_path(str(_IMPLEMENTATION), run_name="__main__")
else:
    _module = _load_implementation()
    for _name, _value in vars(_module).items():
        if _name not in {"__name__", "__loader__", "__package__", "__spec__"}:
            globals()[_name] = _value
