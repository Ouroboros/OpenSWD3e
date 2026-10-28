#include "game/game.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "battle/battle_items.h"
#include "battle/smooth/smooth_draw.h"
#include "battle/smooth/smooth_messages.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "game/screenshot.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "platform/system_info.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

constexpr u16 kBottomBar = 0x2375;  // swd3e
constexpr int kBottomBarY = 0x180;
constexpr int kPanelCursor = 0x2391;
constexpr int kTimerX = 0xA;  // swd3e: one timer
constexpr int kBattleTimer = 1;
constexpr u16 kGrayBandCloses = 2;            // grayBandState once the coin toss is over
constexpr int kCurrentPanelPicture = 0x239F;  // the current role's panel, at its highlight
// 0x004BF15C by place (BattleFrame reads it at 0x004BF10C + 4 * the current unit)
constexpr std::array<int, 4> kCurrentPanelX = {0xEF, 0x4F, 0x18F, 0x18F};
constexpr int kTimerY = 8;
constexpr int kFadeFrames = 8;
constexpr int kQuakePanel = 0x100;
constexpr std::size_t kClearedTurnFrom = 10;  // EndBattle (swd3e): turnTaken[10..19] (0x0057604C, 0x28 bytes)
constexpr std::size_t kClearedTurnUnits = 10;
constexpr std::size_t kClearedFadeUnits = 29;  // fadeActLevels[0..28] (0x00536EAC, 0x1D bytes)
constexpr std::array<i32, 6> kOpeningPlaces = {0x60, 0x60, 0xF0, 0xFA, 0xF2, 0x64};
constexpr u16 kOpeningPanels = 6;
constexpr i32 kAnswersStart = 2;
constexpr i32 kHealPage = 2;
constexpr u16 kNoUnit = 0xFFFF;
constexpr u16 kIntroSound = 0xC0;        // BattleIntro
constexpr u16 kIntroBottomBar = 0x2375;  // swd3e
constexpr int kIntroBottomBarY = 0x180;
constexpr int kIntroPixelBits = 0x10;
constexpr i32 kIntroBufferBytes = 0x96000;
constexpr u16 kIntroSway = 1;
constexpr u16 kIntroZoom = 2;
constexpr int kIntroScaleStart = 9;
constexpr int kIntroZoomGrowth = 0x32;  // the additive zoom of transition 2 (to kIntroZoomEnd)
constexpr int kIntroZoomEnd = 0x672;
constexpr int kIntroTint = -2;  // transition 0: red and green dimmed
constexpr int kIntroAnchorY = 0xF0;
constexpr int kIntroSplitAnchorY = 0xC8;  // transition 2 zooms around (0x140, 0xC8)
constexpr int kIntroShearWidth = 0x2C0;
constexpr int kIntroShearHeight = 0x200;
constexpr int kIntroShearShift = 6;
constexpr int kIntroWave = 0xA;
constexpr int kIntroLastLeft = 0x17C;    // the transitions run while their left edge moves from 0x140 here
constexpr int kIntroSplitStart = 0x225;  // transition 1: the zoom of the battle split in
constexpr int kIntroSplitShift = 8;
constexpr i16 kSameActCause = 0x1D;  // the enrage causes of BattleIntro
constexpr i16 kPartyCause1And2 = 8;
constexpr i16 kPartyCause0And3 = 9;
constexpr int kIntroScaleStep = 0x12;
constexpr int kIntroSwayFrames = 0x21;
constexpr double kIntroPi = 3.140000104904175;            // 0x004AAE00 (3.14f)
constexpr double kIntroSwayRate = 0.030303030303030304;   // 0x004AADE8 (1 / 33)
constexpr double kIntroSplitRate = 0.033333333333333333;  // 0x004AADF8 (1 / 30)
constexpr double kIntroSwayWidth = 256.0;                 // 0x004AADF0
constexpr int kIntroZoomStep = 0x10;
constexpr int kIntroZoomBase = 0x40;
constexpr int kIntroScaleDivisor = 0xA;
constexpr u32 kIntroUnusedDraw = 3;

