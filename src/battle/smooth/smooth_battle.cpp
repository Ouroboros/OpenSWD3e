#include "battle/smooth/smooth_battle.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

#include "battle/battle_frame_constants.h"
#include "battle/smooth/smooth_hud.h"
#include "battle/smooth/smooth_messages.h"
#include "battle/smooth/smooth_unit.h"
#include "crt/msvc.h"
#include "draw/draw_placer.h"
#include "draw/paint_record.h"
#include "game/game.h"
#include "pic/picture_ops.h"
#include "platform/surface.h"
#include "ui/ui_draw.h"

namespace swd3e::smooth
{

namespace
{

// swd3e: the panels by place (the three roles of the party; the last two share theirs).
constexpr std::array<int, Party::Roles> kGaugeX = {0x14C, 0xAC, 0x1EC, 0x1EC};     // 0x004BF12C
constexpr std::array<int, Party::Roles> kHighlightX = {0xEF, 0x4F, 0x18F, 0x18F};  // 0x004BF15C
constexpr double kBarPixels = 48.0;  // 0x004AADE0 (a float): the pixels of a full panel gauge
constexpr int kHighlightLeft = 4;
constexpr int kHighlightTop = 0x184;
constexpr int kHighlightWidth = 0x9C;
constexpr int kHighlightHeight = 0x5C;
constexpr int kGlowFrames = 3;
constexpr i8 kGlowTop = 8;
constexpr u8 kGlowFalling = 0x80;
constexpr u8 kGlowMask = 0x7F;
constexpr int kSecondGaugeY = 0x177;  // the second gauge of the debug overlay
constexpr int kSecondGaugeGreen = 0x18;
constexpr int kGaugeHeight = 3;
constexpr int kNameY = 0x18B;
constexpr int kNameGauge = 0x2380;
constexpr int kNameStyle = 0x10;
constexpr Pixel kNameColor = 0xFFFF;
constexpr Pixel kDefeatedColor = 0xF000;
constexpr Pixel kFlashColor = 0xF3F0;
constexpr int kFlashPeriod = 4;
constexpr int kPortraitY = 0x1CC;
constexpr int kIconsY = 0x1CE;
constexpr int kBlockGauge = 0x2385;
constexpr int kBlockY = 0x1D0;
constexpr i32 kArrowBlock = 0x30;   // the block value of a full fury: the arrow is shown
constexpr int kStatFrame = 0x239E;  // the frames of the three gauges (0..2)
constexpr int kFrameLeft = 0x12;
constexpr int kNumberX = 0x26;
constexpr int kHitPointPicture = 0x236D;
constexpr int kHitPointFrameY = 0x19F;
constexpr int kHitPointGaugeY = 0x1A7;
constexpr int kHitPointNumberY = 0x19A;
constexpr int kSpiritPicture = 0x236E;
constexpr int kSpiritFrameY = 0x1B0;
constexpr int kSpiritGaugeY = 0x1B8;
constexpr int kSpiritNumberY = 0x1AB;
constexpr int kStaminaPicture = 0x236F;
constexpr int kStaminaFrameY = 0x1C1;
constexpr int kStaminaGaugeY = 0x1C9;
constexpr int kStaminaNumberY = 0x1BC;
constexpr int kNormalDigits = 0x2377;
constexpr int kLowDigits = 0x2378;       // the hit points at most a third of the maximum
constexpr int kCriticalDigits = 0x2379;  // at most kCriticalHitPoints
constexpr i32 kCriticalHitPoints = 0xA;
constexpr i32 kLowShare = 3;
constexpr i32 kGaugeDownNear = 1;
constexpr i32 kGaugeDownFar = 3;
constexpr i32 kBlockDownNear = 2;
constexpr i32 kBlockDownFar = 5;
// The talisman ghost's panel (swd3e).
constexpr int kGhostPortraitShift = 3;  // its portrait follows the party's by its kind
constexpr int kGhostPortraitX = 0x258;
constexpr int kGhostPortraitY = 0x1AE;
constexpr int kGhostGaugeX = 0x23F;
constexpr int kGhostNumberX = 0x265;
constexpr int kGhostTurnGaugeY = 0x1BB;
constexpr int kGhostTurnBlue = 0x18;
constexpr int kGhostOrderX = 0x238;
constexpr int kGhostOrderY = 0x1C2;
constexpr int kMeterX = 0x14;  // DrawMeter
constexpr int kMeterY = 0x186;
constexpr std::u8string_view kAutoBattle = u8"自動攻擊";  // 0x004BF3B8
constexpr int kAutoTargetX = 0x44;
constexpr int kAutoSlide = 3;
constexpr int kAutoLeft = 0x3A;
constexpr int kAutoY = 0x162;
constexpr int kAutoWidth = 0x46;
constexpr int kAutoHeight = 0x18;

// The gauges move a step a frame towards their value: a large step when far.
constexpr i32 kNearGauge = 0xA;
constexpr i32 kGaugeUpNear = 1;
constexpr i32 kGaugeUpFar = 3;

constexpr i32 kRollingBit = 0x8000000;  // the original tests bit 27, not the sign, before negating a change
constexpr i32 kRollingShare = 0xA;      // a changed number rolls in a tenth of the change a frame

// A NUL-terminated name kept in its array.
template <std::size_t Size>
std::string_view NameOf(const std::array<char, Size>& name)
{
    return std::string_view(name.data(), strnlen(name.data(), name.size()));
}

// The change of a number shown rolling: the remaining change shrinks by `step` a frame. `floor` is where a falling
// change stops (stamina stops at 1). A value at or below 0 clears it.
template <typename Value>
void RollNumber(Value& value, i32& last, i32& rolling, i32& step, i32 floor)
{
    if (last != value)
    {
        i32 change = last - value;
        rolling = change;

        if ((change & kRollingBit) != 0)
        {
            change = ~change;
        }

        step = change / kRollingShare + 1;
        last = value;
    }

    if (rolling > 0)
    {
        rolling -= step;

        if (rolling < floor)
        {
            rolling = 0;
        }

        if (value <= 0)
        {
            value = 0;
            rolling = 0;
            last = 0;
        }
    }
    else if (rolling < 0)
    {
        rolling += step;

        if (rolling > 0)
        {
            rolling = 0;
        }
    }
}

}  // namespace

BattleFrame::BattleFrame(Game& game)
    : game(game)
{
}

// A panel gauge (inline in Game::DrawRolePanels): drawn at its shown value, which moves towards `target`; when it has
// arrived the gauge takes the new value `next()`.
template <typename Next>
void BattleFrame::StepPanelGaugeAdvance(
    i32& shown,
    i32& target,
    Next next,
    int picture,
    int x,
    int y,
    i32 downNear,
    i32 downFar,
    Scene& scene
)
{
    HudFrame hud(this->game.battleHud);

    if (shown == target)
    {
        target = next();
        hud.DrawGaugeAdvance(picture, x, y, shown, scene);
    }
    else if (shown < target)
    {
        hud.DrawGaugeAltAdvance(picture, x, y, shown, scene);
        shown += target - shown <= kNearGauge ? kGaugeUpNear : kGaugeUpFar;
    }
    else
    {
        hud.DrawGaugeAltAdvance(picture, x, y, shown, scene);
        shown -= shown - target <= kNearGauge ? downNear : downFar;
    }
}

// What Game::DrawBattleBackground decides of the frame; the draws into `scene`.
void BattleFrame::DrawBattleBackgroundAdvance(int shake, Scene& scene)
{
    Game& game = this->game;
    BattleServices& services = game.battleServices;
    ScenePainter painter(services, scene);
    BattleState& battle = game.battleState;
    Picture& background = battle.background;
    const std::span<u16> field(game.drawSurface, kFieldPixels);
    // The original runs over animationCount slots of the eight; the rebuild stops at the eighth (deviations.md).
    const std::size_t animations = std::min<std::size_t>(battle.animationCount, BattleFormation::Animations);

    auto drawBackground = [&painter, &background](u32 mode)
    {
        painter.SetSource(background);
        painter.DrawPicture(0, 0, background.width, background.height, mode);
    };

    auto drawAnimations = [&game, &battle, &scene, animations]()
    {
        for (std::size_t index = 0; index < animations; ++index)
        {
            HudFrame(game.battleHud).DrawAnimationAdvance(battle.backgroundAnimations[index], scene);
        }
    };

    auto shakeAnimations = [&game, &battle, &scene, animations, shake]()
    {
        game.ShakeBattleBackground(shake);
        game.backgroundShifted += shake;

        for (std::size_t index = 0; index < animations; ++index)
        {
            HudFrame(game.battleHud).PlayAnimationShiftedAdvance(battle.backgroundAnimations[index], 0, shake, scene);
        }
    };

    // The rebuild's smooth battle: the background, shifted round by each shake (ShakeBattleBackground), stands at the
    // shifts so far, so that between game frames it moves on from where it was (the same draw whether the frame shakes
    // it or not); its copies a width either side (off the screen while it stands) fill what it leaves, as the shift
    // round does.
    auto standBackground = [&game, &battle, &background, &scene]()
    {
        const auto shifted = std::make_shared<const Picture>(background);
        const int stand = game.backgroundShifted;
        const void* who = &battle.background;
        scene.Add(
            [shifted, stand, who](BattleServices& drawing)
            {
                const DrawPlacer::Holding holding(stand, 0, who);
                Painter& drawer = drawing.painter;

                for (const int across : {0, -1, 1})
                {
                    drawer.Options() = DrawOptions{};
                    drawer.SetSource(shifted->data.data());
                    drawer.DrawPicture(across * shifted->width, 0, shifted->width, shifted->height, Painter::Copy);
                }
            }
        );
    };

    painter.SetSource(background);
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);

