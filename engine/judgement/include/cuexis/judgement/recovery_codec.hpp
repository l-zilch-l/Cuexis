#pragma once

#include <cuexis/judgement/judgement_session.hpp>

namespace cuexis::judgement {
[[nodiscard]] auto encodeReplay(const ReplayArchive&) -> core::Result<std::vector<std::byte>>;
[[nodiscard]] auto decodeReplay(std::span<const std::byte>, const RecoveryInputs&,
                                std::optional<CodecBudget> = {}) -> core::Result<ReplayArchive>;
[[nodiscard]] auto encodeSnapshot(const SnapshotPayload&) -> core::Result<std::vector<std::byte>>;
[[nodiscard]] auto decodeSnapshot(std::span<const std::byte>, const RecoveryInputs&,
                                  std::optional<CodecBudget> = {}) -> core::Result<SnapshotPayload>;
} // namespace cuexis::judgement
