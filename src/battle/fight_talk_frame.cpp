#include "game/game.h"

#include <algorithm>
#include <cstring>

#include "core/error_box.h"
#include "core/fault.h"
#include "core/format.h"
#include "core/spline.h"
#include "core/words.h"

namespace swd3e
{

namespace
{

constexpr int kBattleMusicId = 1;  // the id the battle tune plays as (MusicQueue::SetSlot)
constexpr u16 kTalkEnd = 0xFFFF;   // FightTalkFrame: the commands of the battle talk
constexpr u16 kTalkWait = 1;
constexpr u16 kTalkWindow = 2;
constexpr u16 kTalkStep = 3;
constexpr u16 kTalkSetUnk4A7B54 = 4;
constexpr u16 kTalkMove = 5;
constexpr u16 kTalkPause = 6;
constexpr u16 kTalkHitPoints = 8;
constexpr u16 kTalkOrder = 9;
constexpr u16 kCountdown = 0x8000;  // FightTalk::waitCount and unk53CCD4: counting
constexpr u16 kCountMask = 0x7FFF;
constexpr u16 kWindowShown = 0x8000;  // the high word of FightTalk::windowFlags
constexpr int kTextEndSearch = 0xFF;
constexpr char kTextEscape = '%';
constexpr char kTextEnd = 'Q';
constexpr u32 kNoTimeout = 0xFFFF;
constexpr u16 kWindowSpeed = 2;
constexpr i16 kTalkAction = 0x24;
constexpr i32 kLastEnemy = BattleState::Enemies - 1;  // swd3e: the units below 0x14 are enemies
constexpr i32 kNoActingUnit = -1;                     // BattleState::actingUnit: none
constexpr u16 kOrderForced = 0x8000;                  // BattleState::enemyOrders
constexpr int kEnemySlot = 2;
constexpr int kHangDraws = 0x10000;
constexpr std::size_t kTalkEnemies = 10;  // the enemies the original walks once a talk window closes
constexpr u16 kFixedSpeaker = 1000;       // command 2: a speaker from 1000 talks from a fixed place
constexpr u16 kFixedSpeakerX = 0x1EA;
constexpr u16 kFixedSpeakerY = 0xB4;
constexpr u16 kRoleLineShiftX = 0x3C;  // command 0x3B: the one-line window of a role stands left of and above it
constexpr u16 kRoleLineShiftY = 0x14;
constexpr u32 kLowWordMask = 0xFFFF;
constexpr int kHighWordShift = 16;
constexpr int kEscapeCommand = 0x11;
constexpr int kAimSelfCommand = 0xB;
constexpr int kHoldCommand = 0xC;
constexpr i32 kLastPlayState = 0x61;
constexpr i32 kEndCheckState = 0x63;
constexpr i32 kEndLootState = 0x66;
constexpr u16 kListEnd = 0xFFFF;
constexpr std::size_t kListTail = 4;
constexpr u16 kOrderSpell = 0x4000;
constexpr i32 kSpellOrder = 2;
constexpr u16 kTalkDefeat = 0xA;
constexpr u16 kTalkEscape = 0xB;
constexpr u16 kTalkWin = 0xC;
constexpr u16 kTalkLift = 0xD;
constexpr u16 kTalkAllDone = 0xE;
constexpr u16 kTalkSetMarks = 0xF;
constexpr u16 kTalkRepeat = 0x10;
constexpr u16 kTalkSkip = 0x11;
constexpr u16 kTalkClearMarks = 0x12;
constexpr u16 kTalkRandomJump = 0x13;
constexpr u16 kTalkFace = 0x14;
constexpr u16 kTalkAimSelf = 0x15;
constexpr u16 kTalkShift = 0x16;
constexpr u16 kTalkSkill = 0x17;
constexpr u16 kTalkNextBattle = 0x18;
constexpr u16 kTalkLoot = 0x19;
constexpr u16 kTalkHold = 0x1A;
constexpr u16 kTalkSetAction = 0x1B;
constexpr u16 kTalkVitals = 0x1C;
constexpr u16 kTalkStandAside = 0x1D;
constexpr u16 kTalkSound = 0x1E;
constexpr u16 kTalkChoice = 0x1F;
constexpr u16 kTalkHeadIn = 0x21;
constexpr u16 kTalkHeadOut = 0x22;
constexpr u16 kTalkFlagsA0 = 0x23;
constexpr u16 kTalkLines = 0x24;
constexpr u16 kTalkLinesMode = 0x25;
constexpr u16 kTalkCurve = 0x27;
constexpr u16 kTalkSlide = 0x28;
constexpr u16 kTalkIntro = 0x29;
constexpr u16 kTalkSpeaker = 0x2A;
constexpr u16 kTalkQuietWin = 0x2B;
constexpr u16 kTalkTimedWindow = 0x2C;
constexpr u16 kTalkMirror = 0x2D;
constexpr u16 kTalkSetFlag = 0x2E;
constexpr u16 kTalkClearFlag = 0x2F;
constexpr u16 kTalkFlagJump = 0x30;
constexpr u16 kTalkPartyGone = 0x31;
constexpr u16 kTalkJoinFirst = 0x32;
constexpr u16 kTalkDefeatPose = 0x33;
constexpr u16 kTalkAttributes = 0x34;
constexpr u16 kTalkAddLoot = 0x35;
constexpr u16 kTalkEnemyCommand = 0x36;
constexpr u16 kTalkLearn = 0x37;
constexpr u16 kTalkWords66 = 0x38;
constexpr u16 kTalkBackgroundAgain = 0x39;
constexpr u16 kTalkPartyOrder = 0x3A;
constexpr u16 kTalkLineWindow = 0x3B;
constexpr u16 kTalkMusic = 0x3C;
constexpr u16 kTalkAllCounted = 0x3D;
constexpr u16 kTalkFieldFade = 0x3E;
constexpr u16 kTalkFadeWait = 0x3F;
constexpr u16 kTalkPictureWait = 0x40;
constexpr u16 kTalkHasObjects = 0x41;
constexpr u16 kTalkTakeObjects = 0x42;
constexpr u16 kTalkCritical = 0x43;
constexpr u16 kTalkPlace = 0x44;
constexpr u16 kTalkPanel8 = 0x45;
constexpr u16 kTalkLevel = 0x46;
constexpr u16 kTalkLose = 0x47;
constexpr u16 kTalkBackground = 0x48;
constexpr u16 kTalkSlideTo = 0x49;
constexpr u16 kTalkResistances = 0x4A;
constexpr u16 kTalkWords72 = 0x4B;
constexpr u16 kTalkKill = 0x4C;
constexpr u16 kTalkPanel40 = 0x4D;
constexpr u16 kTalkCatch = 0x4E;
constexpr u16 kTalkDamage = 0x4F;
constexpr u16 kTalkReload = 0x50;
constexpr u16 kTalkUsedItem = 0x51;
constexpr u16 kTalkQuake = 0x52;
constexpr u16 kTalkNoRetreat = 0x53;
constexpr u16 kTalkRepeatLimit = 0x54;  // swd3e: the commands after 0x53
constexpr u16 kTalkChanceJump = 0x55;
constexpr u16 kTalkRaiseLevel = 0x56;
constexpr u16 kTalkAddGold = 0x57;
constexpr u16 kTalkPayGold = 0x58;
constexpr u16 kTalkNoHitTakeBack = 0x59;
constexpr u32 kWindowFlagsA0 = TalkWindow::Overlay | TalkWindow::Skippable;      // command 0x23: for the next window
constexpr std::u8string_view kErrorTitle = u8"錯誤";                             // 0x004B70C4
constexpr std::u8string_view kWrongButtons = u8"按鈕個數不對!!";                 // 0x004BF950
constexpr std::u8string_view kGaonCaption = u8"對話系統(GAON)";                  // 0x004B28C8
constexpr std::u8string_view kAreaOffCaption = u8"對話系統(G_AREA_OFF)";         // 0x004B286C
constexpr std::u8string_view kCodeTooLarge = u8"代碼值太大(%d)";                 // 0x004BF940
constexpr std::u8string_view kCodeRepeated = u8"代碼值重複(%d)";                 // 0x004BF930
constexpr std::u8string_view kOffScreen = u8"位置超出螢幕(x%d ,y%d ,w%d ,h%d)";  // 0x004BF90C
constexpr u16 kEffectNumbers = 0x100;                                            // ScriptEffect numbers
constexpr u16 kEvenPixels = 0xFFFE;
constexpr u16 kRisingLines = 1;
constexpr u16 kStillLines = 2;
constexpr i16 kLineStart = 2;
constexpr u16 kOpenLines = 0;
constexpr u16 kCloseLines = 1;
constexpr u16 kEndLines = 2;
constexpr u16 kCurveSpans = 6;
constexpr std::size_t kCurveBytes = 0x18;
constexpr std::size_t kCurveOffsets = 5;
constexpr u16 kCurveSteps = 0x14;
constexpr u16 kSlideByThirds = 0x10;
constexpr int kSlideThird = 3;
constexpr int kSlideThirdNegative = -3;
constexpr i32 kNoTalkTimeout = 0xFFFF;
constexpr i32 kMirroredPanelSpan = 0x270;
constexpr i32 kQuakeFrames = 2;
constexpr u8 kNoRolesLeft = 0xFF;
constexpr i32 kBackgroundPanel = 1;  // BattleState::panelFlags
constexpr i32 kLostPanel = 0x10;
constexpr i32 kUnkPanel40 = 0x40;  // BattleState::panelFlags bit set by command 0x4D (swd32: no first strike)
constexpr i32 kBattleLostState = 0x67;
constexpr int kCatchCommand = 6;
constexpr i16 kMaxVital = 0x270F;
constexpr int kEnemyKillHeal = -1000000;     // command 0x4C: an enemy loses this much (0xFFF0BDC0)
constexpr std::size_t kResistanceWords = 5;  // swd3e: five elements (swd32: nine)
constexpr std::size_t kCountedSkipWords = 4;

// BattleUnit::GetPosition into the low words of two dwords of the talk, their high words kept (0x0048B500 writes
// words).
void ReadPosition(const BattleUnit& unit, i32& x, i32& y)
{
    u16 lowX = 0;
    u16 lowY = 0;
    unit.GetPosition(lowX, lowY);
    x = static_cast<i32>(MakeWords(HighWord(static_cast<u32>(x)), lowX));
    y = static_cast<i32>(MakeWords(HighWord(static_cast<u32>(y)), lowY));
}

}  // namespace

// swd3e.exe 0x00479860: one step of the battle talk (figtalk.dat): the command at the cursor runs, most of them
// with a battle frame. 0 once the talk and the battle are over (command 0xFFFF); otherwise 1, or the end a frame
// returned (commands 1 and 0x29). Unknown commands return 1 without moving (the original stays on them).
int Game::FightTalkFrame()
{
    FightTalk& talk = this->fightTalk;

    const u16 command = talk.UnsignedWord(0);

    switch (command)
    {
        case kTalkEnd:
            this->FightTalkEnd();
            return 0;

        case kTalkWait:
            return this->FightTalkWait();

        case kTalkWindow:
            return this->FightTalkWindow();

        case kTalkStep:
            talk.battleRuns = 0;
            this->BattleFrame();
            talk.cursor += sizeof(u16);
            talk.battleRuns = 1;
            return 1;

        case kTalkSetUnk4A7B54:
            talk.windowTimeout = talk.Word(1);
            this->BattleFrame();
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkMove:
            return this->FightTalkMove();

        case kTalkPause:
            return this->FightTalkPause();

        case kTalkHitPoints:
            return this->FightTalkHitPoints();

        case kTalkOrder:
            return this->FightTalkOrder();

        case kTalkDefeat:
            return this->FightTalkDefeat();

        case kTalkEscape:
            return this->FightTalkCommand(kEscapeCommand);

        case kTalkWin:
            return this->FightTalkWin();

        case kTalkLift:
            return this->FightTalkLift();

        case kTalkAllDone:
            return this->FightTalkAllDone();

        case kTalkSetMarks:
            return this->FightTalkMarks(true);

        case kTalkRepeat:
            return this->FightTalkRepeat();

        case kTalkSkip:
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkClearMarks:
            return this->FightTalkMarks(false);

        case kTalkRandomJump:
            return this->FightTalkRandomJump();

        case kTalkFace:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            this->TalkUnit(this->TalkCommandUnit()).SetFacing(1);  // 0x0047B796: jb, the unit unsigned
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkAimSelf:
            return this->FightTalkCommand(kAimSelfCommand);

        case kTalkShift:
            return this->FightTalkShift();

        case kTalkSkill:
            return this->FightTalkSkill();

        case kTalkNextBattle:
            return this->FightTalkNextBattle();

        case kTalkLoot:
            return this->FightTalkLoot();

        case kTalkHold:
            return this->FightTalkCommand(kHoldCommand);

        default:
            if (command >= kTalkSetAction && command <= kTalkLinesMode)
            {
                return this->FightTalkUnits(command);
            }

            return this->FightTalkMisc(command);
    }
}

// Every unit reset, the battle ended and the talk closed (inline in 0x00479860, command 0xFFFF and the end of a battle
// run by command 1).
void Game::FightTalkEnd()
{
    BattleState& battle = this->battleState;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].Reset(this->battleServices);
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        this->battleRoles[index].Reset(this->battleServices);
    }

    this->fightTalk.waitCount = 0;
    this->EndBattle();
    this->fightTalk.Close();
}

