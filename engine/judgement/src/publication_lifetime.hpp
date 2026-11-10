#pragma once

#include <cuexis/core/thread_checker.hpp>
#include <cuexis/judgement/publication.hpp>

#include <limits>

namespace cuexis::judgement::detail {
inline auto publicationError(std::string_view reason) -> core::Error {
    return core::Error{"judgement.publication.invalid", std::string{reason}};
}
struct PublicationLifetime final {
    core::ThreadChecker owner;
    std::uint64_t epoch{};
    bool mutating{}, reading{};
};
class PublicationMutation final {
  public:
    explicit PublicationMutation(std::shared_ptr<PublicationLifetime> value)
        : lifetime_(std::move(value)) {
        ++lifetime_->epoch;
        lifetime_->reading = false;
        lifetime_->mutating = true;
    }
    PublicationMutation(PublicationMutation&&) noexcept = default;
    ~PublicationMutation() {
        if (lifetime_)
            lifetime_->mutating = false;
    }

  private:
    std::shared_ptr<PublicationLifetime> lifetime_;
};
inline auto beginPublicationMutation(const std::shared_ptr<PublicationLifetime>& lifetime)
    -> core::Result<PublicationMutation> {
    if (!lifetime->owner.isCurrent())
        return core::unexpected(publicationError("wrong owner thread"));
    if (lifetime->mutating)
        return core::unexpected(publicationError("mutation reentry"));
    if (lifetime->epoch == std::numeric_limits<std::uint64_t>::max())
        return core::unexpected(publicationError("read epoch exhausted"));
    return PublicationMutation{lifetime};
}
} // namespace cuexis::judgement::detail
