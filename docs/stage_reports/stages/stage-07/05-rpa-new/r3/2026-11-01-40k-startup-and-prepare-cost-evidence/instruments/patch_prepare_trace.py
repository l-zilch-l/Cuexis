"""Insert prepare-trace scopes into engine/playback/src/playback_session.cpp.

Byte-level, CRLF-preserving, anchor-unique. Run from D:\\Cuexis-redesign.

Each stage is measured by calling the stage expression inside a lambda that owns the trace scope,
so the scope ends exactly when the stage returns instead of at the end of the enclosing block.
"""

import hashlib
import pathlib
import sys

TARGET = pathlib.Path("engine/playback/src/playback_session.cpp")

S = "const internal::PrepareTraceScope trace"


def wrap(stage: str, call: list[str], indent: str) -> str:
    inner = indent + "    "
    body = f"{inner}{S}{{\"{stage}\"}};\n"
    body += "".join(f"{inner}{line}\n" for line in call)
    return body


PAIRS: list[tuple[str, str]] = [
    # include
    (
        "#include <cuexis_internal/execution_diagnostics.hpp>\n",
        "#include <cuexis_internal/execution_diagnostics.hpp>\n"
        "#include <cuexis_internal/prepare_trace.hpp>\n",
    ),
    # prepareLoad(text): the chart text parse that precedes prepare()
    (
        "        auto source = PlaybackSource::fromChartText(std::string{jsonText});\n",
        "        auto source = [&] {\n"
        '            const internal::PrepareTraceScope trace{"source_parse"};\n'
        "            return PlaybackSource::fromChartText(std::string{jsonText});\n"
        "        }();\n",
    ),
    # loadDocumentStage: chart text load (parse + validation) versus canonicalisation/identity
    (
        "    auto loaded = chart::detail::loadChartForPlayback(context.chartJson, context.limits);\n",
        "    auto loaded = [&] {\n"
        '        const internal::PrepareTraceScope trace{"load_chart"};\n'
        "        return chart::detail::loadChartForPlayback(context.chartJson, context.limits);\n"
        "    }();\n",
    ),
    (
        "    std::string canonicalText;\n",
        '    const internal::PrepareTraceScope traceCanonical{"load_canonical"};\n'
        "    std::string canonicalText;\n",
    ),
    # prepare(): load_document
    (
        "            if (auto loaded = loadDocumentStage(context, artifact, diagnostics); !loaded) {\n",
        "            auto loaded = [&] {\n"
        '                const internal::PrepareTraceScope trace{"load_document"};\n'
        "                return loadDocumentStage(context, artifact, diagnostics);\n"
        "            }();\n"
        "            if (!loaded) {\n",
    ),
    # prepare(): resolve_parameters
    (
        "            if (auto resolved = resolveParametersStage(context, artifact,\n"
        "                                                       sourceState.projectDocuments, diagnostics);\n"
        "                !resolved) {\n",
        "            auto resolved = [&] {\n"
        '                const internal::PrepareTraceScope trace{"resolve_parameters"};\n'
        "                return resolveParametersStage(context, artifact, sourceState.projectDocuments,\n"
        "                                              diagnostics);\n"
        "            }();\n"
        "            if (!resolved) {\n",
    ),
    # prepare(): preflight
    (
        "            if (!preflightCapabilities(*document, additionalCapabilities, context.capabilities,\n"
        "                                       diagnostics)) {\n",
        "            const bool preflightOk = [&] {\n"
        '                const internal::PrepareTraceScope trace{"preflight"};\n'
        "                return preflightCapabilities(*document, additionalCapabilities,\n"
        "                                             context.capabilities, diagnostics);\n"
        "            }();\n"
        "            if (!preflightOk) {\n",
    ),
    # prepare(): animation_compile
    (
        "            compiledAnimation =\n"
        "                compileAnimationProgram(std::move(animationProgram), limits, diagnostics);\n",
        "            compiledAnimation = [&] {\n"
        '                const internal::PrepareTraceScope trace{"animation_compile"};\n'
        "                return compileAnimationProgram(std::move(animationProgram), limits, diagnostics);\n"
        "            }();\n",
    ),
    # prepare(): chart_compile
    (
        "            auto runtimeResult = chart::ChartCompiler::compile(*document, limits);\n",
        "            auto runtimeResult = [&] {\n"
        '                const internal::PrepareTraceScope trace{"chart_compile"};\n'
        "                return chart::ChartCompiler::compile(*document, limits);\n"
        "            }();\n",
    ),
    # prepare(): resources
    (
        "        if (sourceState.database) {\n"
        "            resourceManager = std::make_unique<assets::ResourceManager>(\n"
        "                std::move(*sourceState.database), sourceState.provider);\n"
        "        }\n",
        "        {\n"
        '            const internal::PrepareTraceScope trace{"resources"};\n'
        "            if (sourceState.database) {\n"
        "                resourceManager = std::make_unique<assets::ResourceManager>(\n"
        "                    std::move(*sourceState.database), sourceState.provider);\n"
        "            }\n"
        "        }\n",
    ),
    # prepare(): audio
    (
        "        if (auto acquired = acquireAudioStage(artifact, chartRuntime, diagnostics); !acquired) {\n",
        "        auto acquired = [&] {\n"
        '            const internal::PrepareTraceScope trace{"audio"};\n'
        "            return acquireAudioStage(artifact, chartRuntime, diagnostics);\n"
        "        }();\n"
        "        if (!acquired) {\n",
    ),
    # prepare(): runtime
    (
        "            if (auto runtime = prepareRuntimeStage(artifact, chartRuntime,\n"
        "                                                   std::move(*compiledAnimation), diagnostics);\n"
        "                !runtime) {\n",
        "            auto runtime = [&] {\n"
        '                const internal::PrepareTraceScope trace{"runtime"};\n'
        "                return prepareRuntimeStage(artifact, chartRuntime, std::move(*compiledAnimation),\n"
        "                                           diagnostics);\n"
        "            }();\n"
        "            if (!runtime) {\n",
    ),
    # prepare(): presentation
    (
        "            if (auto presentation =\n"
        "                    preparePresentationStage(context, artifact, chartRuntime, diagnostics);\n"
        "                !presentation) {\n",
        "            auto presentation = [&] {\n"
        '                const internal::PrepareTraceScope trace{"presentation"};\n'
        "                return preparePresentationStage(context, artifact, chartRuntime, diagnostics);\n"
        "            }();\n"
        "            if (!presentation) {\n",
    ),
    # prepare(): commit_runtime
    (
        "            if (auto committed = commitRuntimeStage(artifact); !committed) {\n",
        "            auto committed = [&] {\n"
        '                const internal::PrepareTraceScope trace{"commit_runtime"};\n'
        "                return commitRuntimeStage(artifact);\n"
        "            }();\n"
        "            if (!committed) {\n",
    ),
    # prepare(): commit_frame
    (
        "            if (auto sampled = commitFrameStage(context, artifact, diagnostics); !sampled) {\n",
        "            auto sampled = [&] {\n"
        '                const internal::PrepareTraceScope trace{"commit_frame"};\n'
        "                return commitFrameStage(context, artifact, diagnostics);\n"
        "            }();\n"
        "            if (!sampled) {\n",
    ),
    # prepare(): snapshot_layout
    (
        "            auto layout =\n"
        "                buildSnapshotLayout(*artifact.runtimeSession, chartRuntime, presentationCandidate);\n",
        "            auto layout = [&] {\n"
        '                const internal::PrepareTraceScope trace{"snapshot_layout"};\n'
        "                return buildSnapshotLayout(*artifact.runtimeSession, chartRuntime,\n"
        "                                           presentationCandidate);\n"
        "            }();\n",
    ),
    # prepare(): identity
    (
        "        auto assembledIdentity = assembleIdentityStage(artifact, diagnostics);\n",
        "        auto assembledIdentity = [&] {\n"
        '            const internal::PrepareTraceScope trace{"identity"};\n'
        "            return assembleIdentityStage(artifact, diagnostics);\n"
        "        }();\n",
    ),
    # prepare(): assemble (PreparedPlayback construction; ends with the try block)
    (
        "        if (!assembledIdentity) {\n"
        "            return core::unexpected(std::move(assembledIdentity.error()));\n"
        "        }\n"
        "\n"
        "        diagnostics.sortDeterministically();\n",
        "        if (!assembledIdentity) {\n"
        "            return core::unexpected(std::move(assembledIdentity.error()));\n"
        "        }\n"
        "\n"
        '        const internal::PrepareTraceScope traceAssemble{"assemble"};\n'
        "        diagnostics.sortDeterministically();\n",
    ),
]


def main() -> int:
    raw = TARGET.read_bytes()
    if raw[:3] == b"\xef\xbb\xbf":
        print("refused: BOM present")
        return 1
    crlf = raw.count(b"\r\n")
    lf_only = raw.count(b"\n") - crlf
    before = hashlib.sha256(raw).hexdigest()
    print(f"before: bytes={len(raw)} sha256={before} crlf={crlf} lf_only={lf_only}")
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
