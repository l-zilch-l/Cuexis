"""Insert prepare-trace scopes into engine/chart/src/chart_v4_loader.cpp.

Byte-level, CRLF-preserving, anchor-unique. Decomposes the chart text load into the generic JSON
DOM parse, the typed reader, and the canonical writer that produces the identity bytes.
"""

import hashlib
import pathlib
import sys

TARGET = pathlib.Path("engine/chart/src/chart_v4_loader.cpp")

PAIRS: list[tuple[str, str]] = [
    (
        "#include <cuexis/json/reader.hpp>\n",
        "#include <cuexis/json/reader.hpp>\n#include <cuexis_internal/prepare_trace.hpp>\n",
    ),
    (
        "    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {\n"
        "        return ChartDispatchResult{false, std::nullopt, std::nullopt,\n"
        "                                   {},    std::nullopt, std::move(diagnostics)};\n"
        "    }\n"
        "    auto parsed =\n"
        "        json::parse(jsonText, json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,\n"
        "                                                limits.maxStringBytes});\n",
        "    if (detail::rejectInvalidDiagnosticLimit(diagnostics, limits)) {\n"
        "        return ChartDispatchResult{false, std::nullopt, std::nullopt,\n"
        "                                   {},    std::nullopt, std::move(diagnostics)};\n"
        "    }\n"
        "    auto parsed = [&] {\n"
        '        const internal::PrepareTraceScope trace{"json_dom_parse"};\n'
        "        return json::parse(jsonText,\n"
        "                           json::ParseLimits{limits.maxInputBytes, limits.maxNestingDepth,\n"
        "                                             limits.maxStringBytes});\n"
        "    }();\n",
    ),
    (
        "    if (isV4) {\n"
        "        auto input = std::make_shared<ParsedChartInput>(ParsedChartInput{std::move(*parsed)});\n"
        "        auto loaded = detail::loadV4Value(input->value, limits);\n",
        "    if (isV4) {\n"
        "        auto input = std::make_shared<ParsedChartInput>(ParsedChartInput{std::move(*parsed)});\n"
        "        auto loaded = [&] {\n"
        '            const internal::PrepareTraceScope trace{"chart_typed_read"};\n'
        "            return detail::loadV4Value(input->value, limits);\n"
        "        }();\n",
    ),
    (
        "    auto loaded = detail::loadCanonicalValue(*parsed, limits);\n",
        "    auto loaded = [&] {\n"
        '        const internal::PrepareTraceScope trace{"chart_typed_read"};\n'
        "        return detail::loadCanonicalValue(*parsed, limits);\n"
        "    }();\n",
    ),
    (
        "        if (auto text = detail::writeCanonicalValue(std::move(*parsed), limits)) {\n"
        "            canonicalText = std::move(*text);\n"
        "        }\n",
        "        {\n"
        '            const internal::PrepareTraceScope trace{"chart_canonical_write"};\n'
        "            if (auto text = detail::writeCanonicalValue(std::move(*parsed), limits)) {\n"
        "                canonicalText = std::move(*text);\n"
        "            }\n"
        "        }\n",
    ),
]


def main() -> int:
    raw = TARGET.read_bytes()
    if raw[:3] == b"\xef\xbb\xbf":
        print("refused: BOM present")
        return 1
    crlf = raw.count(b"\r\n")
    lf_only = raw.count(b"\n") - crlf
    print(f"before: bytes={len(raw)} sha256={hashlib.sha256(raw).hexdigest()} "
          f"crlf={crlf} lf_only={lf_only}")
    if lf_only:
        print("refused: mixed line endings")
        return 1
    text = raw.decode("utf-8").replace("\r\n", "\n")
    if "prepare_trace.hpp" in text:
        print("refused: already patched")
        return 1
    for anchor, replacement in PAIRS:
        count = text.count(anchor)
        if count != 1:
            print(f"refused: anchor count {count} for {anchor.splitlines()[0][:70]!r}")
            return 1
        text = text.replace(anchor, replacement)
    out = text.replace("\n", "\r\n").encode("utf-8")
    TARGET.write_bytes(out)
    print(f"after:  bytes={len(out)} sha256={hashlib.sha256(out).hexdigest()}")
    print(f"edits applied: {len(PAIRS)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
