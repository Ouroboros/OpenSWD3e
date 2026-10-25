#pragma once

#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/types.h"

// The statically linked MSVC runtime calls of the game, each a thin wrapper over the current C runtime (user
// requirement 2026-10-18: the current runtime's results are used, the original runtime is not rewritten).
namespace swd3e::crt
{

// swd3e.exe 0x004A10E8 _ftol: truncation towards zero.
i64 Ftol(double value);
// swd3e.exe 0x004A1B81 floor.
double Floor(double value);
// swd3e.exe 0x004A150A difftime.
double Difftime(i32 end, i32 start);
// swd3e.exe 0x004A151B time.
i32 Time();

namespace detail
{

template <typename T>
auto PrintfArgument(const T& value)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        return value.c_str();
    }
    else
    {
        return value;
    }
}

}  // namespace detail

// swd3e.exe 0x004A19E5 vsprintf into the game's 256-byte buffers (std::string arguments as %s);
// the original writes
// past them for longer texts, here the text is cut.
template <typename... Args>
std::string Sprintf(std::string_view format, const Args&... args)
{
    std::array<char, 0x100> buffer = {};
    const std::string pattern(format);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    std::snprintf(buffer.data(), buffer.size(), pattern.c_str(), detail::PrintfArgument(args)...);
#pragma clang diagnostic pop
    return std::string(buffer.data());
}

// swd3e.exe 0x004A14E2 srand and 0x004A14EC rand.
class Random
{
public:
    void Seed(u32 seed);
    int Next();
};

}  // namespace swd3e::crt
