#pragma once
#include "host_runner.hpp"
namespace cuexis_reference_host {
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
[[nodiscard]] auto runRealtimeHost(const HostOptions&, HostReport&) -> int;
#endif
} // namespace cuexis_reference_host
