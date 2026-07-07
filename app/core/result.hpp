#pragma once

#include <cassert>
#include <optional>
#include <string>
#include <utility>

#include "core/error.hpp"

namespace spectra5 {

// Result<T> holds either a value of type T or an Error. Modeled after the
// common Result/Either pattern so the domain layer can report failures without
// exceptions. Use Status (Result<void>) for operations without a return value.
template <class T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {}
    Result(Error error) : error_(std::move(error)) {}

    static Result ok(T value) { return Result(std::move(value)); }
    static Result fail(ErrorCode code, std::string message)
    {
        return Result(Error{code, std::move(message)});
    }

    bool is_ok() const { return value_.has_value(); }
    explicit operator bool() const { return is_ok(); }

    const T& value() const
    {
        assert(value_.has_value());
        return *value_;
    }
    T& value()
    {
        assert(value_.has_value());
        return *value_;
    }

    const Error& error() const
    {
        assert(error_.has_value());
        return *error_;
    }

    T value_or(T fallback) const { return value_.has_value() ? *value_ : std::move(fallback); }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};

// Result without a payload.
class Status {
public:
    Status() = default;
    Status(Error error) : error_(std::move(error)) {}

    static Status ok() { return Status(); }
    static Status fail(ErrorCode code, std::string message)
    {
        return Status(Error{code, std::move(message)});
    }

    bool is_ok() const { return !error_.has_value(); }
    explicit operator bool() const { return is_ok(); }

    const Error& error() const
    {
        assert(error_.has_value());
        return *error_;
    }

private:
    std::optional<Error> error_;
};

}  // namespace spectra5
