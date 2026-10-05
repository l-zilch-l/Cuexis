#pragma once

#include <cuexis/chart/cxt_v2_loader.hpp>
#include <cuexis/gameplay_packed/gameplay_capsule.hpp>
#include <cuexis/json/gameplay_author.hpp>

namespace cuexis::tools::gameplay_author {
struct ChartContext final {
    std::string entryId;
    chart::CanonicalSemanticChart foundation;
};
struct AuthorCompileContext final {
    ChartContext chart;
    std::optional<chart::CxtV2Invocation> invocation;
    std::string compilerProfileToken;
    judgement::CompileCapabilityContext capabilities;
    judgement::ContentProfileLimits contentLimits;
    judgement::PatternCompileBudget patternBudget;
    judgement::ClosureContributions closureContributions;
    judgement::PreparedIdentityDeclarations identityDeclarations;
    chart::ChartLimits foundationLimits;
};
struct AuthorPreparedArtifact final {
    chart::CanonicalSemanticChart chart;
    judgement::PreparedGameplay gameplay;
    std::vector<gameplay_packed::RequirementOwner> owners;
    gameplay_packed::CapsuleProfiles profiles;
    std::vector<judgement::PatternDeclaration> additionalPatterns;
    std::vector<gameplay_packed::MeasureDefinition> additionalMeasures;
    std::string originalSource;
};
[[nodiscard]] auto compile(std::string_view source, json::gameplay::SourceKind,
                           const AuthorCompileContext&) -> core::Result<AuthorPreparedArtifact>;
[[nodiscard]] auto read(const std::filesystem::path&, json::gameplay::SourceKind,
                        const AuthorCompileContext&) -> core::Result<AuthorPreparedArtifact>;
[[nodiscard]] auto compileInto(std::optional<AuthorPreparedArtifact>&, std::string_view,
                               json::gameplay::SourceKind, const AuthorCompileContext&)
    -> core::Result<void>;
} // namespace cuexis::tools::gameplay_author
