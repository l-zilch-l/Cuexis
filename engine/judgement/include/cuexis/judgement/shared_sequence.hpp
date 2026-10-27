#pragma once

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace cuexis::judgement::detail {
// Internal owning value sequence. Published pages are immutable: ordinary reads never
// expose mutable references. A write detaches its directory and affected page first.
// Independent values are compared exactly; shared immutable pages need no traversal.
template <class T, std::size_t PageSize = 64> class SharedSequence final {
    static_assert(PageSize > 0);
    using Page = std::vector<T>;
    using Directory = std::vector<std::shared_ptr<Page>>;
    std::shared_ptr<Directory> pages_;
    std::size_t size_{};

    void detachDirectory() {
        if (!pages_)
            pages_ = std::make_shared<Directory>();
        else if (pages_.use_count() != 1)
            pages_ = std::make_shared<Directory>(*pages_);
    }
    auto writablePage(std::size_t page) -> Page& {
        detachDirectory();
        auto& value = (*pages_)[page];
        if (value.use_count() != 1) {
            auto copy = std::make_shared<Page>();
            copy->reserve(PageSize);
            copy->assign(value->begin(), value->end());
            value = std::move(copy);
        }
        return *value;
    }

  public:
    using value_type = T;
    class const_iterator final {
        const SharedSequence* owner_{};
        std::size_t index_{};

      public:
        using iterator_category = std::random_access_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = const T&;
        using pointer = const T*;
        const_iterator() = default;
        const_iterator(const SharedSequence* owner, std::size_t index)
            : owner_(owner), index_(index) {}
        auto operator*() const -> reference {
            return (*owner_)[index_];
        }
        auto operator->() const -> pointer {
            return &**this;
        }
        auto operator[](difference_type n) const -> reference {
            return *(*this + n);
        }
        auto operator++() -> const_iterator& {
            ++index_;
            return *this;
        }
        auto operator++(int) -> const_iterator {
            auto old = *this;
            ++*this;
            return old;
        }
        auto operator--() -> const_iterator& {
            --index_;
            return *this;
        }
        auto operator--(int) -> const_iterator {
            auto old = *this;
            --*this;
            return old;
        }
        auto operator+=(difference_type n) -> const_iterator& {
            index_ = static_cast<std::size_t>(static_cast<difference_type>(index_) + n);
            return *this;
        }
        auto operator-=(difference_type n) -> const_iterator& {
            return *this += -n;
        }
        friend auto operator+(const_iterator i, difference_type n) -> const_iterator {
            return i += n;
        }
        friend auto operator+(difference_type n, const_iterator i) -> const_iterator {
            return i += n;
        }
        friend auto operator-(const_iterator i, difference_type n) -> const_iterator {
            return i -= n;
        }
        friend auto operator-(const_iterator a, const_iterator b) -> difference_type {
            return static_cast<difference_type>(a.index_) - static_cast<difference_type>(b.index_);
        }
        friend auto operator==(const_iterator, const_iterator) -> bool = default;
        friend auto operator<=>(const_iterator a, const_iterator b) {
            return a.index_ <=> b.index_;
        }
    };
    SharedSequence() = default;
    SharedSequence(const SharedSequence&) noexcept = default;
    auto operator=(const SharedSequence&) noexcept -> SharedSequence& = default;
    SharedSequence(SharedSequence&& other) noexcept
        : pages_(std::move(other.pages_)), size_(std::exchange(other.size_, 0)) {}
    auto operator=(SharedSequence&& other) noexcept -> SharedSequence& {
        if (this != &other) {
            pages_ = std::move(other.pages_);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
    SharedSequence(std::initializer_list<T> values) {
        for (const auto& value : values)
            push_back(value);
    }
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return size_;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return size_ == 0;
    }
    [[nodiscard]] auto max_size() const noexcept -> std::size_t {
        return Page{}.max_size();
    }
    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return pages_ ? pages_->size() * PageSize : 0;
    }
    auto operator[](std::size_t i) const noexcept -> const T& {
        return (*(*pages_)[i / PageSize])[i % PageSize];
    }
    auto at(std::size_t i) const -> const T& {
        if (i >= size_)
            throw std::out_of_range{"SharedSequence index"};
        return (*this)[i];
    }
    auto front() const -> const T& {
        return at(0);
    }
    auto back() const -> const T& {
        return at(size_ - 1);
    }
    auto begin() const noexcept -> const_iterator {
        return {this, 0};
    }
    auto end() const noexcept -> const_iterator {
        return {this, size_};
    }
    auto cbegin() const noexcept -> const_iterator {
        return begin();
    }
    auto cend() const noexcept -> const_iterator {
        return end();
    }
    void reserve(std::size_t n) {
        if (n > max_size())
            throw std::length_error{"SharedSequence capacity"};
        if (n > capacity()) {
            detachDirectory();
            pages_->reserve(n / PageSize + (n % PageSize != 0));
        }
    }
    void clear() noexcept {
        pages_.reset();
        size_ = 0;
    }
    void push_back(T value) {
        if (size_ == max_size())
            throw std::length_error{"SharedSequence size"};
        if (size_ % PageSize == 0) {
            auto page = std::make_shared<Page>();
            page->reserve(PageSize);
            page->push_back(std::move(value));
            detachDirectory();
            pages_->push_back(std::move(page));
        } else {
            writablePage(size_ / PageSize).push_back(std::move(value));
        }
        ++size_;
    }
    template <class... Args> void emplace_back(Args&&... args) {
        push_back(T(std::forward<Args>(args)...));
    }
    // The callback edits a local value. It can copy this sequence or throw without
    // publishing a writable page alias or changing any captured owner.
    template <class F> void update(std::size_t i, F&& write) {
        if (i >= size_)
            throw std::out_of_range{"SharedSequence update"};
        static_assert(std::is_nothrow_move_assignable_v<T>);
        auto value = (*this)[i];
        std::forward<F>(write)(value);
        writablePage(i / PageSize)[i % PageSize] = std::move(value);
    }
    [[nodiscard]] auto materialize() const -> std::vector<T> {
        return {begin(), end()};
    }
    friend auto operator==(const SharedSequence& a, const SharedSequence& b) -> bool {
        if (a.size_ != b.size_)
            return false;
        if (a.pages_ == b.pages_)
            return true;
        for (std::size_t i = 0; i < a.size_; i += PageSize) {
            const auto& left = (*a.pages_)[i / PageSize];
            const auto& right = (*b.pages_)[i / PageSize];
            if (left != right && *left != *right)
                return false;
        }
        return true;
    }
    friend auto operator==(const SharedSequence& a, const std::vector<T>& b) -> bool {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }
};
} // namespace cuexis::judgement::detail
