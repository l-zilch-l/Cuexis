#include "host_report.hpp"

#include <iomanip>
#include <ostream>

namespace cuexis_reference_host {

HostReport::HostReport(std::ostream& sink) noexcept : sink_{sink} {}

void HostReport::event(std::string_view name, std::string_view fields) {
    ++steps_;
    sink_ << "host." << name;
    if (!fields.empty()) {
        sink_ << ' ' << fields;
    }
    sink_ << '\n';
    sink_.flush();
}

void HostReport::failure(std::string_view step, std::string_view message) {
    ok_ = false;
    sink_ << "host.failure step=" << step << " message=" << message << '\n';
    sink_.flush();
}

void HostReport::rejection(std::string_view step, std::string_view code) {
    sink_ << "host.rejection step=" << step << " code=" << code << '\n';
    sink_.flush();
}

void HostReport::note(std::string_view text) {
    sink_ << "host.note " << text << '\n';
    sink_.flush();
}

void HostReport::summary() {
    sink_ << "host.summary outcome=" << (ok_ ? "ok" : "failed") << " steps=" << steps_ << '\n';
    sink_.flush();
}

auto HostReport::ok() const noexcept -> bool {
    return ok_;
}

auto HostReport::steps() const noexcept -> std::size_t {
    return steps_;
}

auto hexIdentity(const unsigned char* bytes, std::size_t size) -> std::string {
    std::string text;
    text.reserve(size * 2U);
    constexpr char digits[] = "0123456789abcdef";
    for (std::size_t index = 0; index < size; ++index) {
        text.push_back(digits[(bytes[index] >> 4U) & 0x0FU]);
        text.push_back(digits[bytes[index] & 0x0FU]);
    }
    return text;
}

} // namespace cuexis_reference_host
