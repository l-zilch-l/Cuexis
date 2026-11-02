#pragma once

// The packing adapters: the wire/hash writer, its numeric workspace and the reader.

#include "gameplay_capsule_common_internal.hpp"

namespace cuexis::gameplay_packed {
namespace capsule_detail {

#include "hash_core_work_internal.hpp"

struct Output final {
    static constexpr bool reading = false;
    Mode mode{Mode::wire};
    std::optional<ByteWriter> writer;
    gp::SemanticByteSink* sink{};
    detail::HashCost* cost{};
    HashCoreWork* operation{};
    bool sizing{};
    std::size_t size{};
    std::size_t limit{std::numeric_limits<std::size_t>::max()};
    std::set<std::string>* collected{};
    std::set<Ref>* collectedRefs{};
    const std::vector<std::string>* strings{};
    const std::vector<Ref>* references{};
    auto bufferedWriter() -> ByteWriter& {
        if (!writer)
            writer.emplace();
        return *writer;
    }
    void add(std::size_t n) {
        if (operation && cost) {
            cost->bytes =
                operation->counting
                    ? detail::cappedAdd(cost->bytes, n, operation->limits.maxSemanticPreimageBytes)
                    : cost->bytes + n;
            size = static_cast<std::size_t>(cost->bytes);
            return;
        }
        if (n > limit - size) {
            fail("packed.budget.decoded_bytes", "expanded payload exceeds budget");
        }
        size += n;
    }
    void raw(std::span<const std::byte> bytes) {
        if (mode == Mode::collect) {
            return;
        }
        add(bytes.size());
        if (!sizing && (!operation || !operation->counting)) {
            if (sink)
                sink->write(sink->context, bytes);
            else
                bufferedWriter().writeBytes(bytes);
        }
    }
    void tag(std::uint8_t v) {
        if (mode == Mode::collect) {
            return;
        }
        const auto byte = static_cast<std::byte>(v);
        raw(std::span{&byte, 1});
    }
    void unsignedValue(std::uint64_t v, bool wide) {
        if (mode == Mode::collect) {
            return;
        }
        if (mode == Mode::hash) {
            std::array<std::byte, 8> bytes{};
            const auto width = wide ? 8U : 4U;
            for (std::size_t i = 0; i < width; ++i)
                bytes[i] = static_cast<std::byte>((v >> (8U * i)) & 255U);
            raw(std::span{bytes}.first(width));
        } else {
            auto copy = v;
            std::size_t n = 1;
            while (copy >= 128) {
                copy >>= 7;
                ++n;
            }
            add(n);
            if (!sizing) {
                bufferedWriter().writeUnsignedLeb128(v);
            }
        }
    }
    void value(const std::uint32_t& v) {
        unsignedValue(v, false);
    }
    void value(const std::uint64_t& v) {
        unsignedValue(v, true);
    }
    void value(const std::int64_t& v) {
        if (mode == Mode::hash) {
            unsignedValue(std::bit_cast<std::uint64_t>(v), true);
        } else {
            unsignedValue((std::bit_cast<std::uint64_t>(v) << 1U) ^ (v < 0 ? UINT64_MAX : 0), true);
        }
    }
    void value(const bool& v) {
        tag(v ? 1 : 0);
    }
    void text(std::string_view v) {
        if (!operation && !utf8(v)) {
            fail("packed.strings.bounds", "token is not valid UTF-8");
        }
        if (mode == Mode::collect) {
            collected->emplace(v);
            return;
        }
        if (mode == Mode::hash) {
            unsignedValue(u32(v.size()), false);
            raw(std::as_bytes(std::span{v.data(), v.size()}));
        } else {
            const auto it = std::lower_bound(strings->begin(), strings->end(), v,
                                             [](const auto& l, auto r) { return textLess(l, r); });
            require(it != strings->end() && *it == v, "STR0", "token missing from dictionary");
            unsignedValue(u32(static_cast<std::size_t>(it - strings->begin())), false);
        }
    }
    void value(const std::string& v) {
        text(v);
    }
    void value(std::string_view v) {
        text(v);
    }
    void ref(std::uint8_t kind, std::string_view v) {
        if (mode == Mode::collect) {
            text(v);
            collectedRefs->emplace(kind, v);
            return;
        }
        if (mode == Mode::hash) {
            tag(kind);
            text(v);
            return;
        }
        Ref key{kind, std::string{v}};
        const auto it = std::lower_bound(references->begin(), references->end(), key);
        require(it != references->end() && *it == key, "REF0", "reference missing from dictionary");
        unsignedValue(u32(static_cast<std::size_t>(it - references->begin())), false);
    }
    void count(std::size_t n) {
        unsignedValue(u32(n), false);
    }
    template <typename T> void value(T& v);
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void list(const std::vector<T>& v) {
        count(v.size());
        for (auto& row : v) {
            value(row);
        }
    }
    template <typename T> void option(const std::optional<T>& v) {
        tag(v ? 1 : 0);
        if (v) {
            value(*v);
        }
    }
};
inline void HashCoreWork::combine(Output& out, const detail::HashCost& value) const {
    require(out.cost != nullptr, "semanticHash.cost", "Missing cost accumulator");
    add(*out.cost, value);
    out.size = static_cast<std::size_t>(out.cost->bytes);
}
inline void HashCoreWork::visitEdge(Output& out) const {
    require(out.cost != nullptr, "semanticHash.cost", "Missing cost accumulator");
    out.cost->visits =
        counting ? detail::cappedAdd(out.cost->visits, 1, limits.maxExpandedReferenceVisits)
                 : out.cost->visits + 1;
}
template <typename Emit>
auto HashCoreWork::resolve(CostGroup groupId, std::size_t index, Emit emit) -> detail::HashCost {
    auto slots = group(groupId);
    require(index < slots.size(), "semanticHash.cache", "Cache index is out of range");
    auto& slot = slots[index];
    if (slot.flags == 1) {
        ++observation.cacheHits;
        return slot.cost;
    }
    require(slot.flags == 0, "semanticHash.cache", "Cyclic semantic cost dependency");
    slot.flags = 2;
    detail::HashCost cost;
    Output child;
    child.mode = Mode::hash;
    child.operation = this;
    child.cost = &cost;
    emit(child);
    slot.cost = cost;
    slot.flags = 1;
    ++observation.uniqueRows;
    return cost;
}

struct Input final {
    static constexpr bool reading = true;
    ByteReader reader;
    const std::vector<std::string>& strings;
    const std::vector<Ref>& references;
    std::string path;
    auto tag() -> std::uint8_t {
        return checked(reader.readU8());
    }
    void value(std::uint32_t& v) {
        v = static_cast<std::uint32_t>(checked(reader.readUnsignedLeb128(UINT32_MAX)));
    }
    void value(std::uint64_t& v) {
        v = checked(reader.readUnsignedLeb128());
    }
    void value(std::int64_t& v) {
        v = checked(reader.readSignedZigZag());
    }
    void value(bool& v) {
        const auto t = tag();
        require(t <= 1, path, "invalid boolean tag");
        v = t == 1;
    }
    void value(std::string& v) {
        std::uint32_t i{};
        value(i);
        require(i < strings.size(), path, "STR0 index out of range");
        v = strings[i];
    }
    void ref(std::uint8_t kind, std::string& v) {
        std::uint32_t i{};
        value(i);
        require(i < references.size() && references[i].first == kind, path,
                "REF0 index or kind mismatch");
        v = references[i].second;
    }
    auto count() -> std::size_t {
        std::uint32_t n{};
        value(n);
        require(n <= reader.remaining(), path, "count exceeds remaining bytes");
        return n;
    }
    template <typename T> void value(T& v);
    template <typename... T> void fields(T&... v) {
        (value(v), ...);
    }
    template <typename T> void list(std::vector<T>& v) {
        const auto n = count();
        v.clear();
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            T row{};
            value(row);
            v.push_back(std::move(row));
        }
    }
    template <typename T> void option(std::optional<T>& v) {
        bool present{};
        value(present);
        if (present) {
            T row{};
            value(row);
            v = std::move(row);
        } else {
            v.reset();
        }
    }
};

template <typename T> void Output::value(T& v) {
    if constexpr (std::is_arithmetic_v<T> || std::is_same_v<std::remove_cv_t<T>, std::string>)
        value(static_cast<const std::remove_cv_t<T>&>(v));
    else
        visit(*this, v);
}
template <typename T> void Input::value(T& v) {
    visit(*this, v);
}

} // namespace capsule_detail
} // namespace cuexis::gameplay_packed