    if (battle.unk577290 == 0 && battle.unk577294 == 0)
    {
        if (shake != 0)
        {
            // The background is shifted, drawn, then each animation shifted and drawn in turn.
            game.ShakeBattleBackground(shake);
            game.backgroundShifted += shake;
            standBackground();

            for (std::size_t index = 0; index < animations; ++index)
            {
                HudFrame(game.battleHud)
                    .PlayAnimationShiftedAdvance(battle.backgroundAnimations[index], 0, shake, scene);
                HudFrame(game.battleHud).DrawAnimationAdvance(battle.backgroundAnimations[index], scene);
            }
        }
        else
        {
            standBackground();
            drawAnimations();

            if (battle.grayBandShown == 1)
            {
                u16 band = battle.grayBand;

                if (band < BattleState::BandFull)
                {
                    band = band < BattleState::BandFastFrom ? static_cast<u16>(band << 1)
                                                            : static_cast<u16>(band + BattleState::BandStep);
                    battle.grayBand = band;
                }

                painter.SetClipClamped(0, BattleState::BandRow - band, Display::Width, BattleState::BandRow);
                drawBackground(Painter::GrayUnder);
                painter.SetClipClamped(0, BattleState::BandRow, Display::Width, battle.grayBand + BattleState::BandRow);
                drawBackground(Painter::GrayUnder);
            }
        }

        battle.shake = 0;

        if (battle.flashing == 1)
        {
            DrawOptions& options = painter.Options();
            const int level = battle.flashLevel;
            options.tintRed = level;
            options.tintGreen = level;
            options.tintBlue = level;
            painter.ShiftRed(field, level);
            painter.ShiftGreenPairs(field, options.tintGreen);
            painter.ShiftBluePairs(field, options.tintBlue);
            battle.flashLevel = static_cast<i8>(battle.flashLevel - BattleState::FlashStep);

            if (battle.flashLevel == 0)
            {
                battle.flashLevel = BattleState::FlashStart;
                battle.flashing = 0;
            }
        }
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);