constexpr std::u8string_view kBattleMusic = u8"music\\Battle01.mp3";  // 0x0045AAD0
constexpr int kBattleMusicId = 1;                                     // the id the battle tune plays as

}  // namespace

// swd3e.exe 0x00465AB0: one frame of every unit in the draw order: a role (units 20 and up, below the role count)
// runs RoleFrame, an enemy below the enemy count EnemyFrame.
void Game::UpdateUnits()
{
    BattleState& battle = this->battleState;
    const u32 units = battle.roleCount + battle.enemyCount;

    for (u32 index = 0; index < units; ++index)
    {
        const i32 unit = battle.drawOrder[index];

        if (unit >= BattleState::FirstRoleUnit)
        {
            if (unit - BattleState::FirstRoleUnit < static_cast<i32>(battle.roleCount))
            {
                this->RoleFrame(unit - BattleState::FirstRoleUnit);
            }
        }
        else if (unit < static_cast<i32>(battle.enemyCount))
        {
            this->EnemyFrame(unit);
        }
    }
}

// swd3e.exe 0x0045B5C0: one frame of the battle; it returns frameResult (1, or the end CheckBattleEnd set), or the
// debug keys' 0 (the frame not drawn, the audio served). The frame's milliseconds are kept; the pointer, the keys and
// the auto battle are handled and the units measured and sorted. The next slot record becomes the acting unit when
// none acts, the battle runs, no menu is open and no undarkening waits (enemyGaugeBoost lets the gauges run while none
// acts and no panel is open); the units slide (never enabled), the background, the units (the acting ones sorted
// around it), the coin toss, the hits, the bottom bar, the current role's panel and cursor, the role panels, the menu,
// the end frame, the messages, the field's lines and heads, the talk, the resistance panel and the timer are drawn;
// the end is checked, a finished field fade fades back from its levels, the frame is shown (shaken while quaking), the
// audio served and the frame saved when asked.
int Game::BattleFrame()
{
    BattleState& battle = this->battleState;
    BattleHud& hud = this->battleHud;
    battle.frameResult = 1;
    battle.frameNow = static_cast<i32>(SystemUptime());
    battle.frameMilliseconds = battle.frameNow - battle.frameLast;
    battle.frameLast = battle.frameNow;

    // The rebuild (deviations.md): a press is a role's own; when another role (or the same one's next turn) is to be
    // given a command, what is held from before counts only once let go and pressed again.
    if (battle.currentUnit != battle.keysUnit)
    {
        if (battle.currentUnit != 0)
        {
            this->input.SpendHeld();
            this->controls.SpendHeld();
        }

        battle.keysUnit = battle.currentUnit;
    }

    this->MouseHover();
    this->BattleKeys();
    this->AutoBattleTurn();
    this->MeasureUnits();
    battle.SortDrawOrder();

    if (this->BattleDebugKeys() == 0)
    {
        this->AudioPump();
        return 0;
    }

    this->drawSurface = this->painter.LockSurface(*this->offscreen);
    this->painter.SetTarget(this->drawSurface);
    this->painter.UnlockSurface(*this->offscreen);

    if (battle.slotRecords[0].unit != -1 && battle.unk577270 == 0 && this->fightTalk.battleRuns == 1 &&
        battle.result == 0 && battle.unk5772C8 == 0)
    {
        BattleSlotRecord acting{
            battle.actingUnit,
            battle.sweepTotal,
            battle.unk53AE78,
            static_cast<u16>(battle.slideBackPicture),
            battle.unk53AE7C,
            battle.unk53AE80,
            battle.wholeSide,
            battle.hitsAtOnce
        };

        this->TakeSlotRecord(acting);
        battle.actingUnit = acting.unit;
        battle.sweepTotal = acting.targetCount;
        battle.unk53AE78 = acting.kind;
        battle.slideBackPicture = static_cast<i16>(acting.unk0A);
        battle.unk53AE7C = acting.unk0C;
        battle.unk53AE80 = acting.unk10;
        battle.wholeSide = acting.unk14;
        battle.hitsAtOnce = acting.unk18;

        if (battle.actingUnit != -1)
        {
            battle.slotRecordFrames = 0;
            battle.unk577270 = 1;
            this->fightTalk.battleActingUnit = battle.actingUnit;
        }
    }

    battle.enemyGaugeBoost = battle.actingUnit == -1 && battle.currentUnit == 0 ? 1 : 0;
    smooth::Scene frameScene;
    this->battleServices.frame = this->smoothMotion || this->battleDeferred ? &frameScene : nullptr;
    this->SlideUnits(battle.slideUnit, battle.slideX, battle.slideTarget);
    this->DrawBattleBackground(battle.shake);

    if (battle.unk577260 == 1 || battle.hitsAllRoles == 1)
    {
        this->SortActingDrawOrder();
    }

    this->UpdateUnits();

    if (battle.coinTossing == 1 && hud.DrawCoinToss() == 1)
    {
        battle.coinTossing = 0;
        battle.grayBandState = kGrayBandCloses;
    }

    this->CheckStoneSides();
    this->ApplyHits();

    if (this->HitFrame() == 1)
    {
        battle.spellStage = static_cast<u16>(battle.spellStage | BattleState::SpellOver);
    }

    hud.DrawPictureFrame0(kBottomBar, 0, kBottomBarY);

    if (battle.currentUnit != 0)
    {
        const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
        hud.DrawPicturePalette(kCurrentPanelPicture, kCurrentPanelX[unit], kBottomBarY);

        if (battle.grayBandShown == 0 && this->battleRoles[unit].Flag26B8() == 0)
        {
            hud.DrawCursorAnimation(kPanelCursor, battle.panelPlaces[2 * unit], battle.panelPlaces[2 * unit + 1]);
        }
    }

    this->DrawRolePanels();
    this->DrawBattleMenu();

    this->BattleEndFrame();
    {
        smooth::Scene scene;
        smooth::MessagesFrame(this->battleMessages).Frame(scene);
        scene.Show(this->battleServices);
    }

    this->fieldView.DrawLines();
    this->fieldView.DrawRoleHeads();
    this->talk.Draw();
    this->battleMessages.DrawResistancePanel();
    this->fieldView.DrawTimer(kTimerX, kTimerY, kBattleTimer);

    if (this->config.debugMode == 1)
    {
        this->DrawBattleDebug();
    }

    this->CheckBattleEnd();
    this->DrawBattlePrompt();

    if (battle.fadingField == 1 && battle.fieldFade.frames <= 0)
    {
        const ColorFade& fade = battle.fieldFade;
        battle.StartFieldFade(
            static_cast<int>(fade.startRed),
            static_cast<int>(fade.startGreen),
            static_cast<int>(fade.startBlue),
            0,
            0,
            0,
            kFadeFrames
        );
        battle.fadingField = 0;
    }

    this->FieldFadeFrame(1);
    this->battleServices.frame = nullptr;

    if (this->smoothMotion)
    {
        this->battleStage.Draw(std::move(frameScene), this->state.pointerX, this->state.pointerY);
    }
    else
    {
        frameScene.DrawAll(this->battleServices);
    }

    const bool smooth = this->smoothMotion;

    if (battle.quaking == 1 || (battle.panelFlags & kQuakePanel) != 0)
    {
        this->QuakeFrame();
    }
    else if (smooth)
    {
        this->CopyToPrimary();
        this->battleFrameShown = true;
    }
    else
    {
        this->ShowFrame();
    }

    this->AudioPump();

    // The original saves the snapshot as c:\snap\<1000 + n>.bmp (0x004BF2FC); the rebuild saves it with the screenshots
    // of the map, under their first free name (deviations.md).
    if (battle.snapNext == 1)
    {
        ++battle.snapNumber;
        const std::u8string name = this->ScreenshotPath();
        const std::span<const u16> pixels(
            this->drawSurface,
            static_cast<std::size_t>(this->painter.ScreenPitch()) * Display::Height
        );
        SaveScreenshot(this->painter, pixels, Display::Width, Display::Height, name);
        battle.snapNext = 0;
    }

    return battle.frameResult;
}

