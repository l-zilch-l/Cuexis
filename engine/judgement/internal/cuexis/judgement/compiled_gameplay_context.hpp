#pragma once

//  Judgement compile side - the shared capability / content-profile context.
//
//  Build-only internal root. This header is the single owner of the two context types the author
//  compiler and the prepared-artifact Reader share; the old author headers re-include it so there
//  is exactly one definition of each type. The identity declarations stay in the running value
//  header and are never defined here.
//
//  The measured-parameter wrapper is the one the author timebase declares: the running mirror in
//  `prepared_gameplay_records.hpp` is the runtime-side copy, and this header deliberately keeps the
//  compile-side type identity so an author `ContentProfileLimits` does not silently change type
//  when the context is extracted. The running value header remains the authority for the mirror.

#include <cuexis/judgement/prepared_identity.hpp>
#include <cuexis/judgement/timebase_profile.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cuexis::judgement {

//  The capability context of one assembly: which capability ids the compiling toolchain
//  recognises, and which of those this compile enabled. No registry is invented here; a derived
//  capability that the context does not recognise is unknown, one it recognises but does not
//  enable is disabled, and neither is silently added to the closure.
struct CompileCapabilityContext final {
    std::vector<std::string> recognisedCapabilityIds;
    std::vector<std::string> enabledCapabilityIds;
};

//  The accepted upper bounds of the content profile. Every bound is a measured-or-pending
//  parameter: a bound no measurement accepted stays pending and is reported as not enforced
//  instead of being spelled as zero, because zero is a literal upper bound.
struct ContentProfileLimits final {
    MeasuredParameter<std::uint64_t> maxRequirements;
    MeasuredParameter<std::uint64_t> maxResources;
    MeasuredParameter<std::uint64_t> maxExclusiveRelations;
    MeasuredParameter<std::uint64_t> maxRelationMembers;
    MeasuredParameter<std::uint64_t> maxMergedDeclarations;
    MeasuredParameter<std::uint64_t> maxPatternNodes;
    MeasuredParameter<std::uint64_t> maxMeasureComponents;
    MeasuredParameter<std::uint64_t> maxFactBindings;
    MeasuredParameter<std::uint64_t> maxDerivedCapabilities;
    MeasuredParameter<std::uint64_t> maxDiagnosticMapEntries;
};

} // namespace cuexis::judgement
