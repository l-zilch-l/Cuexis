#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <type_traits>
#include <utility>

namespace cuexis::core::detail {

// Internal POD workspace. Construction never allocates, including in Debug STL builds.
// Storage growth is explicit and keeps both capacities live until copying completes.
template <typename T> class PmrBuffer final {
    static_assert(std::is_trivially_copyable_v<T> && std::is_nothrow_default_constructible_v<T> &&
                  std::is_nothrow_copy_constructible_v<T>);

  public:
    explicit PmrBuffer(std::pmr::memory_resource* resource) noexcept : resource_{resource} {}
    ~PmrBuffer() noexcept {
        releaseStorage();
    }
    PmrBuffer(const PmrBuffer&) = delete;
    auto operator=(const PmrBuffer&) -> PmrBuffer& = delete;
    PmrBuffer(PmrBuffer&& other) noexcept
        : resource_{other.resource_}, data_{std::exchange(other.data_, nullptr)},
          size_{std::exchange(other.size_, 0)}, capacity_{std::exchange(other.capacity_, 0)} {}
    auto operator=(PmrBuffer&& other) noexcept -> PmrBuffer& {
        if (this != &other) {
            releaseStorage();
            resource_ = other.resource_;
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }
    auto data() noexcept -> T* {
        return data_;
    }
    auto data() const noexcept -> const T* {
        return data_;
    }
    auto size() const noexcept -> std::size_t {
        return size_;
    }
    auto capacity() const noexcept -> std::size_t {
        return capacity_;
    }
    auto empty() const noexcept -> bool {
        return size_ == 0;
    }
    auto front() noexcept -> T& {
        return data_[0];
    }
    auto front() const noexcept -> const T& {
        return data_[0];
    }
    auto back() noexcept -> T& {
        return data_[size_ - 1];
    }
    auto back() const noexcept -> const T& {
        return data_[size_ - 1];
    }
    auto operator[](std::size_t index) noexcept -> T& {
        return data_[index];
    }
    auto operator[](std::size_t index) const noexcept -> const T& {
        return data_[index];
    }
    auto begin() noexcept -> T* {
        return data_;
    }
    auto begin() const noexcept -> const T* {
        return data_;
    }
    auto end() noexcept -> T* {
        return data_ ? data_ + size_ : nullptr;
    }
    auto end() const noexcept -> const T* {
        return data_ ? data_ + size_ : nullptr;
    }
    void reserve(std::size_t requested) {
        if (requested > maxSize())
            throw std::bad_array_new_length{};
        if (requested <= capacity_)
            return;
        auto* replacement = static_cast<T*>(resource_->allocate(requested * sizeof(T), alignof(T)));
        if (size_ != 0)
            std::uninitialized_copy_n(data_, size_, replacement);
        const auto previousSize = size_;
        releaseStorage();
        data_ = replacement;
        size_ = previousSize;
        capacity_ = requested;
    }
    void resize(std::size_t requested) {
        grow(requested);
        if (requested < size_) {
            std::destroy_n(data_ + requested, size_ - requested);
            size_ = requested;
        }
        while (size_ < requested) {
            std::construct_at(data_ + size_);
            ++size_;
        }
    }
    void push_back(T value) {
        if (size_ == maxSize())
            throw std::bad_array_new_length{};
        grow(size_ + 1);
        std::construct_at(data_ + size_, value);
        ++size_;
    }

  private:
    static constexpr auto maxSize() noexcept -> std::size_t {
        return static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T);
    }
    void grow(std::size_t requested) {
        if (requested > maxSize())
            throw std::bad_array_new_length{};
        if (requested > capacity_) {
            const auto half = capacity_ / 2;
            const auto grown = capacity_ > maxSize() - half ? maxSize() : capacity_ + half;
            reserve(std::max(requested, grown));
        }
    }
    void releaseStorage() noexcept {
        if (data_) {
            std::destroy_n(data_, size_);
            resource_->deallocate(data_, capacity_ * sizeof(T), alignof(T));
        }
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }
    std::pmr::memory_resource* resource_;
    T* data_{};
    std::size_t size_{}, capacity_{};
};

} // namespace cuexis::core::detail
