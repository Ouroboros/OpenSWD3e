#pragma once

#include "battle/battle_message.h"
#include "core/types.h"

// The constants of the battle messages that battle_message.cpp and smooth/smooth_messages.cpp share.
namespace swd3e
{

constexpr int kFrameThickness = 8;
constexpr int kFrameBorder = BattleMessage::Frame | kFrameThickness;
constexpr int kShadeMargin = 4;
constexpr int kSmallPanelSide = 0x34;  // DrawSmallPanel, DrawTallPanel, DrawWidePanel: the boxes and their frames
constexpr int kSmallFrameRight = 0x30;
constexpr int kSmallFrameBottom = 0x32;
constexpr int kTallPanelWidth = 0x64;
constexpr int kTallPanelHeight = 0xC8;
constexpr int kTallFrameRight = 0x60;
constexpr int kTallFrameBottom = 0xC4;
constexpr int kWidePanelWidth = 0x82;
constexpr int kWidePanelHeight = 0x1E;
constexpr int kWideFrameRight = 0x7A;
constexpr int kWideFrameBottom = 0x18;
constexpr i32 kRiseStart = -0x20;  // a rising message starts here and is gone above it

constexpr int kResistShadeY = 0x56;
constexpr int kResultShadeY = 0xB0;
constexpr int kShadeBlue = 4;
constexpr int kShadeExtra = 0x28;
constexpr int kShadeGreen = 4;
constexpr int kShadeRed = 0;
}  // namespace swd3e