    if ((battle.unk577290 == 1 || battle.unk577294 == 1) && battle.undarkening == 0)
    {
        if (battle.darkenByTint != 0)
        {
            if (shake != 0)
            {
                shakeAnimations();
                battle.shake = 0;
            }

            drawBackground(Painter::Copy);
            drawAnimations();

            // The tint shown moves a step a frame down to the darkening's tint.
            if (battle.darkRedShown > battle.darkRed)
            {
                --battle.darkRedShown;
            }

            if (battle.darkGreenShown > battle.darkGreen)
            {
                --battle.darkGreenShown;
            }

            if (battle.darkBlueShown > battle.darkBlue)
            {
                --battle.darkBlueShown;
            }

            painter.TintPixels(field, battle.darkRedShown, battle.darkGreenShown, battle.darkBlueShown);
        }
        else if (battle.darkLevel >= 1)
        {
            painter.CopyScreen(*battle.surfaces[static_cast<std::size_t>(battle.darkLevel - 1)], *game.offscreen);
        }

        i32 frames = battle.darkFrames;

        if (frames > BattleState::DarkLevelFrames)
        {
            frames = 0;
            ++battle.darkLevel;

            if (battle.darkLevel > BattleState::MaxDarkLevel)
            {
                battle.darkLevel = BattleState::MaxDarkLevel;
            }
        }

        battle.darkFrames = frames + 1;
    }

