#include "asset_publish_test_fixture.hpp"

// Atomic reader, generation publication, recovery and lock cases.

#if defined(_WIN32)
TEST_CASE("E3 Windows atomic publication survives a briefly held reader",
          "[s6-e3][asset-publish][windows]") {
    const auto root = scratchRoot("rename-reader-released");
    const auto source = root / "staged";
    const auto target = root / "published";
    const auto payload = asBytes(std::string_view{"prefix\0suffix", 13});
    const auto metadata = asBytes("complete generation metadata");
    writeBytes(source / "nested" / "payload.bin", payload);
    writeBytes(source / "metadata.bin", metadata);
    ScopedNativeHandle reader{CreateFileW((source / "nested" / "payload.bin").c_str(), GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    REQUIRE(reader.valid());
    const bool unsharedRename =
        MoveFileExW(source.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    const auto blockedError = GetLastError();
    REQUIRE_FALSE(unsharedRename);
    REQUIRE(blockedError == ERROR_ACCESS_DENIED);

    bool readerClosed = false;
    std::jthread releaser{[&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
        readerClosed = reader.close();
    }};
    const auto replaced = cuexis::tools::detail::replaceAtomically(source, target);
    releaser.join();
    REQUIRE(readerClosed);
    requireOk(replaced);
    CHECK_FALSE(fs::exists(source));
    CHECK(readBytes(target / "nested" / "payload.bin") == payload);
    CHECK(readBytes(target / "metadata.bin") == metadata);
}

TEST_CASE("E3 Windows atomic publication refuses a reader that remains held",
          "[s6-e3][asset-publish][windows]") {
    const auto root = scratchRoot("rename-reader-held");
    const auto source = root / "staged";
    const auto target = root / "published";
    const auto payload = asBytes(std::string_view{"prefix\0suffix", 13});
    const auto metadata = asBytes("complete generation metadata");
    writeBytes(source / "nested" / "payload.bin", payload);
    writeBytes(source / "metadata.bin", metadata);
    ScopedNativeHandle reader{CreateFileW((source / "nested" / "payload.bin").c_str(), GENERIC_READ,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    REQUIRE(reader.valid());
    const auto replaced = cuexis::tools::detail::replaceAtomically(source, target);
    REQUIRE_FALSE(replaced.has_value());
    CHECK(errorCode(replaced.error()) == "asset.publish.replace_failed");
    CHECK(std::ranges::any_of(replaced.error().context(), [](const auto& item) {
        return item.key == "platform_error" &&
               (item.value == "5" || item.value == "32" || item.value == "33");
    }));
    CHECK(std::ranges::any_of(replaced.error().context(), [&](const auto& item) {
        return item.key == "source" && item.value == source.generic_string();
    }));
    CHECK(std::ranges::any_of(replaced.error().context(), [&](const auto& item) {
        return item.key == "target" && item.value == target.generic_string();
    }));
    CHECK_FALSE(fs::exists(target));
    CHECK(readBytes(source / "nested" / "payload.bin") == payload);
    CHECK(readBytes(source / "metadata.bin") == metadata);
    REQUIRE(reader.close());
    std::error_code status;
    fs::remove_all(source, status);
    CHECK_FALSE(status);
    CHECK_FALSE(fs::exists(source));
}
#endif

TEST_CASE("E3 generation publication writes an immutable content-addressed generation",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-basic");
    const auto texture = asBytes("portable-texture-bytes");
    const auto provenance = asBytes("{\"kind\":\"texture\"}\n");

    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", texture}};
    request.provenanceRecords = {PublishEntry{"texture-checker.provenance.json", provenance}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);
    CHECK(published->generationIdentity.size() == 64U);
    CHECK(published->generationPath == root / "generations" / published->generationIdentity);
    CHECK(published->closureEntries == 1U);
    CHECK(published->provenanceEntries == 1U);
    CHECK(published->closureBytes == texture.size());
    CHECK(readBytes(published->generationPath / "textures" / "checker.texture") == texture);
    CHECK(readBytes(published->generationPath / "provenance" / "texture-checker.provenance.json") ==
          provenance);
    CHECK(fs::is_regular_file(published->generationPath / "cuexis.generation"));
    // The staging directory is gone: the rename was the single visible switch.
    CHECK_FALSE(fs::exists(root / "staging" / published->generationIdentity));
    // The audit sidecar is stored beside the closure, not inside it.
    CHECK(fs::is_directory(published->generationPath / "provenance"));
}

TEST_CASE("E3 generation publication is idempotent and content addressed",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-idempotent");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"audio/track.wav", asBytes("canonical-wav-bytes")}};

    const auto first = cuexis::tools::publishGeneration(request);
    requireOk(first);
    const auto second = cuexis::tools::publishGeneration(request);
    requireOk(second);
    CHECK(second->generationIdentity == first->generationIdentity);
    CHECK(second->generationPath == first->generationPath);

    // Different content gets a different immutable directory; the first one survives untouched.
    request.entries = {PublishEntry{"audio/track.wav", asBytes("different-canonical-wav")}};
    const auto third = cuexis::tools::publishGeneration(request);
    requireOk(third);
    CHECK(third->generationIdentity != first->generationIdentity);
    CHECK(fs::is_regular_file(first->generationPath / "audio" / "track.wav"));
    CHECK(readBytes(first->generationPath / "audio" / "track.wav") ==
          asBytes("canonical-wav-bytes"));

    // The label is part of the recorded identity, so relabelling the same content is a new record.
    request.entries = {PublishEntry{"audio/track.wav", asBytes("canonical-wav-bytes")}};
    request.generationId = "26.09.25-1";
    const auto relabelled = cuexis::tools::publishGeneration(request);
    requireOk(relabelled);
    CHECK(relabelled->generationIdentity != first->generationIdentity);
}

TEST_CASE("E3 generation publication rejects invalid batches without touching disk",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-invalid");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";

    const auto empty = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(empty.has_value());
    CHECK(errorCode(empty.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"../escape.texture", asBytes("bytes")}};
    const auto escape = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(escape.has_value());
    CHECK(errorCode(escape.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"textures/a.texture", asBytes("bytes")},
                       PublishEntry{"textures/a.texture", asBytes("other")}};
    const auto duplicate = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(errorCode(duplicate.error()) == "asset.publish.invalid_request");

    // A case-folded collision would collide on a case-insensitive filesystem.
    request.entries = {PublishEntry{"textures/A.texture", asBytes("bytes")},
                       PublishEntry{"textures/a.texture", asBytes("bytes")}};
    const auto folded = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(folded.has_value());
    CHECK(errorCode(folded.error()) == "asset.publish.invalid_request");

    request.entries = {PublishEntry{"textures/empty.texture", {}}};
    const auto blank = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(blank.has_value());
    CHECK(errorCode(blank.error()) == "asset.publish.invalid_request");

    CHECK_FALSE(fs::exists(root / "generations"));
    CHECK_FALSE(fs::exists(root / "staging"));
}

TEST_CASE("E3 a failed generation publish cleans staging and keeps previous generations",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-failure");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("first")}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);

    request.entries = {PublishEntry{"textures/checker.texture", asBytes("second")}};
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_AFTER_STAGING", "1"};
        const auto staged = cuexis::tools::publishGeneration(request);
        REQUIRE_FALSE(staged.has_value());
        CHECK(errorCode(staged.error()) == "asset.publish.staging_failed");
    }
    {
        const ScopedEnv fail{"CUEXIS_ASSET_PUBLISH_FAIL_BEFORE_PUBLISH", "1"};
        const auto published2 = cuexis::tools::publishGeneration(request);
        REQUIRE_FALSE(published2.has_value());
        CHECK(errorCode(published2.error()) == "asset.publish.replace_failed");
    }

    // No partial generation appeared, no staging remained, and the first generation is intact.
    CHECK(readBytes(published->generationPath / "textures" / "checker.texture") ==
          asBytes("first"));
    std::error_code status;
    std::size_t generations = 0;
    for (const auto& entry : fs::directory_iterator{root / "generations", status}) {
        static_cast<void>(entry);
        ++generations;
    }
    CHECK(generations == 1U);
    std::size_t staged = 0;
    for (const auto& entry : fs::directory_iterator{root / "staging", status}) {
        static_cast<void>(entry);
        ++staged;
    }
    CHECK(staged == 0U);
}

TEST_CASE("E3 a tampered published generation is refused instead of silently rebuilt",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("generation-tampered");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("canonical")}};

    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);
    const auto target = published->generationPath / "textures" / "checker.texture";
    writeBytes(target, "tampered-bytes!");

    const auto republished = cuexis::tools::publishGeneration(request);
    REQUIRE_FALSE(republished.has_value());
    CHECK(errorCode(republished.error()) == "asset.publish.validation_failed");
    // The damaged file is still exactly as it was found: nothing was silently overwritten.
    CHECK(readBytes(target) == asBytes("tampered-bytes!"));
}

TEST_CASE("E3 publication recovery removes abandoned staging and temporary files",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("recovery");
    writeBytes(root / "staging" / "abcdef" / "textures" / "half.texture", "half");
    writeBytes(root / "generations" / "abcdef.cuexis-publish.tmp.7", "temp");
    writeBytes(root / "package.cxc.cuexis-publish.tmp.9", "temp");
    writeBytes(root / "keep.txt", "keep");
    writeBytes(root / "generations" / "0123456789abcdef", "published");

    const auto removed = cuexis::tools::recoverPublicationStaging(root);
    requireOk(removed);
    CHECK(*removed == 3U);
    CHECK_FALSE(fs::exists(root / "staging" / "abcdef"));
    CHECK_FALSE(fs::exists(root / "generations" / "abcdef.cuexis-publish.tmp.7"));
    CHECK_FALSE(fs::exists(root / "package.cxc.cuexis-publish.tmp.9"));
    CHECK(fs::is_regular_file(root / "keep.txt"));
    CHECK(fs::is_regular_file(root / "generations" / "0123456789abcdef"));

    const auto missing = cuexis::tools::recoverPublicationStaging(root / "absent");
    REQUIRE_FALSE(missing.has_value());
    CHECK(errorCode(missing.error()) == "asset.publish.invalid_request");
}

TEST_CASE("E3 adoption is explicit and never rewrites a project entry", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("adoption");
    GenerationPublishRequest request;
    request.root = root;
    request.generationId = "26.09.24-1";
    request.entries = {PublishEntry{"textures/checker.texture", asBytes("canonical")}};
    const auto published = cuexis::tools::publishGeneration(request);
    requireOk(published);

    const auto before = cuexis::tools::readAdoptedGeneration(root);
    REQUIRE_FALSE(before.has_value());
    CHECK(errorCode(before.error()) == "asset.publish.validation_failed");

    const auto adopted = cuexis::tools::adoptGeneration(root, published->generationIdentity);
    requireOk(adopted);
    const auto after = cuexis::tools::readAdoptedGeneration(root);
    requireOk(after);
    CHECK(*after == published->generationIdentity);
    // No project configuration was created or modified by publishing or adopting.
    CHECK_FALSE(fs::exists(root / "cuexis.project.json"));

    const auto unknown = cuexis::tools::adoptGeneration(root, std::string(64U, 'a'));
    REQUIRE_FALSE(unknown.has_value());
    CHECK(errorCode(unknown.error()) == "asset.publish.validation_failed");
}

TEST_CASE("E3 the publication lock rejects a second writer in the same process",
          "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("lock-in-process");
    const auto lockPath = root / ".cuexis.publish.lock";
    {
        const auto first = PublicationLock::acquire(lockPath);
        requireOk(first);
        CHECK(first->path() == lockPath);
        const auto second = PublicationLock::acquire(lockPath);
        REQUIRE_FALSE(second.has_value());
        CHECK(errorCode(second.error()) == "asset.publish.busy");
    }
    // Releasing hands the lock over without deleting the lock file.
    const auto third = PublicationLock::acquire(lockPath);
    requireOk(third);
}

TEST_CASE("E3 the publication lock rejects a writer in another process", "[s6-e3][asset-publish]") {
    const auto root = scratchRoot("lock-cross-process");
    const auto lockPath = root / ".cuexis.publish.lock";
    const auto ready = root / "child.ready";
    const auto release = root / "child.release";

    const ScopedEnv lockEnv{"CUEXIS_ASSET_PUBLISH_LOCK_PATH", lockPath.string()};
    const ScopedEnv readyEnv{"CUEXIS_ASSET_PUBLISH_LOCK_READY", ready.string()};
    const ScopedEnv releaseEnv{"CUEXIS_ASSET_PUBLISH_LOCK_RELEASE", release.string()};

    spawnLockHolder();
    const bool childReady = waitForFile(ready, std::chrono::milliseconds{30000});
    REQUIRE(childReady);
    const auto blocked = PublicationLock::acquire(lockPath);
    REQUIRE_FALSE(blocked.has_value());
    CHECK(errorCode(blocked.error()) == "asset.publish.busy");

    writeBytes(release, "release");
    // The child exits after the release file appears; the lock is free once its handle is closed.
    bool released = false;
    for (int attempt = 0; attempt < 600 && !released; ++attempt) {
        const auto probe = PublicationLock::acquire(lockPath);
        if (probe.has_value()) {
            released = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    CHECK(released);
}

// Hidden helper case: the parent case above starts this binary again with this tag, so the lock is
// genuinely held by a different process.
TEST_CASE("publication lock child holder", "[.publish-lock-child]") {
    const auto path = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_PATH");
    const auto ready = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_READY");
    const auto release = readEnv("CUEXIS_ASSET_PUBLISH_LOCK_RELEASE");
    if (path.empty() || ready.empty() || release.empty()) {
        return;
    }
    const auto lock = PublicationLock::acquire(fs::path{path});
    if (!lock.has_value()) {
        return;
    }
    writeBytes(fs::path{ready}, "ready");
    static_cast<void>(waitForFile(fs::path{release}, std::chrono::milliseconds{20000}));
}
