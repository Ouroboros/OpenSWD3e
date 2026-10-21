#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "draw/painter.h"
#include "game/screenshot.h"
#include "test.h"

namespace
{

constexpr int kSide = 2;
constexpr swd3e::u16 kWhite = 0x7FFF;  // RGB555
constexpr swd3e::u16 kBlue = 0x001F;
constexpr swd3e::u16 kRed = 0x7C00;
constexpr swd3e::u16 kBlack = 0;
constexpr swd3e::u8 kFull = 0xF8;  // a full 5-bit channel in 8 bits
constexpr std::size_t kHeaderBytes = 0x36;
constexpr std::size_t kRowBytes = 8;  // two 3-byte pixels padded to 4 bytes
constexpr std::size_t kFileBytes = kHeaderBytes + kSide * kRowBytes;
constexpr std::size_t kSizeAt = 2;
constexpr std::size_t kPixelsAtField = 0xA;
constexpr std::size_t kInfoSizeAt = 0xE;
constexpr std::size_t kWidthAt = 0x12;
constexpr std::size_t kHeightAt = 0x16;
constexpr std::size_t kPlanesAt = 0x1A;
constexpr std::size_t kBitsAt = 0x1C;
constexpr swd3e::u32 kInfoBytes = 0x28;
constexpr swd3e::u32 kBits = 0x18;
constexpr std::string_view kFileName = "swd3_screenshot_test.bmp";

swd3e::u32 Dword(const std::vector<swd3e::u8>& bytes, std::size_t at)
{
    return static_cast<swd3e::u32>(bytes[at]) | static_cast<swd3e::u32>(bytes[at + 1]) << 8 |
           static_cast<swd3e::u32>(bytes[at + 2]) << 16 | static_cast<swd3e::u32>(bytes[at + 3]) << 24;
}

}  // namespace

TEST_CASE("screenshot: a 24-bit BMP written bottom up")
{
    swd3e::Painter painter;
    const std::array<swd3e::u16, kSide * kSide> pixels = {kWhite, kBlue, kRed, kBlack};
    const std::filesystem::path path = std::filesystem::temp_directory_path() / kFileName;
    const int saved = swd3e::SaveScreenshot(painter, pixels, kSide, kSide, path.u8string());
    CHECK_EQ(saved, 1);
    std::ifstream file(path, std::ios::binary);
    const std::vector<swd3e::u8> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    file.close();
    std::filesystem::remove(path);
    CHECK_EQ(bytes.size(), kFileBytes);

    if (bytes.size() != kFileBytes)
    {
        return;
    }

    CHECK_EQ(bytes[0], 'B');
    CHECK_EQ(bytes[1], 'M');
    CHECK_EQ(Dword(bytes, kSizeAt), kFileBytes);
    CHECK_EQ(Dword(bytes, kPixelsAtField), kHeaderBytes);
    CHECK_EQ(Dword(bytes, kInfoSizeAt), kInfoBytes);
    CHECK_EQ(Dword(bytes, kWidthAt), kSide);
    CHECK_EQ(Dword(bytes, kHeightAt), kSide);
    CHECK_EQ(bytes[kPlanesAt], 1);
    CHECK_EQ(bytes[kBitsAt], kBits);
    const std::array<swd3e::u8, kSide * kRowBytes> expected = {
        0,
        0,
        kFull,
        0,
        0,
        0,
        0,
        0,
        kFull,
        kFull,
        kFull,
        kFull,
        0,
        0,
        0,
        0,
    };

    CHECK(std::equal(expected.begin(), expected.end(), bytes.begin() + kHeaderBytes));
}