// Command 1 of FightTalkFrame (inline in the original): the battle runs (its frames) for the count given; a frame
// that ends the battle ends the talk too (its end returned). With no count the talk goes on at once and the unit
// kept by command 9 takes the panel again.
int Game::FightTalkWait()
{
    FightTalk& talk = this->fightTalk;

    if ((talk.waitCount & kCountdown) == 0)
    {
        talk.waitCount = static_cast<u16>(talk.UnsignedWord(1) | kCountdown);
        return 1;
    }

    if ((talk.waitCount & kCountMask) == 0)
    {
        talk.waitCount = 0;
        talk.cursor += 2 * sizeof(u16);
        talk.enabled = 1;

        if (talk.savedUnit != 0)
        {
            this->battleState.currentUnit = talk.savedUnit;
        }

        talk.savedUnit = 0;
        return 1;
    }

    talk.battleRuns = 1;
    talk.unk53CCE8 = this->BattleFrame();

    if (talk.unk53CCE8 == 1)
    {
        return 1;
    }

    const i32 end = talk.unk53CCE8;
    this->FightTalkEnd();
    talk.cursor += 2 * sizeof(u16);
    talk.waitCount = 0;
    talk.enabled = 1;
    return end;
}

// Command 2 of FightTalkFrame (inline in the original): a talk window of the unit in the command's second word (the
// text after it, ended by %Q) opens at the unit's place (from 1000: at 0x1EA, 0xB4; FixedPlace, no timeout, speed 2,
// the flags left in the low word of windowFlags) and the battle waits; the units of the speaker's side unmarked (a
// role speaker takes action 0x24). Once the window is closed every role and the first ten enemies are unmarked and
// the talk goes on after the text.
int Game::FightTalkWindow()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    auto high = [&talk]
    {
        return static_cast<u16>(static_cast<u32>(talk.windowFlags) >> (sizeof(u16) * 8));
    };

    auto setHigh = [&talk](u16 value)
    {
        talk.windowFlags = static_cast<i32>(
            (static_cast<u32>(talk.windowFlags) & kLowWordMask) | (static_cast<u32>(value) << (sizeof(u16) * 8))
        );
    };

    if ((high() & kWindowShown) == 0)
    {
        setHigh(talk.UnsignedWord(1));
        talk.textAt = talk.cursor + sizeof(u16);
        auto window = std::make_unique<TalkWindow>();
        const u16 speaker = high();
        u16 x = 0;
        u16 y = 0;

        if (speaker >= kFixedSpeaker)
        {
            x = kFixedSpeakerX;
            y = kFixedSpeakerY;
        }
        else if (speaker <= kLastEnemy)
        {
            this->battleEnemies[speaker].GetDrawPosition(x, y);

            for (BattleEnemy& enemy : this->battleEnemies)
            {
                enemy.SetAC0(0);
            }
        }
        else
        {
            BattleRole& role = this->battleRoles[static_cast<std::size_t>(speaker - BattleState::FirstRoleUnit)];
            role.GetDrawPosition(x, y);

            for (BattleRole& other : this->battleRoles)
            {
                other.SetAC0(0);
            }

            role.SetAction(kTalkAction);
        }

        talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << (sizeof(u16) * 8)));
        window->flags = TalkWindow::FixedPlace;
        const std::span<const u8> text =
            std::span<const u8>(talk.script).subspan(std::min(talk.textAt, talk.script.size()));
        const std::string_view name(talk.speaker.data(), strnlen(talk.speaker.data(), talk.speaker.size()));
        this->talk.SetupDialog(*window, text, name, Talk::SizedLayout, 0, 0);
        talk.window = window.get();
        this->talk.AppendWindow(std::move(window));
        talk.window->timeout |= kNoTimeout;
        talk.window->speed = kWindowSpeed;
        talk.window->flags |= static_cast<u16>(talk.windowFlags);
        talk.windowFlags = static_cast<i32>(static_cast<u32>(talk.windowFlags) & ~kLowWordMask);
        talk.window->fromX = x;
        talk.window->fromY = y;
        setHigh(static_cast<u16>(high() | kWindowShown));
        battle.unk53BFC0 = 1;
        battle.result = 0;
    }

    if (this->scene.talkWindows != nullptr)
    {
        talk.battleRuns = 0;
        this->BattleFrame();
        return 1;
    }

    for (BattleRole& role : this->battleRoles)
    {
        role.SetAC0(0);
    }

    // The original walks the first ten enemies here.
    for (std::size_t index = 0; index < std::min(kTalkEnemies, this->battleEnemies.size()); ++index)
    {
        this->battleEnemies[index].SetAC0(0);
    }

    int length = 0;

    while (length < kTextEndSearch)
    {
        const std::size_t at = talk.textAt + static_cast<std::size_t>(length);

        if (at + 1 < talk.script.size() && talk.script[at] == kTextEscape && talk.script[at + 1] == kTextEnd)
        {
            break;
        }

        ++length;
    }

    talk.textAt += static_cast<std::size_t>(length) + 2;
    talk.cursor = talk.textAt;
    talk.speaker = {};
    talk.battleRuns = 1;
    battle.unk53BFC0 = 0;
    talk.unk53CE78 = 0;
    setHigh(0);
    return 1;
}

// Command 5 of FightTalkFrame (inline in the original): the unit of the second word moves by the third and fourth
// (a role from 8); the units are measured and sorted again (drawn at once with drawMovedAtOnce).
int Game::FightTalkMove()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.battleRuns = 0;
    const i16 unit = talk.Word(1);
    talk.unk53CE74 = talk.UnsignedWord(2);
    talk.unk53CE76 = talk.UnsignedWord(3);
    BattleUnit& moved =
        unit >= BattleState::FirstRoleUnit
            ? static_cast<BattleUnit&>(this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)])
            : static_cast<BattleUnit&>(this->battleEnemies[static_cast<std::size_t>(unit)]);
    u16 x = 0;
    u16 y = 0;
    moved.GetPosition(x, y);
    talk.unk53CCE8 = x + static_cast<i16>(talk.unk53CE74);
    talk.unk53CCEC = y + static_cast<i16>(talk.unk53CE76);
    moved.SetPosition(static_cast<i16>(talk.unk53CCE8), static_cast<i16>(talk.unk53CCEC));
    talk.unk53CE78 = 0;
    talk.cursor += 4 * sizeof(u16);
    talk.unk53CCE8 = 0;
    talk.unk53CCEC = 0;
    talk.unk53CE74 = 1;
    this->MeasureUnits();
    battle.SortDrawOrder();
    battle.ListEnemyOrder();

    if (talk.drawMovedAtOnce == 1)
    {
        this->BattleFrame();
    }

    talk.battleRuns = 1;
    return 1;
}

// Command 6 of FightTalkFrame (inline in the original): the talk waits (battle frames without the talk) for the
// frames given.
int Game::FightTalkPause()
{
    FightTalk& talk = this->fightTalk;
    talk.battleRuns = 0;

    if ((talk.unk53CCD4 & kCountdown) == 0)
    {
        talk.unk53CCD4 = talk.Word(1) | kCountdown;
        this->BattleFrame();
        return 1;
    }

    if ((talk.unk53CCD4 & kCountMask) != 0)
    {
        --talk.unk53CCD4;
    }
    else
    {
        talk.unk53CCD4 = 0;
        talk.cursor += 2 * sizeof(u16);
        talk.battleRuns = 1;
    }

    this->BattleFrame();
    return 1;
}

// Command 8 of FightTalkFrame (inline in the original): after a frame, with the third word 1 the talk jumps to the
// place in the fifth word when the hit points of the unit in the second are at most the fourth; otherwise it goes on
// after the command (another third word: it stays).
int Game::FightTalkHitPoints()
{
    FightTalk& talk = this->fightTalk;
    talk.cursor += sizeof(u16);
    talk.battleRuns = 1;
    this->BattleFrame();
    const i16 unit = talk.Word(0);

    if (talk.UnsignedWord(1) != 1)
    {
        return 1;
    }

    BattleUnit& checked =
        unit > kLastEnemy
            ? static_cast<BattleUnit&>(this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)])
            : static_cast<BattleUnit&>(this->battleEnemies[static_cast<std::size_t>(unit)]);
    checked.GetHitPoints(talk.unk53CCE8, talk.unk53CCEC);

    if (talk.unk53CCE8 <= talk.Word(2))
    {
        const i16 place = talk.Word(3);
        talk.cursor += 3 * sizeof(u16);
        talk.Seek(place);
        return 1;
    }

    talk.cursor += 5 * sizeof(u16);
    return 1;
}

// Command 9 of FightTalkFrame (inline in the original): a role (from 0x14) is set to attack the enemy in the third
// word (from 0x14 a role, kept for its party order) and the current unit is kept; an enemy is ordered to attack the
// unit in the third word (a defeated enemy target passes to the next standing, a defeated role target to a random
// standing one), the order forced, the turn command of the enemy unk53CCEC cleared (as in the original) and its slot
// record put first. The battle then runs until the unit is done (enemyFaded).
int Game::FightTalkOrder()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) == 0)
    {
        const i16 unit = talk.Word(1);
        const i16 target = talk.Word(2);

        if (unit > kLastEnemy)
        {
            talk.savedUnit = battle.currentUnit;
            battle.turnUnit = unit;
            battle.aimTarget = target + 1;

            if (battle.aimTarget > kLastEnemy)
            {
                battle.unk53BFD0 = 1;
                battle.aimTarget -= BattleState::FirstRoleUnit;
                battle.pendingOrders[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].unk00 = 1;
            }

            this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].SetTarget(1);
            this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
            battle.turnCommand = 1;
            battle.enemyFaded = 0;
            battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = 1;
        }
        else
        {
            u16& order = battle.enemyOrders[static_cast<std::size_t>(unit)];
            order = 0;
            talk.unk53CCE8 = target;

            if (target > kLastEnemy)
            {
                this->TalkStandingRole(talk.unk53CCE8);
                order = static_cast<u16>(talk.unk53CCE8 - BattleState::FirstRoleUnit);
            }
            else
            {
                if (this->battleEnemies[static_cast<std::size_t>(target)].IsDefeated())
                {
                    while (++talk.unk53CCE8 <= kLastEnemy &&
                           this->battleEnemies[static_cast<std::size_t>(talk.unk53CCE8)].IsDefeated())
                    {
                    }
                }

                battle.enemyAimsAtEnemies = 1;
                order = static_cast<u16>(talk.unk53CCE8);
            }

            order = static_cast<u16>(order | kOrderForced);

            // An index outside the enemies is read past them in the original (deviations.md): nothing cleared.
            if (talk.unk53CCEC >= 0 && static_cast<std::size_t>(talk.unk53CCEC) < this->battleEnemies.size())
            {
                this->battleEnemies[static_cast<std::size_t>(talk.unk53CCEC)].ClearTurnCommand();
            }

            battle.InsertSlotRecord(kEnemySlot, unit, 0);
            this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
            battle.enemyFaded = 0;
        }
    }

    battle.unk53BFC0 = 1;
    talk.battleRuns = 1;
    this->BattleFrame();

    if (battle.enemyFaded != 1)
    {
        return 1;
    }

    this->SetTalkCommandUnit(0);
    talk.unk53CCE8 = 0;
    battle.enemyFaded = 0;
    battle.unk53BFC0 = 0;
    talk.cursor += 3 * sizeof(u16);
    return 1;
}

// The unit `unit` of a talk command (inline in 0x00479860): a role from 8, an enemy below.
BattleUnit& Game::TalkUnit(i32 unit)
{
    if (unit >= BattleState::FirstRoleUnit)
    {
        return this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)];
    }

    return this->battleEnemies[static_cast<std::size_t>(unit)];
}

