#pragma once

#include <format>
#include <iostream>
#include <string_view>

namespace spectra5::log {

enum class Level { Info, Warn, Error };

inline void write(Level level, std::string_view tag, const std::string& message)
{
    const char* prefix = "I";
    if (level == Level::Warn) {
        prefix = "W";
    } else if (level == Level::Error) {
        prefix = "E";
    }
    std::cerr << '[' << prefix << '/' << tag << "] " << message << '\n';
}

template <typename... Args>
void tagInfo(std::string_view tag, std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Info, tag, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void tagWarn(std::string_view tag, std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Warn, tag, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void tagError(std::string_view tag, std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Error, tag, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Info, "app", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Warn, "app", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args)
{
    write(Level::Error, "app", std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace spectra5::log
