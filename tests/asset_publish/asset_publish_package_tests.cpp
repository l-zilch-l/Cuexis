#include "asset_publish_test_fixture.hpp"

// Package replacement, pair rollback, R5 recovery and manifest cases.

TEST_CASE("E3 package publication validates the bytes on disk before replacing the target",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-basic");
    const auto target = root / "static.cxc";
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();

    const auto published = cuexis::tools::publishPackage(request);
    requireOk(published);
    CHECK_FALSE(published->replacedExisting);
    CHECK(published->packageIdentity.size() == 64U);
    CHECK(published->bytes == readBytes(target).size());
    CHECK(loadPackage(target).identity().hex() == published->packageIdentity);

    // A republish of the same batch is a byte-identical replacement, not a new package.
    const auto again = cuexis::tools::publishPackage(request);
    requireOk(again);
    CHECK(again->replacedExisting);
    CHECK(again->packageIdentity == published->packageIdentity);

    // No temporary or backup siblings were left behind; only the published package and the
    // persistent lock file remain.
    std::error_code status;
    for (const auto& entry : fs::directory_iterator{root, status}) {
        const auto name = entry.path().filename().string();
        CHECK(name.find(".tmp.") == std::string::npos);
        CHECK((name == target.filename().string() || name == ".cuexis.publish.lock"));
    }
}

TEST_CASE("E3 a failed package replacement keeps the previous valid package",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-failure");
    const auto target = root / "static.cxc";
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();
    const auto published = cuexis::tools::publishPackage(request);
    requireOk(published);
    const auto original = readBytes(target);

    const auto fixture = candidateFixture();
    request.entries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.extensionsJson = fixture.extensionJson;
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_REPLACE", "1"};
        const auto failed = cuexis::tools::publishPackage(request);
        REQUIRE_FALSE(failed.has_value());
        CHECK(errorCode(failed.error()) == "asset.publish.replace_failed");
    }
    // The previous package is byte-identical: a failure never publishes a partial result.
    CHECK(readBytes(target) == original);
    CHECK(loadPackage(target).identity().hex() == published->packageIdentity);
}

TEST_CASE("E3 a missing package target directory fails closed", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-missing-directory");
    PackagePublishRequest request;
    request.target = root / "absent" / "static.cxc";
    request.entries = staticProjectEntries();

    const auto published = cuexis::tools::publishPackage(request);
    REQUIRE_FALSE(published.has_value());
    CHECK(errorCode(published.error()) == "asset.publish.invalid_request");
    CHECK_FALSE(fs::exists(request.target));
}

TEST_CASE("E3 a package target that is not a regular file is refused", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-directory-target");
    const auto target = root / "static.cxc";
    // A real path that happens to be a directory: publishing over it would silently destroy a
    // directory the caller never identified as a package.
    fs::create_directories(target / "keep");
    writeBytes(target / "keep" / "file.bin", "untouched");

    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();
    const auto published = cuexis::tools::publishPackage(request);
    REQUIRE_FALSE(published.has_value());
    CHECK(errorCode(published.error()) == "asset.publish.invalid_request");

    CHECK(fs::is_directory(target));
    CHECK(readBytes(target / "keep" / "file.bin") == asBytes("untouched"));
    // Nothing was staged: the only entries are the refused target and the publication lock file.
    std::size_t entries = 0;
    std::error_code status;
    for (const auto& entry : fs::directory_iterator{root, status}) {
        const auto name = entry.path().filename().string();
        CHECK(name.find(".tmp.") == std::string::npos);
        if (name != cuexis::tools::publicationLockName) {
            ++entries;
        }
    }
    CHECK(entries == 1U);
}