// swd3e.exe 0x00465B00: the battle is over: every enemy and role reset, its surfaces, the background animations, the
// projector, the meter and the background picture freed, the state of the battle cleared field by field (the menu and
// list cursors back to 1, the hit records to -1, unk4A7580..unk4A7594 to their opening places 0x60, 0x60, 0xF0, 0xFA,
// 0xF2, 0x64: the second differs from the exe's 0x180; the first three command names, every listed kind, the turns
// of units 10..19, the fades of the first 29 units), the archives flushed and closed, the sounds stopped, the effects
// off, the messages cleared, the database files closed.
void Game::EndBattle()
{
    BattleState& battle = this->battleState;
    BattleMessages& messages = this->battleMessages;
    BattleServices& services = this->battleServices;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].Reset(services);
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        static_cast<BattleUnit&>(this->battleRoles[index]).Reset(services);
    }

    ReleaseBattleSurfaces(battle);

    for (std::size_t index = 0; index < std::min<std::size_t>(battle.animationCount, BattleFormation::Animations);
         ++index)
    {
        this->battleHud.FreeAnimation(battle.backgroundAnimations[index]);
    }

    battle.projector.Release();
    this->battleHud.ResetMeter();
    battle.background = Picture{};
    battle.roleOrder.fill(0);
    battle.unk524200.fill(0);
    std::fill_n(battle.turnTaken.begin() + kClearedTurnFrom, kClearedTurnUnits, 0);
    battle.unk5240C4 = 0;
    battle.unk537A20.fill(-1);
    battle.enemyOrders.fill(0);
    battle.enemyPlaces.fill(UnitPlacement{});
    battle.rolePlaces.fill(UnitPlacement{});
    battle.slotRecords.fill(BattleSlotRecord{0, 0, 0, 0, 0, 0, 0, 0});
    battle.ClearActing();
    battle.hitActs.fill(ActState{});
    battle.fadeActs.fill(ActState{});
    battle.projector = Projector();
    battle.pointerX = 0;
    battle.pointerY = 0;
    battle.formation = BattleFormation{};
    battle.unk50ED68.fill(0);
    battle.roleListScrolls.fill(0);
    battle.unk5377FC.fill(0);
    battle.unk50D88C.fill(0);
    battle.actionQueue.fill(0);
    battle.commandNames[0] = {};
    battle.commandKinds = {};
    battle.commandNames[1] = {};
    battle.commandNames[2] = {};
    messages.Clear();
    messages.unk5214FC.fill(0);
    battle.hitPointsRolling.fill(0);
    battle.hitPoints.fill(0);
    battle.nameFlashes.fill(0);
    battle.unk52100C.fill(0);
    battle.unk4FF318.fill(0);
    battle.unk53AED8 = 0;
    battle.unk53AEDC = 0;
    battle.unk53AEE0 = 0;
    battle.unk4FF588.fill(0);
    battle.unk5394E8 = ActState{};
    battle.spiritRolling.fill(0);
    battle.spirit.fill(0);
    battle.enemyHitActs.fill(0);
    battle.unk53AEE4 = 0;
    battle.roleListRows.fill(1);
    battle.controlRows = kAnswersStart;
    battle.listRole = kHealPage;
    messages.panelAct.id = BattleMessages::MenuPanelAct;
    this->talk.choice = 0;
    battle.panelFlags = 0;
    battle.unk53BCDC = 0;
    battle.enemyCount = 0;
    battle.roleCount = 0;
    battle.guestCount = 0;
    battle.turnCommand = 0;
    battle.result = 0;
    battle.menuCommand = 1;
    battle.aimTarget = 1;
    battle.actingEnemy = 0;
    battle.orderStep = 1;
    battle.pageListRow = 1;
    battle.roleListRow = 1;
    battle.controlCursor = 1;
    battle.darkenedAt = -1;
    battle.listScroll = 0;
    battle.roleListScroll = 0;
    battle.unk53BD0C = 0;
    battle.specialCursor = 0;
    battle.listCursor = 0;
    battle.menuX = 0;
    battle.menuY = 0;
    battle.unk4A7580 = kOpeningPlaces[0];
    battle.unk53BD4C = 0;
    battle.turnUnit = 0;
    battle.currentUnit = 0;
    battle.heldEnemy = 0;
    battle.unk4A7584 = kOpeningPlaces[1];
    battle.shake = 0;
    battle.unk4A7588 = kOpeningPlaces[2];
    battle.quakePhase = 0;
    battle.unk4A758C = kOpeningPlaces[3];
    battle.unk4A7590 = kOpeningPlaces[4];
    battle.unk4A7594 = kOpeningPlaces[5];
    battle.unk53BF46 = 0;
    battle.unk53BF48 = 0;
    battle.unk53BD74 = 0;
    battle.unk53BD78 = 0;
    battle.sweepTargets = 0;
    battle.unk53BD80 = 0;
    battle.sweepDefeated = 0;
    battle.unk53BD88 = 0;
    std::fill_n(battle.fadeActLevels.begin(), kClearedFadeUnits, i8{0});
    battle.lootIds = {};
    battle.unk520FBC = 0;
    battle.unk520FC0 = 0;
    battle.unk520FC4 = 0;
    battle.unk53BD9C = 0;
    battle.hoverScrollPart = 0;
    battle.battleGold = 0;
    battle.battleExperience = 0;
    battle.battleProficiency = 0;
    battle.unk53BF1C = 0;
    battle.unk53BDAC = 0;
    battle.pendingActionRow = 0;
    battle.pendingAction = 0;
    battle.flashLevel = BattleState::FlashStart;
    battle.defeatedRoles = 0;
    battle.enemiesGone = 0;
    battle.unk53BF02 = 0;
    battle.unk53BF04 = 0;
    battle.unk5771F0 = 0;
    battle.lastRoleHit = kNoUnit;
    battle.lastEnemyHit = kNoUnit;
    battle.lastRoleDefeated = kNoUnit;
    battle.unk4A7626 = kNoUnit;
    battle.unk4FDF7C = 0;
    battle.unk5242F8 = 0;
    battle.unk4A7628 = kOpeningPanels;
    battle.hitDamage = 0;
    battle.lastDamage = 0;
    battle.darkRed = 0;
    battle.darkGreen = 0;
    battle.darkBlue = 0;
    battle.darkCopyTint.fill(-1);
    battle.darkenUnit = -1;
    battle.proficiencyBonus = 0;
    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    battle.listRows = 0;
    battle.autoTarget = -1;
    battle.listedCommands = 0;
    battle.callingEscort = 0;
    battle.escortCount = 0;
    battle.controlledEnemy = kNoUnit;
    battle.anyStateRole = kNoUnit;
    battle.endFrames = 0;
    battle.unk53BDEE = 0;
    battle.fieldFade = ColorFade{};
    messages.craftSucceeded = 0;
    battle.ghostOrdersOpen = 0;
    battle.refineOptionsOpen = 0;
    battle.elementsOpen = 0;
    battle.unk53BF28 = 0;
    battle.unk53BF2A = 0;
    battle.quakeStepFrames = 0;
    battle.unk53C4C0 = 0;
    battle.chargingRole = -1;
    messages.lootCount = 0;
    messages.lootNames = {};
    battle.unitY.fill(0);
    battle.drawOrder.fill(0);
    battle.drawTaken.fill(0);
    messages.lootCounts = {};
    battle.unk5376B8.fill(0);
    battle.hitFading.fill(0);
    battle.unk53BF5C = 0;
    battle.unk53BF60 = 0;
    battle.unk577260 = 0;
    battle.hitsAllRoles = 0;
    battle.unk53BF6C = 0;
    battle.unk53BF70 = 0;
    battle.unk577270 = 0;
    battle.unk53BF78 = 0;
    battle.sweepHit = 0;
    battle.unk53BF80 = 0;
    battle.unk53BF88 = 0;
    battle.gatheringSweep = 0;
    battle.unk53BF90 = 0;
    battle.unk577290 = 0;
    battle.unk577294 = 0;
    battle.unk53BF9C = 0;
    battle.unk53BFA0 = 0;
    battle.unk53BFA4 = 0;
    battle.hitLanded = 0;
    battle.unk53BFAC = 0;
    battle.unk53BFB0 = 0;
    battle.unk53BFB4 = 0;
    battle.unk53BFB8 = 0;
    battle.unk53BFBC = 0;
    battle.unk5772BC = 0;
    battle.unk53BFC4 = 0;
    battle.flashing = 0;
    battle.unk53BFD0 = 0;
    battle.enemyAimsAtEnemies = 0;
    battle.enemyFaded = 0;
    battle.damageShown = 0;
    battle.unk53BFDC = 0;
    battle.endFade = 0;
    battle.unk53BFF0 = 0;
    battle.enemyHitsAll = 0;
    battle.unk5772F8 = 0;
    battle.unk4A763C = 1;
    battle.learnedPending = 0;
    battle.mirrored = 0;
    battle.spellStage = 0;
    battle.victoryWithoutPanel = 0;
    battle.autoBattle = 0;
    battle.unk53C028 = 0;
    battle.darkenByTint = 0;
    this->fightTalk.wonByTalk = 0;
    battle.levelUpPending = 0;
    battle.orderSorted = 0;
    battle.unk57734C = 0;
    this->archive.Flush();
    this->acts.CloseAll();
    this->audio.StopAllSounds();
    this->acts.SetZoomEnabled(0);
    messages.Clear();
    this->database.CloseFiles();
    battle.learnedPending = 0;
    battle.lastLearned = LearnedSkill{};
}

