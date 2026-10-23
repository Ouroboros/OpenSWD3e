#include <SDL3/SDL.h>

#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <string>

#include "audio/audio_manager.h"
#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "config/config.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game_data.h"
#include "golden/hud_scenarios.h"
#include "golden_check.h"
#include "party/party.h"
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

using golden::i32;
using golden::u16;
using golden::u32;
using golden::u64;

namespace
{

constexpr int kGaugeLevels = 6;  // the tall gauge of six levels

class NullRasterizer : public swd3e::GlyphRasterizer
{
public:
    void RenderText(swd3e::Surface&, std::string_view, int, std::string_view, swd3e::u32) override
    {
    }
};

class HudBackend : public golden::HudBackend
{
public:
    HudBackend()
        : screen(swd3e::Display::Width, swd3e::Display::Height),
          codec(this->painter),
          archive(this->codec, this->errors),
          acts(this->errors),
          audio(this->device),
          hud(this->painter, this->archive, this->acts, this->random, this->audio),
          panelFont(this->rasterizer),
          messageFont(this->rasterizer),
          messages(
              this->painter,
              this->archive,
              this->acts,
              this->hud,
              this->state,
              this->panelFont,
              this->messageFont
          ),
          services{
              this->state,
              this->random,
              this->archive,
              this->acts,
              this->painter,
              this->audio,
              this->hud,
              this->messages,
              this->config,
              this->party,
              this->crtRandom,
              this->screenPixels,
              this->errors,
          }

    {
        this->painter.Init(this->screen);
        this->hud.Attach(this->services);
        this->messages.Attach(this->services);
        this->archive.SetGameDirectory(swd3etest::GameDirectory());
        this->acts.SetGameDirectory(swd3etest::GameDirectory());
    }

    void Reset(u32 seed) override
    {
        for (int y = 0; y < swd3e::Display::Height; ++y)
        {
            for (int x = 0; x < swd3e::Display::Width; ++x)
            {
                this->screen.Row(y)[x] = golden::Background(seed, x, y);
            }
        }

        this->painter.SetTarget(this->screen.Pixels());
        this->painter.SetClipClamped(0, 0, swd3e::Display::Width, swd3e::Display::Height);
    }

    void SeedRandom(u32 seed) override
    {
        this->random.Seed(seed);
    }

    int PictureFrame0(int id, int x, int y) override
    {
        return this->hud.DrawPictureFrame0(id, x, y) != 0;
    }

    int Portrait(int index, int x, int y, int dim, bool highlight) override
    {
        this->hud.portraitHighlight[static_cast<std::size_t>(index)] = highlight ? 1 : 0;
        return this->hud.DrawPortrait(static_cast<u16>(index), x, y, dim) != 0;
    }

    int Arrow(int x, int y, int slot) override
    {
        return this->hud.DrawArrow(x, y, slot) != 0;
    }

    int Bar(int id, int frame, int x, int y, int width) override
    {
        return this->hud.DrawBar(id, frame, x, y, width) != 0;
    }

    int PictureFrame(int id, int frame, int x, int y) override
    {
        return this->hud.DrawPictureFrame(id, frame, x, y) != 0;
    }

    int Gauge(int id, int x, int y, int fill) override
    {
        return this->hud.DrawGauge(id, x, y, fill) != 0;
    }

    int GaugeAlt(int id, int x, int y, int fill) override
    {
        return this->hud.DrawGaugeAlt(id, x, y, fill);
    }

    int GaugeWide(int id, int x, int y, int fill) override
    {
        return this->hud.DrawGaugeWide(id, x, y, static_cast<u16>(fill)) != 0;
    }

    int SmallNumber(int font, int value, int x, int y) override
    {
        return this->hud.DrawSmallNumber(font, value, x, y) != 0;
    }

    int Number(int font, int value, int x, int y, int digitMode) override
    {
        this->hud.digitMode = static_cast<u16>(digitMode);
        return this->hud.DrawNumber(static_cast<swd3e::i16>(font), value, x, y);
    }

