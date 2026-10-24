#pragma once

#include "text/big5.h"

#include <algorithm>
#include <array>
#include <string>

// The texts of the menus (Big5 in the executable; u8 here, converted where drawn).
namespace swd3e::menu_text
{

constexpr std::u8string_view kSoundVolume = u8"音效";     // 0x004BED28
constexpr std::u8string_view kMusicVolume = u8"音樂";     // 0x004BED20
constexpr std::u8string_view kCacheSize = u8"置換空間";   // 0x004BED14
constexpr std::u8string_view kMapEffects = u8"地圖效果";  // 0x004BED08
constexpr std::u8string_view kTextSpeed = u8"文字速度";   // 0x004BECFC
constexpr std::u8string_view kBattleSpeed =
    u8"戰鬥速度";  // swd32.exe 0x004A70B4 (not in swd3e.exe: no battle speed setting)
constexpr std::u8string_view kCacheLine = u8"%c碟 ,需要%4dMB";  // 0x004BECEC
constexpr std::u8string_view kOn = u8"開";                      // 0x004BECE8
constexpr std::u8string_view kOff = u8"關";                     // 0x004BECE4
constexpr std::u8string_view kEffectsLine = u8"遊戲效果 %s";    // 0x004BECD8
constexpr std::u8string_view kSpeedFastest = u8"最快  ";        // 0x004BECD0
constexpr std::u8string_view kSpeedFast = u8"快    ";           // 0x004BECC8
constexpr std::u8string_view kSpeedMedium = u8"適中  ";         // 0x004BECC0
constexpr std::u8string_view kSpeedSlower = u8"稍慢  ";         // 0x004BECB8
constexpr std::u8string_view kSpeedSlow = u8"慢    ";           // 0x004BECB0
constexpr std::u8string_view kSample = u8"軒轅劍參外傳    ";    // 0x004BEC9C

constexpr std::array<std::u8string_view, 5> kSpeeds =
    {kSpeedFastest, kSpeedFast, kSpeedMedium, kSpeedSlower, kSpeedSlow};

// The key names of the key setup by DirectInput code (0x004AFA8C, 0x10 bytes each, 0xDE of them, checked against the
// executable).
constexpr std::array<std::u8string_view, 0xDE> kKeyNames = {
    u8"請按一個鍵",
    u8"Escape",
    u8"1",
    u8"2",
    u8"3",
    u8"4",
    u8"5",
    u8"6",
    u8"7",
    u8"8",
    u8"9",
    u8"0",
    u8"-",
    u8"=",
    u8"BackSpace",
    u8"Tab",
    u8"Q",
    u8"W",
    u8"E",
    u8"R",
    u8"T",
    u8"Y",
    u8"U",
    u8"I",
    u8"O",
    u8"P",
    u8"[",
    u8"]",
    u8"Enter",
    u8"L-Ctrl",
    u8"A",
    u8"S",
    u8"D",
    u8"F",
    u8"G",
    u8"H",
    u8"J",
    u8"K",
    u8"L",
    u8";",
    u8"'",
    u8"`",
    u8"L-Shift",
    u8"\\",
    u8"Z",
    u8"X",
    u8"C",
    u8"V",
    u8"B",
    u8"N",
    u8"M",
    u8",",
    u8".",
    u8"/",
    u8"R-Shift",
    u8"*",
    u8"L-Alt",
    u8"Space",
    u8"Caps Lock",
    u8"F1",
    u8"F2",
    u8"F3",
    u8"F4",
    u8"F5",
    u8"F6",
    u8"F7",
    u8"F8",
    u8"F9",
    u8"F10",
    u8"Num Lock",
    u8"Scroll Lock",
    u8"Numpad 7",
    u8"Numpad 8",
    u8"Numpad 9",
    u8"Numpad -",
    u8"Numpad 4",
    u8"Numpad 5",
    u8"Numpad 6",
    u8"Numpad +",
    u8"Numpad 1",
    u8"Numpad 2",
    u8"Numpad 3",
    u8"Numpad 0",
    u8"Numpad .",
    u8"??",
    u8"??",
    u8"??",
    u8"F11",
    u8"F12",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"F13",
    u8"F14",
    u8"F15",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"Kana",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"Convert",
    u8"??",
    u8"NoConvert",
    u8"??",
    u8"Yen",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"n=",
    u8"??",
    u8"??",
    u8"CircumFlex",
    u8"At",
    u8"Colon",
    u8"UnderLine",
    u8"Kanji",
    u8"Stop",
    u8"AX",
    u8"Unlabeled",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"Numpad Enter",
    u8"R-Ctrl",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"n,",
    u8"??",
    u8"n/",
    u8"??",
    u8"SysRq",
    u8"R-Alt",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"Home",
    u8"Arrow Up",
    u8"Page Up",
    u8"??",
    u8"Arrow Left",
    u8"??",
    u8"Arrow Right",
    u8"??",
    u8"End",
    u8"Arrow Down",
    u8"Page Down",
    u8"Insert",
    u8"Delete",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"??",
    u8"L-Win",
    u8"R-Win",
    u8"AppMenu",
};

// The rows of the key setup (0x004BED60, 0xC bytes each).
constexpr std::array<std::u8string_view, 16> kKeyRows = {
    u8"上",
    u8"下",
    u8"左",
    u8"右",
    u8"進入介面",
    u8"確定",
    u8"取消",
    u8"放棄",
    u8"切換一",
    u8"切換二",
    u8"說明文",
    u8"上一頁",
    u8"下一頁",
    u8"跑步",
    u8"自動攻擊",
    u8"逃跑",
};

constexpr std::size_t kSpeedNameBytes = 6;   // the speed names are padded to six bytes
constexpr std::size_t kSampleCharBytes = 2;  // the sample types out by Big5 characters

// The speed line of the settings (inline in the original, 0x004490C0 and 0x0044C160): the name of text speed `speed`
// (none outside 0..4) with the first `words` characters of the sample copied after its six bytes; the line ends at
// its first NUL as drawn.
inline std::string SpeedSample(int speed, std::size_t words)
{
    std::string line(kSpeedNameBytes + kSampleCharBytes * words, '\0');

    if (speed >= 0 && static_cast<std::size_t>(speed) < kSpeeds.size())
    {
        const std::string name = Big5(kSpeeds[static_cast<std::size_t>(speed)]);
        std::copy_n(name.begin(), std::min(name.size(), kSpeedNameBytes), line.begin());
    }

    const std::string sample = Big5(kSample);
    std::copy_n(sample.begin(), std::min(sample.size(), kSampleCharBytes * words), line.begin() + kSpeedNameBytes);
    line.resize(std::min(line.size(), line.find('\0')));
    return line;
}

// Whether the sample has typed out after `words` characters (its byte there is the NUL).
inline bool SampleEnds(std::size_t words)
{
    return kSampleCharBytes * words >= Big5(kSample).size();
}
}  // namespace swd3e::menu_text