// The battle field of the first frame drawn into the second battle surface (inline in 0x004527E0): the background,
// the units in order, the bottom bar and the role panels on the frame, copied.
void Game::DrawIntroField()
{
    BattleState& battle = this->battleState;
    this->DrawBattleBackground(battle.shake);
    battle.SortDrawOrder();
    this->UpdateUnits();
    this->battleHud.DrawPictureFrame0(kIntroBottomBar, 0, kIntroBottomBarY);
    this->DrawRolePanels();
    CopySurface(*this->offscreen, *battle.surfaces[0]);
}

// A surface packed as a picture (inline in 0x004527E0): its rows (screenPitch apart) copied into a 640 x 480 buffer
// and encoded with the RLE codec.
std::vector<u8> Game::PackSurface(Surface& surface)
{
    this->drawSurface = this->painter.LockSurface(surface);
    this->painter.SetTarget(this->drawSurface);
    this->painter.UnlockSurface(surface);
    std::vector<u16> pixels(static_cast<std::size_t>(Display::Width) * Display::Height);
    const auto pitch = static_cast<std::size_t>(this->painter.ScreenPitch());

    for (std::size_t row = 0; row < static_cast<std::size_t>(Display::Height); ++row)
    {
        std::copy_n(
            this->drawSurface + row * pitch,
            Display::Width,
            pixels.begin() + static_cast<std::ptrdiff_t>(row * Display::Width)
        );
    }

    return this->codec.Encode(pixels.data(), Display::Width, Display::Height, kIntroPixelBits);
}

