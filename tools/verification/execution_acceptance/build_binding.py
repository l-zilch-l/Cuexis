"""Bind an explicit configured build to its source, metadata and final executable."""
import json
from pathlib import Path
import re
import subprocess

from .binding import artifact, clean_source, read, sha256, verify_artifact, write_once


INSTRUMENTATION_OPTIONS = (
    "CUEXIS_PD_METRICS_TEST_ONLY", "CUEXIS_ALLOCATION_LIFETIME_OBSERVER",
    "CUEXIS_EXECUTION_ALLOCATION_OBSERVER", "CUEXIS_PLAYBACK_TRACE_HARNESS",
    "CUEXIS_RPA_OBSERVATION_TEST_ONLY", "CUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY",
    "CUEXIS_ENABLE_SANITIZERS", "CUEXIS_ENABLE_COVERAGE", "CUEXIS_ENABLE_CLANG_TIDY")
BOUND_OPTIONS = ("CUEXIS_LIBRARY_TYPE", "CUEXIS_ENABLE_CHART_V5_CANDIDATE") + INSTRUMENTATION_OPTIONS


def cache_entries(path):
    result = {}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r"([^#/:][^:]*):[^=]*=(.*)$", line)
        if match:
            result[match[1]] = match[2]
    return result


def dependency_inventory(build, ninja, target, binary):
    """Validate the whole target closure, including Ninja's compiler header dependencies."""
    inputs = subprocess.check_output([ninja, "-C", str(build), "-t", "inputs", target]).decode("utf-8")
    paths = [Path(line) if Path(line).is_absolute() else build / line for line in inputs.splitlines()]
    objects = [path.resolve() for path in paths if path.suffix in (".obj", ".o")]
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    sources = {str(Path(row["output"]).resolve()): Path(row["file"]).resolve() for row in commands}
    if not objects:
        raise ValueError("target has no compiler object inventory")
    files, records = {}, []
    for path in objects:
        text = subprocess.check_output([ninja, "-C", str(build), "-t", "deps",
                                        path.relative_to(build).as_posix()]).decode("utf-8")
        count = re.search(r"#deps (\d+)", text)
        if count is None or int(count[1]) < 1:
            raise ValueError("missing compiler dependency inventory: " + str(path))
        dependencies = [Path(line.strip()) for line in text.splitlines()[1:] if line.strip()]
        dependencies = [(item if item.is_absolute() else build / item).resolve() for item in dependencies]
        if len(dependencies) != int(count[1]):
            raise ValueError("incomplete compiler dependency inventory")
        if str(path) not in sources:
            raise ValueError("compiler object has no source command: " + str(path))
        if sources[str(path)] not in dependencies:
            dependencies.append(sources[str(path)])
        if max(item.stat().st_mtime_ns for item in dependencies) > path.stat().st_mtime_ns:
            raise ValueError("compiler object is older than an input: " + str(path))
        for item in (path, *dependencies):
            if str(item) not in files:
                files[str(item)] = artifact(item)
        records.append({"object": str(path), "dependencies": [str(item) for item in dependencies],
                        "raw_inventory": text})
    for path in paths:
        if path.suffix in (".lib", ".a"):
            files[str(path.resolve())] = artifact(path)
    if max(Path(name).stat().st_mtime_ns for name in files) > binary.stat().st_mtime_ns:
        raise ValueError("executable predates a compilation/link input")
    return {"schema": "rpa.execution.dependencies.v1", "files": files, "objects": records}


