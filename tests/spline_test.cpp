#include <array>
#include <vector>

#include "core/spline.h"
#include "game_data.h"
#include "test.h"

namespace
{

constexpr float kStart = 6;  // the curve at t 0 is (p0 + 4 p1 + p2) / 6
constexpr std::array<std::array<swd3e::i16, 2>, 4> kLine = {{{0, 0}, {6, 12}, {12, 24}, {18, 36}}};
constexpr swd3e::i32 kStartX = 6;
constexpr swd3e::i32 kStartY = 12;
constexpr float kHalfway = 10;  // 10 twentieths: halfway along the segment
constexpr swd3e::i32 kHalfwayX = 9;
constexpr swd3e::i32 kHalfwayY = 18;
constexpr float kLastStep = 20;
constexpr swd3e::i32 kEndX = 12;
constexpr swd3e::i32 kEndY = 24;

}  // namespace

TEST_CASE("spline: the basis matches swd32.exe")
{
    constexpr swd3e::u32 kBasisAt = 0x004A6474;
    const std::vector<swd3e::u8> exe = swd3etest::ReadGameFile("swd32.exe");

    if (exe.empty())
    {
        std::printf("  (swd32.exe not found, skipped)\n");
        return;
    }

    CHECK(swd3etest::SameTable(exe, kBasisAt, swd3e::spline::Basis));
}

TEST_CASE("spline: the weights at the ends of a segment")
{
    const swd3e::spline::Weights start = swd3e::spline::BasisWeights(0, swd3e::spline::Basis);
    CHECK_EQ(start[0] * kStart, 1.0F);
    CHECK_EQ(start[3], 0.0F);
    const swd3e::spline::Weights end = swd3e::spline::BasisWeights(1, swd3e::spline::Basis);
    CHECK_EQ(end[0], 0.0F);
}

TEST_CASE("spline: the battle talk curve along evenly spaced points")
{
    swd3e::i32 x = 0;
    swd3e::i32 y = 0;
    swd3e::TalkCurvePoint(0, x, y, kLine);
    CHECK_EQ(x, kStartX);
    CHECK_EQ(y, kStartY);
    swd3e::TalkCurvePoint(kHalfway, x, y, kLine);
    CHECK_EQ(x, kHalfwayX);
    CHECK_EQ(y, kHalfwayY);
    swd3e::TalkCurvePoint(kLastStep, x, y, kLine);
    CHECK_EQ(x, kEndX);
    CHECK_EQ(y, kEndY);
}
