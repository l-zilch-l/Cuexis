#pragma once
#include "../../../third_party/fnv/fnv1a64.hpp"
#include "execution_kernel.hpp"
#include "recovery_wire_fields.hpp"
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace cuexis::judgement::detail::wire {
struct Failure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Writer;
class Reader;
template <class T> void put(Writer&, const T&);
template <class T> void get(Reader&, T&);
class Writer final {
    struct EncodedSequence final {
        std::shared_ptr<const void> owner;
        std::vector<std::byte> bytes;
        std::size_t length;
        std::uint32_t revision;
    };
    std::unordered_map<const void*, EncodedSequence> sequences_;
    // Optional per-encoding cache. Full caches use ordinary encoding for new owners.
    static constexpr std::size_t maxCachedBytes = 8 * 1024 * 1024;
    static constexpr std::size_t maxCachedSequences = 128;
    std::size_t cachedBytes_{};
    bool measuring_{};
    std::size_t measured_{};
    void measure(std::size_t extra) {
        if (extra > bytes.max_size() - measured_)
            throw Failure{"codec.length_invalid"};
        measured_ += extra;
    }
    void grow(std::size_t extra) {
        if (extra > bytes.max_size() - bytes.size())
            throw Failure{"codec.length_invalid"};
        const auto required = bytes.size() + extra;
        if (required <= bytes.capacity())
            return;
        const auto capacity = bytes.capacity();
        const auto doubled = capacity > bytes.max_size() / 2 ? bytes.max_size() : capacity * 2;
        bytes.reserve(
            std::max(required, std::max(std::min<std::size_t>(4096, bytes.max_size()), doubled)));
    }

  public:
    explicit Writer(bool measuring = false) : measuring_(measuring) {}
    std::vector<std::byte> bytes;
    std::uint32_t revision{2};
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return measuring_ ? measured_ : bytes.size();
    }
    [[nodiscard]] auto measuring() const noexcept -> bool {
        return measuring_;
    }
    template <class... T> void operator()(const T&... v) {
        (put(*this, v), ...);
    }
    void integer(std::uint64_t v, unsigned width) {
        if (measuring_) {
            measure(width);
            return;
        }
        grow(width);
        const auto offset = bytes.size();
        bytes.resize(offset + width);
        for (unsigned i = 0; i < width; ++i) {
            bytes[offset + i] = static_cast<std::byte>((v >> (i * 8)) & 255U);
        }
    }
    void append(std::span<const std::byte> value) {
        if (measuring_) {
            measure(value.size());
            return;
        }
        grow(value.size());
        bytes.insert(bytes.end(), value.begin(), value.end());
    }
    template <class T, std::size_t N> void sequence(const SharedSequence<T, N>& value) {
        auto owner = value.storageOwner();
        if (owner) {
            const auto found = sequences_.find(owner.get());
            if (found != sequences_.end() && found->second.revision == revision) {
                const auto length = found->second.length;
                if (measuring_) {
                    measure(length);
                    return;
                }
                append(found->second.bytes);
                return;
            }
            if (found != sequences_.end()) {
                cachedBytes_ -= found->second.length;
                sequences_.erase(found);
            }
        }
        const auto offset = size();
        integer(static_cast<std::uint64_t>(value.size()), 8);
        for (const auto& element : value)
            (*this)(element);
        if (owner) {
            const auto length = size() - offset;
            if (length > maxCachedBytes - cachedBytes_ || sequences_.size() >= maxCachedSequences)
                return;
            const auto key = owner.get();
            std::vector<std::byte> encoded;
            if (!measuring_) {
                encoded.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
            }
            sequences_.insert_or_assign(
                key, EncodedSequence{std::move(owner), std::move(encoded), length, revision});
            cachedBytes_ += length;
        }
    }
};
class Reader final {
  public:
    std::span<const std::byte> bytes;
    std::size_t pos{0};
    std::uint64_t elements{0};
    std::optional<CodecBudget> budget;
    KernelProjection initial;
    std::uint32_t revision{2};
    template <class... T> void operator()(T&... v) {
        (get(*this, v), ...);
    }
    auto integer(unsigned width) -> std::uint64_t {
        if (width > bytes.size() - pos) {
            throw Failure{"codec.truncated"};
        }
        std::uint64_t v = 0;
        for (unsigned i = 0; i < width; ++i) {
            v |= static_cast<std::uint64_t>(std::to_integer<unsigned>(bytes[pos++])) << (i * 8);
        }
        return v;
    }
    auto count() -> std::size_t {
        const auto n = integer(8);
        if (n > bytes.size() - pos || n > SIZE_MAX || n > UINT64_MAX - elements) {
            throw Failure{"codec.length_invalid"};
        }
        elements += n;
        if (budget && elements > budget->maxElements) {
            throw Failure{"codec.budget_exceeded"};
        }
        return static_cast<std::size_t>(n);
    }
    template <class T> auto seed() -> T {
        if constexpr (std::is_same_v<T, KernelProjection>) {
            return initial;
        } else if constexpr (std::is_same_v<T, core::Error>) {
            return core::Error{"", ""};
        } else if constexpr (std::is_same_v<T, SnapshotDTO>) {
            return SnapshotDTO{initial, {}, {}, {}, 0, 0, false, {}, {}};
        } else {
            return T{};
        }
    }
};
template <class T> struct Vector : std::false_type {};
template <class T> struct PagedSequence : std::false_type {};
template <class T, std::size_t N> struct PagedSequence<SharedSequence<T, N>> : std::true_type {};
template <class T, class A> struct Vector<std::vector<T, A>> : std::true_type {
    using Value = T;
};
template <class T, std::size_t N> struct Vector<SharedSequence<T, N>> : std::true_type {
    using Value = T;
};
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {
    using Value = T;
};
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct Shared : std::false_type {};
template <class T> struct Shared<std::shared_ptr<T>> : std::true_type {
    using Value = std::remove_const_t<T>;
};
template <class T> constexpr auto maxEnum() -> unsigned {
    if constexpr (std::is_same_v<T, RpaControlKind>) {
        return static_cast<unsigned>(RpaControlKind::resume);
    } else if constexpr (std::is_same_v<T, InputAction>) {
        return static_cast<unsigned>(InputAction::update);
    } else if constexpr (std::is_same_v<T, PhaseKind>) {
        return static_cast<unsigned>(PhaseKind::tail);
    } else if constexpr (std::is_same_v<T, Outcome>) {
        return static_cast<unsigned>(Outcome::miss);
    } else if constexpr (std::is_same_v<T, FactCategory>) {
        return static_cast<unsigned>(FactCategory::holdTail);
    } else if constexpr (std::is_same_v<T, ResourceClaimIntent>) {
        return static_cast<unsigned>(ResourceClaimIntent::claim);
    } else if constexpr (std::is_same_v<T, FaultStage>) {
        return static_cast<unsigned>(FaultStage::control);
    } else if constexpr (std::is_same_v<T, KernelSessionState>) {
        return static_cast<unsigned>(KernelSessionState::Faulted);
    } else if constexpr (std::is_same_v<T, KernelPhaseState>) {
        return static_cast<unsigned>(KernelPhaseState::Expired);
    } else if constexpr (std::is_same_v<T, FactKind>) {
        return static_cast<unsigned>(FactKind::consumeEmpty);
    } else {
        static_assert(!sizeof(T), "wire enum not registered");
    }
}
template <class T> void put(Writer& a, const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        a.integer(v ? 1 : 0, 1);
    } else if constexpr (std::is_same_v<T, std::byte>) {
        a.integer(std::to_integer<unsigned>(v), 1);
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            a.integer(std::bit_cast<std::make_unsigned_t<T>>(v), sizeof(T));
        } else {
            a.integer(v, sizeof(T));
        }
    } else if constexpr (std::is_enum_v<T>) {
        a.integer(static_cast<unsigned>(v), 1);
    } else if constexpr (std::is_same_v<T, std::string>) {
        if (!validUtf8(v)) {
            throw Failure{"codec.utf8_invalid"};
        }
        a.integer(static_cast<std::uint64_t>(v.size()), 8);
        a.append(std::as_bytes(std::span{v.data(), v.size()}));
    } else if constexpr (std::is_same_v<T, Tick> || std::is_same_v<T, TickDelta> ||
                         std::is_same_v<T, IngressSequence>) {
        a(v.value());
    } else if constexpr (std::is_same_v<T, ObservationTick>) {
        a(v.tick());
    } else if constexpr (PagedSequence<T>::value) {
        a.sequence(v);
    } else if constexpr (Vector<T>::value) {
        a.integer(static_cast<std::uint64_t>(v.size()), 8);
        if constexpr (std::is_same_v<typename Vector<T>::Value, std::byte>) {
            a.append(v);
        } else {
            for (const auto& x : v)
                a(x);
        }
    } else if constexpr (Optional<T>::value || Shared<T>::value) {
        a(static_cast<bool>(v));
        if (v) {
            a(*v);
        }
    } else if constexpr (Variant<T>::value) {
        a.integer(v.index(), 1);
        std::visit([&](const auto& x) { a(x); }, v);
    } else if constexpr (std::is_same_v<T, core::Error>) {
        const auto* current = &v;
        while (current) {
            a(std::string{current->code()}, std::string{current->message()});
            a.integer(static_cast<std::uint64_t>(current->context().size()), 8);
            for (const auto& c : current->context()) {
                a(c.key, c.value);
            }
            a(current->cause() != nullptr);
            current = current->cause();
        }
    } else {
        fields(a, v);
    }
}
template <std::size_t I = 0, class T> void getVariant(Reader& a, T& v, unsigned tag) {
    if constexpr (I < std::variant_size_v<T>) {
        if (tag == I) {
            auto x = a.seed<std::variant_alternative_t<I, T>>();
            a(x);
            v = std::move(x);
        } else {
            getVariant<I + 1>(a, v, tag);
        }
    } else {
        throw Failure{"codec.tag_invalid"};
    }
}
template <class T> void get(Reader& a, T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        const auto b = a.integer(1);
        if (b > 1) {
            throw Failure{"codec.tag_invalid"};
        }
        v = b != 0;
    } else if constexpr (std::is_same_v<T, std::byte>) {
        v = static_cast<std::byte>(a.integer(1));
    } else if constexpr (std::is_integral_v<T>) {
        auto bits = static_cast<std::make_unsigned_t<T>>(a.integer(sizeof(T)));
        if constexpr (std::is_signed_v<T>) {
            v = std::bit_cast<T>(bits);
        } else {
            v = bits;
        }
    } else if constexpr (std::is_enum_v<T>) {
        const auto tag = a.integer(1);
        if (tag > maxEnum<T>()) {
            throw Failure{"codec.tag_invalid"};
        }
        v = static_cast<T>(tag);
    } else if constexpr (std::is_same_v<T, std::string>) {
        const auto n = a.count();
        v.clear();
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            v.push_back(static_cast<char>(a.integer(1)));
        }
        if (!validUtf8(v)) {
            throw Failure{"codec.utf8_invalid"};
        }
    } else if constexpr (std::is_same_v<T, Tick> || std::is_same_v<T, TickDelta>) {
        std::int64_t x;
        a(x);
        v = T{x};
    } else if constexpr (std::is_same_v<T, IngressSequence>) {
        std::uint64_t x;
        a(x);
        v = T{x};
    } else if constexpr (std::is_same_v<T, ObservationTick>) {
        Tick x;
        a(x);
        v = ObservationTick{x};
    } else if constexpr (Vector<T>::value) {
        const auto n = a.count();
        v.clear();
        if (n > v.max_size()) {
            throw Failure{"codec.length_invalid"};
        }
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            auto x = a.seed<typename Vector<T>::Value>();
            a(x);
            v.push_back(std::move(x));
        }
    } else if constexpr (Optional<T>::value) {
        bool present;
        a(present);
        v.reset();
        if (present) {
            auto x = a.seed<typename Optional<T>::Value>();
            a(x);
            v = std::move(x);
        }
    } else if constexpr (Shared<T>::value) {
        bool present;
        a(present);
        v.reset();
        if (present) {
            auto x = a.seed<typename Shared<T>::Value>();
            a(x);
            v = std::make_shared<const typename Shared<T>::Value>(std::move(x));
        }
    } else if constexpr (Variant<T>::value) {
        const auto tag = static_cast<unsigned>(a.integer(1));
        if constexpr (std::is_same_v<T, OriginId>) {
            if (a.revision < 3 && tag >= 3)
                throw Failure{"codec.tag_invalid"};
        }
        getVariant(a, v, tag);
    } else if constexpr (std::is_same_v<T, core::Error>) {
        std::vector<core::Error> chain;
        bool cause = true;
        while (cause) {
            std::string code, message;
            a(code, message);
            core::Error error{std::move(code), std::move(message)};
            const auto n = a.count();
            for (std::size_t i = 0; i < n; ++i) {
                std::string k, value;
                a(k, value);
                error.withContext(std::move(k), std::move(value));
            }
            a(cause);
            chain.push_back(std::move(error));
        }
        v = std::move(chain.back());
        chain.pop_back();
        while (!chain.empty()) {
            auto parent = std::move(chain.back());
            chain.pop_back();
            parent.withCause(std::move(v));
            v = std::move(parent);
        }
    } else {
        fields(a, v);
    }
}
inline auto digest(std::span<const std::byte> bytes) noexcept -> std::uint64_t {
    return cuexis::third_party::fnv1a64(bytes);
}
} // namespace cuexis::judgement::detail::wire
