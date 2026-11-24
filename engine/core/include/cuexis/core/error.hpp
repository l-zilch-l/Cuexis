#pragma once

//  Error - the unified error type; must carry a stable error code and a readable message
//  Program logic must branch on the code, never on the message text, which may be localized
//  Error supports attached context key/value pairs and a causal chain (cause) to aid
//  diagnosis
//
//  Two storage shapes share one variant:
//    owned    - both code and message own std::string storage (the historical constructor);
//    interned - both texts point at static storage and carry a fixed stage. Creating or copying
//               an interned Error never allocates, so a delivery path can report a reserved
//               allocation fault while the allocator keeps failing.
//  Context lives behind a lazily created shared list: an Error with no context does not construct
//  a std::vector at all, which matters because some Debug STL builds allocate a container proxy
//  for every container instance (including an empty one). Copies share the list until a
//  withContext call detaches it.

#include <cuexis/core/abi_warnings.hpp>
#include <cuexis/core/core_export.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace cuexis::core {

CUEXIS_ABI_WARNING_PUSH

struct ErrorContext {
    std::string key;
    std::string value;
};

//  Fixed failure stage delivered with an interned Error. The execution host reserved fault slot
//  uses the same enum, so a delivered Error and the slot it came from name the same stage without
//  building a second diagnostic.
enum class ErrorStage : std::uint8_t {
    none,
    create,
    configure,
    copyReceipt,
    feedback,
    publish,
    recordAttempt,
    takeLatest,
    observe,
    expire,
    activate,
    query,
};

//  Non-owning text that may only refer to static storage duration. The array constructor is
//  private; the consteval intern() entry is the only public factory. A temporary std::string or
//  std::string_view and an automatic array cannot be used in a constant expression, so those
//  inputs are rejected at compile time instead of dangling at run time.
class InternedText final {
  public:
    [[nodiscard]] constexpr auto view() const noexcept -> std::string_view {
        return {data_, size_};
    }

  private:
    template <std::size_t N>
    friend consteval auto intern(const char (&text)[N]) noexcept -> InternedText;

    template <std::size_t N>
    constexpr explicit InternedText(const char (&text)[N]) noexcept : data_(text), size_(N - 1) {}

    const char* data_;
    std::uint32_t size_;
};

template <std::size_t N> consteval auto intern(const char (&text)[N]) noexcept -> InternedText {
    return InternedText{text};
}

class CUEXIS_CORE_API Error final {
  public:
    Error(std::string code, std::string message);

    //  Zero-allocation factory for reserved failure paths: the texts must be static literals and
    //  the stage is a fixed enum value. The result owns no dynamically allocated storage.
    [[nodiscard]] static auto interned(InternedText code, InternedText message,
                                       ErrorStage stage = ErrorStage::none) noexcept -> Error;

    Error(const Error&) = default;
    auto operator=(const Error&) -> Error& = default;
    Error(Error&&) noexcept = default;
    auto operator=(Error&&) noexcept -> Error& = default;

    [[nodiscard]] std::string_view code() const noexcept;
    [[nodiscard]] std::string_view message() const noexcept;
    [[nodiscard]] const std::vector<ErrorContext>& context() const noexcept;

    //  True when both texts are interned static storage; such an Error owns no text allocation.
    [[nodiscard]] bool isInterned() const noexcept;
    //  The fixed stage of an interned Error; ErrorStage::none for an owned Error.
    [[nodiscard]] ErrorStage stage() const noexcept;

    // Returns the upstream error in the cause chain; the returned pointer stays valid as long
    // as this Error holds its cause
    [[nodiscard]] const Error* cause() const noexcept;

    // Chained diagnostic context (lvalue overload, returns a reference to itself)
    Error& withContext(std::string key, std::string value) &;
    // Chained diagnostic context (rvalue overload, enables the Error{} << "key" << "value"
    // style)
    Error&& withContext(std::string key, std::string value) &&;

    // Sets the upstream error in the cause chain
    Error& withCause(Error cause) &;
    Error&& withCause(Error cause) &&;

  private:
    struct OwnedTextPair final {
        std::string code;
        std::string message;
    };
    struct InternedTextPair final {
        InternedText code;
        InternedText message;
    };
    struct InternedTag final {};
    Error(InternedTag, InternedText code, InternedText message, ErrorStage stage) noexcept;

    std::variant<OwnedTextPair, InternedTextPair> text_;
    std::shared_ptr<std::vector<ErrorContext>> context_;
    std::shared_ptr<const Error> cause_;
    ErrorStage stage_{ErrorStage::none};
};

static_assert(std::is_nothrow_move_constructible_v<Error>);
static_assert(std::is_nothrow_move_assignable_v<Error>);
static_assert(std::is_copy_constructible_v<Error>);

CUEXIS_ABI_WARNING_POP

} // namespace cuexis::core
