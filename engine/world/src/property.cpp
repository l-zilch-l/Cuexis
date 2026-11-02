#include <cuexis/world/property.hpp>
#include <cuexis_internal/allocation_observer.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/world/resolver_access.hpp>
#include <cuexis/world/world.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "property_helpers.hpp"

namespace cuexis::world {

using namespace property_detail;

auto owningValue(const PropertyWriteValue& value) -> PropertyValue {
    return std::visit(
        [](const auto& item) -> PropertyValue {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string_view>) {
                return std::string{item};
            } else {
                return item;
            }
        },
        value);
}

auto writeValue(const PropertyValue& value) -> PropertyWriteValue {
    return std::visit(
        [](const auto& item) -> PropertyWriteValue {
            using Value = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, std::string>) {
                return std::string_view{item};
            } else {
                return item;
            }
        },
        value);
}

PropertyWriteBuffer::PropertyWriteBuffer(std::size_t maxWrites) : maxWrites_(maxWrites) {
    writes_.reserve(maxWrites_);
}

auto PropertyWriteBuffer::push(PropertyWrite write) -> core::Result<void> {
    if (write.entity == entt::null) {
        return core::unexpected(
            core::Error{"world.property.null_entity", "Property writes require a valid entity"});
    }
    if (writes_.size() >= maxWrites_) {
        return core::unexpected(core::Error{"world.property.write_limit",
                                            "The per-frame PropertyWrite limit was reached"}
                                    .withContext("limit", std::to_string(maxWrites_)));
    }
    if (const auto* view = std::get_if<std::string_view>(&write.value)) {
        if (ownedStringCount_ < ownedStrings_.size()) {
            ownedStrings_[ownedStringCount_] = *view;
        } else {
            ownedStrings_.emplace_back(*view);
        }
        write.value = std::string_view{ownedStrings_[ownedStringCount_]};
        ++ownedStringCount_;
    }
    writes_.push_back(std::move(write));
    return {};
}

void PropertyWriteBuffer::clear() noexcept {
    writes_.clear();
    ownedStringCount_ = 0;
}

void PropertyWriteBuffer::reserveOwnedStrings(std::size_t count, std::size_t bytes) {
    while (ownedStrings_.size() < count) {
        ownedStrings_.emplace_back();
    }
    for (auto& text : ownedStrings_) {
        if (text.capacity() < bytes) {
            text.reserve(bytes);
        }
    }
}

auto PropertyWriteBuffer::writes() const noexcept -> std::span<const PropertyWrite> {
    return writes_;
}

auto PropertyWriteBuffer::size() const noexcept -> std::size_t {
    return writes_.size();
}

auto PropertyWriteBuffer::maxWrites() const noexcept -> std::size_t {
    return maxWrites_;
}

} // namespace cuexis::world