// The unit of a talk command kept in the high word of windowFlags (inline in 0x00479860).
u16 Game::TalkCommandUnit() const
{
    return static_cast<u16>(static_cast<u32>(this->fightTalk.windowFlags) >> kHighWordShift);
}

// The unit kept in the high word of windowFlags set (inline in 0x00479860).
void Game::SetTalkCommandUnit(u16 unit)
{
    FightTalk& talk = this->fightTalk;
    talk.windowFlags = static_cast<i32>(
        (static_cast<u32>(talk.windowFlags) & kLowWordMask) | (static_cast<u32>(unit) << kHighWordShift)
    );
}

// A unit command started by a talk command (inline in 0x00479860, LABEL_198): the command marked running, the
// battle runs a frame.
int Game::TalkCommandStarted()
{
    this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
    this->battleState.enemyFaded = 0;
    this->fightTalk.battleRuns = 1;
    this->BattleFrame();
    return 1;
}

// Command 0xA of FightTalkFrame (inline in the original): the unit of the second word is defeated (an enemy: kept
// for the removal, unk53BFDC; a role: damageShown), the field flashes, a frame runs and the talk goes on.
int Game::FightTalkDefeat()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) == 0)
    {
        const u16 unit = talk.UnsignedWord(1);
        this->SetTalkCommandUnit(unit);
        this->TalkUnit(unit).StartDefeat(1);

        if (unit <= kLastEnemy)
        {
            battle.unk4A7626 = unit;
            battle.unk53BFDC = 1;
            battle.unk502984[unit] = unit;
            battle.flashing = 1;
            battle.enemyFaded = 0;
            talk.battleRuns = 0;
        }
        else
        {
            battle.damageShown = 1;
            battle.flashing = 1;
            battle.lastRoleDefeated = static_cast<u16>(unit - BattleState::FirstRoleUnit);
            battle.enemyFaded = 0;
        }

        this->SetTalkCommandUnit(static_cast<u16>(unit | kWindowShown));
    }

    this->BattleFrame();
    talk.cursor += 2 * sizeof(u16);
    this->SetTalkCommandUnit(0);
    battle.unk53BFC0 = 0;
    talk.battleRuns = 1;
    return 1;
}

// Commands 0xB, 0x15 and 0x1A of FightTalkFrame (inline in the original): the unit of the second word takes command
// `command` (0x11 escape, 0xB, 0xC) with word 2A0A from the third word, its slot record put first (a role's turn
// set up; command 0xB makes an enemy aim at itself and a role's order a party order; the enemy of command 0xB holds
// the panels; the enemy of command 0xC acts at once). The talk goes on once the unit is done (enemyFaded).
int Game::FightTalkCommand(int command)
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) != 0)
    {
        if (battle.enemyFaded != 1)
        {
            if (command == kEscapeCommand)
            {
                talk.battleRuns = 1;
            }

            this->BattleFrame();
            return 1;
        }

        talk.cursor += 3 * sizeof(u16);
        this->SetTalkCommandUnit(0);
        battle.unk53BFC0 = 0;

        if (command == kAimSelfCommand)
        {
            talk.battleRuns = 0;
            this->BattleFrame();
        }
        else if (command == kHoldCommand)
        {
            talk.battleRuns = 0;
        }

        return 1;
    }

    const u16 unit = talk.UnsignedWord(1);
    this->SetTalkCommandUnit(unit);
    BattleUnit& actor = this->TalkUnit(unit);

    if (unit > kLastEnemy)
    {
        battle.turnUnit = unit;
        battle.aimTarget = unit - (BattleState::FirstRoleUnit - 1);
    }

    if (command != kAimSelfCommand)
    {
        actor.Retarget(0);
    }
    else if (unit <= kLastEnemy)
    {
        actor.Retarget(static_cast<i16>(unit));
    }

    actor.SetCommand(command);
    actor.SetWord2A0A(talk.Word(2));

    if (command == kEscapeCommand)
    {
        battle.InsertSlotRecord(unit <= kLastEnemy ? kEnemySlot : 1, unit, 0);
    }
    else if (command == kAimSelfCommand)
    {
        if (unit <= kLastEnemy)
        {
            battle.unk53BFC0 = 1;
        }
        else
        {
            battle.pendingOrders[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].unk00 = 1;
            battle.InsertSlotRecord(1, unit, 0);
        }
    }
    else if (unit <= kLastEnemy)
    {
        battle.actingUnit = unit;
    }
    else
    {
        battle.InsertSlotRecord(1, unit, 0);
    }

    return this->TalkCommandStarted();
}

// Command 0xC of FightTalkFrame (inline in the original): once every defeated enemy is taken out the battle ends as
// won (unless the talk already ended it: every enemy counted gone, the end check state 0x63 unless a later end runs).
int Game::FightTalkWin()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        if (battle.unk502984[index] != -1)
        {
            battle.actingUnit = kNoActingUnit;
            this->BattleFrame();
            talk.unk53CE90 =
                static_cast<i32>((static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (1u << kHighWordShift));
            battle.actingUnit = kNoActingUnit;
            return 1;
        }
    }

    if (talk.wonByTalk == 0)
    {
        battle.enemiesGone = static_cast<u8>(battle.enemyCount);
        battle.unk53BF02 = 0;

        if (battle.result <= kLastPlayState)
        {
            battle.autoBattle = 0;
            battle.result = kEndCheckState;
        }
    }

    talk.cursor += sizeof(u16);
    talk.wonByTalk = 1;
    talk.battleRuns = 1;
    return 1;
}

// Command 0xD of FightTalkFrame (inline in the original): the unit of the second word moves down by the third word.
int Game::FightTalkLift()
{
    FightTalk& talk = this->fightTalk;
    talk.unk53CE74 = talk.UnsignedWord(2);
    BattleUnit& moved = this->TalkUnit(talk.Word(1));
    u16 x = 0;
    u16 y = 0;
    moved.GetPosition(x, y);
    talk.unk53CCE8 = x;
    talk.unk53CCEC = y + static_cast<i16>(talk.unk53CE74);
    moved.SetPosition(static_cast<i16>(talk.unk53CCE8), static_cast<i16>(talk.unk53CCEC));
    talk.cursor += 3 * sizeof(u16);
    talk.unk53CE78 = 0;
    talk.unk53CCE8 = 0;
    talk.unk53CCEC = 0;
    talk.unk53CE74 = 0;
    this->MeasureUnits();
    this->BattleFrame();
    return 1;
}

// Command 0xE of FightTalkFrame (inline in the original): the units listed after it (ended by 0xFFFF) are checked
// (IsDefeatedOrDying); once all of them are, the talk jumps to the place after the list, otherwise it goes on past the
// list and the place (and one more word).
int Game::FightTalkAllDone()
{
    FightTalk& talk = this->fightTalk;
    talk.battleRuns = 1;
    u16 listed = 0;

    while (talk.UnsignedWord(static_cast<std::size_t>(listed) + 1) != kListEnd)
    {
        ++listed;
    }

    talk.unk53CE9E = listed;
    u16 done = 0;

    for (u16 index = 0; index < listed; ++index)
    {
        const u16 unit = talk.UnsignedWord(static_cast<std::size_t>(index) + 1);
        talk.battleRuns = 0;
        this->SetTalkCommandUnit(unit);

        if (this->TalkUnit(unit).IsDefeatedOrDying() && ++done == listed)
        {
            talk.cursor += 2 * static_cast<std::size_t>(listed) + 2 * sizeof(u16);
            talk.Seek(talk.Word(0));
            talk.unk53CE9E = 0;
            talk.unk53CE94 = 0;
            return 1;
        }
    }

    talk.unk53CE94 = 0;

    if (done == listed)
    {
        talk.unk53CE9E = 0;
        return 1;
    }

    talk.cursor += 2 * (static_cast<std::size_t>(listed) + kListTail);
    return 1;
}

// Commands 0xF and 0x12 of FightTalkFrame (inline in the original): the unit of the second word gets (`set`) or loses
// the marks of the third word (a role's DefeatedMark counts it in defeatedRoles).
int Game::FightTalkMarks(bool set)
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if (set)
    {
        talk.battleRuns = 0;
    }

    const i16 unit = talk.Word(1);
    const u16 marks = talk.UnsignedWord(2);
    BattleUnit& marked = this->TalkUnit(unit);

    if (set)
    {
        marked.SetStatus(marks);
    }
    else
    {
        marked.ClearStatus(marks);
    }

    if (unit > kLastEnemy && (marks & BattleUnit::DefeatedMark) != 0)
    {
        if (set)
        {
            ++battle.defeatedRoles;
        }
        else
        {
            --battle.defeatedRoles;
        }
    }

    talk.cursor += 3 * sizeof(u16);
    talk.battleRuns = 1;
    return 1;
}

// Command 0x10 of FightTalkFrame (inline in the original): the talk jumps to the place in the third word the count
// of the second word less one times (a frame each), then goes on.
int Game::FightTalkRepeat()
{
    FightTalk& talk = this->fightTalk;
    talk.battleRuns = 0;

    auto low = [&talk]
    {
        return static_cast<u16>(talk.unk53CE90);
    };

    auto setLow = [&talk](u16 value)
    {
        talk.unk53CE90 = static_cast<i32>((static_cast<u32>(talk.unk53CE90) & ~kLowWordMask) | value);
    };

    if ((low() & kCountdown) == 0)
    {
        setLow(static_cast<u16>((talk.UnsignedWord(1) | kCountdown) - 1));
    }

    if ((low() & kCountMask) != 0)
    {
        const i16 place = talk.Word(2);
        talk.Seek(place);
        setLow(static_cast<u16>(low() - 1));
    }
    else
    {
        setLow(0);
        talk.cursor += 4 * sizeof(u16);
    }

    this->BattleFrame();
    talk.battleRuns = 1;
    return 1;
}

// Command 0x13 of FightTalkFrame (inline in the original): the talk jumps to one of the places listed after it (every
// other word, ended by 0xFFFF) at random (the only one when there is one).
int Game::FightTalkRandomJump()
{
    FightTalk& talk = this->fightTalk;
    talk.cursor += sizeof(u16);
    u16 count = static_cast<u16>(talk.unk53CE94);

    if (count == 0)
    {
        u16 at = static_cast<u16>(static_cast<u32>(talk.unk53CE94) >> kHighWordShift);

        while (true)
        {
            talk.unk53CE74 = talk.UnsignedWord(at);

            if (talk.unk53CE74 == kListEnd)
            {
                break;
            }

            at = static_cast<u16>(at + 2);
            ++count;
            talk.unk53CE94 = static_cast<i32>(count | (static_cast<u32>(at) << kHighWordShift));
        }
    }

    if (count == 1)
    {
        talk.Seek(talk.Word(0));
    }
    else
    {
        talk.unk53CE74 = static_cast<u16>(2 * this->random.Below(static_cast<u16>(talk.unk53CE94)));
        talk.Seek(talk.Word(talk.unk53CE74));
    }

    talk.unk53CE74 = 0;
    talk.unk53CE94 = 0;
    talk.battleRuns = 1;
    return 1;
}

// Command 0x16 of FightTalkFrame (inline in the original): every unit moves across by the second word (also the
// shake of the background).
int Game::FightTalkShift()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.unk53CE74 = talk.UnsignedWord(1);
    battle.shake = static_cast<i16>(talk.unk53CE74);

    // The position is read into the low words of the two dwords, which keep their high words (0x0048B500); they are
    // left set.
    auto shift = [&talk, &battle](BattleUnit& unit)
    {
        ReadPosition(unit, talk.unk53CCE8, talk.unk53CCEC);
        talk.unk53CCE8 = static_cast<i32>(static_cast<u32>(talk.unk53CCE8) + static_cast<u32>(battle.shake));
        unit.SetPosition(static_cast<i16>(talk.unk53CCE8), static_cast<i16>(talk.unk53CCEC));
    };

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        shift(this->battleRoles[index]);
    }

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        shift(this->battleEnemies[index]);
    }

    talk.cursor += 2 * sizeof(u16);
    talk.unk53CE74 = 0;
    return 1;
}