    if (battle.undarkening != 1 || battle.fadingField != 0 || battle.unk5772C8 != 0)
    {
        return;
    }

    if (shake != 0)
    {
        shakeAnimations();
        battle.shake = 0;
    }

    drawBackground(Painter::Copy);

    if (battle.darkRedShown != 0 && battle.darkGreenShown != 0 && battle.darkBlueShown != 0)
    {
        --battle.darkLevel;

        if (battle.darkenedAt == -1 || battle.darkenByTint != 0)
        {
            // The tint shown moves a step a frame back up to 0.
            if (battle.darkRedShown < 0)
            {
                ++battle.darkRedShown;
            }

            if (battle.darkGreenShown < 0)
            {
                ++battle.darkGreenShown;
            }

            if (battle.darkBlueShown < 0)
            {
                ++battle.darkBlueShown;
            }

            painter.TintPixels(field, battle.darkRedShown, battle.darkGreenShown, battle.darkBlueShown);
        }
        else if (battle.darkLevel >= 1)
        {
            // Level 0 reads the null pointer before the copies (0x005379F0): the blit fails and nothing is drawn.
            painter.CopyScreen(*battle.surfaces[static_cast<std::size_t>(battle.darkLevel - 1)], *game.offscreen);
        }

        return;
    }

    battle.darkRed = 0;
    battle.darkGreen = 0;
    battle.darkBlue = 0;
    battle.darkRedShown = 0;
    battle.darkGreenShown = 0;
    battle.darkBlueShown = 0;
    battle.darkLevel = 0;
    battle.darkenUnit = -1;
    battle.unk577294 = 0;
    battle.unk577290 = 0;
    battle.darkenByTint = 0;
    battle.undarkening = 0;
}

