"""Retired standalone canary preparation entry point; use prepare_etw_tools.py.

Previously sealed packages remain historical evidence. This entry point refuses
new preparations before inspecting inputs or creating an output directory.
"""
import argparse
from pathlib import Path


RETIRED_MESSAGE = (
    'prepare_etw_canary.py is retired; use prepare_etw_tools.py '
    '--output <new-directory> --wpr <plan-pinned-wpr.exe> instead. '
    'Do not update existing sealed packages.'
)


def prepare(output, wpr):
    """Keep the import signature, but never produce an obsolete v2 package."""
    raise RuntimeError(RETIRED_MESSAGE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--wpr', type=Path)
    parser.parse_args()
    parser.error(RETIRED_MESSAGE)


if __name__ == '__main__':
    main()