TEST_CASE("E3 a native filesystem failure refuses the generation and leaves no staging",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-native-failure");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("canonical")}};

    // A regular file where the staging directory must be: the operating system refuses the staging
    // directory, without any injection hook, and nothing is published.
    writeBytes(root / "staging", "not-a-directory");
    const auto blockedStaging = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(blockedStaging.has_value());
    CHECK(errorCode(blockedStaging.error()) == "asset.publish.staging_failed");
    CHECK(readBytes(root / "staging") == asBytes("not-a-directory"));
    CHECK_FALSE(fs::exists(root / "generations"));

    // The same story one level later: the published-generation directory is blocked by a regular
    // file, so the single publishing rename cannot happen. Either the directory creation or the
    // rename reports the failure, the blocking file is untouched and staging is cleaned.
    std::error_code status;
    fs::remove(root / "staging", status);
    fs::create_directories(root / "staging", status);
    writeBytes(root / "generations", "not-a-directory");
    const auto blockedPublish = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(blockedPublish.has_value());
    const auto code = errorCode(blockedPublish.error());
    CHECK((code == "asset.publish.staging_failed" || code == "asset.publish.replace_failed"));
    CHECK(readBytes(root / "generations") == asBytes("not-a-directory"));
    std::size_t staged = 0;
    for (const auto& entry : fs::directory_iterator{root / "staging", status}) {
        static_cast<void>(entry);
        ++staged;
    }
    CHECK(staged == 0U);
}

TEST_CASE("E3 one batch publishes a v4 and a candidate package closure", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;

    const auto published = cuexis::tools::publishPackagePair(request);
    requireOk(published);
    CHECK(published->v4.packageIdentity != published->candidate.packageIdentity);

    const auto v4 = loadPackage(request.v4Target);
    const auto candidate = loadPackage(request.candidateTarget);
    CHECK(v4.identity().hex() == published->v4.packageIdentity);
    CHECK(candidate.identity().hex() == published->candidate.packageIdentity);
    // The v4 closure is the shared project closure; only the candidate adds the packed chart.
    CHECK_FALSE(v4.entryBytes(std::string{candidateEntryPath}).has_value());
    CHECK(candidate.entryBytes(std::string{candidateEntryPath}).has_value());
    CHECK(v4.manifest().canonicalExtensionsJson.find("cuexis.chart-entry.v1") == std::string::npos);
    CHECK(candidate.manifest().canonicalExtensionsJson.find("cuexis.chart-entry.v1") !=
          std::string::npos);
}

TEST_CASE("E3 an invalid candidate closure fails before either package is replaced",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair-validation");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    // The extension declares a playback entry the candidate closure does not contain, so the
    // production loader rejects the candidate package before any target is replaced.
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson =
        R"({"cuexis.chart-entry.v1":{"entries":[{"path":"compiled/missing.packed",)"
        R"("kind":"chart","encoding":"packed-chart","playback":true,)"
        R"("compiledSemanticIdentity":"0000000000000000000000000000000000000000000000000000000000000000",)"
        R"("artifactIdentity":"0000000000000000000000000000000000000000000000000000000000000000",)"
        R"("compilerProfile":"candidate.static-tap-lanes4-v1","expandedEntityCount":1,)"
        R"("expandedRequirementCount":1}]}})";

    const auto published = cuexis::tools::publishPackagePair(request);
    REQUIRE_FALSE(published.has_value());
    CHECK(errorCode(published.error()) == "asset.publish.validation_failed");
    CHECK_FALSE(fs::exists(request.v4Target));
    CHECK_FALSE(fs::exists(request.candidateTarget));
}

TEST_CASE("E3 a failed candidate replacement rolls the v4 package back", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair-rollback");
    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = root / "static.cxc";
    request.candidateTarget = root / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;

    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET",
                             request.candidateTarget.filename().string()};
        const auto published = cuexis::tools::publishPackagePair(request);
        REQUIRE_FALSE(published.has_value());
        CHECK(errorCode(published.error()) == "asset.publish.replace_failed");
    }
    // The pair is transactional: no half-updated v4 package is left visible.
    CHECK_FALSE(fs::exists(request.v4Target));
    CHECK_FALSE(fs::exists(request.candidateTarget));
}