// Command 0x17 of FightTalkFrame (inline in the original): the unit of the third word uses the skill (an enemy) or
// the object (a role) of the second word on the unit of the fourth, as command 9 sets an attack: an enemy's order is
// a spell order, its turn command cleared, its act in enemyHitActs and its slot record added; a role's object fills
// its second gauge and its order takes the act's speed and flags. The battle then runs until the unit is done
// (enemyFaded).
int Game::FightTalkSkill()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) == 0)
    {
        talk.unk53CCE8 = talk.Word(1);

        if (talk.Word(2) <= kLastEnemy)
        {
            talk.unk53CCEC = talk.Word(2);
            talk.unk53CCF0 = talk.Word(3);
            const auto actor = static_cast<std::size_t>(talk.unk53CCEC);
            u16& order = battle.enemyOrders[actor];
            order = 0;

            if (talk.unk53CCF0 <= kLastEnemy)
            {
                if (this->battleEnemies[static_cast<std::size_t>(talk.unk53CCF0)].IsDefeated())
                {
                    while (++talk.unk53CCF0 <= kLastEnemy &&
                           this->battleEnemies[static_cast<std::size_t>(talk.unk53CCF0)].IsDefeated())
                    {
                    }
                }

                battle.enemyAimsAtEnemies = 1;
                order = static_cast<u16>(talk.unk53CCF0);
            }
            else
            {
                this->TalkStandingRole(talk.unk53CCF0);
                order = static_cast<u16>(talk.unk53CCF0 - BattleState::FirstRoleUnit);
            }

            order = static_cast<u16>(order | kOrderSpell);
            BattleEnemy& enemy = this->battleEnemies[actor];
            enemy.ClearTurnCommand();
            enemy.LoadSkillAction(this->database, static_cast<u16>(talk.unk53CCE8), battle.enemyHitActs[actor]);
            battle.AddSlotRecord(kEnemySlot, talk.unk53CCEC);
        }
        else
        {
            talk.savedUnit = battle.currentUnit;
            battle.turnUnit = talk.Word(2);
            i32 target = talk.Word(3);
            const auto role = static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit);

            if (target > kLastEnemy)
            {
                battle.unk53BFD0 = 1;
                target -= BattleState::FirstRoleUnit;
                battle.pendingOrders[role].unk00 = 1;
            }

            battle.aimTarget = target + 1;
            BattleRole& actor = this->battleRoles[role];
            actor.SetTarget(1);
            u32 act = static_cast<u32>(battle.unk4FE5D4[role]);
            actor.LoadItemAction(this->database, static_cast<u16>(talk.unk53CCE8), act);
            battle.unk4FE5D4[role] = static_cast<i32>(act);
            actor.FillSecondGauge();
            battle.pendingOrders[role].unk0C = static_cast<u16>(actor.ActSpeed());
            battle.pendingOrders[role].unk0E = 0;

            if (actor.ActFlag1() == 1)
            {
                battle.unk53BFF0 = 1;
                battle.pendingOrders[role].unk08 = 1;
            }

            if (actor.ActFlag0() == 1)
            {
                battle.unk53BF88 = 1;
            }

            battle.turnCommand = kSpellOrder;
            battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = kSpellOrder;
        }

        this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
        battle.enemyFaded = 0;
    }

    battle.unk53BFC0 = 1;
    talk.battleRuns = 1;
    this->BattleFrame();

    if (battle.enemyFaded != 1)
    {
        return 1;
    }

    this->SetTalkCommandUnit(0);
    talk.unk53CCE8 = 0;
    talk.unk53CCEC = 0;
    talk.unk53CCF0 = 0;
    battle.enemyFaded = 0;
    battle.unk53BFC0 = 0;
    talk.cursor += 4 * sizeof(u16);
    return 1;
}

// A defeated role target of a talk order passes to a random standing role that is not autoControl or a guest (inline in
// 0x00479860, commands 9 and 0x17; the original draws for ever when none qualifies).
void Game::TalkStandingRole(i32& unit)
{
    BattleState& battle = this->battleState;
    int draws = 0;

    while (this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].IsDefeated())
    {
        do
        {
            if (++draws > kHangDraws)
            {
                Hang();
            }

            if (battle.roleCount != 0)
            {
                unit = static_cast<i32>(this->random.Below(battle.roleCount)) + BattleState::FirstRoleUnit;
            }
        } while (this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].autoControl == 1 ||

                 this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].guestControl == 1);
    }
}

// Command 0x18 of FightTalkFrame (inline in the original): every unit reset and the battle ended, battle `number` of
// the second word starts (its talk read anew).
int Game::FightTalkNextBattle()
{
    BattleState& battle = this->battleState;
    const u16 number = this->fightTalk.UnsignedWord(1);

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].Reset(this->battleServices);
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        this->battleRoles[index].Reset(this->battleServices);
    }

    this->EndBattle();
    this->StartBattle(number);
    return 1;
}

// Command 0x19 of FightTalkFrame (inline in the original): the battle ends as won (to the loot, 0x66, when there is
// loot and the end is not running yet), every enemy counted gone; the talk waits until a frame ends the battle.
int Game::FightTalkLoot()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if (this->battleMessages.lootCount == 0 || battle.endFade != 0)
    {
        battle.endFade = 1;
    }
    else
    {
        battle.result = kEndLootState;
    }

    talk.wonByTalk = 1;
    battle.enemiesGone = static_cast<u8>(battle.enemyCount);
    battle.unk53BF02 = 0;
    talk.battleRuns = 0;

    if (this->BattleFrame() != 0)
    {
        return 1;
    }

    talk.wonByTalk = 0;
    talk.cursor += sizeof(u16);
    return 1;
}

// Commands 0x1B..0x26 of FightTalkFrame (inline in the original); see FightTalkFrame.
int Game::FightTalkUnits(u16 command)
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    switch (command)
    {
        case kTalkSetAction:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            this->TalkUnit(this->TalkCommandUnit()).SetAction(talk.Word(2));
            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkVitals:
        {
            const u16 unit = talk.UnsignedWord(1);
            this->SetTalkCommandUnit(unit);
            BattleUnit& set = this->TalkUnit(unit);
            set.SetVitals(talk.Word(2), talk.Word(3), talk.Word(4));

            if (unit > kLastEnemy)
            {
                const auto role = static_cast<std::size_t>(unit - BattleState::FirstRoleUnit);
                set.GetHitPoints(talk.unk53CCE8, talk.unk53CCEC);
                battle.hitPoints[role] = talk.unk53CCEC;
                set.GetSpirit(talk.unk53CE98, talk.unk53CE9A);
                battle.spirit[role] = static_cast<i16>(talk.unk53CE9A);  // 0x0047BC61: movsx
            }

            talk.cursor += 5 * sizeof(u16);
            talk.unk53CE98 = 0;
            return 1;
        }

        case kTalkStandAside:
        {
            const u16 unit = talk.UnsignedWord(1);
            this->SetTalkCommandUnit(unit);
            this->TalkUnit(unit).SetAF8(1);

            if (unit <= kLastEnemy)
            {
                ++battle.unk53BF02;
            }

            talk.cursor += 2 * sizeof(u16);
            this->SetTalkCommandUnit(0);
            return 1;
        }

        case kTalkSound:
            talk.battleRuns = 0;
            this->audio.PlaySound(talk.UnsignedWord(1), this->config.soundVolume);
            this->BattleFrame();
            talk.cursor += 2 * sizeof(u16);
            talk.battleRuns = 1;
            return 1;

        case kTalkChoice:
            return this->FightTalkChoice();

        case kTalkHeadIn:
        {
            auto sprite = std::make_unique<ScriptSprite>();
            ResetPlayback(sprite->act);
            sprite->act.id = talk.UnsignedWord(1);
            sprite->act.action = talk.UnsignedWord(2);
            sprite->targetX = talk.Word(3);
            sprite->targetY = talk.Word(4);
            sprite->headX = static_cast<i16>(-ScriptSprite::HeadMargin);

            if (sprite->targetX > Display::Width / 2)
            {
                sprite->headX = static_cast<i16>(Display::Width + ScriptSprite::HeadMargin);
            }

            sprite->headSpeed = 0;
            talk.cursor += 5 * sizeof(u16);
            sprite->next = std::move(this->scene.spritesB);
            this->scene.spritesB = std::move(sprite);
            return 1;
        }

        case kTalkHeadOut:
        {
            const u16 id = talk.UnsignedWord(1);
            const u16 action = talk.UnsignedWord(2);

            for (ScriptSprite* sprite = this->scene.spritesB.get(); sprite != nullptr; sprite = sprite->next.get())
            {
                if (static_cast<u32>(sprite->act.id) == id && static_cast<u32>(sprite->act.action) == action)
                {
                    sprite->headSpeed = -1;

                    if (sprite->headX > Display::Width / 2)
                    {
                        sprite->headSpeed = 1;
                    }

                    break;
                }
            }

            talk.cursor += 3 * sizeof(u16);
            return 1;
        }

        case kTalkFlagsA0:
            talk.windowFlags = static_cast<i32>(static_cast<u32>(talk.windowFlags) | kWindowFlagsA0);
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkLines:
            return this->FightTalkLines();

        case kTalkLinesMode:
            return this->FightTalkLinesMode();

        default:
            return 1;
    }
}

// Command 0x1F of FightTalkFrame (inline in the original): the talk jumps to the place of the choice taken (every
// other word after it, ended by 0xFFFF; Talk::choice); a choice past them shows 按鈕個數不對!! and the talk ends.
int Game::FightTalkChoice()
{
    FightTalk& talk = this->fightTalk;
    talk.cursor += sizeof(u16);
    u16 at = static_cast<u16>(static_cast<u32>(talk.unk53CE94) >> kHighWordShift);

    while (true)
    {
        talk.unk53CE74 = talk.UnsignedWord(at);

        if (talk.unk53CE74 == kListEnd)
        {
            break;
        }

        at = static_cast<u16>(at + 2);
        talk.unk53CE94 = static_cast<i32>(
            (static_cast<u32>(talk.unk53CE94) & kLowWordMask) | (static_cast<u32>(at) << kHighWordShift)
        );
        ++talk.unk53CEA0;
    }

    if (static_cast<u32>(this->talk.choice + 1) <= static_cast<u32>(talk.unk53CEA0))
    {
        talk.Seek(talk.Word(2 * static_cast<std::size_t>(this->talk.choice)));
        talk.unk53CEA0 = 0;
        talk.unk53CE94 = static_cast<i32>(static_cast<u32>(talk.unk53CE94) & kLowWordMask);
        return 1;
    }

    this->errors.ShowError(kErrorTitle, kWrongButtons);
    return 0;
}

// Command 0x24 of FightTalkFrame (inline in the original): line effect `number` (below 0x100) of the screen starts
// unless it runs (unk53CE90 then 1): `count` columns from (x, y), falling, rising (mode 1, from limit less 2) or
// still (mode 2); one off the screen is dropped.
int Game::FightTalkLines()
{
    FightTalk& talk = this->fightTalk;
    const u16 number = talk.UnsignedWord(1);

    auto running = [this, number]
    {
        for (const ScriptEffect* effect = this->scene.effects.get(); effect != nullptr; effect = effect->next.get())
        {
            if (static_cast<u8>(effect->mode) == number)
            {
                return true;
            }
        }

        return false;
    };

    if (number < kEffectNumbers)
    {
        talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);

        if (running())
        {
            talk.unk53CE90 =
                static_cast<i32>((static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (1u << kHighWordShift));
            ErrorBox(this->errors, kGaonCaption, kCodeRepeated, talk.UnsignedWord(2));
        }
        else
        {
            auto effect = std::make_unique<ScriptEffect>();
            effect->mode = number;
            effect->color = talk.Word(2);
            effect->x = static_cast<i16>(talk.UnsignedWord(4) & kEvenPixels);
            effect->y = static_cast<i16>(talk.UnsignedWord(5) & kEvenPixels);
            effect->limit = static_cast<i16>(talk.UnsignedWord(6) & kEvenPixels);
            effect->count = static_cast<i16>(talk.UnsignedWord(7) & kEvenPixels);

            if (effect->x >= 0 && effect->y >= 0 && effect->x + effect->limit <= Display::Width &&
                effect->count + effect->y <= Display::Height)
            {
                const auto columns = static_cast<std::size_t>(std::max<int>(effect->count, 0));
                effect->offsets.assign(columns, 0);
                effect->lengths.assign(columns, 0);
                u16 start = 0;
                u16 kind = ScriptEffect::Falling;
                const u16 lines = talk.UnsignedWord(3);

                if (lines == kRisingLines)
                {
                    start = static_cast<u16>(effect->limit - kLineStart);
                    kind = ScriptEffect::Rising;
                }

                if (lines == kStillLines)
                {
                    start = 0;
                    kind = ScriptEffect::Still;
                }

                talk.unk53CE94 = static_cast<i32>(start | (static_cast<u32>(kind) << kHighWordShift));
                effect->mode = static_cast<u16>(effect->mode | kind);

                for (int column = 0; column < effect->count; ++column)
                {
                    effect->offsets[static_cast<std::size_t>(column)] = static_cast<i16>(start);
                    effect->lengths[static_cast<std::size_t>(column)] = kLineStart;
                }

                talk.unk53CE90 = static_cast<i32>(
                    (static_cast<u32>(talk.unk53CE90) & kLowWordMask) |
                    (static_cast<u32>(std::max<int>(effect->count, 0)) << kHighWordShift)
                );
                effect->next = std::move(this->scene.effects);
                this->scene.effects = std::move(effect);
            }
            else
            {
                ErrorBox(this->errors, kGaonCaption, kOffScreen, effect->x, effect->y, effect->limit, effect->count);
            }
        }
    }
    else
    {
        ErrorBox(this->errors, kGaonCaption, kCodeTooLarge, talk.UnsignedWord(2));
    }

    talk.cursor += 8 * sizeof(u16);
    return 1;
}