// The frame target set to the frame surface (inline in 0x004527E0).
void Game::TargetFrame()
{
    this->drawSurface = this->painter.LockSurface(*this->offscreen);
    this->painter.SetTarget(this->drawSurface);
    this->painter.UnlockSurface(*this->offscreen);
}

// The frame cleared (inline in 0x004527E0): 0x96000 bytes from its start.
void Game::ClearFrame()
{
    std::fill_n(this->drawSurface, static_cast<std::size_t>(Display::Width) * Display::Height, u16{0});
}

// swd3e.exe 0x0045AAD0: the opening of battle with transition `style` (kIntroSound): the screen shown is kept as a
// picture and the first battle frame drawn into the battle surfaces. Over 30 frames the old screen is sheared and
// waved away dimmed (0), split sideways (1) or split up and down (2), zoomed; then the battle slides in row by row (0;
// 2 after the old screen zoomed in additively) or is split in (1: by the truncated sine, which keeps the halves
// together, as in the original). The battle music (music\Battle01.mp3) starts unless a battle talk gave one; a
// battle fought past the talk's repeat limit counts once more; enemies of the same act check enrage cause 0x1D for
// each other, and the standing enemies check 8 (roles 1 and 2 in the party) and 9 (roles 0 and 3).
void Game::BattleIntro(u16 style)
{
    BattleState& battle = this->battleState;
    Painter& painter = this->painter;
    DrawOptions& options = painter.Options();
    int swing = 0;
    this->audio.PlaySound(kIntroSound, this->config.soundVolume);
    battle.unk5772BC = 1;
    Surface* primary = this->graphics.GetSurface(Graphics::SurfaceId::Primary);
    Surface& field = *battle.surfaces[0];
    Surface& shown = *battle.surfaces[1];
    CopySurface(*primary, shown);
    battle.unk5771F0 = kIntroBufferBytes;
    const std::vector<u8> oldScreen = this->PackSurface(shown);
    this->TargetFrame();
    this->DrawIntroField();
    std::vector<u8> newScreen;

    if (style != 0)
    {
        newScreen = this->PackSurface(field);
    }

    this->TargetFrame();
    painter.SetSource(oldScreen.data());
    painter.DrawPicture(0, 0, Display::Width, Display::Height, 0);
    int frame = 0;
    int scale = kIntroScaleStart;
    int left = Display::Width / 2;

    auto zoomOf = [](int at)
    {
        return kIntroZoomStep * (kIntroZoomBase - at / kIntroScaleDivisor);
    };

    while (true)
    {
        if (style == 0)
        {
            options.tintRed = kIntroTint;
            options.tintGreen = kIntroTint;
            options.tintBlue = 0;
            painter.SetZoomSheared(
                left,
                kIntroAnchorY,
                left + kIntroShearWidth,
                2 * (kIntroShearHeight - frame),
                frame << kIntroShearShift
            );
            painter.SetWave(kIntroWave);
            painter.DrawPicture(0, 0, Display::Width, Display::Height, Painter::ZoomAddTinted);
            painter.SetWave(0);
        }
        else if (style == kIntroSway)
        {
            const auto sway = static_cast<i32>(
                crt::Ftol(std::sin(static_cast<double>(swing) * kIntroPi * kIntroSwayRate) * kIntroSwayWidth)
            );
            this->ClearFrame();
            painter.SetZoomSheared(Display::Width / 2, kIntroAnchorY, zoomOf(scale), zoomOf(scale), 0);
            painter.DrawPicture(sway, 0, Display::Width, Display::Height, Painter::ZoomCopy);
            painter.DrawPicture(-sway, 0, Display::Width, Display::Height, Painter::ZoomCopy);
            ++swing;
        }
        else if (style == kIntroZoom)
        {
            this->ClearFrame();
            const auto sway = static_cast<i32>(
                crt::Ftol(std::sin(static_cast<double>(swing) * kIntroPi * kIntroSplitRate) * kIntroSwayWidth)
            );
            painter.SetZoom(Display::Width / 2, kIntroSplitAnchorY, zoomOf(scale), zoomOf(scale));
            painter.DrawPicture(0, -sway, Display::Width, Display::Height, Painter::ZoomCopy);
            painter.DrawPicture(0, sway, Display::Width, Display::Height, Painter::ZoomCopy);
            ++swing;
        }

        scale += kIntroScaleStep;
        ++frame;
        left += 2;
        this->ShowFrame();

        if (left >= kIntroLastLeft)
        {
            break;
        }
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    this->TargetFrame();

    auto slideIn = [this, primary, &field, &shown]
    {
        CopySurface(*primary, shown);
        this->DrawIntroField();
        CopySurface(shown, *primary);
        this->graphics.Present();
        this->random.Below(kIntroUnusedDraw);
        this->SlideBattleIn(field, shown);
    };

    if (style == 0)
    {
        slideIn();
    }
    else if (style == kIntroZoom)
    {
        for (int zoom = 0; zoom < kIntroZoomEnd; zoom += kIntroZoomGrowth)
        {
            painter.SetZoom(Display::Width / 2, kIntroSplitAnchorY, zoom, zoom);
            painter.DrawPicture(0, 0, Display::Width, Display::Height, Painter::ZoomAddTinted);
            this->ShowFrame();
        }

        slideIn();
    }
    else if (style == kIntroSway)
    {
        int fade = kIntroSplitStart;
        this->ClearFrame();
        painter.SetSource(newScreen.data());

        for (int step = 0; step < kIntroSwayFrames; ++step)
        {
            // The sine is truncated before it is scaled (as in the original): the halves stay together.
            const auto sway =
                static_cast<i32>(crt::Ftol(std::sin(static_cast<double>(step) * kIntroPi * kIntroSwayRate)))
                << kIntroSplitShift;
            painter.SetZoomSheared(Display::Width / 2, kIntroAnchorY, zoomOf(fade), zoomOf(fade), 0);
            this->ClearFrame();
            painter.DrawPicture(-sway, 0, Display::Width, Display::Height, Painter::ZoomCopy);
            painter.DrawPicture(sway, 0, Display::Width, Display::Height, Painter::ZoomCopy);
            fade -= kIntroScaleStep;
            this->ShowFrame();
        }
    }

    battle.unk5772BC = 0;
    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    this->StartBattleMusic();
    battle.musicLevel = this->config.musicVolume;
    battle.panelFlags &= ~BattleState::MusicGiven;

    // Battle numbers past the table read and write the variables after it in the original (deviations.md).
    const auto repeat = static_cast<std::size_t>(battle.battle - 1);

    if (this->fightTalk.repeatLimit != 0 && repeat < battle.battleRepeats.size() &&
        battle.battleRepeats[repeat] <= this->fightTalk.repeatLimit)
    {
        ++battle.battleRepeats[repeat];
    }

    // Past the 20 enemy units the original reads beyond its unit array (deviations.md): those are left out.
    const u32 units = std::min<u32>(battle.enemyCount, BattleState::Enemies);

    for (u32 first = 0; first < units; ++first)
    {
        for (u32 second = 0; second < units; ++second)
        {
            if (first != second && battle.enemyPlaces[first].actId == battle.enemyPlaces[second].actId)
            {
                this->battleEnemies[first].EnrageCheck(this->battleServices, kSameActCause, 0);
                this->battleEnemies[second].EnrageCheck(this->battleServices, kSameActCause, 0);
            }
        }
    }

    auto enrageStanding = [this, units](i16 cause)
    {
        for (u32 enemy = 0; enemy < units; ++enemy)
        {
            if (!this->battleEnemies[enemy].IsDefeated())
            {
                this->battleEnemies[enemy].EnrageCheck(this->battleServices, cause, 0);
            }
        }
    };

    if (this->state.GameFlag(GameState::InParty + 1) && this->state.GameFlag(GameState::InParty + 2))
    {
        enrageStanding(kPartyCause1And2);
    }

    if (this->state.GameFlag(GameState::InParty) && this->state.GameFlag(GameState::InParty + 3))
    {
        enrageStanding(kPartyCause0And3);
    }

    this->TargetFrame();
}

// The battle music of BattleIntro (inline in 0x0045AAD0): unless a battle talk gave it (MusicGiven, then cleared by
// BattleIntro), music\Battle01.mp3 becomes the third list of the music queue and plays once the playing piece has
// stopped, at the options' volume.
void Game::StartBattleMusic()
{
    BattleState& battle = this->battleState;

    if ((battle.panelFlags & BattleState::MusicGiven) != 0)
    {
        return;
    }

    MusicQueue& queue = this->audio.Queue();
    battle.battleMusic = this->config.installDirectory + std::u8string(kBattleMusic);
    queue.SetSlot(battle.battleMusic, kBattleMusicId, 0, MusicQueue::ThirdList);
    queue.Switch(MusicQueue::ThirdList, MusicQueue::RepeatList, 0);
    queue.SetVolume(MusicQueue::MaxVolume * this->config.musicVolume / Config::MaxVolume);
}
}  // namespace swd3e