TEST_CASE("E3 a failed candidate replacement restores the previous valid v4 package",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("package-pair-restore");
    const auto fixture = candidateFixture();
    const auto v4Target = root / "static.cxc";
    const auto candidateTarget = root / "static.candidate.cxc";

    // A valid v4 package is already published, and a candidate package from an earlier batch too.
    PackagePublishRequest baseline;
    baseline.target = v4Target;
    baseline.entries = staticProjectEntries();
    const auto previous = cuexis::tools::publishPackage(baseline);
    requireOk(previous);
    const auto previousBytes = readBytes(v4Target);

    PackagePublishRequest earlierCandidate;
    earlierCandidate.target = candidateTarget;
    earlierCandidate.entries = staticProjectEntries();
    earlierCandidate.entries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    earlierCandidate.extensionsJson = fixture.extensionJson;
    requireOk(cuexis::tools::publishPackage(earlierCandidate));
    const auto earlierCandidateBytes = readBytes(candidateTarget);

    // The same batch is published again, but the candidate replacement fails.
    PackagePairRequest request;
    request.v4Target = v4Target;
    request.candidateTarget = candidateTarget;
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_REPLACE_TARGET",
                             candidateTarget.filename().string()};
        const auto published = cuexis::tools::publishPackagePair(request);
        REQUIRE_FALSE(published.has_value());
        CHECK(errorCode(published.error()) == "asset.publish.replace_failed");
    }

    // The last valid pair survives the failure byte for byte, and no temporary sibling remains.
    CHECK(readBytes(v4Target) == previousBytes);
    CHECK(readBytes(candidateTarget) == earlierCandidateBytes);
    CHECK(loadPackage(v4Target).identity().hex() == previous->packageIdentity);
    std::error_code status;
    for (const auto& entry : fs::directory_iterator{root, status}) {
        const auto name = entry.path().filename().string();
        CHECK(name.find(".tmp.") == std::string::npos);
    }
}

// --- R5 publication transaction -----------------------------------------------------------------

TEST_CASE("R5 a replacement failure never leaves the target missing",
          "[s6-e3][asset-publish][r5]") {
    const auto root = scratchRoot("r5-no-missing-window");
    const auto target = root / "static.cxc";

    PackagePublishRequest baseline;
    baseline.target = target;
    baseline.entries = staticProjectEntries();
    const auto previous = cuexis::tools::publishPackage(baseline);
    requireOk(previous);
    const auto previousBytes = readBytes(target);

    // The failure is injected after validation and before the replacement, with the temporary file
    // absent so the replacement cannot succeed. The previous package must still be the package on
    // disk: a two-rename implementation that moved the target aside first would have deleted it by
    // this point and left nothing behind.
    const auto fixture = candidateFixture();
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();
    request.entries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.extensionsJson = fixture.extensionJson;
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_TEMPORARY_TARGET",
                             target.filename().string()};
        const auto failed = cuexis::tools::publishPackage(request);
        REQUIRE_FALSE(failed.has_value());
        CHECK(errorCode(failed.error()) == "asset.publish.replace_failed");
    }
    CHECK(fs::exists(target));
    CHECK(readBytes(target) == previousBytes);
    CHECK(loadPackage(target).identity().hex() == previous->packageIdentity);
}

