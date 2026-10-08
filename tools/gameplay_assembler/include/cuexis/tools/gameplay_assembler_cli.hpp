#pragma once
namespace cuexis::tools {
// One developer-tool dispatch, shared by the existing CLI and the compatibility executable.
[[nodiscard]] auto runGameplayAssembler(int argc, char** argv) -> int;
} // namespace cuexis::tools
