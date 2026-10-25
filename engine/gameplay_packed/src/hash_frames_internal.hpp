#pragma once

#include "hash_work_internal.hpp"
#include <cuexis/judgement/gameplay_graph.hpp>

#include <array>
#include <cassert>
#include <memory>

namespace cuexis::gameplay_packed::detail {
struct HashFrame final {
    const judgement::PatternNodeDeclaration* node{};
    std::uint64_t cursor{};
};
static_assert(sizeof(HashFrame) == 16);

// The base 255 frames occupy 4080 bytes of the reserved 64 KiB fixed workspace.
// Further depth uses checked 4096-byte pages; no flat node list or recursive C++ walk.
class HashFrames final {
  public:
    explicit HashFrames(HashScratch& scratch) : scratch_{scratch} {}
    HashFrames(const HashFrames&) = delete;
    auto operator=(const HashFrames&) -> HashFrames& = delete;
    ~HashFrames() noexcept {
        clear();
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return depth_ == 0;
    }
    auto top() noexcept -> HashFrame& {
        assert(depth_);
        return page_ ? page_->frames[page_->used - 1] : base_[depth_ - 1];
    }
    void push(HashFrame frame) {
        if (depth_ < base_.size())
            base_[depth_] = frame;
        else {
            if (!page_ || page_->used == 255) {
                auto* storage = scratch_.allocate(sizeof(Page), alignof(Page));
                page_ = std::construct_at(static_cast<Page*>(storage), Page{page_, 0, {}});
            }
            page_->frames[page_->used++] = frame;
        }
        ++depth_;
    }
    void pop() noexcept {
        assert(depth_);
        if (page_ && --page_->used == 0) {
            auto* old = page_;
            page_ = old->previous;
            std::destroy_at(old);
            scratch_.deallocate(old, sizeof(Page), alignof(Page));
        }
        --depth_;
    }
    void clear() noexcept {
        while (!empty())
            pop();
    }

  private:
    struct Page final {
        Page* previous;
        std::uint64_t used;
        std::array<HashFrame, 255> frames;
    };
    static_assert(sizeof(Page) == 4096);
    HashScratch& scratch_;
    std::array<HashFrame, 255> base_{};
    Page* page_{};
    std::size_t depth_{};
};
} // namespace cuexis::gameplay_packed::detail
