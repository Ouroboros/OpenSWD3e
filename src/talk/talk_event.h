#pragma once

#include <string_view>

namespace swd3e
{

class ErrorReporter;
struct Role;

// The failed run of a role's act, reported in the error box (0x0040C530) titled `where`: its GUID, act, action and
// direction in the text 0x004AF244.
void ReportActError(ErrorReporter& errors, std::u8string_view where, const Role& role);

}  // namespace swd3e
