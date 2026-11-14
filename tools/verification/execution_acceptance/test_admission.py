import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.verification.execution_acceptance import admission
from tools.verification.execution_acceptance.binding import artifact, write_once
from tools.verification.execution_acceptance import build_binding


class AdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.sequence = 0
        self.source = {"sha": "a" * 40, "tree": "b" * 40, "files": {}}
        budget = self.file({"schema": "rpa.r4.budget-proposal.v1", "profiles": [{"id": "scale"}]})
        acceptance = self.file({"schema": "rpa.r4.o1-acceptance.v1", "accepted": True,
                                "accepted_input_sha256": {
                                    "tools/verification/execution_acceptance/budget-proposal.v1.json":
                                    budget["sha256"]}})
        proof = lambda kind: self.file({"schema": "rpa.execution." + kind + ".v1",
                                       "passed": True, "unit": "seek-natural",
                                       "source_sha": self.source["sha"]})
        self.document = dict(schema="rpa.execution.preparation.v1", formal_registration=False,
                             baseline_source_binding={"sha": admission.PRE_PD, "tree": "c" * 40,
                                                      "files": {}}, windows=3,
                             processes_per_window=3, gap_seconds=600,
                             orders=admission.ORDERS, regression_limit=1.10, noise_limit=.15,
                             source_binding=self.source, acceptance_binding=acceptance,
                             budget_binding=budget, authorization_binding=self.file({
                                 "schema": "rpa.r4.execution-authorization.v1",
                                 "attempt_ledger": str(self.root / "ledger"),
                                 "automatic_work_authorized": True}),
                             attempt_ledger=str(self.root / "ledger"),
                             tool_bindings={"driver": self.file({"tool": 1})},
                             fixture_bindings={"content": self.file({"fixture": 1})},
                             binary_bindings={"A": self.file({"binary": "A"}),
                                              "B": self.file({"binary": "B"})},
                             baseline_sha=admission.PRE_PD, invalid_process_rules=["none"],
                             units=[dict(id="seek-natural", stable_protocol_id="seek-natural-64-release",
                                         workload="seek", seek_class="natural", entities=64, config="Release",
                                         comparison="A/B", baseline="pre-PD", cold_process=False,
                                         requests_per_process=16, completion_endpoint="target-present",
                                         profile="scale", requires_o1_scale=True,
                                         baseline_binary="A", candidate_binary="B", fixture="content",
                                         required_metrics=["seek"], measurement_mode="normal",
                                         semantic_binding=proof("semantic"), pilot_binding=proof("pilot"),
                                         environment_binding=proof("environment"))])
        self.document["binary_build_bindings"] = {}
        for side, source in (("B", self.source), ("A", self.document["baseline_source_binding"])):
            metadata = dict(schema="rpa.execution.build-binding.v1", source_sha=source["sha"],
                            source_tree=source["tree"], source_content={}, config="Release",
                            binary=self.document["binary_bindings"][side], source_root=str(self.root),
                            options={key: "OFF" for key in build_binding.BOUND_OPTIONS})
            metadata["options"].update(CUEXIS_LIBRARY_TYPE="STATIC", CUEXIS_ENABLE_CHART_V5_CANDIDATE="ON")
            for key in ("cache", "compile_commands", "compiler", "build_log", "vcpkg_manifest", "vcpkg_configuration", "target_commands"):
                metadata[key] = self.file({"metadata": key})
            cache = Path(metadata["cache"]["path"])
            cache.write_text("\n".join(f"{key}:STRING={value}" for key, value in dict(
                metadata["options"], CMAKE_BUILD_TYPE="Release", CMAKE_HOME_DIRECTORY=str(self.root)).items()), encoding="utf-8")
            metadata["cache"] = artifact(cache)
            metadata["dependencies"] = self.file({"schema": "rpa.execution.dependencies.v1",
                                                  "files": {}, "objects": [{"synthetic": True}]})
            compiler = Path(metadata["compiler"]["path"])
            compiler.write_text('set(CMAKE_CXX_COMPILER "cl.exe")\nset(CMAKE_CXX_COMPILER_ID "MSVC")\nset(CMAKE_CXX_COMPILER_VERSION "19.51")\nset(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "x64")\nset(CMAKE_CXX_PLATFORM_ID "Windows")\n', encoding="utf-8")
            metadata["compiler"] = artifact(compiler)
            from tools.verification.execution_acceptance.build_binding import toolchain_signature
            metadata["toolchain_signature"] = toolchain_signature(compiler, Path(metadata["vcpkg_configuration"]["path"]))
            self.document["binary_build_bindings"][side] = self.file(metadata)
        for kind in ("semantic", "pilot", "environment"):
            path = Path(self.document["units"][0][kind + "_binding"]["path"])
            value = json.loads(path.read_text(encoding="utf-8"))
            value.update(endpoint="target-present",
                         candidate_binary_sha256=self.document["binary_bindings"]["B"]["sha256"],
                         raw_bindings=[self.file({"raw": kind})])
            path.write_text(json.dumps(value), encoding="utf-8")
            self.document["units"][0][kind + "_binding"] = artifact(path)
        # The default callable is captured at definition time; replace the validator wrapper.
        original = admission.validate_preparation
        self.validation_patch = patch.object(admission, "validate_preparation",
            side_effect=lambda doc: original(doc, source_verifier=lambda bound: None))
        self.validation_patch.start()
        self.addCleanup(self.validation_patch.stop)

    def file(self, value):
        self.sequence += 1
        path = self.root / f"file-{self.sequence}.json"
        write_once(path, value)
        return artifact(path)

    def test_binding_and_observer_failures_block_admission(self):
        admission.validate_preparation(self.document)
        mutations = [lambda d: d.update(baseline_sha="b" * 40),
                     lambda d: d.update(invalid_process_rules=[]),
                     lambda d: d.update(tool_bindings={}),
                     lambda d: d["units"][0].update(measurement_mode="memory"),
                     lambda d: d["units"][0].update(requests_per_process=3),
                     lambda d: d["units"][0].update(fixture="missing")]
        for mutate in mutations:
            document = copy.deepcopy(self.document)
            mutate(document)
            with self.subTest(mutation=mutate), self.assertRaises((ValueError, KeyError)):
                admission.validate_preparation(document)
        Path(self.document["binary_bindings"]["B"]["path"]).write_bytes(b"changed")
        with self.assertRaises(ValueError):
            admission.validate_preparation(self.document)

    def test_claim_is_single_use_and_bound_to_exact_preparation(self):
        path = self.root / "preparation.json"
        write_once(path, self.document)
        claims = admission.claim(self.document, self.root / "ledger")
        sealed = self.root / "seal.json"
        admission.seal(path, claims, sealed)
        self.assertEqual(admission.read_sealed(sealed), self.document)
        with self.assertRaises(FileExistsError):
            admission.seal(path, claims, self.root / "other-seal.json")
        with self.assertRaises((ValueError, KeyError)):
            admission.claim(self.document, self.root / "ledger")
        path.write_text(json.dumps(dict(self.document, noise_limit=.2)), encoding="utf-8")
        with self.assertRaises(ValueError):
            admission.read_sealed(sealed)

    def test_p8_allowance_stays_one_even_when_a_remedy_is_supplied(self):
        unit = self.document["units"][0]
        unit.update(workload="p8", comparison="P8-single-sided", invocations=1, entities=40000,
                    reuse="low", stable_protocol_id="p8-40000-low-release")
        unit["retry_disposition_binding"] = self.file({"remedy": True})
        self.assertEqual(admission.allowance(unit), 1)

    def test_memory_and_p8_reject_forged_complete_proofs_before_claim_or_seal(self):
        for workload, entities in (("memory", 64), ("memory", 2048), ("p8", 40000)):
            for backend in ("cpu", "device"):
                for reuse in ("low", "high"):
                    document = copy.deepcopy(self.document)
                    unit = document["units"][0]
                    suffix = "-device" if backend == "device" else ""
                    unit.update(id=f"{workload}-{entities}-{reuse}", workload=workload,
                                stable_protocol_id=f"{workload}-{entities}-{reuse}{suffix}-release",
                                entities=entities, rows=4096 if workload == "p8" else entities,
                                backend=backend, reuse=reuse, adapter="capacity",
                                measurement_mode="memory", requests_per_process=1, invocations=1,
                                comparison="P8-single-sided" if workload == "p8" else "B-only",
                                completion_endpoint="active-close-final-release")
                    for kind in ("semantic", "pilot", "environment", "observer"):
                        unit[kind + "_binding"] = self.file({
                            "schema": "rpa.execution." +
                                      ("observer-admission" if kind == "observer" else kind) + ".v1",
                            "passed": True, "complete_semantic_graph": True,
                            "formal_acceptance": True, "unit": unit["id"], "mode": "memory",
                            "source_sha": self.source["sha"],
                            "endpoint": unit["completion_endpoint"],
                            "candidate_binary_sha256": document["binary_bindings"]["B"]["sha256"],
                            "raw_bindings": [self.file({"forged_complete": True})]})
                    metadata = json.loads(Path(document["binary_build_bindings"]["B"]["path"]).read_text())
                    cache = Path(metadata["cache"]["path"])
                    text = cache.read_text()
                    for option in ("CUEXIS_ALLOCATION_LIFETIME_OBSERVER",
                                   "CUEXIS_EXECUTION_ALLOCATION_OBSERVER"):
                        metadata["options"][option] = "ON"
                        text = text.replace(option + ":STRING=OFF", option + ":STRING=ON")
                    cache.write_text(text)
                    metadata["cache"] = artifact(cache)
                    document["binary_build_bindings"]["B"] = self.file(metadata)
                    preparation = Path(self.file(document)["path"])
                    ledger, sealed = self.root / "ledger", self.root / "seal.json"
                    with self.subTest(workload=workload, entities=entities, backend=backend, reuse=reuse):
                        for operation in (
                                lambda: admission.validate_preparation(document),
                                lambda: admission.claim(document, ledger),
                                lambda: admission.seal(preparation, [], sealed)):
                            with self.assertRaisesRegex(ValueError, "complete semantic ownership/lifetime graph validator"):
                                operation()
                        self.assertFalse(ledger.exists())
                        self.assertFalse(sealed.exists())

    def test_p8_cannot_masquerade_as_another_comparison(self):
        for comparison in ("A/B", "B-only"):
            document = copy.deepcopy(self.document)
            document["units"][0].update(workload="p8", comparison=comparison, entities=40000,
                                        reuse="low", stable_protocol_id="p8-40000-low-release")
            with self.subTest(comparison=comparison), self.assertRaisesRegex(ValueError, "P8 workload"):
                admission.claim(document, self.root / "ledger")

    def test_normal_build_rejects_instrumentation_and_forged_options(self):
        original = json.loads(Path(self.document["binary_build_bindings"]["B"]["path"]).read_text())
        for option in build_binding.INSTRUMENTATION_OPTIONS:
            metadata = copy.deepcopy(original)
            metadata["options"][option] = "ON"
            cache = Path(metadata["cache"]["path"])
            text = cache.read_text()
            cache.write_text(text.replace(option + ":STRING=OFF", option + ":STRING=ON"))
            metadata["cache"] = artifact(cache)
            with self.subTest(option=option), self.assertRaisesRegex(ValueError, "instrumentation"):
                build_binding.verify(metadata, self.source, "normal")
            cache.write_text(text)
        metadata = copy.deepcopy(original)
        metadata["options"]["CUEXIS_PLAYBACK_TRACE_HARNESS"] = "ON"
        with self.assertRaisesRegex(ValueError, "bound CMake cache"):
            build_binding.verify(metadata, self.source, "normal")
        for flag in ("-fsanitize=address", "/fsanitize:address", "--coverage", "-fprofile-arcs",
                     "-ftest-coverage", "-DCUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY=1",
                     '/D"CUEXIS_RPA_OBSERVATION_TEST_ONLY=1"',
                     '-DCUEXIS_EXECUTION_DIAGNOSTICS_TEST_ONLY=ON'):
            metadata = copy.deepcopy(original)
            commands = Path(self.file({"commands": flag})["path"])
            commands.write_text(flag, encoding="utf-8")
            metadata["target_commands"] = artifact(commands)
            with self.subTest(flag=flag), self.assertRaisesRegex(ValueError, "instrumentation"):
                build_binding.verify(metadata, self.source, "normal")

    def test_sealed_first_attempt_allows_one_bound_remedy_and_never_a_third(self):
        from tools.verification.execution_acceptance.binding import sha256
        path = self.root / "preparation.json"
        write_once(path, self.document)
        claims = admission.claim(self.document, self.root / "ledger")
        admission.seal(path, claims, self.root / "seal.json")
        unit = self.document["units"][0]
        unit["retry_disposition_binding"] = self.file({
            "schema": "rpa.execution.retry-disposition.v1",
            "stable_protocol_id": unit["stable_protocol_id"],
            "previous_claim_sha256": sha256(claims[0]),
            "verified_remedy_binding": self.file({"verified": True})})
        unit["id"] = "renamed-seek"
        for kind in ("semantic", "pilot", "environment"):
            proof = json.loads(Path(unit[kind + "_binding"]["path"]).read_text(encoding="utf-8"))
            proof["unit"] = unit["id"]
            unit[kind + "_binding"] = self.file(proof)
        second = admission.claim(self.document, self.root / "ledger")
        self.assertEqual(second[0].name, "attempt-2.json")
        with self.assertRaisesRegex(ValueError, "allowance exhausted"):
            admission.claim(self.document, self.root / "ledger")
        with self.assertRaisesRegex(ValueError, "ledger differs"):
            admission.claim(self.document, self.root / "other-ledger")


if __name__ == "__main__":
    unittest.main()
