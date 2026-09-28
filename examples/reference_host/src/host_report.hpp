#pragma once

// Host-owned run record. Every step of the reference host appends one canonical
// line so an operator or an automated gate can inspect the run without a GPU,
// a window or a debugger.

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>

namespace cuexis_reference_host {

class HostReport final {
  public:
    explicit HostReport(std::ostream& sink) noexcept;

    // Appends "host.<name> <fields>". Fields are host-authored key=value text.
    void event(std::string_view name, std::string_view fields);

    // Records a failed host self-check and marks the run as failed.
    void failure(std::string_view step, std::string_view message);

    // Records a step that was expected to fail and did.
    void rejection(std::string_view step, std::string_view code);

    // Records a host diagnostic that ends the run. Unlike rejection(), this
    // marks the run as failed: a refusal the host did not plan for is an
    // outcome, never a success.
    void diagnostic(std::string_view step, std::string_view code, std::string_view detail);

    void note(std::string_view text);

    void summary();

    [[nodiscard]] auto ok() const noexcept -> bool;

  private:
    std::ostream& sink_;
    std::size_t steps_{0};
    bool ok_{true};
};

[[nodiscard]] auto hexIdentity(const unsigned char* bytes, std::size_t size) -> std::string;

} // namespace cuexis_reference_host