TEST_CASE("R5 recovery restores a backup instead of deleting it", "[s6-e3][asset-publish][r5]") {
    const auto root = scratchRoot("r5-backup-recovery");

    // A backup left behind by an interrupted two-rename publication is the only surviving copy of
    // the last valid package. Recovery must put it back, not delete it. The name has the shape
    // `uniqueSibling` produced in the discontinued format: `<stem>.cuexis-backup.tmp.<token><ext>`.
    const auto target = root / "static.cxc";
    PackagePublishRequest baseline;
    baseline.target = target;
    baseline.entries = staticProjectEntries();
    const auto previous = cuexis::tools::publishPackage(baseline);
    requireOk(previous);
    const auto packageBytes = readBytes(target);

    const auto backup = root / "static.cuexis-backup.tmp.424242.cxc";
    writeBytes(backup, packageBytes);
    std::error_code status;
    fs::remove(target, status);
    REQUIRE_FALSE(fs::exists(target));

    const auto removed = cuexis::tools::recoverPublicationStaging(root);
    requireOk(removed);
    CHECK(fs::exists(target));
    CHECK(readBytes(target) == packageBytes);
    CHECK_FALSE(fs::exists(backup));
}

TEST_CASE("R5 a write failure leaves the previous package and no temporary file",
          "[s6-e3][asset-publish][r5]") {
    const auto root = scratchRoot("r5-write-failure");
    const auto target = root / "static.cxc";

    PackagePublishRequest baseline;
    baseline.target = target;
    baseline.entries = staticProjectEntries();
    const auto previous = cuexis::tools::publishPackage(baseline);
    requireOk(previous);
    const auto previousBytes = readBytes(target);

    const auto fixture = candidateFixture();
    PackagePublishRequest request;
    request.target = target;
    request.entries = staticProjectEntries();
    request.entries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.extensionsJson = fixture.extensionJson;
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_WRITE_TARGET", target.filename().string()};
        const auto failed = cuexis::tools::publishPackage(request);
        REQUIRE_FALSE(failed.has_value());
        CHECK(errorCode(failed.error()) == "asset.publish.io_failed");
    }
    CHECK(readBytes(target) == previousBytes);
    CHECK(loadPackage(target).identity().hex() == previous->packageIdentity);
    std::error_code status;
    std::size_t leftovers = 0;
    for (const auto& entry : fs::directory_iterator{root, status}) {
        const auto name = entry.path().filename().string();
        if (name.find(".tmp.") != std::string::npos) {
            ++leftovers;
        }
    }
    CHECK(leftovers == 0U);
}

TEST_CASE("R5 the pair lock covers both target parent directories", "[s6-e3][asset-publish][r5]") {
    const auto root = scratchRoot("r5-pair-lock-scope");
    const auto v4Directory = root / "v4";
    const auto candidateDirectory = root / "candidate";
    std::error_code status;
    fs::create_directories(v4Directory, status);
    fs::create_directories(candidateDirectory, status);

    const auto fixture = candidateFixture();
    PackagePairRequest request;
    request.v4Target = v4Directory / "static.cxc";
    request.candidateTarget = candidateDirectory / "static.candidate.cxc";
    request.entries = staticProjectEntries();
    request.candidateEntries.push_back(
        cuexis::cxc::CxcWriteEntry{std::string{candidateEntryPath}, fixture.bytes});
    request.candidateExtensionsJson = fixture.extensionJson;

    // A writer that already holds the candidate-side lock must block the whole pair transaction:
    // the transaction writes that directory, so a v4-only lock would let the two writers race on
    // the candidate target. The v4-side lock alone must block it for the same reason.
    for (const auto& held : {candidateDirectory, v4Directory}) {
        auto blocker =
            cuexis::tools::PublicationLock::acquire(held / cuexis::tools::publicationLockName);
        requireOk(blocker);
        const auto blocked = cuexis::tools::publishPackagePair(request);
        REQUIRE_FALSE(blocked.has_value());
        CHECK(errorCode(blocked.error()) == "asset.publish.busy");
        CHECK_FALSE(fs::exists(request.v4Target));
        CHECK_FALSE(fs::exists(request.candidateTarget));
    }

    // With the lock free again the same batch publishes, so the refusal above was the lock and not
    // an input problem.
    const auto published = cuexis::tools::publishPackagePair(request);
    requireOk(published);
    CHECK(fs::exists(request.v4Target));
    CHECK(fs::exists(request.candidateTarget));
}

