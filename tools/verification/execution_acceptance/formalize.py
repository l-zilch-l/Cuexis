"""Claim and seal one admitted preparation. This file never launches a workload."""
import argparse
from pathlib import Path

from . import admission
from .binding import artifact, read


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preparation", type=Path, required=True)
    parser.add_argument("--seal", type=Path, required=True)
    parser.add_argument("--waiver", type=Path,
                        help="owner attempt-waiver artifact when the default allowance is spent")
    args = parser.parse_args()
    preparation = Path(args.preparation).resolve()
    document = admission.validate_preparation(read(preparation))
    waiver = read(args.waiver) if args.waiver else None
    claims = admission.claim(document, document["attempt_ledger"], waiver,
                             artifact(args.waiver) if args.waiver else None)
    admission.seal(preparation, claims, args.seal)
    print("SEAL " + str(Path(args.seal).resolve()), flush=True)


if __name__ == "__main__":
    main()
