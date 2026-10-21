#pragma once

#include <span>
#include <string_view>

#include "core/types.h"

namespace swd3e
{

class Painter;

int SaveScreenshot(
    const Painter& painter,
    std::span<const u16> pixels,
    int width,
    int height,
    std::u8string_view fileName
);

}  // namespace swd3e