// Command 0x25 of FightTalkFrame (inline in the original): running line effect `number` opens (mode 0), closes (1)
// or ends (2); another mode gives it the kind left in unk53CE90.
int Game::FightTalkLinesMode()
{
    FightTalk& talk = this->fightTalk;
    const u16 number = talk.UnsignedWord(1);

    if (number < kEffectNumbers)
    {
        std::unique_ptr<ScriptEffect>* link = &this->scene.effects;

        while (*link != nullptr && static_cast<u8>((*link)->mode) != number)
        {
            link = &(*link)->next;
        }

        if (*link != nullptr)
        {
            const u16 mode = talk.UnsignedWord(2);

            if (mode == kOpenLines)
            {
                talk.unk53CE90 = static_cast<i32>(
                    (static_cast<u32>(talk.unk53CE90) & kLowWordMask) |
                    (static_cast<u32>(ScriptEffect::Opening) << kHighWordShift)
                );
            }

            if (mode == kCloseLines)
            {
                talk.unk53CE90 = static_cast<i32>(
                    (static_cast<u32>(talk.unk53CE90) & kLowWordMask) |
                    (static_cast<u32>(ScriptEffect::Closing) << kHighWordShift)
                );
            }

            if (mode == kEndLines)
            {
                std::unique_ptr<ScriptEffect> ended = std::move(*link);
                *link = std::move(ended->next);
            }
            else
            {
                (*link)->mode = static_cast<u16>(
                    static_cast<u8>((*link)->mode) |
                    static_cast<u16>(static_cast<u32>(talk.unk53CE90) >> kHighWordShift)
                );
            }
        }
    }
    else
    {
        ErrorBox(this->errors, kAreaOffCaption, kCodeTooLarge, talk.UnsignedWord(2));
    }

    talk.cursor += 3 * sizeof(u16);
    return 1;
}

// Every unit moved across by the background's shake (inline in 0x00479860, commands 0x28 and 0x49).
void Game::ShiftTalkUnits()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    auto shift = [&talk, &battle](BattleUnit& unit)
    {
        u16 x = 0;
        u16 y = 0;
        unit.GetPosition(x, y);
        x = static_cast<u16>(x + battle.shake);
        talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << kHighWordShift));
        unit.SetPosition(static_cast<i16>(x), static_cast<i16>(y));
    };

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        shift(this->battleRoles[index]);
    }

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        shift(this->battleEnemies[index]);
    }
}

// Command 0x27 of FightTalkFrame (inline in the original): the unit of the second word moves along a curve through
// the six offsets after it (from its place), 21 steps a span over six spans, a frame each.
int Game::FightTalkCurve()
{
    FightTalk& talk = this->fightTalk;
    u16 span = talk.unk53CE76;

    if ((this->TalkCommandUnit() & kWindowShown) != 0)
    {
        if (span >= kCurveSpans)
        {
            talk.cursor += kCurveBytes;
            this->SetTalkCommandUnit(0);
            talk.unk53CE74 = 0;
            talk.unk53CE76 = 0;
            talk.unk53CCE8 = 0;
            talk.unk53CCEC = 0;
            return 1;
        }
    }
    else
    {
        this->SetTalkCommandUnit(static_cast<u16>(talk.UnsignedWord(1) | kWindowShown));
        u16 x = 0;
        u16 y = 0;
        this->TalkUnit(this->TalkCommandUnit() & kCountMask).GetPosition(x, y);
        talk.unk53CCE8 = static_cast<i32>((static_cast<u32>(talk.unk53CCE8) & ~kLowWordMask) | x);
        talk.unk53CCEC = static_cast<i32>((static_cast<u32>(talk.unk53CCEC) & ~kLowWordMask) | y);
        talk.curve[0] = {static_cast<i16>(x), static_cast<i16>(y)};
        talk.curve[1] = talk.curve[0];

        for (std::size_t point = 0; point < kCurveOffsets; ++point)
        {
            talk.curve[point + 2] = {
                static_cast<i16>(x + talk.UnsignedWord(2 * point + 2)),
                static_cast<i16>(y + talk.UnsignedWord(2 * point + 3))
            };
        }

        talk.curve[kCurveOffsets + 2] = talk.curve[kCurveOffsets + 1];
        talk.curve[kCurveOffsets + 3] = talk.curve[kCurveOffsets + 1];
        span = 0;
        talk.unk53CE74 = 1;
        talk.unk53CE76 = 0;
    }

    const std::array<std::array<i16, 2>, 4> points =
        {talk.curve[span], talk.curve[span + 1u], talk.curve[span + 2u], talk.curve[span + 3u]};
    TalkCurvePoint(static_cast<float>(static_cast<i16>(talk.unk53CE74)), talk.unk53CCE8, talk.unk53CCEC, points);
    this->TalkUnit(this->TalkCommandUnit() & kCountMask)
        .SetPosition(static_cast<i16>(talk.unk53CCE8), static_cast<i16>(talk.unk53CCEC));

    if (++talk.unk53CE74 > kCurveSteps)
    {
        ++talk.unk53CE76;
        talk.unk53CE74 = 0;
    }

    this->MeasureUnits();
    this->BattleFrame();
    return 1;
}

// Command 0x28 of FightTalkFrame (inline in the original): every unit slides across, by a third of what is left each
// frame of the second word (above 16), or to bring the unit of the second word to the middle of the screen; the talk
// goes on once the slide is 0.
int Game::FightTalkSlide()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    i16 left = static_cast<i16>(talk.unk53CE74);
    const u16 value = talk.UnsignedWord(1);
    talk.unk53CE76 = value;

    if (talk.unk53CE74 == 0)
    {
        left = static_cast<i16>(value);
        talk.unk53CE74 = value;
    }

    i32 slide = 0;

    if (value > kSlideByThirds)
    {
        slide = left / kSlideThirdNegative;
        talk.unk53CE74 = static_cast<u16>(left / kSlideThird);
    }
    else
    {
        u16 x = 0;
        u16 y = 0;
        this->TalkUnit(static_cast<i16>(value)).GetPosition(x, y);
        talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << kHighWordShift));
        talk.unk53CE74 = static_cast<u16>((static_cast<i16>(x) + Display::Width / 2) / 2);
        slide = Display::Width / 2 - static_cast<i16>(talk.unk53CE74);
    }

    battle.shake = slide;
    talk.battleRuns = 0;

    if (slide == 0)
    {
        talk.cursor += 2 * sizeof(u16);
        talk.unk53CE78 = 0;
        talk.unk53CE74 = 0;
        talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
        talk.unk53CE98 = 0;
        talk.unk53CE76 = 0;
        talk.battleRuns = 1;
        return 1;
    }

    this->ShiftTalkUnits();
    this->MeasureUnits();
    this->BattleFrame();
    return 1;
}

// Command 0x2A of FightTalkFrame (inline in the original): the speaker of the next window (up to 0x20 bytes before
// %Q, none without it), a nickname the texts write for a role turned into the role's; the battle does not run on.
int Game::FightTalkSpeaker()
{
    FightTalk& talk = this->fightTalk;
    talk.textAt = talk.cursor + sizeof(u16);
    talk.speaker = {};
    std::size_t length = 0;

    while (true)
    {
        const std::size_t at = talk.textAt + length;

        if (at + 1 < talk.script.size() && talk.script[at] == kTextEscape && talk.script[at + 1] == kTextEnd)
        {
            std::copy_n(talk.script.begin() + static_cast<std::ptrdiff_t>(talk.textAt), length, talk.speaker.begin());
            length += 2;
            break;
        }

        if (++length >= talk.speaker.size())
        {
            break;
        }
    }

    auto nameOf = [](const auto& buffer)
    {
        return std::string_view(buffer.data(), strnlen(buffer.data(), buffer.size()));
    };

    // The speaker is compared with the first four bytes of the nickname the texts write for each role (a dword copied
    // into a cleared buffer); a match has that nickname replaced by the role's.
    std::size_t role = 0;
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);

    while (role < Party::Roles)
    {
        const std::string tag = this->party.nameTags[role].Nickname().substr(0, sizeof(u32));

        if (nameOf(talk.speaker) == tag)
        {
            break;
        }

        ++role;
        talk.unk53CE90 = static_cast<i32>(
            (static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (static_cast<u32>(role) << kHighWordShift)
        );
    }

    if (role < Party::Roles)
    {
        std::string text(talk.speaker.data(), talk.speaker.size());
        Talk::ReplacePrefix(
            text,
            0,
            this->party.nameTags[role].Nickname(),
            this->party.names[role].Nickname(),
            static_cast<int>(talk.speaker.size())
        );
        std::copy_n(text.begin(), std::min(text.size(), talk.speaker.size()), talk.speaker.begin());
    }

    talk.textAt += length;
    talk.cursor = talk.textAt;
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
    return 1;
}

// Command 0x2C of FightTalkFrame (inline in the original): a timed window of the unit of the second word that the
// keys do not advance (timeout windowTimeout, which goes back to 0xFFFF; a speaker from 1000 at the fixed place), and
// the talk goes on at once after its text.
int Game::FightTalkTimedWindow()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    const u16 unit = talk.UnsignedWord(1);
    this->SetTalkCommandUnit(unit);
    talk.textAt = talk.cursor + sizeof(u16);
    auto window = std::make_unique<TalkWindow>();
    window->flags = TalkWindow::FixedPlace;
    const std::span<const u8> text =
        std::span<const u8>(talk.script).subspan(std::min(talk.textAt, talk.script.size()));
    const std::string_view name(talk.speaker.data(), strnlen(talk.speaker.data(), talk.speaker.size()));
    this->talk.SetupDialog(*window, text, name, Talk::SizedLayout, 0, 0);
    talk.window = window.get();
    this->talk.AppendWindow(std::move(window));
    talk.window->timeout |= static_cast<u32>(talk.windowTimeout);
    talk.window->flags |= TalkWindow::Timed;
    talk.window->flags |= TalkWindow::NoAdvance;
    talk.window->speed = kWindowSpeed;
    talk.window->flags |= static_cast<u16>(talk.windowFlags);
    talk.windowFlags = static_cast<i32>(static_cast<u32>(talk.windowFlags) & ~kLowWordMask);
    talk.windowTimeout = kNoTalkTimeout;
    u16 x = 0;
    u16 y = 0;

    if (unit >= kFixedSpeaker)
    {
        x = kFixedSpeakerX;
        y = kFixedSpeakerY;
    }
    else if (unit <= kLastEnemy)
    {
        BattleEnemy& enemy = this->battleEnemies[unit];
        enemy.GetDrawPosition(x, y);
        enemy.SetAC0(1);

        for (BattleEnemy& other : this->battleEnemies)
        {
            other.SetAC0(0);
        }
    }
    else
    {
        BattleRole& role = this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)];
        role.GetDrawPosition(x, y);

        for (BattleRole& other : this->battleRoles)
        {
            other.SetAC0(0);
        }

        role.SetAction(kTalkAction);
    }

    talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << kHighWordShift));
    talk.window->fromX = x;
    talk.window->fromY = y;
    battle.result = 0;
    this->ClearTalkMarks();
    this->SkipTalkText();
    return 1;
}