// What Game::DrawRolePanels decides of the frame; the draws into `scene`.
void BattleFrame::DrawRolePanelsAdvance(Scene& scene)
{
    Game& game = this->game;
    BattleState& battle = game.battleState;
    // The panels stay where they are (a number that changes is not taken for another); the automatic battle's box
    // slides in.
    std::optional<Scene::Still> still(std::in_place, scene);
    SceneFont font(game.mediumFont, scene);
    HudFrame hud(game.battleHud);
    font.SetShadow(0);
    font.SetBackground(TextFont::NoBackground);

    for (int index = 0; index < static_cast<int>(battle.roleCount); ++index)
    {
        const auto slot = static_cast<std::size_t>(index);
        BattleRole& unit = game.battleRoles[slot];

        if (unit.AutoControl() == 1 || unit.GuestControl() == 1 || unit.IsGuarding())
        {
            continue;
        }

        if (unit.Ghost() == 1)
        {
            const i32 kind = game.party.variables[Party::GhostVariable];
            hud.DrawPortraitAdvance(
                static_cast<u16>(kind + kGhostPortraitShift),
                kGhostPortraitX,
                kGhostPortraitY,
                0,
                scene
            );
            u16 maximum = 0;
            u16 value = 0;
            unit.GetSpirit(maximum, value);
            const auto spirit = static_cast<i16>(value);
            // Both words are sign-extended before the division (0x00464302).
            const auto bar = static_cast<int>(
                crt::Ftol(static_cast<double>(spirit) / static_cast<double>(static_cast<i16>(maximum)) * kBarPixels)
            );
            hud.DrawGaugeAdvance(kSpiritPicture, kGhostGaugeX, kSpiritGaugeY, bar, scene);
            hud.DrawSmallNumberAdvance(kNormalDigits, spirit, kGhostNumberX, kSpiritNumberY, scene);
            const u32 color = game.painter.MakeColor(0, 0, kGhostTurnBlue);
            const auto gauge = static_cast<u16>(unit.GaugeLength(game.battleServices));
            hud.FillColor(kGhostGaugeX, kGhostTurnGaugeY, gauge + 1, kGaugeHeight, color, scene);
            const std::string order(
                Big5(BattleState::GhostOrderNames[static_cast<std::size_t>(battle.ghostOrderRow - 1)])
            );
            font.DrawText(game.drawSurface, kGhostOrderX, kGhostOrderY, order, battle.lightColor, kNameStyle);
            continue;
        }

        const int gaugeX = kGaugeX[slot];
        const int portraitX = BattleState::PortraitX[slot];
        const int nameX = BattleState::NameX[slot];
        const bool current = battle.currentUnit - BattleState::FirstRoleUnit == index;

        if (current && battle.panelGrown[slot] == 1)
        {
            const int level = battle.panelGlow & kGlowMask;
            Box(game.painter,
                kHighlightX[slot] + kHighlightLeft,
                kHighlightTop,
                kHighlightWidth,
                kHighlightHeight,
                level,
                level,
                level,
                FillMode::Tint,
                scene);

            if (++battle.glowFrames == kGlowFrames)
            {
                battle.glowFrames = 0;
                u8 glow = battle.panelGlow;

                if ((glow & kGlowFalling) != 0)
                {
                    --glow;

                    if ((glow & kGlowMask) == 0)
                    {
                        glow = 0;
                    }
                }

                if ((glow & kGlowFalling) == 0)
                {
                    ++glow;

                    if (static_cast<i8>(glow) > kGlowTop)
                    {
                        glow |= kGlowFalling;
                    }
                }

                battle.panelGlow = glow;
            }
        }

        if (battle.unk537A18 == 1)
        {
            const auto second = static_cast<u16>(unit.SecondGaugeLength());

            if (second != 0)
            {
                const u32 color = game.painter.MakeColor(0, kSecondGaugeGreen, 0);
                hud.FillColor(nameX, kSecondGaugeY, second, kGaugeHeight, color, scene);
            }
        }

        const auto partyRole = static_cast<std::size_t>(battle.partyRoles[slot]);
        const std::string_view name = NameOf(battle.roleNames[partyRole]);

        if (!unit.IsDefeated())
        {
            const auto gauge = static_cast<u16>(unit.GaugeLength(game.battleServices));

            if (unit.unk2FF0 == 0)
            {
                hud.DrawGaugeWideAdvance(kNameGauge, nameX, kNameY, gauge, scene);
            }
        }

        if (unit.IsDefeated())
        {
            font.DrawText(game.drawSurface, nameX, kNameY, name, kDefeatedColor, kNameStyle);
        }

        if (unit.unk2FF0 == 1 && battle.panelGrown[slot] == 1)
        {
            if (current)
            {
                font.DrawText(game.drawSurface, nameX, kNameY, name, kNameColor, kNameStyle);
                const Pixel color = battle.nameFlashes[slot] % kFlashPeriod != 0 ? kFlashColor : kNameColor;
                font.DrawText(game.drawSurface, nameX, kNameY, name, color, kNameStyle);
                ++battle.nameFlashes[slot];
            }
            else
            {
                font.DrawText(game.drawSurface, nameX, kNameY, name, kNameColor, kNameStyle);
            }
        }

        hud.DrawPortraitAdvance(static_cast<u16>(partyRole), portraitX, kPortraitY, unit.IsDefeated() ? 1 : 0, scene);
        UnitFrame(unit).DrawEffectIconsAdvance(game.battleServices, gaugeX, kIconsY, scene);
        this->StepPanelGaugeAdvance(
            battle.blockValuesShown[slot],
            battle.blockValues[slot],
            [&unit]
            {
                return *unit.roleBlock;
            },
            kBlockGauge,
            portraitX,
            kBlockY,
            kBlockDownNear,
            kBlockDownFar,
            scene
        );

        i32 hitPoints = 0;
        i32 maximum = 0;
        unit.GetHitPoints(hitPoints, maximum);
        float ratio = static_cast<float>(static_cast<double>(hitPoints) / static_cast<double>(maximum) * kBarPixels);
        RollNumber(hitPoints, battle.hitPoints[slot], battle.hitPointsRolling[slot], battle.hitPointSteps[slot], 0);
        hud.DrawPictureFrameAdvance(kStatFrame, 0, gaugeX - kFrameLeft, kHitPointFrameY, scene);
        this->StepPanelGaugeAdvance(
            battle.hitPointBarsShown[slot],
            battle.hitPointBars[slot],
            [ratio]
            {
                return static_cast<i32>(crt::Ftol(ratio));
            },
            kHitPointPicture,
            gaugeX,
            kHitPointGaugeY,
            kGaugeDownNear,
            kGaugeDownFar,
            scene
        );
        const int digits = hitPoints > kCriticalHitPoints
                               ? (hitPoints > maximum / kLowShare ? kNormalDigits : kLowDigits)
                               : kCriticalDigits;
        hud.DrawSmallNumberAdvance(
            digits,
            battle.hitPointsRolling[slot] + hitPoints,
            gaugeX + kNumberX,
            kHitPointNumberY,
            scene
        );

        u16 spiritMaximum = 0;
        u16 spiritValue = 0;
        unit.GetSpirit(spiritMaximum, spiritValue);
        i16 spirit = static_cast<i16>(spiritValue);
        ratio = static_cast<float>(
            static_cast<double>(spirit) / static_cast<double>(static_cast<i16>(spiritMaximum)) * kBarPixels
        );
        RollNumber(spirit, battle.spirit[slot], battle.spiritRolling[slot], battle.spiritSteps[slot], 0);
        hud.DrawPictureFrameAdvance(kStatFrame, 1, gaugeX - kFrameLeft, kSpiritFrameY, scene);
        this->StepPanelGaugeAdvance(
            battle.spiritBarsShown[slot],
            battle.spiritBars[slot],
            [ratio]
            {
                return static_cast<i32>(crt::Ftol(ratio));
            },
            kSpiritPicture,
            gaugeX,
            kSpiritGaugeY,
            kGaugeDownNear,
            kGaugeDownFar,
            scene
        );
        hud.DrawSmallNumberAdvance(
            kNormalDigits,
            battle.spiritRolling[slot] + spirit,
            gaugeX + kNumberX,
            kSpiritNumberY,
            scene
        );

        u16 staminaMaximum = 0;
        u16 staminaValue = 0;
        unit.GetStamina(staminaMaximum, staminaValue);
        i16 stamina = static_cast<i16>(staminaValue);
        ratio = static_cast<float>(
            static_cast<double>(stamina) / static_cast<double>(static_cast<i16>(staminaMaximum)) * kBarPixels
        );
        RollNumber(stamina, battle.stamina[slot], battle.staminaRolling[slot], battle.staminaSteps[slot], 1);
        hud.DrawPictureFrameAdvance(kStatFrame, 2, gaugeX - kFrameLeft, kStaminaFrameY, scene);
        this->StepPanelGaugeAdvance(
            battle.staminaBarsShown[slot],
            battle.staminaBars[slot],
            [ratio]
            {
                return static_cast<i32>(crt::Ftol(ratio));
            },
            kStaminaPicture,
            gaugeX,
            kStaminaGaugeY,
            kGaugeDownNear,
            kGaugeDownFar,
            scene
        );
        hud.DrawSmallNumberAdvance(
            kNormalDigits,
            battle.staminaRolling[slot] + stamina,
            gaugeX + kNumberX,
            kStaminaNumberY,
            scene
        );

        if (unit.unk2FF0 == 1 && unit.PartyMember() == 1 && battle.panelGrown[slot] == 0)
        {
            battle.panelGrown[slot] =
                static_cast<u8>(game.battleHud.DrawGrowing(index, kNameGauge, nameX, kNameY, scene));
        }

        if (*unit.roleBlock == kArrowBlock && !unit.IsDefeated())
        {
            hud.DrawArrowAdvance(portraitX, kBlockY, index, scene);
        }
    }

    game.battleHud.DrawMeter(kMeterX, kMeterY, scene);
    still.reset();

    if (battle.autoBattle == 1)
    {
        const i32 step = (kAutoTargetX - battle.autoBoxX) / kAutoSlide;
        battle.autoBoxX += step;
        battle.autoBoxStep = step;
        MessagesFrame(game.battleMessages)
            .DrawMessageBoxAdvance(
                battle.autoBoxX - kAutoLeft,
                kAutoY,
                kAutoWidth,
                kAutoHeight,
                0,
                0,
                kAutoBattle,
                scene
            );
    }
}

}  // namespace swd3e::smooth