TEST_CASE("R5 an idempotent republish reports the real closure size",
          "[s6-e3][asset-publish][r5]") {
    const auto root = scratchRoot("r5-idempotent-closure-bytes");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.28-1";
    const auto bytes = asBytes("canonical");
    request.entries = {PublishEntry{"textures/checker.texture", bytes}};

    const auto first = cuexis::tools::publishGeneration(request);
    requireOk(first);
    CHECK(first->closureBytes == bytes.size());

    // The same batch again is a verified no-op, and it reports the same closure size: a zero here
    // would claim an empty closure for a generation that has content.
    const auto second = cuexis::tools::publishGeneration(request);
    requireOk(second);
    CHECK(second->generationIdentity == first->generationIdentity);
    CHECK(second->closureBytes == first->closureBytes);
    CHECK(second->closureBytes == bytes.size());
}

TEST_CASE("S7A78 generation manifest switches complete immutable output sets",
          "[candidate][asset-publish][s7a78]") {
    const auto root = scratchRoot("s7a78-manifest");
    GenerationPublishRequest first{root,
                                   "gameplay",
                                   {{"graph.json", asBytes("graph-a")},
                                    {"chart.packed", asBytes("packed-a")},
                                    {"closure.json", asBytes("closure-a")}},
                                   {}};
    auto old = cuexis::tools::publishAndAdoptGeneration(first);
    requireOk(old);
    CHECK(*cuexis::tools::readAdoptedGeneration(root) == old->generationIdentity);
    const auto captured = old->generationPath;
    auto second = first;
    second.entries[0].bytes = asBytes("graph-b");
    second.entries[1].bytes = asBytes("packed-b");
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_ADOPT", "1"};
        CHECK_FALSE(cuexis::tools::publishAndAdoptGeneration(second));
    }
    CHECK(*cuexis::tools::readAdoptedGeneration(root) == old->generationIdentity);
    CHECK(readBytes(captured / "graph.json") == asBytes("graph-a"));
    CHECK(readBytes(captured / "chart.packed") == asBytes("packed-a"));
    auto fresh = cuexis::tools::publishAndAdoptGeneration(second);
    requireOk(fresh);
    CHECK(*cuexis::tools::readAdoptedGeneration(root) == fresh->generationIdentity);
    CHECK(readBytes(fresh->generationPath / "graph.json") == asBytes("graph-b"));
    CHECK(readBytes(fresh->generationPath / "chart.packed") == asBytes("packed-b"));
    CHECK(readBytes(captured / "graph.json") == asBytes("graph-a"));
    std::reverse(second.entries.begin(), second.entries.end());
    auto reordered = cuexis::tools::publishAndAdoptGeneration(second);
    requireOk(reordered);
    CHECK(reordered->generationIdentity == fresh->generationIdentity);
    {
        auto held =
            PublicationLock::acquire(root / std::string{cuexis::tools::publicationLockName});
        requireOk(held);
        auto blocked = cuexis::tools::publishAndAdoptGeneration(first);
        REQUIRE_FALSE(blocked);
        CHECK(blocked.error().code() == "asset.publish.busy");
    }
    CHECK(*cuexis::tools::readAdoptedGeneration(root) == fresh->generationIdentity);
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_AFTER_ADOPT", "1"};
        auto ambiguous = cuexis::tools::publishAndAdoptGeneration(first);
        REQUIRE_FALSE(ambiguous);
        CHECK(ambiguous.error().code() == "asset.publish.io_failed");
        CHECK(std::ranges::any_of(ambiguous.error().context(), [](const auto& c) {
            return c.key == "commitVisible" && c.value == "true";
        }));
    }
    CHECK(*cuexis::tools::readAdoptedGeneration(root) == old->generationIdentity);
}