    int SlotAnimation(int id, int slot, int x, int y) override
    {
        return this->hud.DrawSlotAnimation(id, slot, x, y) != 0;
    }

    int Fill(int x, int y, int width, int height, u32 color) override
    {
        return this->hud.FillColor(x, y, width, height, color) != 0;
    }

    int Effect(int id, int action, int x, int y, int mirror) override
    {
        return this->hud.DrawEffectAt(id, action, x, y, mirror);
    }

    int ResetEffect() override
    {
        return this->hud.ResetEffectAt();
    }

    int Cursor(int id, int x, int y) override
    {
        return this->hud.DrawCursorAnimation(id, x, y) != 0;
    }

    int Digit(int font, int digit, int x, int y, int digitMode) override
    {
        this->hud.digitMode = static_cast<u16>(digitMode);
        return this->hud.DrawDigit(font, digit, x, y);
    }

    int ScrollBar(int id, int x, int y, int rows, int offset, int style, int count, int first) override
    {
        this->hud.scrollCount = count;
        this->hud.scrollFirst = first;
        return this->hud.DrawScrollBar(static_cast<u16>(id), x, y, rows, offset, style) != 0;
    }

    int CoinToss() override
    {
        return this->hud.DrawCoinToss();
    }

    int TimingBar(int x, int y) override
    {
        return this->hud.DrawTimingBar(x, y);
    }

    int TallGauge(int x, int y, int level) override
    {
        return this->hud.DrawTallGauge(x, y, level, kGaugeLevels) != 0;
    }

    int AnimationLoad(int x, int y, int id, int divisor) override
    {
        return this->hud.LoadAnimation(this->animation, 0, x, y, static_cast<u16>(id), static_cast<u16>(divisor));
    }

    int AnimationDraw() override
    {
        return this->hud.DrawAnimation(this->animation);
    }

    int AnimationPlayShifted(int amount) override
    {
        return this->hud.PlayAnimationShifted(this->animation, 0, amount);
    }

    int AnimationFree() override
    {
        return this->hud.FreeAnimation(this->animation);
    }

    std::vector<i32> State() override
    {
        return {
            this->hud.numberX,
            this->hud.numberValue,
            this->hud.scrollStep,
            this->hud.scrollThumbTop,
            this->hud.scrollThumbBottom,
            this->hud.coinFrames,
            static_cast<i32>(this->hud.coinState),
            static_cast<i32>(this->hud.coinDelay),
            this->hud.timingCounter,
            this->hud.timingHit,
        };
    }

    const u16* Screen() const override
    {
        return this->screen.Pixels();
    }

private:
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
    swd3e::BattleAnimation animation;
    swd3e::BattleState state;
    NullRasterizer rasterizer;
    swd3e::TextFont panelFont;
    swd3e::TextFont messageFont;
    swd3e::BattleMessages messages;
    swd3e::Config config;
    swd3e::Party party;
    swd3e::crt::Random crtRandom;
    swd3e::u16* screenPixels = nullptr;
    swd3e::BattleServices services;
};

}  // namespace

TEST_CASE("battle hud: matches the golden results of the legacy helpers")
{
    const std::map<std::string, std::string> golden =
        swd3etest::LoadGolden(SWD3_SOURCE_DIR "/tests/golden/hud_golden.txt");

    if (golden.empty() || swd3etest::ReadGameFile("all_sys.act").empty())
    {
        std::printf("  (game data or golden file not found, skipped)\n");
        return;
    }

    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    HudBackend backend;
    swd3etest::GoldenCheck check(golden);
    golden::RunHud(backend, std::ref(check));
    std::printf("  compared %d scenarios\n", check.compared);
    CHECK_EQ(check.mismatches, 0);
    CHECK_EQ(check.compared, static_cast<int>(golden.size()));
}
