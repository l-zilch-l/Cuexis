// Included inside the isolated Capsule implementation's anonymous namespace.
void admissionAuditImpl(const EncodeRequest& request) {
    const chart::PackedChartLimits limits;
    auto model = makeModel(request, limits);
    model.bind();
    const auto n = model.graph.requirements.size();
    std::uint64_t occupying{};
    for (const auto& claim : model.claims)
        occupying += claim.declaration.intent != ResourceClaimIntent::observe;
    require(model.claims.size() <= n, "audit.claims", "fanout exceeds current subset");
    const auto derived = admission_audit::add(admission_audit::multiply(n, 2), occupying);

    // Build existing bounded static sections with only the proposed physical dictionary.
    // No new Header, complete revision-4 artifact, Reader or publication is produced.
    std::set<std::string> proposedStrings, legacyStrings;
    std::set<Ref> proposedRefs, legacyRefs;
    Output collect;
    collect.mode = Mode::collect;
    collect.admissionProposed = true;
    collect.collected = &proposedStrings;
    collect.collectedRefs = &proposedRefs;
    allRows(collect, model);
    Output oldCollect;
    oldCollect.mode = Mode::collect;
    oldCollect.collected = &legacyStrings;
    oldCollect.collectedRefs = &legacyRefs;
    allRows(oldCollect, model);
    const std::vector<std::string> extraStrings{proposedStrings.begin(), proposedStrings.end()};
    const std::vector<Ref> extraRefs{proposedRefs.begin(), proposedRefs.end()};
    auto tables =
        checked(gp::encodeStatic(model.chart, model.owners, extraStrings, extraRefs, limits));
    counts(model, tables.references.size());
    legacyStrings.insert(tables.strings.begin(), tables.strings.end());
    legacyRefs.insert(tables.references.begin(), tables.references.end());
    const auto dictionary = [](std::string_view label, const auto& strings, const auto& refs) {
        std::uint64_t utf8Bytes{};
        for (const auto& text : strings)
            utf8Bytes = admission_audit::add(utf8Bytes, text.size());
        const auto section = admission_audit::add(
            admission_audit::add(12, admission_audit::multiply(4, strings.size())), utf8Bytes);
        std::cout << "ADMISSION_DICT {\"layout\":\"" << label << "\",\"strings\":" << strings.size()
                  << ",\"references\":" << refs.size() << ",\"utf8_bytes\":" << utf8Bytes
                  << ",\"str0_bytes\":" << section << "}\n";
    };
    dictionary("legacy3", legacyStrings, legacyRefs);
    dictionary("proposed4", tables.strings, tables.references);

    struct Sizing {
        std::string code;
        std::uint64_t bytes, records;
        bool serialized;
    };
    std::vector<Sizing> sections;
    for (const auto& section : tables.sections)
        sections.push_back({std::string{section.code.begin(), section.code.end()},
                            section.bytes.size(), section.records, true});
    const auto gpSection = [&](std::string code, std::uint64_t records, auto visitor) {
        Output size;
        size.mode = Mode::wire;
        size.sizing = true;
        size.admissionProposed = true;
        size.strings = &tables.strings;
        size.references = &tables.references;
        visitor(size, model);
        bool serialized = false;
        if (n <= 64) {
            Output bytes;
            bytes.mode = Mode::wire;
            bytes.admissionProposed = true;
            bytes.limit = limits.maxPackedSectionBytes;
            bytes.strings = &tables.strings;
            bytes.references = &tables.references;
            visitor(bytes, model);
            require(bytes.writer.bytes().size() == size.size, "audit.sizing",
                    "payload sizing mismatch");
            serialized = true;
        }
        sections.push_back(
            {std::move(code), admission_audit::add(16, size.size), records, serialized});
    };
    gpSection("GPH0", 1, [](auto& out, auto& m) { globalRow(out, m); });
    gpSection("GPR0", n, [](auto& out, auto& m) {
        for (std::size_t i = 0; i < m.graph.requirements.size(); ++i)
            requirementRow(out, m.graph.requirements[i], m.indices[i], m);
    });
    gpSection("GPD0", sum(std::span{model.counts}.subspan(1, 4)),
              [](auto& out, auto& m) { definitions(out, m); });
    gpSection("GRC0", sum(std::span{model.counts}.subspan(5, 5)),
              [](auto& out, auto& m) { coordination(out, m); });
    std::uint64_t decoded{}, maximumSection{};
    for (const auto& section : sections) {
        decoded = admission_audit::add(decoded, section.bytes);
        maximumSection = std::max(maximumSection, section.bytes);
        std::cout << "ADMISSION_SECTION {\"code\":\"" << section.code
                  << "\",\"bytes\":" << section.bytes << ",\"records\":" << section.records
                  << ",\"serialized_validation\":" << (section.serialized ? "true" : "false")
                  << "}\n";
    }
    const auto directory = admission_audit::multiply(32, sections.size());
    const auto total = admission_audit::add(admission_audit::add(96, directory), decoded);

    // Hash counting uses exactly the unmodified revision-3 semantic visitor and byte order.
    // It measures demand; no production budget is changed and no oversized artifact is emitted.
    auto prefix = checked(gp::staticPreimage(model.chart, model.owners, 3));
    Output hash;
    hash.mode = Mode::hash;
    hash.sizing = true;
    admission_audit::Counters counters;
    core::detail::Sha256 digest;
    hash.admissionCounters = &counters;
    hash.admissionDigest = &digest;
    std::map<std::string, std::uint64_t> stages;
    const auto stage = [&](std::string label, auto visitor) {
        const auto start = hash.size;
        visitor();
        stages.emplace(std::move(label), hash.size - start);
    };
    stage("static_prefix", [&] { hash.raw(prefix); });
    stage("global", [&] { globalRow(hash, model); });
    stage("requirements", [&] {
        hash.count(n);
        for (std::size_t i = 0; i < n; ++i)
            requirementRow(hash, model.graph.requirements[i], model.indices[i], model);
    });
    stage("definitions", [&] { definitions(hash, model); });
    stage("coordination", [&] { coordination(hash, model); });
    const auto digestBytes = digest.finish();
    const auto semanticDigest = core::detail::sha256Hex(digestBytes);
    for (const auto& [name, bytes] : stages)
        std::cout << "ADMISSION_HASH_STAGE {\"stage\":\"" << name << "\",\"bytes\":" << bytes
                  << "}\n";
    for (const auto& [name, expansion] : counters.expansions)
        std::cout << "ADMISSION_EXPANSION {\"table\":\"" << name
                  << "\",\"calls\":" << expansion.calls << ",\"bytes\":" << expansion.bytes
                  << ",\"maximum\":" << expansion.maximum << "}\n";
    const bool physicalFit = tables.strings.size() <= limits.maxPackedStrings &&
                             tables.references.size() <= limits.maxPackedReferences &&
                             decoded <= limits.maxPackedDecodedBytes &&
                             total <= limits.maxPackedFileBytes &&
                             maximumSection <= limits.maxPackedSectionBytes;
    std::cout << "ADMISSION_COUNTS {\"entities\":" << model.entities.size()
              << ",\"requirements\":" << n << ",\"claims\":" << model.claims.size()
              << ",\"occupying_claims\":" << occupying
              << ",\"logical_derived_projections\":" << derived
              << ",\"header_bytes\":96,\"directory_bytes\":" << directory
              << ",\"decoded_bytes\":" << decoded << ",\"proposed_file_bytes\":" << total
              << ",\"proposed_physical_fit\":" << (physicalFit ? "true" : "false")
              << ",\"semantic_preimage_bytes\":" << hash.size << ",\"semantic_digest\":\""
              << semanticDigest << "\",\"projection_visits\":" << counters.projectionCalls
              << ",\"projection_text_bytes\":" << counters.projectionBytes
              << ",\"maximum_projection_bytes\":" << counters.maximumProjection
              << ",\"prefix_size\":" << prefix.size()
              << ",\"prefix_capacity\":" << prefix.capacity()
              << ",\"hash_output_buffer_bytes\":" << hash.writer.bytes().size()
              << ",\"proposed_wire_accepted\":false,\"performance_acceptance\":false}\n";

    auto original = encode(request);
    if (n <= 64) {
        require(original.has_value(), "audit.legacy", "small original Writer failed");
        const auto originalPreimage = checked(semanticPreimage(request));
        require(originalPreimage.size() == hash.size &&
                    core::detail::sha256Hex(originalPreimage) == semanticDigest,
                "audit.hash", "counter/digest differs from original semantic export");
        require(std::equal(digestBytes.begin(), digestBytes.end(), original->begin() + 32,
                           [](auto l, auto r) { return l == std::to_integer<std::uint8_t>(r); }),
                "audit.header", "original Header digest mismatch");
        // Existing complete Writer output remains the independent small-input sizing oracle.
        auto old = staticTables(model, limits);
        auto oldBytes = envelope(model, std::move(old), limits);
        require(oldBytes == *original, "audit.legacy", "existing canonical bytes differ");
    } else {
        require(!original && original.error().code() == "packed.budget.strings", "audit.legacy",
                "original rejection changed");
    }
    std::cout << "ADMISSION_RESULT {\"legacy_wire_accepted\":" << (original ? "true" : "false")
              << ",\"legacy_code\":\"" << (original ? "" : std::string{original.error().code()})
              << "\",\"legacy_file_bytes\":" << (original ? original->size() : 0)
              << ",\"small_sizing_and_hash_verified\":" << (n <= 64 ? "true" : "false")
              << ",\"host_reached\":false,\"proposed_wire_accepted\":false}\n";
}
