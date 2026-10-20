#pragma once

#include "core/log.h"

namespace swd3e
{

// swd3e.exe 0x0040C560: the game's debug log. The shipped build compiled it to an empty function (it is also the
// default pixel converter, see PixelFormat); here its messages go to the debug level of the rebuild's log. Formats
// are UTF-8. Many reports swd32 sent here are error boxes in swd3e (ErrorBox, 0x0040C530).
template <typename... Args>
void DebugLog(const LogFormat& format, const Args&... args)
{
    Log(LogLevel::Debug, format, args...);
}

// swd3e.exe 0x00411F10: the error log, also compiled to an empty function in the shipped build; here its messages go
// to the warning level of the rebuild's log.
template <typename... Args>
void ErrorLog(const LogFormat& format, const Args&... args)
{
    Log(LogLevel::Warning, format, args...);
}

}  // namespace swd3e