// Every role and the first ten enemies unmarked once a talk window is set up or closed (inline in 0x00479860).
void Game::ClearTalkMarks()
{
    for (BattleRole& role : this->battleRoles)
    {
        role.SetAC0(0);
    }

    for (std::size_t index = 0; index < std::min(kTalkEnemies, this->battleEnemies.size()); ++index)
    {
        this->battleEnemies[index].SetAC0(0);
    }
}

// The talk goes on after the text of its window (inline in 0x00479860, LABEL_68): past %Q (within 0xFF bytes), the
// speaker cleared.
void Game::SkipTalkText()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    int length = 0;

    while (length < kTextEndSearch)
    {
        const std::size_t at = talk.textAt + static_cast<std::size_t>(length);

        if (at + 1 < talk.script.size() && talk.script[at] == kTextEscape && talk.script[at + 1] == kTextEnd)
        {
            break;
        }

        ++length;
    }

    talk.textAt += static_cast<std::size_t>(length) + 2;
    talk.cursor = talk.textAt;
    talk.battleRuns = 1;
    battle.unk53BFC0 = 0;
    talk.speaker = {};
    talk.unk53CE78 = 0;
    this->SetTalkCommandUnit(0);
}

// Command 0x2D of FightTalkFrame (inline in the original): the sides swap: every unit faces the other way (a guest
// role away) at the mirrored place and the role panels move across; when mirrored already only the flag goes back.
int Game::FightTalkMirror()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if (battle.mirrored == 1)
    {
        talk.cursor += sizeof(u16);
        battle.mirrored = 0;
        return 1;
    }

    battle.mirrored = 1;

    auto mirror = [&talk](BattleUnit& unit)
    {
        u16 x = 0;
        u16 y = 0;
        unit.GetPosition(x, y);
        x = static_cast<u16>(Display::Width - x);
        talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << kHighWordShift));
        unit.SetPosition(static_cast<i16>(x), static_cast<i16>(y));
    };

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];
        enemy.SetFacing(1);
        mirror(enemy);
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];
        role.SetFacing(role.guestControl == 1 ? 0 : 1);
        mirror(role);
        i32& panelX = battle.panelPlaces[2 * index];
        panelX = kMirroredPanelSpan - panelX;
    }

    talk.cursor += sizeof(u16);
    return 1;
}

// Command 0x36 of FightTalkFrame (inline in the original): enemy `actor` (the third word; a role: nothing) takes the
// command of choice `choice` (the second word) at the unit of the fourth, as command 0x17 aims: its turn command
// cleared, its act in enemyHitActs, its order a forced order when the command loads (a spell order otherwise) and its
// slot record put first. The battle then runs until the enemy is done (enemyFaded).
int Game::FightTalkEnemyCommand()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) == 0)
    {
        talk.unk53CCE8 = talk.Word(1);
        talk.unk53CCEC = talk.Word(2);
        talk.unk53CCF0 = talk.Word(3);

        if (talk.unk53CCEC > kLastEnemy)
        {
            talk.cursor += 4 * sizeof(u16);
            return 1;
        }

        const auto actor = static_cast<std::size_t>(talk.unk53CCEC);
        u16& order = battle.enemyOrders[actor];
        order = 0;

        if (talk.unk53CCF0 <= kLastEnemy)
        {
            if (this->battleEnemies[static_cast<std::size_t>(talk.unk53CCF0)].IsDefeated())
            {
                while (++talk.unk53CCF0 <= kLastEnemy &&
                       this->battleEnemies[static_cast<std::size_t>(talk.unk53CCF0)].IsDefeated())
                {
                }
            }

            battle.enemyAimsAtEnemies = 1;
            order = static_cast<u16>(talk.unk53CCF0);
        }
        else
        {
            this->TalkStandingRole(talk.unk53CCF0);
            order = static_cast<u16>(talk.unk53CCF0 - BattleState::FirstRoleUnit);
        }

        BattleEnemy& enemy = this->battleEnemies[actor];
        enemy.ClearTurnCommand();
        u32 act = static_cast<u32>(battle.enemyHitActs[actor]);
        const int loaded = enemy.LoadEnemyCommand(this->database, talk.unk53CCE8, act);
        battle.enemyHitActs[actor] = static_cast<i32>(act);
        order = static_cast<u16>(order | (loaded == 1 ? kOrderForced : kOrderSpell));
        battle.InsertSlotRecord(1, talk.unk53CCEC, 0);
        this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
        battle.enemyFaded = 0;
    }

    battle.unk53BFC0 = 1;
    talk.battleRuns = 1;
    this->BattleFrame();

    if (battle.enemyFaded != 1)
    {
        return 1;
    }

    this->SetTalkCommandUnit(0);
    talk.unk53CCE8 = 0;
    talk.unk53CCEC = 0;
    talk.unk53CCF0 = 0;
    battle.enemyFaded = 0;
    battle.unk53BFC0 = 0;
    talk.cursor += 4 * sizeof(u16);
    return 1;
}

// Command 0x3B of FightTalkFrame (inline in the original): as command 2, a window of one line placed at the unit (its
// drawing x and the y of its origin; a role's 0x3C left and 0x14 above, a speaker from 1000 at the fixed place).
int Game::FightTalkLineWindow()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if ((this->TalkCommandUnit() & kWindowShown) == 0)
    {
        const u16 unit = talk.UnsignedWord(1);
        this->SetTalkCommandUnit(unit);
        talk.textAt = talk.cursor + sizeof(u16);
        auto window = std::make_unique<TalkWindow>();
        u16 x = 0;
        u16 y = 0;

        if (unit >= kFixedSpeaker)
        {
            x = kFixedSpeakerX;
            y = kFixedSpeakerY;
        }
        else
        {
            BattleUnit& speaker = this->TalkUnit(unit);
            speaker.GetDrawPosition(x, y);
            u16 originX = 0;
            speaker.GetOrigin(originX, y);
            talk.unk53CE74 = originX;

            if (unit <= kLastEnemy)
            {
                for (BattleEnemy& enemy : this->battleEnemies)
                {
                    enemy.SetAC0(0);
                }
            }
            else
            {
                for (BattleRole& role : this->battleRoles)
                {
                    role.SetAC0(0);
                }

                speaker.SetAction(kTalkAction);
                x = static_cast<u16>(x - kRoleLineShiftX);
                y = static_cast<u16>(y - kRoleLineShiftY);
            }
        }

        talk.unk53CE78 = static_cast<i32>(x | (static_cast<u32>(y) << kHighWordShift));
        window->flags = TalkWindow::FixedPlace;
        const std::span<const u8> text =
            std::span<const u8>(talk.script).subspan(std::min(talk.textAt, talk.script.size()));
        const std::string_view name(talk.speaker.data(), strnlen(talk.speaker.data(), talk.speaker.size()));
        this->talk.SetupDialog(*window, text, name, Talk::OneLine, static_cast<i16>(x), static_cast<i16>(y));
        talk.window = window.get();
        this->talk.AppendWindow(std::move(window));
        talk.window->timeout |= kNoTimeout;
        talk.window->speed = kWindowSpeed;
        talk.window->flags |= static_cast<u16>(talk.windowFlags);
        talk.windowFlags = static_cast<i32>(static_cast<u32>(talk.windowFlags) & ~kLowWordMask);
        talk.window->x = x;
        talk.window->y = y;
        talk.window->fromX = x;
        talk.window->fromY = y;
        this->SetTalkCommandUnit(static_cast<u16>(this->TalkCommandUnit() | kWindowShown));
        battle.unk53BFC0 = 1;
        battle.result = 0;
    }

    if (this->scene.talkWindows != nullptr)
    {
        talk.battleRuns = 0;
        this->BattleFrame();
        return 1;
    }

    this->ClearTalkMarks();
    this->SkipTalkText();
    return 1;
}

// Command 0x3E of FightTalkFrame (inline in the original): the field fades from the colour of words 2..4 to that of
// words 5..7 over the frames of word 8 (zero-extended: 0x0047D539 clears ecx before 0x0047D55D loads the word).
int Game::FightTalkFieldFade()
{
    ColorFade& fade = this->battleState.fieldFade;
    FightTalk& talk = this->fightTalk;
    fade.red = static_cast<float>(talk.Word(1));
    fade.green = static_cast<float>(talk.Word(2));
    fade.blue = static_cast<float>(talk.Word(3));
    talk.unk53CE98 = talk.UnsignedWord(4);
    talk.unk53CE9A = talk.UnsignedWord(5);
    talk.unk53CE9C = talk.UnsignedWord(6);
    const i32 toRed = static_cast<i16>(talk.unk53CE98);
    const i32 toGreen = static_cast<i16>(talk.unk53CE9A);
    const i32 toBlue = static_cast<i16>(talk.unk53CE9C);
    fade.startRed = static_cast<float>(toRed);
    fade.startGreen = static_cast<float>(toGreen);
    fade.startBlue = static_cast<float>(toBlue);
    fade.frames = static_cast<i32>(talk.UnsignedWord(7));
    const auto frames = static_cast<double>(fade.frames);
    fade.stepRed = static_cast<float>(static_cast<double>(toRed - static_cast<i32>(crt::Ftol(fade.red))) / frames);
    fade.stepGreen =
        static_cast<float>(static_cast<double>(toGreen - static_cast<i32>(crt::Ftol(fade.green))) / frames);
    fade.stepBlue = static_cast<float>(static_cast<double>(toBlue - static_cast<i32>(crt::Ftol(fade.blue))) / frames);
    talk.cursor += 8 * sizeof(u16);
    talk.unk53CE98 = 0;
    talk.unk53CE9A = 0;
    talk.unk53CE9C = 0;
    return 1;
}

// Commands 0x41 and 0x42 of FightTalkFrame (inline in the original): 0x41 jumps to the place after it when the
// inventory holds (counted and pending) at least the third word of the object of the second; 0x42 takes that many away
// (the count first; an object left with none leaves the list; a missing object faults as in the original).
int Game::FightTalkObjects(bool take)
{
    FightTalk& talk = this->fightTalk;
    const i16 id = talk.Word(1);
    talk.unk53CE90 = static_cast<i32>(
        (static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (static_cast<u32>(static_cast<u16>(id)) << kHighWordShift)
    );
    u16 wanted = talk.UnsignedWord(2);
    ItemNode* node = FindItemOfKind(this->party.inventory, id);

    auto finish = [this, &talk](std::size_t bytes)
    {
        this->BattleFrame();
        talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
        talk.unk53CE94 = static_cast<i32>(static_cast<u32>(talk.unk53CE94) & ~kLowWordMask);
        talk.cursor += bytes;
        return 1;
    };

    if (!take)
    {
        talk.unk53CE94 = static_cast<i32>((static_cast<u32>(talk.unk53CE94) & ~kLowWordMask) | wanted);

        if (node != nullptr && node->count + node->pending >= wanted)
        {
            talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
            talk.cursor += 3 * sizeof(u16);
            talk.Seek(talk.Word(0));
            return 1;
        }

        return finish(5 * sizeof(u16));
    }

    if (node == nullptr)
    {
        Fault(this->errors, u8"battle talk command 0x42: the object is not in the inventory");
    }

    if (node->count != 0)
    {
        if (node->count > static_cast<i32>(wanted))
        {
            node->count = static_cast<i16>(node->count - wanted);
            wanted = 0;
        }
        else
        {
            wanted = static_cast<u16>(wanted - node->count);
            node->count = 0;
        }
    }

    if (node->pending != 0)
    {
        node->pending = static_cast<i16>(node->pending - wanted);
    }

    if (node->count == 0 && node->pending == 0)
    {
        UnlinkItem(this->party.inventory, id);
    }

    return finish(3 * sizeof(u16));
}

// Command 0x49 of FightTalkFrame (inline in the original): every unit slides across in the steps of the third word
// until the slide (unk53CE74) reaches the second word; then the talk goes on.
int Game::FightTalkSlideTo()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.unk53CE78 = static_cast<i32>((static_cast<u32>(talk.unk53CE78) & ~kLowWordMask) | talk.UnsignedWord(1));
    talk.unk53CE98 = talk.UnsignedWord(2);

    if (static_cast<i16>(talk.unk53CE98) == 0)
    {
        Fault(this->errors, u8"battle talk command 0x49: an integer division by zero");
    }

    const i32 slide =
        (static_cast<i16>(talk.unk53CE78) - static_cast<i16>(talk.unk53CE74)) / static_cast<i16>(talk.unk53CE98);
    talk.battleRuns = 0;
    battle.shake = slide;
    talk.unk53CE74 = static_cast<u16>(talk.unk53CE74 + slide);

    if (slide == 0)
    {
        talk.cursor += 3 * sizeof(u16);
        talk.unk53CE78 = 0;
        talk.unk53CE74 = 0;
        talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
        talk.unk53CE98 = 0;
        talk.unk53CE76 = 0;
        talk.battleRuns = 1;
        return 1;
    }

    this->ShiftTalkUnits();
    this->MeasureUnits();
    this->BattleFrame();
    return 1;
}

