"""Split serializeCanonical into its two halves under the prepare trace.

`chart_canonical_write` currently reports one number (656 ms at 40000 entities) but
serializeCanonical does two very different things:

  1. canonicalize() - a recursive walk that normalises numbers and ratifies the
     deterministic array order. It only reads the tree.
  2. json::serialize() - which, per json_support/src/parse.cpp:153, converts the whole
     json::Value tree into an nlohmann::json tree and then dumps it. That is a second
     allocation of every node in the document.

Without the split there is no way to know whether the fast win is "walk less" or
"stop building a second tree", so the split is measured before either is attempted.

The scopes are the same env-gated, default-off, no-op-when-disabled PrepareTraceScope
already used by the chart loader, so the default path is unchanged and every recorded
measurement stays valid on the same binary.
"""

import hashlib
import pathlib
import sys

REL = "engine/chart/src/chart_writer.cpp"

INCLUDE_OLD = "#include \"chart_writer_internal.hpp\"\n"
INCLUDE_NEW = "#include <cuexis_internal/prepare_trace.hpp>\n\n#include \"chart_writer_internal.hpp\"\n"

BODY_OLD = """    auto normalized = canonicalize(value, sortBehaviorArrays);
    if (!normalized) {
        return core::unexpected(std::move(normalized.error()));
    }
    auto serialized = json::serialize(value, json::SerializeStyle::Pretty);
    if (!serialized) {
        return serialized;
    }
"""

BODY_NEW = """    if (auto normalized = [&] {
            const internal::PrepareTraceScope trace{"chart_canonical_normalize"};
            return canonicalize(value, sortBehaviorArrays);
        }();
        !normalized) {
        return core::unexpected(std::move(normalized.error()));
    }
    auto serialized = [&] {
        const internal::PrepareTraceScope trace{"chart_canonical_dump"};
        return json::serialize(value, json::SerializeStyle::Pretty);
    }();
    if (!serialized) {
        return serialized;
    }
"""


def describe(payload: bytes) -> str:
    crlf = payload.count(b"\r\n")
    lf_only = payload.count(b"\n") - crlf
    return f"bytes={len(payload)} sha256={hashlib.sha256(payload).hexdigest()} crlf={crlf} lf_only={lf_only}"


def main() -> int:
    path = pathlib.Path(REL)
    payload = path.read_bytes()
    if payload.startswith(b"\xef\xbb\xbf"):
        print("refuse: file has a BOM", file=sys.stderr)
        return 2
    raw = payload.decode("utf-8")
    crlf = raw.count("\r\n")
    if crlf and crlf != raw.count("\n"):
        print("refuse: mixed line endings", file=sys.stderr)
        return 2
    # Match in LF space so the anchors stay readable, then restore the original ending.
    text = raw.replace("\r\n", "\n")
    if "PrepareTraceScope" in text:
        print("refuse: already patched", file=sys.stderr)
        return 2

    for name, old in (("include", INCLUDE_OLD), ("body", BODY_OLD)):
        if text.count(old) != 1:
            print(f"refuse: anchor {name} count {text.count(old)}", file=sys.stderr)
            return 2

    text = text.replace(INCLUDE_OLD, INCLUDE_NEW).replace(BODY_OLD, BODY_NEW)
    if crlf:
        text = text.replace("\n", "\r\n")
    after = text.encode("utf-8")

    print(f"eol: {'CRLF' if crlf else 'LF'}")
    print(f"before: {describe(payload)}")
    print(f"after:  {describe(after)}")
    path.write_bytes(after)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
