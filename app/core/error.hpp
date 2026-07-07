#pragma once

#include <string>
#include <utility>

namespace spectra5 {

enum class ErrorCode {
    None,
    NotFound,
    AlreadyExists,
    InvalidArgument,
    IoError,
    Unavailable,
    Cancelled,
    Conflict,
    Internal,
};

// Lightweight error value carried by Result<T>. No exceptions are thrown across
// the domain/application boundary; errors are returned explicitly.
struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string m) : code(c), message(std::move(m)) {}
};

inline const char* error_code_name(ErrorCode code)
{
    switch (code) {
        case ErrorCode::None:            return "None";
        case ErrorCode::NotFound:        return "NotFound";
        case ErrorCode::AlreadyExists:   return "AlreadyExists";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::IoError:         return "IoError";
        case ErrorCode::Unavailable:     return "Unavailable";
        case ErrorCode::Cancelled:       return "Cancelled";
        case ErrorCode::Conflict:        return "Conflict";
        case ErrorCode::Internal:        return "Internal";
    }
    return "Internal";
}

}  // namespace spectra5
