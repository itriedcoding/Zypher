// Result<T> - explicit success/failure return type used by every fallible API.
//
// The engine deliberately avoids exceptions across module boundaries; failures
// are values that must be handled. `Result` is modelled on std::expected but
// keeps a small hand written implementation so the code base compiles on every
// supported toolchain without relying on library features that are still
// landing in places.
#pragma once

#include "zypher/core/Assert.h"
#include "zypher/core/Base.h"

#include <string>
#include <variant>

namespace zy {

enum class ErrorCode : u8 {
    None = 0,
    InvalidArgument,
    NotFound,
    Io,
    Parse,
    Unsupported,
    OutOfMemory,
    Overflow,
    Unavailable,
    Internal,
};

[[nodiscard]] constexpr std::string_view errorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::None: return "none";
    case ErrorCode::InvalidArgument: return "invalid-argument";
    case ErrorCode::NotFound: return "not-found";
    case ErrorCode::Io: return "io";
    case ErrorCode::Parse: return "parse";
    case ErrorCode::Unsupported: return "unsupported";
    case ErrorCode::OutOfMemory: return "out-of-memory";
    case ErrorCode::Overflow: return "overflow";
    case ErrorCode::Unavailable: return "unavailable";
    case ErrorCode::Internal: return "internal";
    }
    return "unknown";
}

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    Error() = default;
    Error(ErrorCode errorCode, std::string text) : code(errorCode), message(std::move(text)) {}

    [[nodiscard]] std::string toString() const {
        std::string result(errorCodeName(code));
        result += ": ";
        result += message;
        return result;
    }
};

[[nodiscard]] inline Error makeError(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

template <class T>
class [[nodiscard]] Result {
public:
    using ValueType = T;

    Result(T value) : m_data(std::in_place_index<0>, std::move(value)) {} // NOLINT(google-explicit-constructor)
    Result(Error error) : m_data(std::in_place_index<1>, std::move(error)) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept { return m_data.index() == 0; }
    [[nodiscard]] bool hasError() const noexcept { return m_data.index() == 1; }
    explicit operator bool() const noexcept { return hasValue(); }

    [[nodiscard]] T& value() & {
        ZY_ASSERT(hasValue(), "Result::value() called on an error");
        return std::get<0>(m_data);
    }
    [[nodiscard]] const T& value() const& {
        ZY_ASSERT(hasValue(), "Result::value() called on an error");
        return std::get<0>(m_data);
    }
    [[nodiscard]] T&& value() && {
        ZY_ASSERT(hasValue(), "Result::value() called on an error");
        return std::get<0>(std::move(m_data));
    }

    [[nodiscard]] T valueOr(T fallback) const {
        return hasValue() ? std::get<0>(m_data) : std::move(fallback);
    }

    [[nodiscard]] Error& error() & {
        ZY_ASSERT(hasError(), "Result::error() called on a value");
        return std::get<1>(m_data);
    }
    [[nodiscard]] const Error& error() const& {
        ZY_ASSERT(hasError(), "Result::error() called on a value");
        return std::get<1>(m_data);
    }

    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }
    [[nodiscard]] T& operator*() { return value(); }
    [[nodiscard]] const T& operator*() const { return value(); }

private:
    std::variant<T, Error> m_data;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)), m_ok(false) {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept { return m_ok; }
    [[nodiscard]] bool hasError() const noexcept { return !m_ok; }
    explicit operator bool() const noexcept { return m_ok; }

    [[nodiscard]] Error& error() & {
        ZY_ASSERT(hasError(), "Result::error() called on a value");
        return m_error;
    }
    [[nodiscard]] const Error& error() const& {
        ZY_ASSERT(hasError(), "Result::error() called on a value");
        return m_error;
    }

private:
    Error m_error;
    bool m_ok = true;
};

using Status = Result<void>;

[[nodiscard]] inline Status ok() { return Status{}; }

#define ZY_TRY(expression)                                                                    \
    do {                                                                                      \
        auto zy_try_result_ = (expression);                                                   \
        if (zy_try_result_.hasError()) {                                                      \
            return ::zy::Error{zy_try_result_.error().code, zy_try_result_.error().message};  \
        }                                                                                     \
    } while (false)

#define ZY_TRY_ASSIGN(name, expression)                                                       \
    auto zy_try_value_##name = (expression);                                                  \
    if (zy_try_value_##name.hasError()) {                                                     \
        return ::zy::Error{zy_try_value_##name.error().code, zy_try_value_##name.error().message}; \
    }                                                                                         \
    auto name = std::move(zy_try_value_##name).value()

} // namespace zy