def bind(build, executable, source, build_log):
    build, source = Path(build).resolve(), Path(source).resolve()
    identity = clean_source(source)
    cache_path = build / "CMakeCache.txt"
    cache = cache_entries(cache_path)
    if Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != source:
        raise ValueError("configured source root does not match bound source")
    required = {"CUEXIS_LIBRARY_TYPE": "STATIC", "CUEXIS_ENABLE_CHART_V5_CANDIDATE": "ON"}
    if any(cache.get(key) != value for key, value in required.items()):
        raise ValueError("reference build configuration is not admitted")
    commands = build / "compile_commands.json"
    rows = json.loads(commands.read_text(encoding="utf-8"))
    if not rows or not any(source.is_relative_to(Path(row["file"]).resolve().parent) or
                           Path(row["file"]).resolve().is_relative_to(source) for row in rows):
        raise ValueError("empty or unrelated compile command inventory")
    compiler_files = sorted(build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"))
    if len(compiler_files) != 1:
        raise ValueError("ambiguous configured compiler identity")
    binary = artifact(build / "bin" / executable)
    command_file = Path(build_log).with_name(Path(build_log).stem + "-" + executable + ".commands.txt")
    command_text = subprocess.check_output([cache["CMAKE_MAKE_PROGRAM"], "-C", str(build),
                                            "-t", "commands", executable.removesuffix(".exe")])
    with command_file.open("xb") as stream:
        stream.write(command_text)
    dependency_file = command_file.with_suffix(".dependencies.json")
    write_once(dependency_file, dependency_inventory(build, cache["CMAKE_MAKE_PROGRAM"],
                                                    executable.removesuffix(".exe"), Path(binary["path"])))
    return {"schema": "rpa.execution.build-binding.v1", "binary": binary,
            "source_sha": identity["sha"], "source_tree": identity["tree"],
            "source_root": str(source), "config": cache["CMAKE_BUILD_TYPE"],
            "options": {key: cache.get(key, "OFF") for key in BOUND_OPTIONS},
            "cache": artifact(cache_path), "compile_commands": artifact(commands),
            "compiler": artifact(compiler_files[0]), "build_log": artifact(build_log),
            "target_commands": artifact(command_file),
            "dependencies": artifact(dependency_file),
            "toolchain_signature": toolchain_signature(compiler_files[0], source / "vcpkg-configuration.json"),
            "vcpkg_manifest": artifact(source / "vcpkg.json"),
            "vcpkg_configuration": artifact(source / "vcpkg-configuration.json"),
            "source_content": {name: item["sha256"] for name, item in identity["files"].items()}}


def toolchain_signature(compiler, dependencies):
    text = Path(compiler).read_text(encoding="utf-8-sig")
    fields = {}
    for key in ("CMAKE_CXX_COMPILER", "CMAKE_CXX_COMPILER_ID", "CMAKE_CXX_COMPILER_VERSION",
                "CMAKE_CXX_COMPILER_ARCHITECTURE_ID", "CMAKE_CXX_PLATFORM_ID"):
        match = re.search(r"set\(" + key + r' "?([^"\)\n]+)"?\)', text)
        if not match:
            raise ValueError("missing compiler identity field: " + key)
        fields[key] = match[1]
    fields["dependency_baseline_sha256"] = sha256(dependencies)
    return fields


def verify(document, source_binding, mode):
    if (document.get("schema") != "rpa.execution.build-binding.v1" or
            document.get("source_sha") != source_binding["sha"] or
            document.get("source_tree") != source_binding["tree"]):
        raise ValueError("build is bound to different source identity")
    for key in ("binary", "cache", "compile_commands", "compiler", "build_log",
                "target_commands", "dependencies", "vcpkg_manifest", "vcpkg_configuration"):
        verify_artifact(document[key])
    dependencies = read(document["dependencies"]["path"])
    if dependencies.get("schema") != "rpa.execution.dependencies.v1" or not dependencies.get("objects"):
        raise ValueError("missing target compiler dependency validation")
    for item in dependencies["files"].values():
        verify_artifact(item)
    if document["toolchain_signature"] != toolchain_signature(
            Path(document["compiler"]["path"]), Path(document["vcpkg_configuration"]["path"])):
        raise ValueError("compiler or dependency baseline identity changed")
    if document["source_content"] != {name: item["sha256"] for name, item in source_binding["files"].items()}:
        raise ValueError("build source content changed")
    options = document["options"]
    cache = cache_entries(Path(document["cache"]["path"]))
    if options != {key: cache.get(key, "OFF") for key in BOUND_OPTIONS}:
        raise ValueError("declared options differ from the bound CMake cache")
    if (cache.get("CMAKE_BUILD_TYPE") != document["config"] or
            Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != Path(document["source_root"]).resolve()):
        raise ValueError("configured source/configuration differs from build binding")
    if options["CUEXIS_LIBRARY_TYPE"] != "STATIC" or options["CUEXIS_ENABLE_CHART_V5_CANDIDATE"] != "ON":
        raise ValueError("reference build configuration is not admitted")
    if mode == "normal" and any(options[key] != "OFF" for key in INSTRUMENTATION_OPTIONS):
        raise ValueError("normal timing build contains disabled instrumentation")
    if mode == "normal":
        commands = Path(document["target_commands"]["path"]).read_text(encoding="utf-8")
        for macro in INSTRUMENTATION_OPTIONS + ("CUEXIS_PD_ALLOCATION_OBSERVER",):
            definitions = re.findall(r'[/\-]D\s*"?' + macro + r'(?:=([^\s"]+))?(?=[\s"]|$)', commands)
            if any(value.lower() not in ("0", "off", "false") for value in definitions):
                raise ValueError("target compile closure enables forbidden instrumentation: " + macro)
        if re.search(r"(?:-fsanitize[=\s]|/fsanitize:|--coverage\b|-fprofile[^\s]*|-ftest-coverage\b|(?:^|\s)-pg(?:\s|$)|/Gh\b|/GH\b)", commands):
            raise ValueError("target compile closure contains compiler instrumentation")
    if mode == "memory" and (options["CUEXIS_ALLOCATION_LIFETIME_OBSERVER"] != "ON" or
                             options["CUEXIS_EXECUTION_ALLOCATION_OBSERVER"] != "ON"):
        raise ValueError("memory build lacks extended lifetime attribution")
    if mode == "stage-timing" and (options["CUEXIS_PD_METRICS_TEST_ONLY"] != "ON" or
            any(options[key] != "OFF" for key in INSTRUMENTATION_OPTIONS
                if key != "CUEXIS_PD_METRICS_TEST_ONLY")):
        raise ValueError("stage timing requires only its dedicated metrics instrumentation")
    if mode == "player-trace" and (options["CUEXIS_PLAYBACK_TRACE_HARNESS"] != "ON" or
            any(options[key] != "OFF" for key in INSTRUMENTATION_OPTIONS
                if key != "CUEXIS_PLAYBACK_TRACE_HARNESS")):
        raise ValueError("Player trace requires only its dedicated test harness")
    return document
