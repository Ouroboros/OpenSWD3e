#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#include "audio/audio_manager.h"
#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_state.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game_data.h"
#include "pic/rle.h"
#include "platform/audio_device.h"
#include "platform/display.h"
#include "platform/glyph_rasterizer.h"
#include "platform/surface.h"
#include "random/random.h"
#include "recording_reporter.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"
#include "test.h"

namespace
{

using swd3e::BattleMessage;
using swd3e::u8;

class NullRasterizer : public swd3e::GlyphRasterizer
{
public:
    void RenderText(swd3e::Surface&, std::string_view, int, std::string_view, swd3e::u32) override
    {
    }
};

struct Messages
{
    Messages()
        : screen(swd3e::Display::Width, swd3e::Display::Height),
          codec(this->painter),
          archive(this->codec, this->errors),
          acts(this->errors),
          audio(this->device),
          hud(this->painter, this->archive, this->acts, this->random, this->audio),
          panelFont(this->rasterizer),
          messageFont(this->rasterizer),
          messages(this->painter, this->archive, this->acts, this->hud, this->state, this->panelFont, this->messageFont)
    {
        this->painter.Init(this->screen);
        this->painter.SetTarget(this->screen.Pixels());
        this->painter.SetClipClamped(0, 0, swd3e::Display::Width, swd3e::Display::Height);
    }

    swd3e::Surface screen;
    swd3e::Painter painter;
    swd3e::RleCodec codec;
    swd3etest::RecordingReporter errors;
    swd3e::TswArchive archive;
    swd3e::ActArchive acts;
    swd3e::Random random;
    swd3e::AudioDevice device;
    swd3e::AudioManager audio;
    swd3e::BattleHud hud;
    swd3e::BattleState state;
    NullRasterizer rasterizer;
    swd3e::TextFont panelFont;
    swd3e::TextFont messageFont;
    swd3e::BattleMessages messages;
};

// Where swd32.exe keeps the texts.
constexpr std::array<std::pair<swd3e::u32, std::u8string_view>, 6> kTexts = {{
    {0x004A7E20, swd3e::BattleMessages::ResistAllEnd},
    {0x004A80E4, swd3e::BattleMessages::DefenseDownEnd},
    {0x004A7F3C, swd3e::BattleMessages::ResistPhysicalEnd},
    {0x004A7B3C, swd3e::BattleMessages::CraftSuccess},
    {0x004A7AD4, swd3e::BattleMessages::MasteryLine},
    {0x004A7EB4, swd3e::BattleMessages::SpeedDown},
}};

constexpr swd3e::u32 kLootLineAt = 0x004A7AE8;
constexpr swd3e::u32 kElementTable = 0x004A79A8;  // the pointers to the element names

constexpr int kFirstX = 1;
constexpr int kFirstY = 2;
constexpr std::string_view kFirstText = "abcd";
constexpr int kSecondX = 3;
constexpr int kSecondY = 4;
constexpr int kSecondFrames = 5;
constexpr int kThirdX = 5;
constexpr int kThirdY = 6;
constexpr swd3e::i32 kRiseStart = -0x20;  // a rising message starts this far below
constexpr int kPanelTop = 0xD4;
constexpr int kPanelBottom = 0xF4;
constexpr int kGrowDivisor = 3;
constexpr std::size_t kEarlierRow = 1;  // two queued actions; the one in the earlier row is taken
constexpr std::size_t kEarlierColumn = 3;
constexpr std::size_t kLaterRow = 2;
constexpr std::size_t kLaterColumn = 5;
constexpr swd3e::i16 kEarlierAction = 7;
constexpr swd3e::i16 kLaterAction = 9;

}  // namespace

TEST_CASE("battle message: texts match swd32.exe")
{
    const std::vector<u8> exe = swd3etest::ReadGameFile("swd32.exe");

    if (exe.empty())
    {
        std::printf("  (swd32.exe not found, skipped)\n");
        return;
    }

    for (const auto& [address, text] : kTexts)
    {
        CHECK(swd3etest::SameText(exe, address, text));
    }

    CHECK(swd3etest::SameText(exe, kLootLineAt, swd3e::BattleMessages::LootLine));

    for (std::size_t element = 0; element < swd3e::BattleMessages::ElementNames.size(); ++element)
    {
        const swd3e::u32 pointer = swd3etest::ExeDword(exe, kElementTable + sizeof(swd3e::u32) * element);
        CHECK(swd3etest::SameText(exe, pointer, swd3e::BattleMessages::ElementNames[element]));
    }
}

TEST_CASE("battle message: list, panel growth and the action queue")
{
    Messages all;
    all.messages.Add(kFirstX, kFirstY, 0, kFirstText, BattleMessage::BlackBar);
    all.messages.Add(kSecondX, kSecondY, kSecondFrames, "xy", BattleMessage::Rise);
    all.messages.Add(kThirdX, kThirdY, 0, "z", 0);
    BattleMessage* first = all.messages.messages.get();
    CHECK(first != nullptr);

    if (first == nullptr)
    {
        return;
    }

    CHECK_EQ(first->length, static_cast<int>(kFirstText.size()));
    BattleMessage* second = first->next.get();
    CHECK(second != nullptr);

    if (second == nullptr)
    {
        return;
    }

    CHECK_EQ(second->y, 1);
    CHECK_EQ(second->slideY, kRiseStart);
    CHECK(second->next != nullptr);
    second->frames = 0;
    second->flags = 0;
    all.messages.Draw();
    CHECK(all.messages.messages == nullptr);

    all.messages.panelHeight = 0;
    CHECK_EQ(all.messages.GrowPanel(kPanelTop, kPanelBottom, kGrowDivisor), 0);
    CHECK_EQ(all.messages.panelHeight, (kPanelBottom - kPanelTop) / kGrowDivisor);
    int calls = 1;

    while (all.messages.GrowPanel(kPanelTop, kPanelBottom, kGrowDivisor) == 0)
    {
        ++calls;
    }

    CHECK_EQ(all.messages.panelHeight, kPanelBottom - kPanelTop - (kGrowDivisor - 1));  // a step of 0 ends it
    CHECK(calls > 1);

    constexpr std::size_t kEarlier = kEarlierRow * swd3e::BattleState::QueueColumns + kEarlierColumn;
    constexpr std::size_t kLater = kLaterRow * swd3e::BattleState::QueueColumns + kLaterColumn;
    all.state.actionQueue[kEarlier] = kEarlierAction;
    all.state.actionQueue[kLater] = kLaterAction;
    all.state.TakePendingAction();
    CHECK_EQ(all.state.pendingAction, kEarlierAction);
    CHECK_EQ(all.state.pendingActionRow, kEarlierRow);
    CHECK_EQ(all.state.actionQueue[kEarlier], 0);
    all.state.TakePendingAction();
    CHECK_EQ(all.state.pendingAction, kEarlierAction);  // kept until it is used
}

TEST_CASE("battle message: the element tints match swd3e.exe")
{
    constexpr swd3e::u32 kElementTintsAt = 0x004BF674;
    const std::vector<swd3e::u8> exe = swd3etest::ReadGameFile("swd3e.exe");

    if (exe.empty())
    {
        std::printf("  (swd3e.exe not found, skipped)\n");
        return;
    }

    CHECK(swd3etest::SameTable(exe, kElementTintsAt, swd3e::BattleMessages::ElementTints));
}
