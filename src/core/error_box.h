#pragma once

#include <string>
#include <string_view>

#include "core/format.h"
#include "core/log.h"
#include "platform/message_box.h"

namespace swd3e
{

// swd3e.exe 0x0040C530: the game's error report: the text formatted (vsprintf into the buffer 0x004C4658) and shown in
// a message box titled `caption`. swd32 sent these reports to its empty debug log; swd3e shows them. The rebuild also
// writes them to the warning level of its log. Formats are UTF-8.
template <typename... Args>
void ErrorBox(ErrorReporter& errors, std::u8string_view caption, std::u8string_view format, const Args&... args)
{
    const std::u8string text = Utf8Format(format, {LogArgument(args)...});
    Log(LogLevel::Warning, u8"%s %s", caption, text);
    errors.ShowError(caption, text);
}

}  // namespace swd3e