// Command 0x4E of FightTalkFrame (inline in the original): the role of the second word (healed fully and cured of
// bit 0x8000 when defeated) catches the enemy of the third (command 6, at once: the queues and records cleared, the
// role acting); the talk waits until the catch is over (unk53C02C).
int Game::FightTalkCatch()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if (talk.unk53CE98 == 0)
    {
        const u16 unit = talk.UnsignedWord(1);
        talk.unk53CE90 = static_cast<i32>(
            (static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (static_cast<u32>(unit) << kHighWordShift)
        );
        battle.aimTarget = talk.Word(2) + 1;
        BattleRole& role = this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)];

        if (role.IsDefeated())
        {
            role.CureEffects(this->battleServices, BattleUnit::ReviveBit);
            role.Damage(this->battleServices, -kMaxVital, kMaxVital, kMaxVital);
        }

        battle.currentUnit = 0;
        role.Retarget(static_cast<i16>(battle.aimTarget - 1));
        role.SetCommand(kCatchCommand);
        battle.turnCommand = kCatchCommand;
        battle.roleOrder.fill(0);
        battle.slotRecords.fill(BattleSlotRecord{});
        battle.turnTaken[unit] = kCatchCommand;
        battle.unk53BF74 = 1;
        battle.unk53BFC0 = 1;
        battle.retargeted = 1;
        battle.hitsAllRoles = 0;
        battle.actingUnit = unit;
        this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].IncreaseHits();
        battle.unk53C02C = 1;
        talk.unk53CE98 = 1;
    }

    talk.battleRuns = 1;
    this->BattleFrame();

    if (battle.unk53C02C != 0)
    {
        return 1;
    }

    talk.unk53CE98 = 0;
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
    talk.cursor += 3 * sizeof(u16);
    return 1;
}

// Command 0x4F of FightTalkFrame (inline in the original): the unit of the second word takes the damage of words 3..5
// (an enemy is healed by the first: the original negates it), a role with its damage digits.
int Game::FightTalkDamage()
{
    FightTalk& talk = this->fightTalk;
    const u16 unit = talk.UnsignedWord(1);
    this->SetTalkCommandUnit(unit);
    talk.unk53CE98 = talk.UnsignedWord(2);
    talk.unk53CE9A = talk.UnsignedWord(3);
    talk.unk53CE9C = talk.UnsignedWord(4);

    if (unit <= kLastEnemy)
    {
        this->battleEnemies[unit].Damage(
            this->battleServices,
            -static_cast<i32>(static_cast<i16>(talk.unk53CE98)),
            static_cast<i16>(talk.unk53CE9A),
            static_cast<i16>(talk.unk53CE9C)
        );
    }
    else
    {
        this->SetTalkCommandUnit(static_cast<u16>(unit - BattleState::FirstRoleUnit));
        BattleRole& role = this->battleRoles[this->TalkCommandUnit()];
        role.Damage(
            this->battleServices,
            static_cast<i16>(talk.unk53CE98),
            static_cast<i16>(talk.unk53CE9A),
            static_cast<i16>(talk.unk53CE9C)
        );
        role.RaiseFlag26B8(this->battleState);
        role.PushTarget(BattleUnit::DamageTarget);
        role.SetDamageDigits(this->battleServices, static_cast<i16>(talk.unk53CE98));
        role.StepTargetCount(this->battleServices, 1);
    }

    talk.unk53CE98 = 0;
    talk.unk53CE9A = 0;
    talk.unk53CE9C = 0;
    this->SetTalkCommandUnit(0);
    talk.cursor += 5 * sizeof(u16);
    return 1;
}

// Commands 0x29..0x53 of FightTalkFrame not handled on their own (inline in the original); see FightTalkFrame.
int Game::FightTalkMisc(u16 command)
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    switch (command)
    {
        case kTalkIntro:
            return this->FightTalkIntro();

        case kTalkQuietWin:
            battle.enemiesGone = static_cast<u8>(battle.enemyCount);
            battle.unk53BF02 = 0;
            battle.victoryWithoutPanel = 1;
            battle.result = kEndCheckState;
            battle.autoBattle = 0;
            this->audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
            talk.wonByTalk = 1;
            talk.battleRuns = 0;
            this->BattleFrame();
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkSetFlag:
            this->state.SetGameFlag(talk.Word(1));
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkClearFlag:
            this->state.ClearGameFlag(talk.Word(1));
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkFlagJump:
            talk.unk53CCE8 = talk.Word(1);

            if (this->state.GameFlag(talk.unk53CCE8))
            {
                talk.cursor += 2 * sizeof(u16);
                talk.Seek(talk.Word(0));
                return 1;
            }

            talk.cursor += 4 * sizeof(u16);
            return 1;

        case kTalkPartyGone:
            battle.defeatedRoles = kNoRolesLeft;
            battle.roleCount = 0;
            this->MeasureUnits();
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkJoinFirst:
        {
            const auto joined = static_cast<std::size_t>(talk.Word(1));

            // An enemy past the enemies is read and written past them by the original.
            if (joined >= this->battleEnemies.size())
            {
                talk.cursor += 2 * sizeof(u16);
                return 1;
            }

            BattleEnemy& enemy = this->battleEnemies[joined];
            talk.unk53CE90 = static_cast<i32>(
                (static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (static_cast<u32>(enemy.ActId()) << kHighWordShift)
            );
            ReadPosition(this->battleEnemies[0], talk.unk53CCE8, talk.unk53CCEC);
            enemy.SetPosition(static_cast<i16>(talk.unk53CCE8), static_cast<i16>(talk.unk53CCEC));
            talk.cursor += 2 * sizeof(u16);
            talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
            return 1;
        }

        case kTalkDefeatPose:
            // An enemy past the enemies is written past them by the original.
            if (static_cast<std::size_t>(talk.Word(1)) < this->battleEnemies.size())
            {
                this->battleEnemies[static_cast<std::size_t>(talk.Word(1))].SetDefeatPoseFixed(
                    static_cast<u8>(talk.UnsignedWord(2))
                );
            }

            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkAttributes:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            this->TalkUnit(this->TalkCommandUnit()).SetAttributes(talk.Word(2), talk.Word(3), talk.Word(4));
            talk.cursor += 5 * sizeof(u16);
            return 1;

        case kTalkAddLoot:
            return this->FightTalkAddLoot();

        case kTalkAllCounted:
            return this->FightTalkCounters();

        case kTalkCurve:
            return this->FightTalkCurve();

        case kTalkSlide:
            return this->FightTalkSlide();

        case kTalkSpeaker:
            return this->FightTalkSpeaker();

        case kTalkTimedWindow:
            return this->FightTalkTimedWindow();

        case kTalkMirror:
            return this->FightTalkMirror();

        case kTalkLineWindow:
            return this->FightTalkLineWindow();

        case kTalkFieldFade:
            return this->FightTalkFieldFade();

        case kTalkHasObjects:
            return this->FightTalkObjects(false);

        case kTalkTakeObjects:
            return this->FightTalkObjects(true);

        case kTalkSlideTo:
            return this->FightTalkSlideTo();

        case kTalkEnemyCommand:
            return this->FightTalkEnemyCommand();

        case kTalkLearn:
            talk.unk53CCE8 = talk.Word(1);
            talk.unk53CCEC = talk.Word(2);  // 0x0047CCF3: movsx

            if (LearnSkill(
                    talk.unk53CCE8 - BattleState::FirstRoleUnit,
                    static_cast<i16>(talk.unk53CCEC),
                    this->party,
                    this->database,
                    battle.lastLearned,
                    this->errors
                ) == 1)
            {
                battle.learnedPending = 1;
            }

            talk.unk53CCE8 = 0;
            talk.unk53CCEC = 0;
            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkWords66:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));

            // 0x0047CD70: always an enemy; a unit past them is written past the enemies.
            if (this->TalkCommandUnit() < this->battleEnemies.size())
            {
                this->battleEnemies[this->TalkCommandUnit()]
                    .SetWords66(talk.Word(2), talk.Word(3), talk.Word(4), talk.Word(5), talk.Word(6), talk.Word(7));
            }

            talk.cursor += 8 * sizeof(u16);
            return 1;

        case kTalkBackgroundAgain:
            battle.panelFlags |= kBackgroundPanel;
            this->LoadBattleBackground(battle.formation.background, 1);
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkPartyOrder:
            if (talk.Word(1) <= kLastEnemy)
            {
                this->FightTalkEnemyFollows(talk.Word(1));
                talk.cursor += 2 * sizeof(u16);
                return 1;
            }

            talk.savedUnit = battle.currentUnit;
            battle.turnUnit = talk.Word(1);
            battle.aimTarget = talk.Word(2) + 1;

            if (battle.aimTarget > kLastEnemy)
            {
                battle.unk53BFD0 = 1;
                battle.aimTarget -= BattleState::FirstRoleUnit;
                battle.pendingOrders[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].unk00 = 1;
            }

            this->battleRoles[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].SetTarget(1);
            battle.turnCommand = 1;
            battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = 1;
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkMusic:
            return this->FightTalkMusic();

        case kTalkFadeWait:
            if (battle.fieldFade.frames <= 0)
            {
                talk.cursor += sizeof(u16);
            }

            talk.battleRuns = 0;
            this->BattleFrame();
            talk.battleRuns = 1;
            return 1;

        case kTalkPictureWait:
        {
            const u16 unit = talk.UnsignedWord(1);
            this->SetTalkCommandUnit(unit);
            talk.unk53CE98 = talk.UnsignedWord(2);

            if (this->TalkUnit(unit).ShowsPicture(static_cast<i16>(talk.unk53CE98)))
            {
                this->SetTalkCommandUnit(0);
                talk.unk53CE98 = 0;
                talk.cursor += 3 * sizeof(u16);
            }

            this->BattleFrame();
            this->SetTalkCommandUnit(0);
            talk.unk53CE98 = 0;
            return 1;
        }

        case kTalkCritical:
            battle.criticalAttack = 1;
            talk.cursor += sizeof(u16);
            this->BattleFrame();
            return 1;

        case kTalkPlace:
            talk.unk53CCE8 = talk.Word(1);
            talk.unk53CE98 = talk.UnsignedWord(2);
            talk.unk53CE9A = talk.UnsignedWord(3);
            this->TalkUnit(talk.unk53CCE8)
                .SetPosition(static_cast<i16>(talk.unk53CE98), static_cast<i16>(talk.unk53CE9A));
            talk.unk53CCE8 = 0;
            talk.unk53CE98 = 0;
            talk.unk53CE9A = 0;
            talk.battleRuns = 0;
            this->BattleFrame();
            talk.battleRuns = 1;
            talk.cursor += 4 * sizeof(u16);
            return 1;

        case kTalkPanel8:
            talk.cursor += sizeof(u16);
            battle.panelFlags |= BattleState::UnkPanel8;
            return 1;

        case kTalkLevel:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));

            // 0x0047BD81: always an enemy, as command 0x38.
            if (this->TalkCommandUnit() < this->battleEnemies.size())
            {
                this->battleEnemies[this->TalkCommandUnit()].SetLevel(talk.Word(2));
            }

            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkLose:
            talk.cursor += sizeof(u16);
            battle.defeatedRoles = static_cast<u8>(battle.roleCount);
            battle.panelFlags |= kLostPanel;
            battle.result = kBattleLostState;
            return 1;

        case kTalkBackground:
            this->LoadBattleBackground(talk.Word(1), 1);
            talk.cursor += 5 * sizeof(u16);
            return 1;

        case kTalkResistances:
        {
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            std::array<i16, kResistanceWords> values = {};

            for (std::size_t index = 0; index < values.size(); ++index)
            {
                values[index] = talk.Word(index + 2);
            }

            // 0x0047D979: always an enemy, as command 0x38.
            if (this->TalkCommandUnit() < this->battleEnemies.size())
            {
                this->battleEnemies[this->TalkCommandUnit()].SetResistances(values);
            }

            talk.cursor += (kResistanceWords + 2) * sizeof(u16);
            return 1;
        }

        case kTalkWords72:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));

            // 0x0047CDC5: always an enemy, as command 0x38.
            if (this->TalkCommandUnit() < this->battleEnemies.size())
            {
                this->battleEnemies[this->TalkCommandUnit()]
                    .SetWords72(talk.Word(2), talk.Word(3), talk.Word(4), talk.Word(5));
            }

            talk.cursor += 6 * sizeof(u16);
            return 1;

        case kTalkKill:
        {
            const u16 unit = talk.UnsignedWord(1);
            this->SetTalkCommandUnit(unit);

            // The original cures the role 0x14 units before an enemy's number, outside the roles (deviations.md): only
            // a role is cured.
            if (unit > kLastEnemy)
            {
                this->TalkUnit(unit).CureEffects(this->battleServices, BattleUnit::ReviveBit);
            }

            if (unit <= kLastEnemy)
            {
                this->battleEnemies[unit].Damage(this->battleServices, kEnemyKillHeal, 0, 0);
            }
            else
            {
                this->TalkUnit(unit).Damage(this->battleServices, -kMaxVital, kMaxVital, kMaxVital);
            }

            talk.cursor += 2 * sizeof(u16);
            return 1;
        }

        case kTalkPanel40:
            talk.cursor += sizeof(u16);
            battle.panelFlags |= kUnkPanel40;
            return 1;

        case kTalkCatch:
            return this->FightTalkCatch();

        case kTalkDamage:
            return this->FightTalkDamage();

        case kTalkReload:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            talk.unk53CCE8 = talk.Word(2);

            // 0x0047DDF9: always an enemy, as command 0x38.
            if (this->TalkCommandUnit() < this->battleEnemies.size())
            {
                this->battleEnemies[this->TalkCommandUnit()]
                    .Reload(static_cast<u16>(talk.unk53CCE8), this->database, this->acts);
            }

            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkUsedItem:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));

            if (battle.usedUpItem == this->TalkCommandUnit())
            {
                battle.usedUpItem = 0;
                this->SetTalkCommandUnit(0);
                talk.cursor += 2 * sizeof(u16);
                talk.Seek(talk.Word(0));
                return 1;
            }

            talk.cursor += 4 * sizeof(u16);
            return 1;

        case kTalkQuake:
            battle.quakeStepFrames = kQuakeFrames;
            this->SetTalkCommandUnit(talk.UnsignedWord(1));

            if (talk.UnsignedWord(1) == 1)
            {
                battle.panelFlags |= BattleState::UnkPanel100;
            }
            else
            {
                battle.panelFlags &= ~BattleState::UnkPanel100;
                battle.quakeStepFrames = 0;
            }

            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkNoRetreat:
            battle.panelFlags |= BattleState::NoRetreat;
            talk.cursor += sizeof(u16);
            return 1;

        case kTalkRepeatLimit:
            talk.repeatLimit = talk.UnsignedWord(1);
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkChanceJump:
            return this->FightTalkChanceJump();

        case kTalkRaiseLevel:
            this->SetTalkCommandUnit(talk.UnsignedWord(1));
            talk.unk53CE90 = static_cast<i32>(
                (static_cast<u32>(talk.unk53CE90) & kLowWordMask) |
                (static_cast<u32>(talk.UnsignedWord(2)) << kHighWordShift)
            );
            this->RaiseLevel(
                static_cast<u16>(this->TalkCommandUnit() - BattleState::FirstRoleUnit),
                talk.UnsignedWord(2)
            );
            this->SetTalkCommandUnit(0);
            talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
            talk.cursor += 3 * sizeof(u16);
            return 1;

        case kTalkAddGold:
            this->party.variables[Party::GoldVariable] += talk.Word(1);
            talk.cursor += 2 * sizeof(u16);
            return 1;

        case kTalkPayGold:
        {
            const i16 price = talk.Word(1);
            i32& gold = this->party.variables[Party::GoldVariable];

            // An unsigned compare of the gold with the price sign-extended (0x0047E025).
            if (static_cast<u32>(gold) < static_cast<u32>(static_cast<i32>(price)))
            {
                talk.cursor += 4 * sizeof(u16);
                return 1;
            }

            talk.cursor += 2 * sizeof(u16);
            gold -= price;
            talk.Seek(talk.Word(0));
            return 1;
        }

        case kTalkNoHitTakeBack:
            battle.panelFlags |= BattleState::UnkPanel800;
            talk.cursor += sizeof(u16);
            return 1;

        default:
            return 1;
    }
}

// Command 0x29 of FightTalkFrame (inline in the original): the opening of the battle (BattleIntro with the second word)
// unless the units were drawn moved already (drawMovedAtOnce, set once it ran); then the battle runs its frames until
// the start pose is over (the talk goes on) or a frame ends the battle (the talk ends with it, its end returned).
int Game::FightTalkIntro()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;

    if (talk.drawMovedAtOnce == 0)
    {
        talk.drawMovedAtOnce = 0;
        this->BattleIntro(talk.UnsignedWord(1));
        talk.drawMovedAtOnce = 1;
    }

    if ((battle.startPoseRole & BattleState::StartPoseDone) != 0)
    {
        talk.cursor += 2 * sizeof(u16);
        return 1;
    }

    talk.unk53CCE8 = this->BattleFrame();

    if (talk.unk53CCE8 == 1)
    {
        return 1;
    }

    const i32 end = talk.unk53CCE8;
    this->FightTalkEnd();
    talk.cursor += 2 * sizeof(u16);
    talk.enabled = 1;
    return end;
}

// The enemy part of command 0x3A of FightTalkFrame (inline in the original): enemy `enemy` is ordered to attack the
// role acting for the talk (battleActingUnit) when it is a party member or an escort: its order forced at the role,
// its turn command cleared and its slot record put first.
void Game::FightTalkEnemyFollows(i16 enemy)
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.unk53CCE8 = talk.battleActingUnit;
    const i32 role = talk.unk53CCE8 - BattleState::FirstRoleUnit;

    // An acting unit that is no role is read outside the roles in the original (deviations.md): nothing is ordered.
    if (role < 0 || static_cast<std::size_t>(role) >= this->battleRoles.size())
    {
        return;
    }

    const BattleRole& acting = this->battleRoles[static_cast<std::size_t>(role)];

    if (acting.PartyMember() != 1 && acting.AutoControl() != 1)
    {
        return;
    }

    u16& order = battle.enemyOrders[static_cast<std::size_t>(enemy)];
    order = 0;
    order = static_cast<u16>(role);
    order = static_cast<u16>(order | kOrderForced);
    this->battleEnemies[static_cast<std::size_t>(enemy)].ClearTurnCommand();
    battle.InsertSlotRecord(kEnemySlot, enemy, 0);
}

// Command 0x55 of FightTalkFrame (inline in the original): a roll of the second word below the third jumps to the
// place in the fourth (unk5774E9 set), otherwise the talk goes on after the command.
int Game::FightTalkChanceJump()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    const u16 range = talk.UnsignedWord(1);
    const u16 below = talk.UnsignedWord(2);
    talk.unk53CE90 = static_cast<i32>(
        (static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (static_cast<u32>(range) << kHighWordShift)
    );
    talk.unk53CE94 = static_cast<i32>((static_cast<u32>(talk.unk53CE94) & ~kLowWordMask) | below);
    battle.unk5774E9 = 0;
    const bool hit = this->random.Below(range) < below;
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);
    talk.unk53CE94 = static_cast<i32>(static_cast<u32>(talk.unk53CE94) & ~kLowWordMask);
    talk.unk53CCE8 = 0;

    if (hit)
    {
        battle.unk5774E9 = 1;
        talk.cursor += 3 * sizeof(u16);
        talk.Seek(talk.Word(0));
        return 1;
    }

    talk.cursor += 5 * sizeof(u16);
    return 1;
}

// Command 0x35 of FightTalkFrame (inline in the original): one of the object of the second word added to the
// inventory and to the loot of the battle: counted on its slot when listed (an id from 0x8000 never is: the original
// compares the slot's word with the id sign-extended), otherwise in a new slot. The original writes a new slot past
// the ten after them: those are not kept.
int Game::FightTalkAddLoot()
{
    BattleState& battle = this->battleState;
    BattleMessages& messages = this->battleMessages;
    FightTalk& talk = this->fightTalk;
    const u16 id = talk.UnsignedWord(1);
    talk.unk53CE90 = static_cast<i32>(static_cast<u32>(talk.unk53CE90) & kLowWordMask);

    for (std::size_t slot = 0; slot < battle.lootIds.size(); ++slot)
    {
        if (static_cast<i32>(battle.lootIds[slot]) == static_cast<i32>(static_cast<i16>(id)))
        {
            AddOneItem(this->party.inventory, id, 1, this->database);
            ++messages.lootCounts[slot];
            talk.unk53CE90 =
                static_cast<i32>((static_cast<u32>(talk.unk53CE90) & kLowWordMask) | (1u << kHighWordShift));
            talk.cursor += 2 * sizeof(u16);
            return 1;
        }
    }

    const std::size_t slot = messages.lootCount;
    ObjectRecord* record = AddOneItem(this->party.inventory, id, 1, this->database);

    if (slot < messages.lootNames.size())
    {
        messages.lootNames[slot] = record != nullptr
                                       ? std::string_view(record->data.name.data(), ObjectName(*record).size())
                                       : std::string_view();
        ++messages.lootCounts[slot];
        battle.lootIds[slot] = id;
    }

    ++messages.lootCount;
    talk.cursor += 2 * sizeof(u16);
    return 1;
}

// Command 0x3C of FightTalkFrame (inline in the original): the battle music becomes music\<name> (the text after it,
// ended by %Q): the music stops, the tune becomes the third list and plays at once; BattleIntro then keeps it
// (BattleState::MusicGiven).
int Game::FightTalkMusic()
{
    BattleState& battle = this->battleState;
    FightTalk& talk = this->fightTalk;
    talk.textAt = talk.cursor + sizeof(u16);
    std::size_t length = 0;

    while (length < kTextEndSearch)
    {
        const std::size_t at = talk.textAt + length;

        if (at + 1 < talk.script.size() && talk.script[at] == kTextEscape && talk.script[at + 1] == kTextEnd)
        {
            break;
        }

        ++length;
    }

    const std::size_t available = talk.textAt < talk.script.size() ? talk.script.size() - talk.textAt : 0;
    const std::string name(
        reinterpret_cast<const char*>(talk.script.data()) + std::min(talk.textAt, talk.script.size()),
        std::min(length, available)
    );
    talk.textAt += length + 2;
    talk.cursor = talk.textAt;
    battle.battleMusic = PathFormat("%smusic\\%s", {this->config.installDirectory, std::string_view(name.c_str())});
    MusicQueue& queue = this->audio.Queue();
    queue.Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
    queue.SetSlot(battle.battleMusic, kBattleMusicId, 0, MusicQueue::ThirdList);
    queue.Switch(MusicQueue::ThirdList, MusicQueue::RepeatList, 0);
    queue.SetVolume(MusicQueue::MaxVolume * this->config.musicVolume / Config::MaxVolume);
    this->AudioPump();
    battle.panelFlags |= BattleState::MusicGiven;
    return 1;
}

// Command 0x3D of FightTalkFrame (inline in the original): the units listed after it (ended by 0xFFFF) are checked in
// turn; once as many have no counter (Counter2A76) as are listed, the talk jumps to the place after the list;
// otherwise it goes on two words past that place. An empty list keeps the talk on the command.
int Game::FightTalkCounters()
{
    FightTalk& talk = this->fightTalk;
    u16 count = 0;
    talk.unk53CE9E = 0;
    talk.battleRuns = 0;

    while (talk.UnsignedWord(static_cast<std::size_t>(count) + 1) != kListEnd)
    {
        ++count;
        talk.unk53CE9E = count;
    }

    u16 cleared = 0;

    if (count != 0)
    {
        for (u16 index = 0; index < count; ++index)
        {
            const u16 unit = talk.UnsignedWord(static_cast<std::size_t>(index) + 1);
            talk.battleRuns = 0;
            this->SetTalkCommandUnit(unit);

            if (this->TalkUnit(unit).Counter2A76() == 0 && ++cleared == count)
            {
                talk.cursor += (static_cast<std::size_t>(count) + 2) * sizeof(u16);
                talk.Seek(talk.Word(0));
                break;
            }
        }
    }

    talk.unk53CE94 = 0;

    if (cleared == count)
    {
        talk.unk53CE9E = 0;
    }
    else
    {
        talk.cursor += (static_cast<std::size_t>(count) + kCountedSkipWords) * sizeof(u16);
    }

    return 1;
}

}  // namespace swd3e
