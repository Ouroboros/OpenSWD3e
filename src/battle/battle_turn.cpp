#include "battle/battle_turn_constants.h"
#include "battle/smooth/smooth_turn.h"
#include "core/fault.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "font/text_font.h"
#include "game/game.h"
#include "item/item_list.h"
#include "org/fame.h"
#include "pic/picture_ops.h"
#include "platform/display.h"
#include "platform/system_info.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

constexpr i32 kBattleWon = 0x63;
constexpr i32 kBattleLost = 0x67;
constexpr i32 kTurnsRunning = 1;           // BattleState::result while no side has lost
constexpr u32 kGhostRescueAct = 0x17B7;    // ApplyHits: the act of the ghost healing its master (swd3e)
constexpr int kGhostRescuePercent = 0x50;  // of the master's hit points
constexpr u16 kBossReward = 0x14;          // battleProficiency gained by an enemy of ObjectData::UnkKind20
constexpr u16 kEnemyReward = 3;
constexpr i16 kDefeatCount = 1;         // the bestiary count of a defeated enemy
constexpr u16 kItemsBackSound = 0x117;  // RemoveDefeated (swd3e): the stolen items back
constexpr i16 kItemsBackFrames = 0x32;
constexpr std::u8string_view kItemsBackText = u8"取回被偷的物品!!";  // 0x004BF3C4
constexpr i16 kLastStandingCause = 2;  // the enrage causes of a defeat: the last enemy standing, a role down
constexpr i16 kRoleDownCause = 0x17;
constexpr u32 kGhostGoneStatus = 0x5008;   // the status of the ghost when battle role 0 falls
constexpr u32 kChargeEndsAilment = 0x200;  // the ailment that ends 蓄勁 (DrawBattleMenu)
constexpr int kChargePanelX = 0x32;
constexpr u32 kRandomActs = 3;  // RandomRoleAct: guard, attack or leave
constexpr u32 kRandomGuard = 0;
constexpr u32 kRandomLeave = 2;
constexpr std::size_t kLastTurnUnit = BattleState::RoleUnits - 1;

constexpr u16 kThrownObject = 0x300;  // the object a thrown attack takes (given back unless it lands)
constexpr i32 kLostEnd = 2;           // frameResult when the party is down
constexpr i32 kLostQuietly = 0x68;    // a battle state that ends with frameResult 0
constexpr i32 kPromptFrames = 0x12C;
constexpr i32 kWaitState1 = 1;  // the battle states of the wait pointer
constexpr i32 kWaitState2 = 2;
constexpr i32 kTurnState = 3;
constexpr i32 kWaitState4 = 4;
constexpr i32 kWaitState5 = 5;
constexpr i32 kWaitState8 = 8;
constexpr i32 kWaitState1B = 0x1B;
constexpr i32 kWaitState1E = 0x1E;
constexpr i32 kWaitState29 = 0x29;     // swd3e
constexpr i32 kWaitState50 = 0x50;     // swd3e
constexpr i32 kWaitStateA = 0xA;       // swd3e: 融合 places its escort (the keys as state 5)
constexpr i32 kMenuSlideStart = 0x50;  // BattleState::menuSlide when a list of state 0x29 or 0x50 scrolls (swd3e)
constexpr i32 kTurnState9 = 9;         // swd3e: prompts as kTurnState
constexpr int kNoUnitPointer = 0x238C;
constexpr int kUnitPointer = 0x238D;
constexpr int kWaitPointer = 0x238E;
constexpr int kTurnPointer = 0x238F;
constexpr int kEnemyPointer = 0x2393;  // NextEnemyTurn: the pointer over the enemy taking its turn (swd3e)
constexpr int kYinYangScale = 0x960;   // YinYangAttack (swd3e): the damage in percent of the proficiency
constexpr int kYinYangPercent = 100;
constexpr int kYinYangBase = 100;
constexpr i32 kYinYangAct = 0x1775;  // the act after the cast pose
constexpr u16 kGrayBandCloses = 2;   // BattleState::grayBandState
constexpr u16 kYinYangHitPicture = 0x2389;
constexpr i32 kYinYangMost = 0x270F;  // the number shown is at most 9999
constexpr u8 kYinYangDefeatPose = 6;
constexpr u16 kWhitePixel = 0xFFFF;       // the frame whitened by a defeat (0xFF bytes)
constexpr i8 kTargetFewestHitPoints = 1;  // PickRoleTarget (swd3e): the criteria
constexpr i8 kTargetMostSpirit = 2;
constexpr i8 kTargetMostStamina = 3;
constexpr i8 kTargetLeastAgility = 4;
constexpr i8 kTargetLeastDefense = 5;
constexpr i8 kTargetMostAttack = 6;
constexpr int kNoTurnPointer = 0x2393;
constexpr int kMarkEffect = 0x23A0;
constexpr i32 kListPage = 7;  // a back key scrolls a list up by this
constexpr u16 kBackSound = 0x2E;
constexpr i32 kListState2 = 2;
constexpr i32 kListState4 = 4;
constexpr i32 kListState1B = 0x1B;
// The panel units of SelectPreviousRole and SelectNextRole keep the numbering of PanelUnits (8..11, 0x0046F38F),
// below FirstRoleUnit: the current unit (a role unit) is never among them, so the role keys never move the panel.
constexpr i32 kFirstPanelUnit = 8;
constexpr i32 kLastPanelUnit = 0xB;
constexpr u16 kNoPanelRole = 0xFFFF;
constexpr i32 kMenuState = 1;  // the command menu

constexpr i32 kMenuFixedCommands = 5;  // before the accessory commands

constexpr i32 kMenuStep = 5;
constexpr i32 kMenuStepMost = 10;
constexpr i32 kElements = 5;           // the elements of 改變
constexpr u16 kGhostListed = 2;        // 煉妖 and 符鬼 only: the menu keeps 符鬼
constexpr i32 kGatheringCommands = 2;  // the role that took 蓄勁 has commands 1 and 2

constexpr std::size_t kAccessorySlots = 2;
constexpr std::size_t kFirstAccessorySlot = 7;  // the equipment slots of the accessories
constexpr std::size_t kClearedNameStart = 4;    // AnnounceMastery: the name bytes of an emptied accessory cleared
constexpr std::u8string_view kEmptySlotName = u8"無";  // 0x004AF630 the name of an emptied accessory

constexpr u16 kLastAccessoryCommand = 0x31;  // swd3e: past the names, kinds 0x2F..0x31 still pass
constexpr u16 kGhostMenuCommand = 7;         // 符鬼, its second with game flag 0x5A (swd3e)
constexpr u16 kGatherCommand = 0x27;         // 蓄勁: below kGatherLeast percent the command is off
constexpr u16 kStrikeBackCommand = 0x28;     // 逆攻 (釋放 while the meter shows)
constexpr u16 kFuseCommand = 0x29;           // 融合: off without escorts
constexpr u16 kGatherLeast = 10;
constexpr u16 kFullProficiency = 100;

constexpr i32 kTargetState = 3;  // battle states of the menu
constexpr i32 kTalkState = 7;
constexpr i32 kSkillListState = 8;
constexpr i32 kSpellCommand = 2;  // the menu commands that open a list
constexpr i32 kItemCommand = 3;
constexpr i32 kSkillCommand = 4;
constexpr u16 kNoTalkRole = 0xFFFF;
constexpr i32 kListRows = 5;

constexpr u16 kLevelUpSound = 0x12B;
constexpr std::u8string_view kNewSpellLine = u8"%s學到新奇術";    // 0x004BF830
constexpr std::u8string_view kNewSpecialLine = u8"%s學到新絕技";  // 0x004BF840

constexpr std::size_t kNeededBattles = 2;  // RoleBattleBlock::unk00: the battles the next special needs
constexpr std::size_t kNextSpecial = 3;    // the next special skill with bit 31, -1: none
constexpr i32 kNoSpecial = -1;
constexpr u32 kSpecialMark = 0x80000000u;
constexpr u32 kSpecialIdMask = 0x7FFFFFFF;
constexpr u16 kSpecialType = 0x14;  // the skill type LearnSpecials reads (swd3e; swd32 0x1F)

constexpr i32 kLastMenuCommand = 9;      // swd3e: the last command of the grid (the up key from below 0, the down key)
constexpr i32 kFewAimRoles = 3;          // AimStep: fewer roles step through the roles, more through RoleCycle
constexpr i32 kPastRoleCycle = -1;       // AimStep: the string bytes past RoleCycleRead name no role
constexpr i32 kListSlideStart = 0x50;    // the lists of states 0x29 and 0x50 slide in again from this
constexpr u8 kRefineOptionTaken = 0x80;  // refineOptionsOpen once a 煉妖 choice was taken (MenuConfirm)
constexpr i32 kControlRowsStart = 2;     // controlRows when the 控制 menu closes
constexpr i32 kEscortRows = 2;
constexpr int kHangTries = 2;      // full cycles after which a search of the original would never end
constexpr u32 kDebugPause = 0xC8;  // the debug keys
constexpr u32 kDebugShortPause = 0x64;
constexpr int kDebugMessageX = 0x208;
constexpr int kDebugMessageY = 0xA;
constexpr i16 kDebugMessageFrames = 0x1E;
constexpr std::u8string_view kStrongAttack = u8"強攻擊";    // 0x004BF3F0
constexpr std::u8string_view kNormalPact = u8"正常靈契";    // 0x004BF3E4
constexpr std::u8string_view kAbsolutePact = u8"絕對靈契";  // 0x004BF3D8
constexpr i16 kRefill = -1;
constexpr int kFillFury = -1;
constexpr i16 kDebugAttack = 0x1964;  // swd3e (swd32: 0x26AC)
constexpr i16 kDebugDefense = 0x9B;
constexpr i16 kDebugAgility = 0x64;  // swd3e (swd32: 0xC8)
constexpr int kSmallHurt = 0x32;     // swd3e: D hurts without a drain
constexpr int kBigHurt = 0x1F4;      // swd3e: R
constexpr i16 kBigDrain = -5;
constexpr int kSlowFrameInterval = 0x64;  // swd3e: F5
constexpr int kEnemyHurt = 10;
constexpr int kDefeatHurt = 0x7530;
constexpr int kToggleStates = 2;
constexpr std::size_t kClearedTurnFirst = 10;  // swd3e: W clears turnTaken 10..19 (0x28 bytes from 0x0057604C)
constexpr std::size_t kClearedTurnUnits = 10;
constexpr i16 kDebugStep = 10;

constexpr int kPanelsTop = 0x182;  // the pointer over the battle
constexpr int kPanelLeftMargin = 0x18;
constexpr int kPanelWidth = 0x74;
constexpr int kMenuHitLeft = 0xA;
constexpr int kMenuHitRight = 0xAC;  // swd3e: three columns of 0x36
constexpr int kMenuHitTop = 0x28;
constexpr int kMenuHitBottom = 0x70;  // swd3e: three rows
constexpr int kMenuHitColumn = 0x36;
constexpr int kGhostOrdersLeft = 0x158;  // swd3e: the choices of the menu under the pointer
constexpr int kGhostOrdersRight = 0x1B0;
constexpr int kGhostOrdersFirst = 0x8A;
constexpr int kGhostOrdersEnd = 0x14A;
constexpr int kRefineOptionsLeft = 0x180;
constexpr int kRefineOptionsRight = 0x1AC;
constexpr int kRefineOptionsFirst = 0xE4;
constexpr int kRefineOptionsEnd = 0x114;
constexpr int kElementsHitTop = 0x112;
constexpr int kElementsHitBottom = 0x12C;
constexpr int kElementsHitFirst = 0x11C;
constexpr int kElementsHitEnd = 0x194;
constexpr int kGatherHitTop = 0x2C;  // the two commands of the role that took 蓄勁
constexpr int kGatherHitBottom = 0x46;
constexpr int kGatherHitFirst = 0x6A;
constexpr int kMenuColumnWidth = 0x30;
constexpr int kTabsHitTop = 0x82;
constexpr int kTabsHitBottom = 0xA0;
constexpr int kTabsHitFirst = 0x10A;
constexpr int kSpellTabsEnd = 0x188;
constexpr int kItemTabsEnd = 0x1B2;
constexpr int kListHitLeft = 0xE0;
constexpr int kSpellListRight = 0x180;
constexpr int kItemListRight = 0x19C;
constexpr int kThrowListRight = 0x194;
constexpr int kListHitFirst = 0xBE;
constexpr int kListHitEnd = 0x14A;
constexpr int kScrollUpTop = 0x9E;
constexpr int kScrollUpBottom = 0xAE;
constexpr int kSpellScrollLeft = 0x192;
constexpr int kSpellScrollRight = 0x1A4;
constexpr int kSpellScrollDownTop = 0x12A;
constexpr int kSpellScrollDownBottom = 0x13A;
constexpr int kItemScrollLeft = 0x19E;
constexpr int kItemScrollRight = 0x1B0;
constexpr int kItemScrollDownTop = 0x12E;
constexpr int kItemScrollDownBottom = 0x13E;
constexpr int kThrowScrollDownBottom = 0x140;
constexpr i32 kScrollUp = 1;  // BattleState::hoverScrollPart: the scroll part under the pointer
constexpr i32 kScrollDown = 2;
constexpr i32 kScrollAbove = 3;
constexpr i32 kScrollBelow = 4;
constexpr int kEscortHitLeft = 0xC4;
constexpr int kEscortHitRight = 0x178;
constexpr int kEscortHitFirst = 0xEC;
constexpr int kEscortHitEnd = 0x118;
constexpr int kSpecialHitRight = 0x199;
constexpr int kSpecialHitFirst = 0xBA;
constexpr int kSpecialHitEnd = 0x162;
constexpr int kRefineHitLeft = 0xEA;
constexpr int kRefineHitFirst = 0x15A;
constexpr int kRefineHitEnd = 0x23A;
constexpr int kRefineRowFirst = 0xBA;
constexpr int kRefineRowEnd = 0x11E;
constexpr int kHitSquare = 8;
constexpr int kEnemyHitStep = 2;
constexpr int kRoleHitStep = 1;  // every pixel of the square over a role
constexpr i32 kRefineMenuCommand = 6;
constexpr int kPortraitRowTop = 0x180;  // swd3e: HoverTarget, the portraits of the party
constexpr int kPortraitHitMargin = 0xA;
constexpr int kPortraitHitRight = 0x96;

constexpr i32 kFirstAccessoryMenuCommand = 6;
constexpr int kListPointerX = 0x108;
constexpr int kListPointerY = 0xB0;
constexpr int kListMouseX = 0xF0;
constexpr int kListMouseY = 0xC6;
constexpr i32 kOpenListRows = 4;
constexpr i32 kAttackCommand = 1;
constexpr i32 kGuardCommand = 5;
constexpr i32 kAimedCommand15 = 0x15;
constexpr i32 kAimedCommand17 = 0x17;
constexpr i32 kAimedCommand1D = 0x1D;
constexpr i32 kAimedCommand1F = 0x1F;
constexpr i32 kAimedCommand21 = 0x21;
constexpr i32 kAimedCommand25 = 0x25;      // swd3e
constexpr i32 kChangeCommand = 0x26;       // 改變: the elements choice, then the role acts at once (swd3e)
constexpr i32 kLockOrder = 8;              // 鎖定目標, the last order of the ghost: its target is aimed (state 9)
constexpr i32 kCaptureOption = 1;          // 收化, the first 煉妖 choice: aims at an enemy
constexpr i32 kBaitOption = 2;             // 誘餌: state 0x50
constexpr i32 kRefineOptionsSlide = 0x50;  // the menu slides of the choices
constexpr i32 kGhostOrdersSlide = 0x64;
constexpr i32 kElementsSlide = 0x32;
constexpr int kNoticeX = 0x118;  // the notices of MenuConfirm
constexpr int kNoticeY = 0xA;
constexpr i16 kNoticeFrames = 0x14;
constexpr int kNoticeFlags = BattleMessage::BlackBar | BattleMessage::Centered;  // 0x40000002
constexpr std::u8string_view kGatherRefused = u8"法寶經驗不足,無法繼續蓄勁!!";   // 0x004BF5E8
constexpr std::u8string_view kFusedAlready = u8"已融合過,不能再選!!";            // 0x004BF5B0
constexpr std::u8string_view kFuseRefused = u8"法寶經驗不足,無法融合!!";         // 0x004BF598
constexpr std::u8string_view kNoEscortToFuse = u8"沒有護駕,不能融合!!";          // 0x004BF5C4
constexpr int kFuseCategory = 3;  // the object categories of SelectObject for 融合 and 誘餌
constexpr int kBaitCategory = 6;
constexpr i32 kLeaveCommand = 0x11;
constexpr i32 kLeaveState = 6;
constexpr i32 kSelfCommand = 0x16;
constexpr i32 kPartyAimCommand = 0x18;
constexpr i32 kControlMark = 0x10;
constexpr i32 kStealCommand = 0x1A;
constexpr i32 kOnceCommand1C = 0x1C;
constexpr i32 kOnceCommand20 = 0x20;
constexpr i32 kOnceCommand22 = 0x22;
constexpr i32 kOnceCommand23 = 0x23;
constexpr i32 kOnceCommand24 = 0x24;
constexpr i32 kHealPage = 2;
constexpr i32 kThrowOrder = 0xD;
constexpr u16 kNoObjectKind = 0xFFFF;
constexpr i32 kItemCategory3 = 3;
constexpr int kRefusalMessageX = 0x118;
constexpr int kRefusalMessageY = 0xA;
constexpr i16 kRefusalMessageFrames = 0x14;
constexpr int kRefusalMessageFlags = BattleMessage::BlackBar | BattleMessage::Centered;
constexpr std::u8string_view kStaminaShort = u8"體力點數不夠!!";  // 0x004BF5D8
constexpr u16 kRefusedSound = 0x8C;
constexpr i32 kEscortOrder = 0xF;
constexpr i16 kEscortX = 0x177;  // swd3e: the place of a called escort and of the first row
constexpr i16 kSecondEscortX = 0x190;
constexpr i16 kFirstEscortY = 0xC8;
constexpr i16 kSecondEscortY = 0x15E;
constexpr i32 kTalkOrder = 0x63;
constexpr int kRefineCategory = 5;
constexpr i32 kEndCraft = 0x62;  // battle states of the end
constexpr i32 kEndVictory = 0x64;
constexpr i32 kEndLevelUps = 0x65;
constexpr i32 kEndLoot = 0x66;
constexpr i32 kEndStats = 0x6E;
constexpr i32 kEndNewSpell = 0x6F;
constexpr i32 kEndSpecials = 0x70;
constexpr i32 kEndMastery = 0x71;
constexpr i32 kCloseMenuState = 0xC8;
constexpr i32 kEndMessageFrames = 0x14;
constexpr i32 kEndLevelUpFrames = 0x1E;
constexpr u16 kLevelUpPanelSound = 0x2C;
constexpr i16 kLearnedPose = 4;  // the pose of a learned skill (0x00471B93, 0x00475FAC, 0x0047606C)
constexpr u16 kOpenSound = 0x2D;
constexpr i32 kTalkFixedAnswers = 2;  // 攻擊 and 釋放
constexpr i32 kRepeatDelay = 0xF;     // the keys of the battle: held keys repeat from this frame
constexpr i32 kRepeatStep = 3;
constexpr i32 kConfirmRepeatStep = 6;
// swd3e: the keypad keys of commands 1..9 (7 8 9 / 4 5 6 / 1 2 3 as the menu grid)
constexpr std::array<int, 9> kCommandKeys = {0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4F, 0x50, 0x51};
constexpr i32 kUnk577088Bit30 = 0x40000000;  // with bit 31: the confirm key sets bit 0
constexpr int kDigitKey1 = 2;                // the scan code of 1
constexpr i32 kSpaceMark = 1;
constexpr u16 kRetreatMark = 0x4000;
constexpr u32 kRefusedPause = 0x14;
constexpr u32 kRetreatPause = 0x32;
constexpr i16 kRetreatMessageFrames = 5;
constexpr std::u8string_view kNoRetreatText = u8"無法撤退!!";  // 0x004BF540
constexpr i32 kThumbPart = 5;
constexpr i32 kConfirmIdle = 0x12C;
constexpr i32 kChoiceSignBit = 0x8000;
constexpr int kListX = 0xE0;
constexpr int kListY = 0x7E;
constexpr int kSpellListX = 0xE8;
constexpr int kSpellListY = 0x86;
constexpr i32 kListGrown = 7;
constexpr i32 kListOpened = 0xA;
constexpr u16 kListScrollBar = 0x232A;
constexpr int kSpellScrollX = 0x190;
constexpr int kThrowScrollX = 0x192;
constexpr int kListScrollY = 0x9E;
constexpr int kListScrollRows = 4;
constexpr int kEscortListX = 0xC4;
constexpr int kEscortListY = 0xCE;
constexpr int kControlMenuX = 0xC;
constexpr int kControlMenuY = 8;

constexpr int kRoleAimDrop = 0xA;
constexpr int kTargetTagX = 0xC;
constexpr int kTargetTagY = 0xE;
constexpr i32 kEndBack = 0x60;  // BattleEndFrame
constexpr i32 kEndGlow = 0x61;
constexpr i32 kEndCheck = 0x63;
constexpr i32 kEndPanelFrames = 0x96;
constexpr i32 kEndQuietFrames = 0x1E;
constexpr i16 kLevelUpPose = 2;

// The name of listed command `kind` (0x004BF01C by kind): the accessory names, then the five commands (0x2A..0x2E);
// kinds past the table read the menu variables after it as pointers in the original (deviations.md): no name here.
std::u8string_view CommandKindName(u16 kind)
{
    const auto at = static_cast<std::size_t>(kind - kFirstAccessoryCommand);

    if (at < BattleState::AccessoryCommandNames.size())
    {
        return BattleState::AccessoryCommandNames[at];
    }

    const std::size_t fixed = at - BattleState::AccessoryCommandNames.size();
    return fixed < BattleState::FixedCommandNames.size() ? BattleState::FixedCommandNames[fixed] : std::u8string_view{};
}

}  // namespace

// swd3e.exe 0x004729B0: role unit `unit` that may not choose (StatusBit5) acts by chance: one in three guards (the
// menu confirming 防禦), attacks or leaves (without the retreat, NoRetreat, one in two guards or attacks). An attack
// aims, while an enemy stands, at a random role (one in two, with more than one role standing besides the left-out
// and escort ones: not defeated, not the ghost, not itself; unk26B8 bit 28 raised) or a random enemy, going on past
// defeated ones for at most enemyCount tries.
void Game::RandomRoleAct(int unit)
{
    BattleState& battle = this->battleState;
    u32 choice = this->random.Below(kRandomActs);

    if ((battle.panelFlags & BattleState::NoRetreat) != 0)
    {
        choice = this->random.Below(kRandomActs - 1);
    }

    BattleRole& role = this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)];

    auto takeTurn = [&battle, &role, unit](i32 command)
    {
        battle.turnUnit = unit;
        battle.turnCommand = command;
        battle.result = 0;
        battle.unk57725C = 1;
        battle.turnTaken[static_cast<std::size_t>(unit)] = command;
        role.SetTarget(1);
        battle.unk5772C0 = 1;
        battle.unk5772BC = 1;
    };

    if (choice == kRandomGuard)
    {
        battle.menuCommand = kGuardCommand;
        battle.menuGrow = kMenuOpened;
        battle.result = kMenuState;
        battle.unk5772B4 = 1;
        this->MenuConfirm();
        return;
    }

    if (choice == kRandomLeave)
    {
        takeTurn(kLeaveCommand);
        return;
    }

    takeTurn(kAttackCommand);

    if (battle.enemiesGone >= static_cast<i32>(battle.enemyCount))
    {
        return;
    }

    const u32 aimsAtRole = this->random.Below(2);
    const u32 standing = battle.roleCount - battle.defeatedRoles - battle.unk5770D4 - battle.escortCount;

    // An unsigned compare (0x00472AEA).
    if (standing > 1 && aimsAtRole != 0)
    {
        battle.pendingOrders[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].unk00 = 1;
        battle.aimTarget = static_cast<i32>(this->random.Below(battle.roleCount)) + 1;
        this->battleRoles[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].RaiseUnk26B8Bit28();

        while (true)
        {
            const BattleRole& aimed = this->battleRoles[static_cast<std::size_t>(battle.aimTarget - 1)];

            if (!aimed.IsDefeated() && aimed.Ghost() != 1 &&
                battle.aimTarget != battle.turnUnit - (BattleState::FirstRoleUnit - 1))
            {
                return;
            }

            battle.aimTarget = static_cast<i32>(this->random.Below(battle.roleCount)) + 1;
        }
    }

    const auto count = static_cast<i32>(battle.enemyCount);
    battle.aimTarget = static_cast<i32>(this->random.Below(battle.enemyCount)) + 1;
    int tries = 0;

    while (this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].IsDefeated())
    {
        if (++battle.aimTarget > count)
        {
            battle.aimTarget = 1;
        }

        if (++tries >= count)
        {
            break;
        }
    }
}

// swd3e.exe 0x00465460: when `enemy` is unk4BF1CA, each role but `role` aiming at it turns to the first standing enemy
// other than it (retargeted set). With none and one enemy left, such a role's command, target and turn are reset and
// the turn order is cleared.
void Game::RetargetRoles(int role, int enemy)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (battle.unk4BF1CA != enemy)
    {
        return;
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        if (static_cast<int>(index) == role)
        {
            continue;
        }

        BattleRole& unit = this->battleRoles[index];
        const auto aim = static_cast<i16>(unit.GetWord29A2());

        if (aim < 0 || static_cast<u16>(aim) != battle.unk4BF1CA)
        {
            continue;
        }

        const auto count = static_cast<i32>(battle.enemyCount);
        bool turned = false;

        for (int other = 0; other < count; ++other)
        {
            if (battle.unk4BF1CA != other && !this->battleEnemies[static_cast<std::size_t>(other)].IsDefeated())
            {
                unit.Retarget(static_cast<i16>(other));
                battle.retargeted = 1;
                turned = true;
                break;
            }
        }

        if (turned || battle.enemiesGone + 1 != count)
        {
            continue;
        }

        unit.SetCommand(0);
        unit.SetTarget(0);
        unit.ResetTurn(services);
        battle.unk577260 = 0;
        battle.roleOrder = {};
        battle.turnUnit = 0;
        battle.currentUnit = 0;
        battle.actingUnit = -1;
        battle.WipeSlotRecords();
    }
}

// swd3e.exe 0x00464ED0: one frame of the defeat of unit `index` (a role when `role` is 1, else an enemy); 1 once it is
// over. An enemy (none counted once the battle is won) adds its rewards (battleExperience, battleGold unless unk5772F8,
// battleProficiency), halved past the battle talk's repeat limit; one that ran away gives none and no bestiary count;
// its stolen items come back (unless absorbSucceeded) with their notice. All down wins the battle, a last one standing
// is enraged, and a lone standing first enemy of a unk577210 battle ends the others. A role counts as defeated (an
// escort clears its place and returned object: once all are down their places go and the units are measured again),
// drops 蓄勁, its command, slot record, order and its panel; a role with a special act left is not counted. Battle role
// 0 with the ghost takes the ghost out; with no role left standing the battle is lost, else the enemies are enraged.
int Game::RemoveDefeated(int index, int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (role != 1)
    {
        BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(index)];

        if (enemy.DefeatFrame(services) != 1)
        {
            return 0;
        }

        if (battle.result == kEndVictory)
        {
            return 1;
        }

        u16 first = 0;
        u16 second = 0;
        enemy.Unk62And8A(first, second);
        battle.lastDefeatedObject = enemy.Data();
        const bool boss = (battle.lastDefeatedObject->data.kind & ObjectData::UnkKind20) != 0;
        u16 third = boss ? kBossReward : kEnemyReward;
        u8& escaped = battle.escapedEnemies[static_cast<std::size_t>(index)];

        if (escaped == 1)
        {
            escaped = 0;
            first = 0;
            second = 0;
            third = 0;
        }
        else
        {
            AddCounterCapped(this->party.fame.lists[Fame::FameList], static_cast<i16>(enemy.ActId()), kDefeatCount);
        }

        if (battle.absorbSucceeded == 1)
        {
            first = 0;
            battle.absorbSucceeded = 0;
        }
        else if (escaped == 0)
        {
            bool returned = false;

            for (u16& item : battle.stolenItems[static_cast<std::size_t>(index)])
            {
                if (item != 0)
                {
                    AddOneItem(this->party.inventory, item, 1, this->database);
                    item = 0;
                    returned = true;
                }
            }

            if (returned)
            {
                this->audio.PlaySound(kItemsBackSound, this->config.soundVolume);
                this->battleMessages.Add(
                    kRefusalMessageX,
                    kRefusalMessageY,
                    kItemsBackFrames,
                    kItemsBackText,
                    BattleMessage::Frame | BattleMessage::Centered
                );
            }

            escaped = 0;
        }

        // Battle numbers past the table read the variables after it in the original (deviations.md): taken as 0.
        const auto repeat = static_cast<std::size_t>(battle.battle - 1);
        const u8 repeats = repeat < battle.battleRepeats.size() ? battle.battleRepeats[repeat] : u8{0};

        if (this->fightTalk.repeatLimit != 0 && repeats > this->fightTalk.repeatLimit)
        {
            first = static_cast<u16>(first >> 1);
            second = static_cast<u16>(second >> 1);
        }

        battle.battleExperience = static_cast<u16>(battle.battleExperience + first);

        if (battle.unk5772F8 == 0)
        {
            battle.battleGold = static_cast<u16>(battle.battleGold + second);
        }

        battle.battleProficiency = static_cast<u16>(battle.battleProficiency + third);
        this->CureHeldBy(index);
        battle.enemyHits[static_cast<std::size_t>(index)] = 0;
        battle.RemoveSlotRecord(index);
        const auto count = static_cast<i32>(battle.enemyCount);

        if (index >= 0 && index <= count)
        {
            ++battle.enemiesGone;
        }

        if (battle.enemiesGone - battle.unk5771EA >= count)
        {
            battle.result = kBattleWon;
            battle.autoBattle = 0;
            battle.unk5772C0 = 1;
            battle.unk5772BC = 1;
        }

        if (count + battle.unk5771EA - battle.enemiesGone == 1)
        {
            EnrageEnemies(services, kLastStandingCause);
        }

        if (battle.unk577210 != 0 && !this->battleEnemies[0].IsDefeated() &&
            static_cast<i32>(battle.enemyCount) - battle.enemiesGone == 1)
        {
            battle.enemyCount = 1;
            battle.enemiesGone = 0;
            battle.unk577210 = 0;
            this->MeasureUnits();
            battle.SortDrawOrder();
            battle.ListEnemyOrder();
        }

        battle.unk50CE6C[static_cast<std::size_t>(index)] = 0;
        return 1;
    }

    BattleRole& unit = this->battleRoles[static_cast<std::size_t>(index)];

    if (unit.DefeatFrame(services) != 1)
    {
        return 0;
    }

    if (unit.AutoControl() == 1)
    {
        const u16 escorts = battle.escortCount;
        const auto roles = static_cast<i32>(battle.roleCount);
        const i32 row = index + escorts - roles;
        battle.rolePlaces[static_cast<std::size_t>(index)].actId = 0;
        ++battle.unk5771E9;

        // Rows outside the escorts write the words beside givenBack in the original; none here.
        if (row >= 0 && static_cast<std::size_t>(row + 1) < battle.givenBack.size())
        {
            battle.givenBack[static_cast<std::size_t>(row + 1)] = 0;
        }

        if (battle.unk5771E9 < escorts)
        {
            if (row == 1)
            {
                battle.escortCount = static_cast<u16>(escorts - 1);
                battle.roleCount = static_cast<u32>(roles - 1);
                battle.rolePlaces[static_cast<std::size_t>(roles - 1)] = UnitPlacement{};
            }
        }
        else
        {
            // The original clears the places from the end of the list down, before it when there are fewer.
            for (u16 cleared = 0; cleared < escorts && cleared < static_cast<u16>(roles); ++cleared)
            {
                battle.rolePlaces[static_cast<std::size_t>(roles - 1 - cleared)] = UnitPlacement{};
            }

            battle.escortCount = 0;
            battle.roleCount = static_cast<u32>(roles - escorts);
            battle.unk5771E9 = 0;
        }

        this->MeasureUnits();
        battle.SortDrawOrder();
        battle.ListEnemyOrder();
    }
    else
    {
        ++battle.defeatedRoles;
    }

    const int unitId = index + BattleState::FirstRoleUnit;

    if (battle.chargingRole == unitId)
    {
        battle.chargingRole = -1;
    }

    unit.CancelCommand();
    battle.RemoveSlotRecord(unitId);
    battle.unk5376B8[static_cast<std::size_t>(index)] = 0;

    if (battle.currentUnit == unitId)
    {
        for (u32 other = 0; other < battle.roleCount; ++other)
        {
            this->battleRoles[other].SetAC0(0);
        }

        for (u32 other = 0; other < battle.enemyCount; ++other)
        {
            this->battleEnemies[other].SetAC0(0);
        }

        battle.currentUnit = 0;
        battle.turnCommand = 0;
        battle.aimTarget = 1;
        battle.result = kTurnsRunning;
        battle.unk5772C0 = 0;
        battle.unk5772BC = 0;
        battle.unk577270 = 0;
    }

    if (battle.actingUnit == unitId)
    {
        battle.actingUnit = -1;
        battle.unk577270 = 0;
    }

    for (std::size_t at = 0; static_cast<i32>(at) < static_cast<i32>(battle.roleCount); ++at)
    {
        if (battle.roleOrder[at] != unitId)
        {
            continue;
        }

        for (std::size_t next = at; next < kLastTurnUnit; ++next)
        {
            battle.roleOrder[next] = battle.roleOrder[next + 1];
        }

        battle.roleOrder[kLastTurnUnit] = 0;
        break;
    }

    if (unit.StartSpecialAct(battle) == 1)
    {
        --battle.defeatedRoles;
    }

    if (battle.unk5770D4 == 1 && index == 0)
    {
        battle.unk577370 = 1;
        this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)].SetStatus(kGhostGoneStatus);
        battle.RemoveSlotRecord(battle.ghostSlot + BattleState::FirstRoleUnit);
    }

    // Unsigned as in the original (0x004651C7): more left out than in the battle wraps around and enrages.
    if (static_cast<u32>(battle.defeatedRoles) <
        battle.roleCount - battle.guestCount - battle.unk5770D4 - battle.escortCount)
    {
        EnrageEnemies(services, kRoleDownCause);
    }
    else
    {
        battle.result = kBattleLost;
    }

    return 1;
}

// swd3e.exe 0x00465750: the draw order with the acting unit (actingUnit) drawn right after the unit it aims at (none
// while unk577A50, unk575F24 or unk575F28 is set or no unit acts): a role aims at an enemy, an enemy at a role.
void Game::SortActingDrawOrder()
{
    BattleState& battle = this->battleState;

    if (battle.unk577A50 == 1 || battle.unk575F24 == 1 || battle.unk575F28 == 1 || battle.actingUnit == -1)
    {
        return;
    }

    const i32 acting = battle.actingUnit;
    i32 target = 0;

    if (acting >= BattleState::FirstRoleUnit)
    {
        const auto role = static_cast<std::size_t>(acting - BattleState::FirstRoleUnit);
        target = static_cast<i16>(this->battleRoles[role].GetWord29A2());

        // Never so here (the original returned above).
        if (battle.unk575F24 == 1)
        {
            target += BattleState::FirstRoleUnit;
        }
    }
    else
    {
        target = static_cast<i16>(this->battleEnemies[static_cast<std::size_t>(acting)].GetWord29A2());

        if (battle.unk575F28 == 0)
        {
            target += BattleState::FirstRoleUnit;
        }
    }

    battle.SortDrawOrderAround(acting, target);
}

// swd3e.exe 0x00466290: one frame of the field fading to black: the frame's pixels shifted by the fade (blackFade,
// also the painter's tint), 2 darker a frame; 1 once past -32 (the fade reset to 0).
int Game::FadeFieldOut()
{
    smooth::Scene scene;
    const auto result = smooth::TurnFrame(*this).FadeFieldOutAdvance(scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x004697A0: the objects held back in the battle given to the inventory: givenBack[1] and [2], and one
// kThrownObject for each still thrown (unk577994); the counts and escortCount are cleared.
void Game::ReturnBattleItems()
{
    BattleState& battle = this->battleState;

    for (std::size_t at = 1; at < battle.givenBack.size(); ++at)
    {
        if (battle.givenBack[at] != 0)
        {
            AddOneItem(this->party.inventory, battle.givenBack[at], 0, this->database);
        }
    }

    for (i32 thrown = 0; thrown < battle.unk577994; ++thrown)
    {
        AddOneItem(this->party.inventory, kThrownObject, 0, this->database);
    }

    battle.escortCount = 0;
    battle.unk577994 = 0;
    battle.givenBack[1] = 0;
    battle.givenBack[2] = 0;
}

// swd3e.exe 0x004692E0: with every party role down (unk577258 set; besides the escorts and guests, unk5770D4 more
// roles are left out) the field fades out (endFade), then the objects return and frameResult becomes 2 (0, with the
// music before the battle back, for a battle state of 0x68 or UnkPanel8); with every enemy down or wonByTalk set it
// fades out too, the battle music stops, the music before the battle comes back, the objects return and frameResult
// becomes 0.
void Game::CheckBattleEnd()
{
    BattleState& battle = this->battleState;
    MusicQueue& queue = this->audio.Queue();

    if (battle.defeatedRoles >= battle.roleCount - battle.unk5770D4 - battle.escortCount - battle.guestCount)
    {
        battle.unk577258 = 1;

        if (battle.endFade == 1 && this->FadeFieldOut() == 1)
        {
            this->ReturnBattleItems();
            battle.frameResult = kLostEnd;

            if (battle.result == kLostQuietly)
            {
                queue.Switch(battle.musicBeforeBattle, MusicQueue::RepeatList, 0);
                battle.frameResult = 0;
            }

            if ((battle.panelFlags & BattleState::UnkPanel8) != 0)
            {
                queue.Switch(battle.musicBeforeBattle, MusicQueue::RepeatList, 0);
                battle.frameResult = 0;
            }
        }
    }

    if (battle.enemiesGone - battle.unk5771EA >= static_cast<i32>(battle.enemyCount) || this->fightTalk.wonByTalk == 1)
    {
        battle.unk577258 = 1;

        if (battle.endFade == 1 && this->FadeFieldOut() == 1)
        {
            queue.Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
            queue.Switch(battle.musicBeforeBattle, MusicQueue::RepeatList, 0);
            this->ReturnBattleItems();
            battle.frameResult = 0;
        }
    }
}

// swd3e.exe 0x00469410: the pointer prompt of the battle state for its first 300 frames (always while unk577088 is
// negative): kWaitPointer in states 1, 2, 4, 5, 8, 0x1B, 0x1E, 0x29 and 0x50; in states 3 and 9 (with unk5772B8)
// kTurnPointer or kNoTurnPointer by turnPointer; otherwise kUnitPointer with a current unit, else kNoUnitPointer, or
// with a negative unk577088 the effect kMarkEffect at (unk577246, unk577248) (unk57724C cleared once it ends).
void Game::DrawBattlePrompt()
{
    BattleState& battle = this->battleState;
    BattleHud& hud = this->battleHud;
    const int x = this->state.pointerX;
    const int y = this->state.pointerY;
    ++battle.promptFrames;

    if (battle.promptFrames >= kPromptFrames && battle.unk577088 >= 0)
    {
        return;
    }

    switch (battle.result)
    {
        case kWaitState1:
        case kWaitState2:
        case kWaitState4:
        case kWaitState5:
        case kWaitState8:
        case kWaitState1B:
        case kWaitState1E:
        case kWaitState29:
        case kWaitState50:
        {
            const smooth::Scene::Pointer following(this->battleServices);
            hud.DrawEffectAt(kWaitPointer, 0, x, y, 0);
            break;
        }

        case kTurnState:
        case kTurnState9:
            if (battle.unk5772B8 == 1)
            {
                const smooth::Scene::Pointer following(this->battleServices);
                hud.DrawEffectAt(battle.turnPointer != 0 ? kTurnPointer : kNoTurnPointer, 0, x, y, 0);
            }

            break;

        default:
            if (battle.unk577088 >= 0)
            {
                const smooth::Scene::Pointer following(this->battleServices);
                hud.DrawEffectAt(battle.currentUnit != 0 ? kUnitPointer : kNoUnitPointer, 0, x, y, 0);
            }
            else if (
                hud.DrawEffectAt(
                    kMarkEffect,
                    battle.unk57724C,
                    battle.unk577246,
                    battle.unk577248,
                    battle.mirrored != 0 ? 1 : 0
                ) == 1
            )
            {
                battle.unk57724C = 0;
            }

            break;
    }
}

// swd3e.exe 0x00467B50: one frame of the field fade (while any step is set): `counting` counts its frames down; while
// frames are left (not negative) the levels move by the steps, otherwise the steps take the fade's end levels (the
// original copies them into the steps, not the levels). The field rows of the frame are tinted by the levels.
void Game::FieldFadeFrame(int counting)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).FieldFadeFrameAdvance(counting, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00469BC0: while no unit acts and the battle is not kLostQuietly, a side that is all down or turned to
// stone (UnkStatus4) loses: the party (auto, guest and ghost units and the defeated left out; the count against the
// roles less unk5770D4, the escorts and the guests, in 8 bits) unless endFade, the stone roles counted as defeated and
// the battle state 0x67; then, unless unk57734C, the standing enemies, counted as defeated, unk57734C set, the auto
// battle off and the battle state 0x63. 1 when a side lost.
int Game::CheckStoneSides()
{
    BattleState& battle = this->battleState;

    if (battle.actingUnit != -1 || battle.result == kLostQuietly)
    {
        return 0;
    }

    u8 stoneRoles = 0;

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];

        if (role.AutoControl() != 1 && role.GuestControl() != 1 && role.Ghost() != 1 && !role.IsDefeated() &&
            role.HasEffectBits(BattleUnit::UnkStatus4) == 1)
        {
            ++stoneRoles;
        }
    }

    const auto partyRoles = static_cast<u8>(
        static_cast<u8>(battle.roleCount) - static_cast<u8>(battle.unk5770D4) - static_cast<u8>(battle.escortCount) -
        static_cast<u8>(battle.guestCount)
    );

    if (stoneRoles != 0 && battle.defeatedRoles + stoneRoles >= partyRoles && battle.endFade == 0)
    {
        battle.defeatedRoles = static_cast<u8>(battle.defeatedRoles + stoneRoles);
        battle.result = kBattleLost;
        return 1;
    }

    if (battle.unk57734C != 0)
    {
        return 0;
    }

    u8 stoneEnemies = 0;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];

        if (!enemy.IsDefeated() && enemy.HasEffectBits(BattleUnit::UnkStatus4) == 1)
        {
            ++stoneEnemies;
        }
    }

    if (stoneEnemies == 0 || battle.enemiesGone + stoneEnemies < static_cast<i32>(battle.enemyCount) ||
        battle.endFade != 0)
    {
        return 0;
    }

    battle.enemiesGone = static_cast<u8>(battle.enemiesGone + stoneEnemies);
    battle.unk57734C = 1;
    battle.autoBattle = 0;
    battle.result = kBattleWon;
    return 1;
}

// swd3e.exe 0x00469D80: the lasting poisons of one side spread (the enemies with `enemies`, the party roles
// otherwise): each unit counts its poison turn, and one poisoned more than three turns passes it (its base and level)
// to a unit of its side picked at random (0..its count - 1), not itself, defeated or with unk3038, nor for the roles a
// ghost, a StatusBit13 or an auto role. The enemies' pick is checked on the role of that index (as in the original;
// past the ten roles the original reads after them, the poison does not spread here).
void Game::SpreadPoison(int enemies)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;

    if (enemies != 0)
    {
        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];
            enemy.CountPoisonTurn();

            if (!enemy.PoisonLasted())
            {
                continue;
            }

            const auto pick = static_cast<u16>(services.random.Below(battle.enemyCount));

            if (pick >= this->battleRoles.size())
            {
                continue;
            }

            const BattleRole& checked = this->battleRoles[pick];

            if (!checked.IsDefeated() && checked.unk3038 != 1 && pick != index)
            {
                const u8 level = enemy.poisonLevel;
                this->battleEnemies[pick].AddPoison(services, enemy.PoisonBase(), level);
            }
        }

        return;
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];
        role.CountPoisonTurn();

        if (!role.PoisonLasted())
        {
            continue;
        }

        const auto pick = static_cast<u16>(services.random.Below(battle.roleCount));
        BattleRole& target = this->battleRoles[pick];

        if (!target.IsDefeated() && target.unk3038 != 1 && target.Ghost() != 1 && target.StatusBit13() != 1 &&
            target.AutoControl() != 1 && pick != index)
        {
            const u8 level = role.poisonLevel;
            target.AddPoison(services, role.PoisonBase(), level);
        }
    }
}

// swd3e.exe 0x0046A000: one frame of the 陰陽 command of role `role`: its damage 0x960 percent of the proficiency of
// kind Unk16 plus 100 (hitDamage, lastDamage) while the cast pose of act 0x1775 plays; once it is done (1) the grey
// band closes (2) and every standing enemy takes it: by the side of the coin (coinState) an enemy of the other kind
// (ObjectUnkA2, 1 on 陽, 0 on 陰) is healed (minus the damage, HealTarget), the others are hit (flag 26B8 raised),
// the number shown (at most 9999) after the hit picture 0x2389; one defeated becomes unk4BF1CA (its unk513664 entry
// set), flashes the field (defeat pose 6) and whitens the frame. 0 while the pose plays.
int Game::YinYangAttack(int role)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    BattleRole& caster = this->battleRoles[static_cast<std::size_t>(role)];
    const bool yang = static_cast<u16>(this->battleHud.coinState) != 0;
    const auto proficiency = static_cast<u16>(caster.AccessoryProficiency(
        static_cast<u16>(AccessoryKind::Unk16),
        this->party.fame.lists[Fame::ProficiencyList]
    ));
    battle.hitDamage = kYinYangScale * proficiency / kYinYangPercent + kYinYangBase;
    battle.lastDamage = battle.hitDamage;

    if (caster.CastPoseFrame(services, kYinYangAct) != 1)
    {
        return 0;
    }

    battle.grayBandState = kGrayBandCloses;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        BattleEnemy& enemy = this->battleEnemies[index];

        if (enemy.IsDefeated())
        {
            continue;
        }

        battle.lastDamage = battle.hitDamage;
        enemy.PushTarget(kYinYangHitPicture);
        const bool healed = yang ? enemy.ObjectUnkA2() == 1 : enemy.ObjectUnkA2() == 0;

        if (healed)
        {
            battle.lastDamage = -battle.lastDamage;
            enemy.PushTarget(BattleUnit::HealTarget);
        }
        else
        {
            enemy.RaiseFlag26B8(battle);
        }

        if (battle.lastDamage >= kYinYangMost)
        {
            battle.lastDamage = kYinYangMost;
        }

        enemy.SetDamageDigits(services, battle.lastDamage);
        enemy.StepTargetCount(services, 1);

        if (enemy.Damage(services, battle.lastDamage, 0, 0) == 1)
        {
            battle.unk4BF1CA = static_cast<u16>(index);
            battle.unk513664[index] = static_cast<i32>(index);
            battle.flashing = 1;
            enemy.SetDefeatPose(kYinYangDefeatPose);
            std::fill_n(this->drawSurface, static_cast<std::size_t>(Display::Width) * Display::Height, kWhitePixel);
        }
    }

    battle.hitDamage = 0;
    battle.lastDamage = 0;
    return 1;
}

// swd3e.exe 0x0046A320: the party role (1-based) an enemy aims at by `criterion` (its low byte): 1 the fewest hit
// points, 2 the most spirit, 3 the most stamina, 4 the least agility, 5 the least defence, 6 the most attack bonus
// (16-bit compares, signed for 2 and 3, unsigned for 4..6); 0 for other criteria or without a party role to aim at
// (none past the escorts and unk5770D4). The reference is the value of the first standing party member; every later
// role (from the second) that beats it is taken in turn, so the last one does.
u32 Game::PickRoleTarget(i16 criterion)
{
    BattleState& battle = this->battleState;
    const auto kind = static_cast<i8>(criterion);

    if (kind < kTargetFewestHitPoints || kind > kTargetMostAttack)
    {
        return 0;
    }

    auto value = [kind](const BattleRole& role) -> i32
    {
        u16 maximum = 0;
        u16 have = 0;
        i32 hitPoints = 0;
        i32 most = 0;

        switch (kind)
        {
            case kTargetFewestHitPoints:
                role.GetHitPoints(hitPoints, most);
                return hitPoints;

            case kTargetMostSpirit:
                role.GetSpirit(maximum, have);
                return static_cast<i16>(have);

            case kTargetMostStamina:
                role.GetStamina(maximum, have);
                return static_cast<i16>(have);

            case kTargetLeastAgility:
                return static_cast<u16>(role.RoleAgility());

            case kTargetLeastDefense:
                return static_cast<u16>(role.Defense());

            default:
                return role.RoleAttackBonus();
        }
    };

    auto beats = [kind](i32 candidate, i32 reference)
    {
        switch (kind)
        {
            case kTargetFewestHitPoints:
            case kTargetLeastAgility:
            case kTargetLeastDefense:
                return candidate < reference;

            default:
                return candidate > reference;
        }
    };

    const u32 limit = battle.roleCount - battle.unk5770D4 - battle.escortCount;

    if (limit < 1)
    {
        return 0;
    }

    u32 chosen = 1;
    i32 reference = value(this->battleRoles[0]);

    while (true)
    {
        const BattleRole& role = this->battleRoles[chosen - 1];

        if (!role.IsDefeated() && role.PartyMember() != 0)
        {
            break;
        }

        // The next role's value (past the ten roles the original reads after them; none is needed then).
        if (chosen < this->battleRoles.size())
        {
            reference = value(this->battleRoles[chosen]);
        }

        if (++chosen > limit)
        {
            return 0;
        }
    }

    for (u32 at = 1; at < battle.roleCount; ++at)
    {
        const BattleRole& role = this->battleRoles[at];

        if (!role.IsDefeated() && role.PartyMember() != 0 && beats(value(role), reference))
        {
            chosen = at + 1;
        }
    }

    return chosen;
}

// swd3e.exe 0x0046ACB0: the next slot record taken into `taken`: the records of held roles (IsHeld) at the
// front are passed over, the first role (unit 20) not counting as one; a used record taken is removed (the later ones
// move up, the free ones after the used ones cleared). The original reads past the records when all are held; the
// search stops there here.
void Game::TakeSlotRecord(BattleSlotRecord& taken)
{
    BattleState& battle = this->battleState;
    std::size_t at = 0;

    while (at < battle.slotRecords.size())
    {
        const i32 unit = battle.slotRecords[at].unit;

        if (unit <= BattleState::FirstRoleUnit ||
            this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].IsHeld() != 1)
        {
            break;
        }

        ++at;
    }

    battle.TakeSlotRecordAt(at, taken);
}

// swd3e.exe 0x00464E70: one frame of the damage numbers of role `index` (`role` 1) or enemy `index`; 1 once they are
// done
int Game::DamageNumbersFrame(int index, int role)
{
    BattleServices& services = this->battleServices;
    const auto at = static_cast<std::size_t>(index);
    BattleUnit& unit =
        role == 1 ? static_cast<BattleUnit&>(this->battleRoles[at]) : static_cast<BattleUnit&>(this->battleEnemies[at]);
    return unit.DrawDamageNumbers(services) == 1 ? 1 : 0;
}

// swd3e.exe 0x00469930: the units in draw order show their guard text and their damage numbers, then take their
// pending damage: a unit that does has its unk513664 entry set to its index and the slot record of that index removed.
// A role whose numbers are done (its slotTargets entry cleared) or whose damage is taken sets damageShown; then, out of
// the menu, with the talisman ghost (ghostSlot) obeying, the first role standing below half its hit points and no
// rescue yet, the ghost heals it when it can spend its spirit (kGhostRescuePercent): its act kGhostRescueAct, its
// order put first in the slot records and ghostRescued set. An enemy's numbers set nothing.
void Game::ApplyHits()
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto units = static_cast<std::size_t>(battle.roleCount + battle.enemyCount);

    for (std::size_t order = 0; order < units; ++order)
    {
        const i32 unitId = battle.drawOrder[order];

        if (unitId < BattleState::FirstRoleUnit)
        {
            const auto at = static_cast<std::size_t>(unitId);
            BattleEnemy& enemy = this->battleEnemies[at];
            enemy.DrawGuardText(services);
            this->DamageNumbersFrame(unitId, 0);

            if (enemy.ApplyPendingDamage(services))
            {
                battle.unk513664[at] = unitId;
                battle.RemoveSlotRecord(unitId);
            }

            continue;
        }

        const int index = unitId - BattleState::FirstRoleUnit;
        const auto at = static_cast<std::size_t>(index);
        BattleRole& role = this->battleRoles[at];
        role.DrawGuardText(services);

        if (this->DamageNumbersFrame(index, 1) == 1)
        {
            battle.damageShown = 1;
            battle.slotTargets[at] = -1;
        }

        if (role.ApplyPendingDamage(services))
        {
            battle.damageShown = 1;
            battle.unk513664[at] = index;
            battle.RemoveSlotRecord(index);
        }

        if (battle.damageShown != 1)
        {
            continue;
        }

        battle.damageShown = 0;
        const auto ghostAt = static_cast<std::size_t>(battle.ghostSlot);
        BattleRole& ghost = this->battleRoles[ghostAt];
        BattleRole& master = this->battleRoles[0];

        if (battle.result != 0 || ghost.GhostObeys(services) != 1 || master.LowHitPoints(0) != 1 ||
            master.IsDefeated() || battle.ghostRescued != 0)
        {
            continue;
        }

        i32 hitPoints = 0;
        i32 maximum = 0;
        master.GetHitPoints(hitPoints, maximum);

        if (ghost.SpendGhostSpirit(hitPoints, maximum, kGhostRescuePercent) != 1)
        {
            continue;
        }

        battle.unk50D88C[ghostAt] = static_cast<i32>(kGhostRescueAct);
        battle.aimTarget = 1;
        battle.pendingOrders[ghostAt].unk00 = 1;
        ghost.Retarget(0);
        battle.InsertSlotRecord(1, battle.ghostSlot + BattleState::FirstRoleUnit, 0);
        battle.unk5772BC = 1;
        battle.unk5772C0 = 1;
        battle.ghostRescued = 1;
    }
}

// swd3e.exe 0x00473150: the next enemy takes its turn: enemy actingEnemy (0 past the enemies); a defeated one passes
// on through orderStep (1-based in enemyOrder, wrapping) for at most enemyCount tries, after which the battle state
// is 1. The enemy is marked (SetAC0 1), aimed at (aimTarget, 1-based) and pointed at (kEnemyPointer, slot 0, the
// middle of its shadow; a rectangle the unit leaves unset is 0 here, uninitialised in the original).
void Game::NextEnemyTurn()
{
    BattleState& battle = this->battleState;
    const auto count = static_cast<i32>(battle.enemyCount);

    if (battle.actingEnemy >= count)
    {
        battle.actingEnemy = 0;
    }

    if (this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
    {
        int tries = 0;

        while (true)
        {
            if (++battle.orderStep > count)
            {
                battle.orderStep = 1;
            }

            battle.actingEnemy = battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)];

            if (++tries >= count)
            {
                battle.result = kTurnsRunning;
                break;
            }

            if (!this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
            {
                break;
            }
        }
    }

    this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].SetAC0(1);
    battle.unk5772B4 = 1;
    battle.aimTarget = battle.actingEnemy + 1;
    UnitRect rect;
    this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].GetShadowRect(this->battleServices, rect);
    this->battleHud.DrawSlotAnimation(kEnemyPointer, 0, rect.x + (rect.width >> 1), rect.y + (rect.height >> 1));
}

// swd3e.exe 0x00473270: the party role whose turn follows: orderStep steps (1-based, wrapping over the party roles)
// through RoleCycle until it names the current unit's role, which aimTarget then aims at (1-based). With the ghost
// the party roles reach past the cycle: the original reads on (RoleCycleRead). It loops for ever when the cycle never
// names the role.
void Game::NextRoleTurn()
{
    BattleState& battle = this->battleState;
    const auto roles = static_cast<i32>(battle.roleCount - battle.escortCount - battle.guestCount);
    const i32 wanted = battle.currentUnit - BattleState::FirstRoleUnit;
    i32 at = battle.orderStep;
    int steps = 0;

    while (true)
    {
        if (++at > roles)
        {
            at = 1;
        }

        if (battle.RoleCycleRead(at) == wanted)
        {
            break;
        }

        // Every place read once without the role: the original never ends.
        if (++steps > std::max(roles, 1))
        {
            Hang();
        }
    }

    battle.orderStep = at;
    battle.aimTarget = wanted + 1;
    battle.actingEnemy = 0;
    battle.unk5772B4 = 1;
}

// swd3e.exe 0x0046E630: the back key in a list (kBackSound): battle state 2 scrolls listScroll up by kListPage, states
// 4, 0x1B, 0x29 and 0x50 roleListScroll (state 4 also keeps roleListRow and the scroll for role listRole, 0x29 and 0x50
// slide in again from menuSlide 0x50); each first only marks
// its list (pageListRow, roleListRow) unless unk577078.
void Game::ScrollListBack()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    auto scroll = [](i32& top)
    {
        top -= kListPage;

        if (top < 0)
        {
            top = 0;
        }
    };

    if (battle.result == kListState2)
    {
        this->audio.PlaySound(kBackSound, this->config.soundVolume);

        if (battle.unk577078 == 0 && battle.pageListRow != 1)
        {
            battle.pageListRow = 1;
            return;
        }

        scroll(battle.listScroll);
        battle.unk5772B4 = 1;
        return;
    }

    if (battle.result != kListState4)
    {
        if (battle.result != kListState1B)
        {
            // swd3e: the lists of states 0x29 and 0x50 slide in again.
            if (battle.result != kWaitState29 && battle.result != kWaitState50)
            {
                return;
            }

            this->audio.PlaySound(kBackSound, this->config.soundVolume);
            battle.menuSlide = kMenuSlideStart;

            if (battle.unk577078 == 0 && battle.roleListRow != 1)
            {
                battle.roleListRow = 1;
                return;
            }

            scroll(battle.roleListScroll);
            battle.unk5772B4 = 1;
            return;
        }

        this->audio.PlaySound(kBackSound, this->config.soundVolume);

        if (battle.unk577078 == 0 && battle.roleListRow != 1)
        {
            battle.roleListRow = 1;
            return;
        }

        scroll(battle.roleListScroll);
        battle.unk5772B4 = 1;
        return;
    }

    this->audio.PlaySound(kBackSound, this->config.soundVolume);

    if (battle.unk577078 == 0 && battle.roleListRow != 1)
    {
        battle.roleListRow = 1;
        return;
    }

    scroll(battle.roleListScroll);
    battle.unk5772B4 = 1;
    const auto role = static_cast<std::size_t>(battle.listRole);
    battle.roleListRows[role] = battle.roleListRow;
    battle.roleListScrolls[role] = battle.roleListScroll;
}

// swd3e.exe 0x0046E820: the forward key in a list (kBackSound): battle state 2 scrolls listScroll down by kListPage
// within listEntries entries (nothing below a page), states 4, 0x1B, 0x29 and 0x50 roleListScroll within
// roleListEntries (0x29 and 0x50 sliding in again from menuSlide 0x50, otherwise as 0x1B); each
// first only marks the last row of its page (pageListRow, roleListRow, at most the entries) unless unk577078. A list
// shorter than a page goes back to 0 with the mark on its end. State 4, and the first mark of state 0x1B (the original
// shares that code), keep the mark and the scroll for role listRole.
void Game::ScrollListForward()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    auto keepForRole = [&battle](i32 mark)
    {
        const auto role = static_cast<std::size_t>(battle.listRole);
        battle.unk5772B4 = 1;
        battle.roleListRows[role] = mark;
        battle.roleListScrolls[role] = battle.roleListScroll;
    };

    auto markPageEnd = [&battle, &keepForRole]
    {
        battle.roleListRow = std::min<i32>(kListPage, battle.roleListEntries);
        keepForRole(battle.roleListRow);
    };

    if (battle.result == kListState2)
    {
        this->audio.PlaySound(kBackSound, this->config.soundVolume);

        if (static_cast<i8>(battle.listEntries) < kListPage)
        {
            return;
        }

        if (battle.unk577078 == 0 && battle.pageListRow != kListPage)
        {
            battle.pageListRow = kListPage;
            return;
        }

        battle.listScroll += kListPage;

        if (battle.listScroll + kListPage > static_cast<i8>(battle.listEntries))
        {
            battle.listScroll = static_cast<i8>(battle.listEntries) - kListPage;
        }

        if (battle.listScroll < 0)
        {
            battle.listScroll = 0;
            battle.pageListRow = static_cast<i8>(battle.listEntries);
        }

        battle.unk5772B4 = 1;
        return;
    }

    const bool slides = battle.result == kWaitState29 || battle.result == kWaitState50;

    if (battle.result != kListState4 && battle.result != kListState1B && !slides)
    {
        return;
    }

    this->audio.PlaySound(kBackSound, this->config.soundVolume);

    // swd3e: the lists of states 0x29 and 0x50 slide in again, as state 0x1B otherwise.
    if (slides)
    {
        battle.menuSlide = kMenuSlideStart;
    }

    if (battle.unk577078 == 0 && battle.roleListRow != kListPage)
    {
        markPageEnd();
        return;
    }

    battle.roleListScroll += kListPage;

    if (battle.roleListScroll + kListPage > battle.roleListEntries)
    {
        battle.roleListScroll = battle.roleListEntries - kListPage;
    }

    if (battle.roleListScroll < 0)
    {
        battle.roleListScroll = 0;
        battle.roleListRow = battle.roleListEntries;
    }

    if (battle.result == kListState1B || slides)
    {
        battle.unk5772B4 = 1;
        return;
    }

    keepForRole(battle.roleListRow);
}

// swd3e.exe 0x00467CF0: the auto battle takes the current role's turn (unless in battle state 3): with no remembered
// enemy (autoTarget) the state becomes 3; otherwise the role acts on it (aimTarget); a defeated enemy gives way to the
// first standing one, and with none the auto battle ends.
void Game::AutoBattleTurn()
{
    BattleState& battle = this->battleState;

    if (battle.autoBattle != 1 || battle.currentUnit == 0 || battle.result == kTurnState)
    {
        return;
    }

    battle.turnCommand = 1;
    battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = 1;

    if (battle.autoTarget == -1)
    {
        battle.unk5772B4 = 1;
        battle.result = kTurnState;
        return;
    }

    battle.turnUnit = battle.currentUnit;
    battle.currentUnit = 0;
    battle.unk57725C = 1;
    BattleRole& role = this->battleRoles[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)];
    role.SetTarget(1);
    battle.aimTarget = battle.autoTarget;
    battle.result = 0;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 0;

    if (!this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].IsDefeated())
    {
        return;
    }

    for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
    {
        if (!this->battleEnemies[enemy].IsDefeated())
        {
            battle.aimTarget = static_cast<i32>(enemy) + 1;
            return;
        }
    }

    role.SetTarget(0);
    battle.turnCommand = 0;
    battle.aimTarget = 1;
    battle.autoTarget = -1;
    battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = 0;
    battle.autoBattle = 0;
}

// swd3e.exe 0x0046F530: whether role unit `unit` can take the current panel: it is in roleOrder (among the roleCount
// first) and not held
bool Game::CanSelectRole(int unit)
{
    BattleState& battle = this->battleState;

    for (u32 at = 0; at < battle.roleCount; ++at)
    {
        if (battle.roleOrder[at] == unit)
        {
            return this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].IsHeld() == 0;
        }
    }

    return false;
}

// The place of `unit` in PanelUnits for 0x00464DD0 and 0x00464E40 (inline in the original; past the end when absent).
std::size_t Game::PanelPlace(int unit)
{
    std::size_t place = 0;

    while (place < BattleState::PanelUnits.size() && BattleState::PanelUnits[place] != unit)
    {
        ++place;
    }

    return place;
}

// swd3e.exe 0x00472C90: the first role unit from `unit` on in PanelUnits (wrapping, four tries) that can take the
// panel; 0 when none can
int Game::NextSelectableRole(int unit)
{
    std::size_t place = Game::PanelPlace(unit);

    // Never so: the callers pass panel units (the original reads past the table).
    if (place == BattleState::PanelUnits.size())
    {
        return 0;
    }

    int candidate = BattleState::PanelUnits[place];

    if (this->CanSelectRole(candidate))
    {
        return candidate;
    }

    ++place;

    for (int tries = 1; tries < static_cast<int>(BattleState::PanelUnits.size()); ++tries)
    {
        if (place == BattleState::PanelUnits.size())
        {
            place = 0;
        }

        candidate = BattleState::PanelUnits[place];
        ++place;

        if (this->CanSelectRole(candidate))
        {
            return candidate;
        }
    }

    return 0;
}

// swd3e.exe 0x00472D00: the first role unit from `unit` back in PanelUnits (wrapping, four tries) that can take the
// panel; 0 when none can
int Game::PreviousSelectableRole(int unit)
{
    auto place = static_cast<int>(Game::PanelPlace(unit));

    // Never so: the callers pass panel units (the original reads past the table).
    if (place == static_cast<int>(BattleState::PanelUnits.size()))
    {
        return 0;
    }

    for (int tries = 0; tries < static_cast<int>(BattleState::PanelUnits.size()); ++tries)
    {
        const int candidate = BattleState::PanelUnits[static_cast<std::size_t>(place)];

        if (--place < 0)
        {
            place = static_cast<int>(BattleState::PanelUnits.size()) - 1;
        }

        if (this->CanSelectRole(candidate))
        {
            return candidate;
        }
    }

    return 0;
}

// swd3e.exe 0x0046F480: role unit `unit` takes the current panel (in battle state 0, or with anyStateRole set): when it
// is among the first roleCount - 1 of roleOrder and not held, it trades places with the current unit; the inputs of
// the panel (commandNames, listedCommands) are cleared either way.
void Game::SelectRoleUnit(int unit)
{
    BattleState& battle = this->battleState;

    if (battle.result != 0 && battle.anyStateRole == kNoPanelRole)
    {
        return;
    }

    if (battle.roleCount != 1)
    {
        for (u32 at = 0; at < battle.roleCount - 1; ++at)
        {
            const i32 listed = battle.roleOrder[at];

            if (listed != 0 && listed == unit &&
                this->battleRoles[static_cast<std::size_t>(listed - BattleState::FirstRoleUnit)].IsHeld() != 1)
            {
                battle.roleOrder[at] = battle.currentUnit;
                battle.currentUnit = listed;
                break;
            }
        }
    }

    battle.commandNames = {};
    battle.listedCommands = 0;
}

// swd3e.exe 0x0046F380: the panel goes from the current role unit (8..11) to the one numbered before it (11 before 8)
// or, past it, the next in PanelUnits that can take the panel
void Game::SelectPreviousRole()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;
    const i32 unit = battle.currentUnit;

    if (unit < kFirstPanelUnit || unit > kLastPanelUnit)
    {
        return;
    }

    this->SelectRoleUnit(this->NextSelectableRole(unit == kFirstPanelUnit ? kLastPanelUnit : unit - 1));
}

// swd3e.exe 0x0046F400: the panel goes from the current role unit (8..11) to the one numbered after it (8 after 11)
// or, before it, the previous in PanelUnits that can take the panel
void Game::SelectNextRole()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;
    const i32 unit = battle.currentUnit;

    if (unit < kFirstPanelUnit || unit > kLastPanelUnit)
    {
        return;
    }

    this->SelectRoleUnit(this->PreviousSelectableRole(unit == kLastPanelUnit ? kFirstPanelUnit : unit + 1));
}

// swd3e.exe 0x0046F590: the right key of the battle menu. State 1: the 符鬼 and 煉妖 choices take no key; the elements
// of 改變 step on (wrapping, without a sound); otherwise (unless on 符鬼 with two listed commands) the role that took
// 蓄勁 steps between commands 1 and 2, the others step a command on in its row (wrapping), past empty listed commands
// (MenuKind), with kBackSound. State 2 turns the page of the list (listPage, 0..2) to its top; state 4 goes to the
// next party role's list (listRole, its kept mark and scroll); state 0x1E raises roleListRow by 5 up to 10.
void Game::MenuRight()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    if (battle.result == kMenuState)
    {
        if (battle.ghostOrdersOpen == 1 || battle.refineOptionsOpen == 1)
        {
            return;
        }

        if (battle.elementsOpen == 1)
        {
            if (++battle.elementRow > kElements)
            {
                battle.elementRow = 1;
            }

            return;
        }

        if (battle.menuCommand == kGhostMenuEntry && battle.listedCommands == kGhostListed)
        {
            return;
        }

        if (battle.chargingRole == battle.currentUnit)
        {
            if (++battle.menuCommand > kGatheringCommands)
            {
                battle.menuCommand = 1;
            }
        }
        else
        {
            i32 command = battle.menuCommand + 1;

            if (command % kMenuColumns == 1)
            {
                command -= kMenuColumns;
            }

            if (command > kMenuFixedCommands)
            {
                while (battle.MenuKind(command) == 0)
                {
                    if (++command % kMenuColumns == 1)
                    {
                        command -= kMenuColumns;
                    }
                }
            }

            battle.menuCommand = command;
        }

        this->audio.PlaySound(kBackSound, this->config.soundVolume);
        return;
    }

    if (battle.result == kListState2)
    {
        if (++battle.listPage >= kListPages)
        {
            battle.listPage = 0;
        }

        battle.pageListRow = 1;
        battle.listScroll = 0;
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }

    if (battle.result == kListState4)
    {
        if (++battle.listRole >= static_cast<i32>(Party::Roles))
        {
            battle.listRole = 0;
        }

        const auto role = static_cast<std::size_t>(battle.listRole);
        battle.roleListRow = battle.roleListRows[role];
        battle.roleListScroll = battle.roleListScrolls[role];
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }

    if (battle.result == kWaitState1E)
    {
        battle.roleListRow = std::min(battle.roleListRow + kMenuStep, kMenuStepMost);
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }
}

// swd3e.exe 0x0046F760: the left key of the battle menu, as MenuRight the other way: state 1 steps the elements back
// (wrapping to 5), the role that took 蓄勁 between commands 2 and 1, the others a command back in its row; state 2
// turns the page back (wrapping to 2, the scroll kept); state 4 goes to the previous party role's list; state 0x1E
// lowers roleListRow by 5 down to 1.
void Game::MenuLeft()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    if (battle.result == kMenuState)
    {
        if (battle.ghostOrdersOpen == 1 || battle.refineOptionsOpen == 1)
        {
            return;
        }

        if (battle.elementsOpen == 1)
        {
            if (--battle.elementRow < 1)
            {
                battle.elementRow = kElements;
            }

            return;
        }

        if (battle.menuCommand == kGhostMenuEntry && battle.listedCommands == kGhostListed)
        {
            return;
        }

        if (battle.chargingRole == battle.currentUnit)
        {
            if (--battle.menuCommand < 1)
            {
                battle.menuCommand = kGatheringCommands;
            }
        }
        else
        {
            i32 command = battle.menuCommand - 1;

            if (command % kMenuColumns == 0)
            {
                command += kMenuColumns;
            }

            if (command > kMenuFixedCommands)
            {
                while (battle.MenuKind(command) == 0)
                {
                    if (--command % kMenuColumns == 0)
                    {
                        command += kMenuColumns;
                    }
                }
            }

            battle.menuCommand = command;
        }

        this->audio.PlaySound(kBackSound, this->config.soundVolume);
        return;
    }

    if (battle.result == kListState2)
    {
        if (--battle.listPage < 0)
        {
            battle.listPage = kListPages - 1;
        }

        battle.pageListRow = 1;
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }

    if (battle.result == kListState4)
    {
        if (--battle.listRole < 0)
        {
            battle.listRole = static_cast<i32>(Party::Roles) - 1;
        }

        const auto role = static_cast<std::size_t>(battle.listRole);
        battle.roleListRow = battle.roleListRows[role];
        battle.roleListScroll = battle.roleListScrolls[role];
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }

    if (battle.result == kWaitState1E)
    {
        battle.roleListRow = std::max(battle.roleListRow - kMenuStep, 1);
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    }
}

// swd3e.exe 0x00472D50: the command menu of the current role: the five commands enabled; then the listed commands
// after them: for battle role 0 in the party first 煉妖 (kind kRefineMenuCommand) and, with game flag 0x5A, 符鬼
// (kGhostMenuCommand), and the accessory commands (kinds kFirstAccessoryCommand..kLastAccessoryCommand of the two
// accessory slots, named by kind) of the first party role; otherwise those of its own party role. A 逆攻 accessory
// reads 釋放 while the meter shows, a 融合 one is off without escorts and a 蓄勁 one below 10 percent. 奇術 is off for
// the role that started the meter until its 逆攻 is fully grown, and without MarkBit8Clear; 絕技 without HasBlock30;
// the listed commands CanUseAccessory refuses are off. A MarkBit4 role keeps 攻擊 and 防禦 only.
void Game::BuildCommandMenu()
{
    BattleState& battle = this->battleState;
    battle.commandEnabled = {1, 1, 1, 1, 1, 0, 0, 0, 0};
    battle.commandNames = {};
    battle.commandKinds = {};
    battle.listedCommands = 0;
    const int index = battle.currentUnit - BattleState::FirstRoleUnit;
    BattleRole& role = this->battleRoles[static_cast<std::size_t>(index)];

    auto add = [&battle](u16 kind, std::u8string_view name) -> u8&
    {
        const std::size_t listed = battle.listedCommands;
        u8& enabled = battle.commandEnabled[kFixedCommands + listed];
        enabled = 1;
        battle.commandNames[listed] = name;
        battle.commandKinds[listed] = kind;
        return enabled;
    };

    auto addAccessories = [this, &battle, &role, &add](std::size_t partyRole)
    {
        for (std::size_t slot = 0; slot < kAccessorySlots; ++slot)
        {
            const std::unique_ptr<ItemNode>& node =
                this->party.equipment[partyRole * Party::SlotsPerRole + kFirstAccessorySlot + slot];

            // An empty slot holds the empty entry; a missing one is read through a null pointer in the original.
            const u16 kind = node != nullptr ? node->record.data.unk48 : u16{0};

            if (kind < kFirstAccessoryCommand || kind > kLastAccessoryCommand)
            {
                continue;
            }

            const std::size_t listed = battle.listedCommands;
            u8& enabled = add(kind, CommandKindName(kind));

            if (kind == kStrikeBackCommand)
            {
                if (this->battleHud.meterShown == 1)
                {
                    battle.commandNames[listed] = BattleState::ReleaseCommandName;
                }
            }
            else if (kind == kFuseCommand)
            {
                if (battle.escortCount == 0)
                {
                    enabled = 0;
                }
            }
            else if (
                kind == kGatherCommand &&
                static_cast<u16>(role.AccessoryProficiency(kGatherCommand, this->ProficiencyList())) < kGatherLeast
            )
            {
                enabled = 0;
            }

            ++battle.listedCommands;
        }
    };

    if (battle.currentUnit == BattleState::FirstRoleUnit && battle.inParty[0] == 1)
    {
        add(static_cast<u16>(kRefineMenuCommand), BattleState::RefineCommandName);
        battle.listedCommands = 1;

        if (this->state.GameFlag(GameState::Unk5A))
        {
            add(kGhostMenuCommand, BattleState::GhostCommandName);
            ++battle.listedCommands;
        }

        addAccessories(0);
    }
    else
    {
        addAccessories(static_cast<std::size_t>(battle.partyRoles[static_cast<std::size_t>(index)]));
    }

    if (this->battleHud.meterShown == 1 && battle.meterRole == battle.currentUnit &&
        static_cast<u16>(role.AccessoryProficiency(kStrikeBackCommand, this->ProficiencyList())) != kFullProficiency)
    {
        battle.commandEnabled[1] = 0;
    }

    if (!role.MarkBit8Clear())
    {
        battle.commandEnabled[1] = 0;
    }

    if (!role.HasBlock30())
    {
        battle.commandEnabled[3] = 0;
    }

    for (std::size_t listed = 0; listed < battle.listedCommands; ++listed)
    {
        if (role.CanUseAccessory(battle.commandKinds[listed]) == 0)
        {
            battle.commandEnabled[kFixedCommands + listed] = 0;
        }
    }

    if (role.MarkBit4() == 1)
    {
        battle.commandEnabled = {1, 0, 0, 0, 1, 0, 0, 0, 0};
    }
}

// swd3e.exe 0x004732D0: the command menu of the current role at (x, y), a grid of three a row: the five commands
// (FixedCommandNames), then the listed ones (dark, and disabled, when CanUseAccessory refuses), light when enabled
// and dark otherwise; the entry `cursor` (1-based) is drawn again over a dark box. The original first enables 絕技
// for a role whose fury is full (BuildCommandMenu then sets it anew). An open choice covers the menu: the ghost's
// orders (marking 符鬼) in a tall panel, the 煉妖 choices in a small one, the elements of 改變 in a wide one once
// its slide is over, each with its row marked.
void Game::DrawCommandMenu(int x, int y, int cursor)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawCommandMenuAdvance(x, y, cursor, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00477EE0: the first party role (not auto, guest, ghost or defeated) whose accessory proficiency has just
// filled (FinishProficiency): its object read into lastObject (the description freed), its name kept in lastLearned
// for the message, learnedPending set and levelUpRole the role. An object that turns into another (unk42) empties the
// worn accessory of that id (its name 無 over four cleared bytes, the rest of its record kept), adds the new
// object to the inventory and keeps its name in evolvedName (cleared first).
void Game::AnnounceMastery()
{
    BattleState& battle = this->battleState;
    battle.evolvedName.clear();

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];

        if (role.AutoControl() == 1 || role.GuestControl() == 1 || role.Ghost() == 1 || role.IsDefeated())
        {
            continue;
        }

        const u16 finished = role.FinishProficiency(this->party.fame.lists[Fame::ProficiencyList]);

        if (finished == 0)
        {
            continue;
        }

        this->database.LoadObject(battle.lastObject, finished);
        Database::FreeDescription(battle.lastObject);
        battle.learnedPending = 1;
        battle.lastLearned.name = ObjectName(battle.lastObject);
        battle.levelUpRole = static_cast<u8>(index);
        const u16 evolved = battle.lastObject.data.unk42;

        if (evolved == 0)
        {
            return;
        }

        const auto first =
            static_cast<std::size_t>(battle.partyRoles[index]) * Party::SlotsPerRole + kFirstAccessorySlot;

        for (std::size_t slot = 0; slot < kAccessorySlots; ++slot)
        {
            const std::unique_ptr<ItemNode>& node = this->party.equipment[first + slot];

            // A missing slot is read through a null pointer in the original.
            // A word compare (0x00477FC0).
            if (node == nullptr || node->id != finished)
            {
                continue;
            }

            node->id = ItemNode::Empty;
            node->count = 1;
            node->pending = 0;
            node->workCount = 0;
            std::fill_n(node->record.data.name.begin(), kClearedNameStart, '\0');
            const std::string empty = Big5(kEmptySlotName);
            std::copy(empty.begin(), empty.end(), node->record.data.name.begin());
            AddOneItem(this->party.inventory, evolved, 1, this->database);
            this->database.LoadObject(battle.lastObject, evolved);
            Database::FreeDescription(battle.lastObject);
            battle.evolvedName = ObjectName(battle.lastObject);
            return;
        }

        return;
    }
}

// swd3e.exe 0x0046EAC0: the cancel key of the battle menu. In the auto battle it ends it (the aimed enemy unmarked,
// the listed commands cleared; the menu of a role on its panel opens again). Otherwise by battle state: 1 closes an
// open choice (the ghost's orders back on 符鬼 at the ghost's order, 煉妖, the elements of 改變 at the command before
// it) and keeps the menu; 2, 8, 0x1B, 0x1E and 0x29 go back to the menu (the list's name cleared, the role's command
// cancelled; 0x1B, 0x1E and 0x29 with the command listReturnCommand); 3 (choosing a target) goes back to the list the
// menu command opened (state 2, 4, 0x1B, 0x1E or 8 for commands 2, 3, 0x1B, 0x1E, 4) with the role's order cleared,
// otherwise to the menu (or the talk) with the command listReturnCommand, or to the 煉妖 choices (state 0x50) once
// one was taken, every unit unmarked and the portraits plain; 4 goes back to the menu; 5 to state 4; 7 closes the
// menu with the command back on 1; 9 back to the ghost's orders; 0xA to state 0x29; 0x50 back to the 煉妖 choices.
// The panels close unless the menu shows.
void Game::MenuCancel()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 1;

    auto cancelCommand = [this, &battle]
    {
        this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)].CancelCommand();
    };

    auto closePanels = [&battle]
    {
        battle.listPanelFrames = 0;
        battle.menuGrow = 0;
    };

    auto clearList = [&battle]
    {
        battle.listName = {};
        battle.unk5772C4 = 0;
        battle.unk5772AC = 0;
        battle.unk5777F0 = 1;
        battle.listRows = kListRows;
    };

    if (battle.autoBattle == 1)
    {
        battle.commandNames = {};
        battle.unk5772B4 = 0;
        battle.unk577258 = 1;
        battle.unk5772C0 = 0;
        battle.unk5772BC = 0;
        battle.autoBattle = 0;
        battle.result = 0;
        battle.autoTarget = -1;
        battle.listedCommands = 0;
        this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(0);

        if (battle.currentUnit != 0)
        {
            battle.result = kMenuState;
            battle.unk5772C0 = 1;
            battle.unk5772BC = 1;
        }

        return;
    }

    switch (battle.result)
    {
        case kMenuState:
            if (battle.ghostOrdersOpen == 1)
            {
                battle.ghostOrdersOpen = 0;
                battle.menuCommand = kGhostMenuEntry;
                BattleRole& ghost = this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)];
                battle.ghostOrderRow = ghost.GhostOrder();
            }

            if (battle.refineOptionsOpen == 1)
            {
                battle.refineOptionsOpen = 0;
                battle.menuCommand = kRefineMenuCommand;
            }

            if (battle.elementsOpen == 1)
            {
                battle.elementsOpen = 0;
                battle.elementRow = 1;
                battle.menuCommand = battle.listReturnCommand;
            }

            break;

        case kListState2:
            battle.result = kMenuState;
            battle.pageListRow = 1;
            clearList();
            cancelCommand();
            break;

        case kTargetState:
        {
            if (battle.talkRole == kNoTalkRole)
            {
                battle.result = kMenuState;
                battle.listRows = kListRows;
                battle.unk5772AC = 0;
            }
            else
            {
                battle.orderedSkill = 0;
                battle.unk577018 = 0;
                battle.result = kTalkState;
            }

            battle.turnPointer = 1;
            battle.unk577330 = 0;
            cancelCommand();

            for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
            {
                this->battleEnemies[enemy].SetAC0(0);
            }

            for (std::size_t panel = 0; panel < static_cast<std::size_t>(Party::Roles); ++panel)
            {
                this->battleRoles[panel].SetAC0(0);
                this->battleHud.portraitHighlight[panel] = 0;
            }

            i32 list = 0;

            switch (battle.menuCommand)
            {
                case kSpellCommand:
                    battle.unk577290 = 0;
                    list = kListState2;
                    break;

                case kItemCommand:
                    list = kListState4;
                    break;

                case kListState1B:
                    list = kListState1B;
                    break;

                case kWaitState1E:
                    list = kWaitState1E;
                    break;

                case kSkillCommand:
                    list = kSkillListState;
                    break;

                default:
                    battle.menuCommand = battle.listReturnCommand;

                    // A 煉妖 choice taken (0x80) brings its choices back.
                    if ((battle.refineOptionsOpen & kRefineOptionTaken) != 0)
                    {
                        closePanels();
                        battle.refineOptionsOpen = 1;
                        battle.result = kWaitState50;
                        battle.menuCommand = kRefineMenuCommand;
                        return;
                    }

                    break;
            }

            if (list == 0)
            {
                break;
            }

            battle.result = list;
            battle.unk577284 = 0;
            battle.unk5772F0 = 0;
            battle.unk5772D0 = 0;
            const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
            battle.unk50D88C[unit] = 0;
            closePanels();
            battle.pendingOrders[unit].unk08 = 0;
            battle.pendingOrders[unit].unk00 = 0;
            return;
        }

        case kListState4:
            battle.result = kMenuState;
            battle.roleListScroll = 0;
            battle.listRows = kListRows;
            battle.unk5772C4 = 0;
            battle.unk5772AC = 0;
            cancelCommand();
            break;

        case kWaitState5:
            battle.result = kListState4;
            cancelCommand();
            break;

        case kTalkState:
            battle.controlCursor = 1;
            battle.menuCommand = 1;
            battle.result = 0;
            closePanels();
            battle.controlRows = kControlRowsStart;
            return;

        case kSkillListState:
            battle.result = kMenuState;
            battle.roleListRow = 1;
            battle.specialRow = 1;
            clearList();
            cancelCommand();
            break;

        case kTurnState9:
            battle.ghostOrdersOpen = 1;
            battle.result = kMenuState;
            this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(0);
            break;

        case kWaitStateA:
            battle.result = kWaitState29;
            cancelCommand();
            break;

        case kListState1B:
            battle.result = kMenuState;
            battle.menuCommand = battle.listReturnCommand;
            battle.roleListScroll = 0;
            battle.roleListRow = 1;
            clearList();
            cancelCommand();
            break;

        case kWaitState1E:
            battle.result = kMenuState;
            battle.menuCommand = battle.listReturnCommand;
            battle.roleListRow = 1;
            clearList();
            cancelCommand();
            break;

        case kWaitState29:
            battle.menuCommand = battle.listReturnCommand;
            battle.result = kMenuState;
            battle.roleListRow = 1;
            clearList();
            cancelCommand();
            battle.unk5772D0 = 1;
            battle.pendingOrders[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)].unk00 = 0;
            break;

        case kWaitState50:
            battle.result = kMenuState;
            battle.refineOptionsOpen = 1;
            battle.menuCommand = kRefineMenuCommand;
            return;

        default:
            break;
    }

    if (battle.result != kMenuState)
    {
        closePanels();
    }
}

// swd3e.exe 0x00473880: the list of battle state 2 at (x, y): its three page tabs (kTabEffect, frames 2..0 right to
// left) and the open page's tab over them (frame 4 + page), the panel act restarted (listPanelFrames 10), a
// quarter-dark box with the panel frame below the tabs; menuGrow grows by 2 a call up to 7.
void Game::DrawListTabs(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawListTabsAdvance(x, y, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x004743C0: the 投擲 list of the current role at (x, y): a darkened box with a title frame (投擲) and a
// body frame, then up to 7 listed objects from entry `top` + 1 with their counts ("%2d"); roleListEntries counts them.
// Row `cursor` (from 1) is drawn again a pixel up and left under a gradient bar, and its entry becomes listCursor.
void Game::DrawThrowList(int x, int y, int cursor, int top)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawThrowListAdvance(x, y, cursor, top, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00474AF0: the 煉符 grid of the current role at (x, y): two columns of five cells. The first cells, one
// per talisman paper (kThrownObject, their count kept in roleListEntries), show its name; then each throwable object
// once per piece; the rest 無. Cell `cursor` (from 1) gets a gradient bar and sets listCursor to the object under it
// (less 1 without paper). roleListEntries then adds the throwable pieces.
void Game::DrawRefineList(int x, int y, int cursor)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawRefineListAdvance(x, y, cursor, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00474E80: the 絕技 list of the current role at (x, y): the panel act run on (not restarted), a darkened
// box with a title and a body frame, then up to 7 skills of type 1 (their names kept in listName, cut to its bytes:
// the original copies the whole name over what follows); specialCount counts them. Row `cursor` (from 1) is drawn again
// a pixel up and left under a gradient bar with the skill's description centred in a bar at the bottom, and its skill
// index becomes specialCursor.
void Game::DrawSpecialList(int x, int y, int cursor)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawSpecialListAdvance(x, y, cursor, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x004751D0: the 佈置護駕 panel: the panel act run on, a shade with a title frame (佈置護駕) and a body
// frame, a gradient bar on row guardCursor at (x, y), and the names of the escorts (the last escortCount battle roles)
// in white at an 18-pixel advance, a defeated one as 無 (dark), with a dark 無 below a lone escort.
void Game::DrawEscortList(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawEscortListAdvance(x, y, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00475680: the name tag of the aimed enemy (aimTarget, 1-based) at (x, y), unless the current role's
// order is taken or unk577284: a quarter-dark box and a frame as wide as the name (20 pixels per two bytes; from the
// right edge when the sides are swapped), the name in white. Once the bestiary counts 10 of it the box takes the tint
// of its element and its hit points follow right of the frame ("生命:%d/%d"); from 15 its action gauge as a dark red
// bar below them.
void Game::DrawTargetTag(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawTargetTagAdvance(x, y, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00475940: the 控制 menu of the controlled enemy (controlledEnemy) at (x, y): a tile box with the title, a
// tile box of controlRows rows, then 攻擊, its objects by name, its skills as 特殊1 and 特殊2, and 釋放, 20 pixels a
// row. Entry `cursor` (from 1) is drawn again over a dark background; an object sets orderedSkill to its id (always the
// first object's, see ItemName) and a skill unk577018 to its number.
void Game::DrawControlMenu(int x, int y, int cursor)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawControlMenuAdvance(x, y, cursor, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x004739B0: the spells of the current role's open page (listPage) from entry `top` + 1 at (x, y), up to
// 7 rows (listEntries counts them): each with its element icon, its name (kept in listName) and its cost ("%3d"; with
// bit 15 a stamina cost), dark when the role has less than it. Row `cursor` (from 1) is drawn again a pixel up and
// left under a gradient bar, with the spell's description centred in a bar at the bottom; its index becomes
// specialCursor.
void Game::DrawSpellList(int x, int y, int cursor, int top)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawSpellListAdvance(x, y, cursor, top, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00473E10: the object list of category listRole at (x, y): its four page tabs (frames 3..0 right to
// left, the open one frame 4 + category), the panel act restarted, a quarter-dark box and a frame; its scroll bar past
// 7 entries; then from entry `top` + 1 up to 7 shown objects (roleListEntries counts them) with their counts ("%2d"),
// dark when too costly, a "+" before the marked ones. Row `cursor` (from 1) is drawn again a pixel up and left under a
// gradient bar, its entry becomes listCursor, category 3 shows DrawObjectStats, and the object's description is
// centred in a bar at the bottom.
void Game::DrawItemList(int x, int y, int cursor, int top)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawItemListAdvance(x, y, cursor, top, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00478990: the stats panel of the selected object (攻, 防, 敏 of its record in lastObject, green, after
// the light labels) in a quarter-dark box, tinted by the object's first element once its fame reaches kTintedFame;
// the panel act runs and the menu slide is cleared. Returns x.
int Game::DrawObjectStats(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawObjectStatsAdvance(x, y, scene);
    scene.Show(this->battleServices);
    return x;
}

// swd3e.exe 0x00478C50: the refine stats panel: the panel act run, the menu slide cleared, a quarter-dark box and a
// frame, then 攻, 防 and 敏 of the selected object and the object given back at guardCursor added up (as dwords: each
// sum carries the next field of the second object 65536 times over), each followed by an arrow (0x23B6 frame 0x13).
// Returns x.
int Game::DrawRefineStats(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawRefineStatsAdvance(x, y, scene);
    scene.Show(this->battleServices);
    return x;
}

// swd3e.exe 0x00478FA0: the 蓄勁 panel of the current role: a quarter-dark box with two frames, the role's icon, its
// charge count ("X%2d", unk2EB2 + 1) and the choices 繼續 and 放勁 in columns (繼續 dark once unk2EB2 reaches a tenth
// of the proficiency of kind 0x27), choice `cursor` drawn again a pixel up and left on the highlight. Returns 2.
int Game::DrawChargePanel(int x, int y, int cursor)
{
    smooth::Scene scene;
    const int result = smooth::TurnFrame(*this).DrawChargePanelAdvance(x, y, cursor, scene);
    scene.Show(this->battleServices);
    return result;
}

// swd3e.exe 0x004753C0: the 護駕融合 panel: as the 佈置護駕 panel (DrawEscortList), an escort with flag 26B8 bit 17
// dark and the refine stats (DrawRefineStats) by the escort counted guardCursor from the last, the gradient bar on row
// guardCursor drawn after the names.
void Game::DrawEscortFusionList(int x, int y)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawEscortFusionListAdvance(x, y, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00474710: the object list of `category` with `title` at (x, y): the panel act restarted, a quarter-dark
// box with a title and a body frame, its scroll bar past 7 entries, then from entry `top` + 1 up to 7 objects with
// their counts ("%2d", light). Row `cursor` (from 1) is drawn again a pixel up and left under a gradient bar, its
// entry becomes listCursor, and category 3 shows DrawObjectStats at x 0x194.
void Game::DrawTitledItemList(int x, int y, int cursor, int top, int category, std::u8string_view title)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawTitledItemListAdvance(x, y, cursor, top, category, title, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00476200: the 戰鬥勝利 panel. The first time (the gold not yet given, bit 15 of battleGold): the meter is
// reset, the music and the sounds stop, kVictorySound plays; unless unk5772F8 each enemy's drop is added to the
// inventory as pending and to the loot list (a new object at the end, at most LootSlots of them); the roulette's
// experienceBonus raises the experience and moneyTimes multiplies the gold; each party role (not auto, guest, ghost or
// defeated) below the level cap (levelCap) and not turned to stone gains battleExperience experience, and unless
// unk5772F8 every worn accessory battleProficiency proficiency (accessoryGained when one counts) and, with a next
// special, its battle count 1 (2 against three enemies or more); its main act restarts and its effects end; then the
// gold is given and the bonuses cleared. Once the panel has grown, the experience, the gold and the proficiency are
// shown.
void Game::DrawVictoryPanel()
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawVictoryPanelAdvance(scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00476650: the 升級 panel while a role levels up (levelUpRole) or a skill was learned (learnedPending): a
// shade with a title and a body frame; once the panel has grown, "<name>升第<level>級" of the role that levelled up.
void Game::DrawLevelUpPanel()
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawLevelUpPanelAdvance(scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x004767D0: the first party role (not auto, guest or ghost) with the experience of its next level (below
// the level cap) levels up: the record is kept in levelUpBefore, the next level's maxima and attributes (LevelUp of
// both levels; a skill learned sets learnedPending) are added to it as the difference, hit points, spirit and stamina
// refilled, its level and experience mark taken; levelUpRole becomes the role and the level-up sound plays. Otherwise
// levelUpRole is kNoLevelUp. unk53C4C8 is set either way.
void Game::CheckLevelUps()
{
    BattleState& battle = this->battleState;

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        const BattleRole& unit = this->battleRoles[index];

        if (unit.AutoControl() == 1 || unit.GuestControl() == 1 || unit.Ghost() == 1)
        {
            continue;
        }

        const auto role = battle.partyRoles[index];
        RoleRecord& record = this->party.roles[static_cast<std::size_t>(role)];
        const int next = record.level + 1;
        u32 needed = 0;

        if (record.level >= battle.levelCap ||
            (this->database.LoadLevelEntry(role + 1, next, needed), record.totalExperience < static_cast<i32>(needed)))
        {
            battle.levelUpRole = kNoLevelUp;
            continue;
        }

        RoleRecord before;
        RoleRecord after;
        battle.levelUpBefore = record;
        LevelUp(
            before,
            role + 1,
            record.level,
            battle.learnedPending,
            this->party,
            this->database,
            battle.lastLearned,
            this->errors
        );
        LevelUp(
            after,
            role + 1,
            next,
            battle.learnedPending,
            this->party,
            this->database,
            battle.lastLearned,
            this->errors
        );
        record.level = after.level;
        record.experience = after.experience;
        record.maxHitPoints = static_cast<i16>(record.maxHitPoints + after.maxHitPoints - before.maxHitPoints);
        record.hitPoints = record.maxHitPoints;
        record.maxSpirit = static_cast<i16>(record.maxSpirit + after.maxSpirit - before.maxSpirit);
        record.spirit = record.maxSpirit;
        record.maxStamina = static_cast<i16>(record.maxStamina + after.maxStamina - before.maxStamina);
        record.stamina = record.maxStamina;
        record.strength = static_cast<i16>(record.strength + after.strength - before.strength);
        record.endurance = static_cast<i16>(record.endurance + after.endurance - before.endurance);
        record.wisdom = static_cast<i16>(record.wisdom + after.wisdom - before.wisdom);
        record.agility = static_cast<i16>(record.agility + after.agility - before.agility);
        record.rapport = static_cast<i16>(record.rapport + after.rapport - before.rapport);
        record.reaction = static_cast<i16>(record.reaction + after.reaction - before.reaction);
        record.ailmentChance = static_cast<i16>(record.ailmentChance + after.ailmentChance - before.ailmentChance);
        record.evasion = static_cast<i16>(record.evasion + after.evasion - before.evasion);
        battle.levelUpRole = static_cast<u8>(index);
        this->audio.StopSound(kVictorySound);
        this->audio.PlaySound(kLevelUpSound, this->config.soundVolume);
        break;
    }

    battle.unk53C4C8 = 1;
}

// swd3e.exe 0x00476A90: party role `role` raised level by level up to `level` (below the level cap): each step keeps
// the record in levelUpBefore and adds the difference of the two levels' records (LevelUp; the learned flag goes to a
// local the original leaves unset) to the maxima and attributes (hit points, spirit and stamina refilled), takes the
// next level, its experience mark, and adds the difference of the marks to the total experience.
void Game::RaiseLevel(u16 role, u16 level)
{
    BattleState& battle = this->battleState;
    RoleRecord& record = this->party.roles[role];
    i32 learned = 0;

    for (u32 at = record.level; at < level && at < battle.levelCap; ++at)
    {
        RoleRecord before;
        RoleRecord after;
        battle.levelUpBefore = record;
        LevelUp(
            before,
            role + 1,
            static_cast<int>(at),
            learned,
            this->party,
            this->database,
            battle.lastLearned,
            this->errors
        );
        LevelUp(
            after,
            role + 1,
            static_cast<int>(at) + 1,
            learned,
            this->party,
            this->database,
            battle.lastLearned,
            this->errors
        );
        record.level = after.level;
        record.experience = after.experience;
        record.totalExperience += after.experience - before.experience;
        record.maxHitPoints = static_cast<i16>(record.maxHitPoints + after.maxHitPoints - before.maxHitPoints);
        record.hitPoints = record.maxHitPoints;
        record.maxSpirit = static_cast<i16>(record.maxSpirit + after.maxSpirit - before.maxSpirit);
        record.spirit = record.maxSpirit;
        record.maxStamina = static_cast<i16>(record.maxStamina + after.maxStamina - before.maxStamina);
        record.stamina = record.maxStamina;
        record.strength = static_cast<i16>(record.strength + after.strength - before.strength);
        record.endurance = static_cast<i16>(record.endurance + after.endurance - before.endurance);
        record.wisdom = static_cast<i16>(record.wisdom + after.wisdom - before.wisdom);
        record.agility = static_cast<i16>(record.agility + after.agility - before.agility);
        record.rapport = static_cast<i16>(record.rapport + after.rapport - before.rapport);
        record.reaction = static_cast<i16>(record.reaction + after.reaction - before.reaction);
        record.ailmentChance = static_cast<i16>(record.ailmentChance + after.ailmentChance - before.ailmentChance);
        record.evasion = static_cast<i16>(record.evasion + after.evasion - before.evasion);
    }
}

// swd3e.exe 0x004776B0: the box of a new spell (%s學到新奇術) of the role that levelled up
void Game::DrawNewSpell()
{
    this->DrawLearnedBox(kNewSpellLine, false);
}

// swd3e.exe 0x00477850: the box of a new special skill (%s學到新絕技), with kLearnedSound as it opens
void Game::DrawNewSpecial()
{
    this->DrawLearnedBox(kNewSpecialLine, true);
}

// The box of 0x004776B0 and 0x00477850 (inline in each): while a skill was learned (learnedPending), the line with the
// role's name in a box as wide as it (20 pixels per two bytes), grown open; then the line in white and the skill's
// name in brackets, centred below it on a dark background.
void Game::DrawLearnedBox(std::u8string_view line, bool sound)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawLearnedBoxAdvance(line, sound, scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x00477A00: the first battle role (not auto, guest, ghost or defeated) whose next special skill is not
// exhausted (battle block [3] not -1) and has fought enough battles learns it: its first type 0x14 skill (from
// Party::skills by party role) sets, while [3] is not yet marked, the battle block's needed battles ([2], the skill's
// counterMax) and next special ([3], its unk42 with bit 31). With [1] battles at least [2] and a next special, the
// special is appended to the list, its name kept for the message (learnedPending, levelUpRole the role) and the block
// restarted from it (-1 without a further one); no other role learns in the same call.
void Game::LearnSpecials()
{
    BattleState& battle = this->battleState;

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];
        const auto partyRole = static_cast<std::size_t>(battle.partyRoles[index]);
        RoleBattleBlock& block = this->party.battleBlocks[partyRole];
        std::unique_ptr<ItemNode>& list = this->party.skills[partyRole];

        // An empty list is read through a null pointer in the original.
        if (role.AutoControl() == 1 || role.GuestControl() == 1 || role.Ghost() == 1 || role.IsDefeated() ||
            block.values[kNextSpecial] == kNoSpecial || list == nullptr)
        {
            continue;
        }

        for (ItemNode* node = list->next.get(); node != nullptr; node = node->next.get())
        {
            if (node->record.data.category != kSpecialType || block.values[kNextSpecial] < 0)
            {
                continue;
            }

            this->database.LoadObject(battle.lastObject, node->id);
            Database::FreeDescription(battle.lastObject);
            block.values[kNeededBattles] = battle.lastObject.data.counterMax;
            block.values[kNextSpecial] = static_cast<i32>(battle.lastObject.data.unk42 | kSpecialMark);
        }

        if (block.values[kBattlesFought] < block.values[kNeededBattles] ||
            (static_cast<u32>(block.values[kNextSpecial]) & kSpecialIdMask) == 0)
        {
            continue;
        }

        const auto id = static_cast<u16>(block.values[kNextSpecial]);
        this->database.LoadObject(battle.lastObject, id);
        Database::FreeDescription(battle.lastObject);
        ItemNode* last = list.get();

        while (last->next != nullptr)
        {
            last = last->next.get();
        }

        last->next = std::make_unique<ItemNode>();
        ItemNode& learned = *last->next;
        learned.id = id;
        this->database.LoadObject(learned.record, id);
        battle.learnedPending = 1;
        battle.lastLearned.name = ObjectName(learned.record);
        battle.levelUpRole = static_cast<u8>(index);
        block.values[kBattlesFought] = 0;
        block.values[kNeededBattles] = battle.lastObject.data.counterMax;
        block.values[kNextSpecial] = static_cast<i32>(battle.lastObject.data.unk42 | kSpecialMark);

        if (battle.lastObject.data.unk42 == 0)
        {
            block.values[kNextSpecial] = kNoSpecial;
        }

        return;
    }
}

// swd3e.exe 0x00476CB0: the stats of the role that levelled up (levelUpRole): its name over a frame of the seven
// values before the level-up (levelUpBefore); once grown, after kRollStart frames (levelUpFrames, kShowNow shows at
// once) each value that rose shows " ==> " and the new value less a roll that counts down a point a frame (kRollSound
// each point), one after another: the maxima first, then the attributes (strength does not wait for the maxima, as in
// the original).
void Game::DrawLevelUpStats()
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawLevelUpStatsAdvance(scene);
    scene.Show(this->battleServices);
}

// A 1-based place moved by `step`, wrapping at the end it moves towards only (0x00460C40: below 1 to `count`;
// 0x00461240: past `count` to 1). A place left past the other end by another count stays there.
void Game::StepWrapped(i32& place, int step, i32 count)
{
    place += step;

    if (step < 0 && place < 1)
    {
        place = count;
    }
    else if (step > 0 && place > count)
    {
        place = 1;
    }
}

// swd3e.exe 0x0046D180: the up key of the battle menu (kBackSound), by battle state. 1 steps the ghost's orders
// (wrapping to 8) or the 煉妖 choices (to 2), none with the elements of 改變; otherwise a command a row up (from the
// first row to the last one listed: the third row, else the second), none for the role that took 蓄勁 (a command below
// 0 goes to 9). 2, 4, 0x1B, 0x29 and 0x50 move the list cursor up, then scroll (4 keeps the list for role listRole,
// 0x29 and 0x50 restart the menu slide at 0x50); 5 and 0xA cycle 2..1; 7 and 8 cycle their entries; 0x1E cycles 10..1.
// 3 (unless unk577284) moves the aim (AimStep), 9 the ghost's target through the standing enemies.
void Game::MenuUp()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    auto tick = [this]
    {
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    };

    // The list cursor of states 2, 0x1B, 0x29 and 0x50 (inline in the original): false when it stopped at the top.
    auto listUp = [](i32& cursor, i32& scroll, i32 entries)
    {
        if (--cursor >= 1)
        {
            return true;
        }

        cursor = entries != 0 ? 1 : 0;

        if (--scroll < 0)
        {
            scroll = 0;
        }

        return false;
    };

    switch (battle.result)
    {
        case kMenuState:
        {
            tick();

            if (battle.ghostOrdersOpen == 1)
            {
                if (--battle.ghostOrderRow <= 0)
                {
                    battle.ghostOrderRow = static_cast<i32>(BattleState::GhostOrderNames.size());
                }

                return;
            }

            if (battle.refineOptionsOpen == 1)
            {
                if (--battle.refineOptionRow <= 0)
                {
                    battle.refineOptionRow = static_cast<i32>(BattleState::RefineOptionNames.size());
                }

                return;
            }

            if (battle.elementsOpen == 1)
            {
                return;
            }

            i32 command = battle.menuCommand;

            if (command < 0)
            {
                battle.menuCommand = kLastMenuCommand;
                return;
            }

            if (battle.chargingRole == battle.currentUnit)
            {
                return;
            }

            if (command <= kMenuColumns)
            {
                const i32 last = battle.listedCommands + kMenuFixedCommands;
                command += 2 * kMenuColumns;
                battle.menuCommand = command;

                if (command <= last)
                {
                    battle.unk5772B4 = 1;
                    return;
                }

                command -= kMenuColumns;
                battle.menuCommand = command;

                if (command <= last)
                {
                    battle.unk5772B4 = 1;
                    return;
                }
            }

            battle.unk5772B4 = 1;
            battle.menuCommand = command - kMenuColumns;
            return;
        }

        case kListState2:
            if (--battle.pageListRow >= 1)
            {
                tick();
                battle.unk5772B4 = 1;
                return;
            }

            battle.pageListRow = 1;

            if (battle.listScroll > 0)
            {
                tick();
            }

            battle.unk5772B4 = 1;

            if (--battle.listScroll < 0)
            {
                battle.listScroll = 0;
            }

            return;

        case kTargetState:
            if (battle.unk577284 == 1)
            {
                return;
            }

            tick();
            this->AimStep(-1);
            return;

        case kListState4:
            if (--battle.roleListRow < 1)
            {
                battle.roleListRow = battle.roleListEntries != 0 ? 1 : 0;

                if (--battle.roleListScroll < 0)
                {
                    battle.roleListScroll = 0;
                }
            }

            tick();
            battle.unk5772B4 = 1;
            battle.roleListRows[static_cast<std::size_t>(battle.listRole)] = battle.roleListRow;
            battle.roleListScrolls[static_cast<std::size_t>(battle.listRole)] = battle.roleListScroll;
            return;

        case kWaitState5:
        case kWaitStateA:
            tick();

            if (--battle.guardCursor < 1)
            {
                battle.guardCursor = kEscortRows;
            }

            return;

        case kTalkState:
            battle.orderedSkill = 0;
            battle.unk577018 = 0;
            tick();

            if (--battle.controlCursor < 1)
            {
                battle.controlCursor = battle.controlRows;
            }

            return;

        case kSkillListState:
            if (battle.specialCount == 0)
            {
                return;
            }

            if (--battle.specialRow < 1)
            {
                battle.specialRow = static_cast<i8>(battle.specialCount);
            }

            tick();
            battle.unk5772B4 = 1;
            return;

        case kTurnState9:
            tick();
            this->AimGhostStep(-1);
            return;

        case kListState1B:
            if (listUp(battle.roleListRow, battle.roleListScroll, battle.roleListEntries))
            {
                tick();
            }

            battle.unk5772B4 = 1;
            return;

        case kWaitState1E:
            if (--battle.roleListRow < 1)
            {
                battle.roleListRow = kMenuStepMost;
            }

            tick();
            battle.unk5772B4 = 1;
            return;

        case kWaitState29:
        case kWaitState50:
            if (listUp(battle.roleListRow, battle.roleListScroll, battle.roleListEntries))
            {
                tick();
            }

            battle.unk5772B4 = 1;
            battle.menuSlide = kListSlideStart;
            return;

        default:
            return;
    }
}

// swd3e.exe 0x0046DB70: the down key of the battle menu, as MenuUp the other way: 1 steps the ghost's orders (wrapping
// to 1) or the 煉妖 choices; otherwise a command a row down (wrapping to the first row, past empty listed commands; a
// command past 9 goes to 1), none for the role that took 蓄勁; 2, 4, 0x1B, 0x29 and 0x50 move the list cursor down to
// the seventh row, then scroll, stopping at the end of the list; 5 and 0xA cycle 1..2; 7, 8 and 0x1E cycle forward;
// 3 and 9 move the aim forward.
void Game::MenuDown()
{
    BattleState& battle = this->battleState;
    battle.unk5772B8 = 0;

    auto tick = [this]
    {
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
    };

    // The list cursor of states 2, 4, 0x1B, 0x29 and 0x50 (inline in the original): false when it stopped at the end.
    auto listDown = [](i32& cursor, i32& scroll, i32 entries)
    {
        if (++cursor <= kListRowsShown)
        {
            if (cursor <= entries)
            {
                return true;
            }

            cursor = entries;
            return false;
        }

        cursor = kListRowsShown;

        if (++scroll + kListRowsShown <= entries)
        {
            return true;
        }

        scroll = entries - kListRowsShown;

        return false;
    };

    switch (battle.result)
    {
        case kMenuState:
        {
            tick();

            if (battle.ghostOrdersOpen == 1)
            {
                if (++battle.ghostOrderRow > static_cast<i32>(BattleState::GhostOrderNames.size()))
                {
                    battle.ghostOrderRow = 1;
                }

                return;
            }

            if (battle.refineOptionsOpen == 1)
            {
                if (++battle.refineOptionRow > static_cast<i32>(BattleState::RefineOptionNames.size()))
                {
                    battle.refineOptionRow = 1;
                }

                return;
            }

            if (battle.elementsOpen == 1 || battle.chargingRole == battle.currentUnit)
            {
                return;
            }

            i32 command = battle.menuCommand;

            if (command > kLastMenuCommand)
            {
                battle.menuCommand = 1;
                return;
            }

            do
            {
                command += kMenuColumns;

                if (command > kLastMenuCommand)
                {
                    command -= kLastMenuCommand;
                }
            } while (command > kMenuFixedCommands && battle.MenuKind(command) == 0);

            battle.unk5772B4 = 1;
            battle.menuCommand = command;
            return;
        }

        case kListState2:
            if (listDown(battle.pageListRow, battle.listScroll, static_cast<i8>(battle.listEntries)))
            {
                tick();
            }

            battle.unk5772B4 = 1;
            return;

        case kTargetState:
            if (battle.unk577284 == 1)
            {
                return;
            }

            tick();
            this->AimStep(1);
            return;

        case kListState4:
            if (listDown(battle.roleListRow, battle.roleListScroll, battle.roleListEntries))
            {
                tick();
            }

            battle.roleListRows[static_cast<std::size_t>(battle.listRole)] = battle.roleListRow;
            battle.roleListScrolls[static_cast<std::size_t>(battle.listRole)] = battle.roleListScroll;
            battle.unk5772B4 = 1;
            return;

        case kWaitState5:
        case kWaitStateA:
            tick();

            if (++battle.guardCursor > kEscortRows)
            {
                battle.guardCursor = 1;
            }

            return;

        case kTalkState:
            battle.orderedSkill = 0;
            battle.unk577018 = 0;
            tick();

            if (++battle.controlCursor > battle.controlRows)
            {
                battle.controlCursor = 1;
            }

            return;

        case kSkillListState:
            if (battle.specialCount == 0)
            {
                return;
            }

            if (++battle.specialRow > static_cast<i8>(battle.specialCount))
            {
                battle.specialRow = 1;
            }

            tick();
            battle.unk5772B4 = 1;
            return;

        case kTurnState9:
            tick();
            this->AimGhostStep(1);
            return;

        case kListState1B:
            if (listDown(battle.roleListRow, battle.roleListScroll, battle.roleListEntries))
            {
                tick();
            }

            battle.unk5772B4 = 1;
            return;

        case kWaitState1E:
            if (++battle.roleListRow > kMenuStepMost)
            {
                battle.roleListRow = 1;
            }

            tick();
            battle.unk5772B4 = 1;
            return;

        case kWaitState29:
        case kWaitState50:
            battle.menuSlide = kListSlideStart;

            if (listDown(battle.roleListRow, battle.roleListScroll, battle.roleListEntries))
            {
                tick();
            }

            battle.unk5772B4 = 1;
            return;

        default:
            return;
    }
}

// The aim of MenuUp (`step` -1) and MenuDown (1) in battle state 3 (inline in each). An order on the party: 融合 aims
// at a standing escort (down: the first from the role after the aim; up: the last from the role before the last one,
// as in the original); with fewer than three roles beside the left-out, guest and escort ones the aim steps through
// the 1-based roles, otherwise through RoleCycle, skipping auto-controlled, guest, guarding and ghost roles. The four
// party roles are unmarked (and their portraits) and the aimed one marked. Otherwise the aim steps through the
// standing enemies (AimEnemyStep).
void Game::AimStep(int step)
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);

    if (battle.pendingOrders[unit].unk00 == 0)
    {
        UnitRect rect;
        this->AimEnemyStep(step, rect);
        return;
    }

    // Roles past the array (RoleCycle read past its end, the role after the last one) are not taken (deviations.md).
    auto role = [this](i32 index) -> BattleRole*
    {
        return index >= 0 && index < static_cast<i32>(BattleState::RoleUnits)
                   ? &this->battleRoles[static_cast<std::size_t>(index)]
                   : nullptr;
    };

    auto aimable = [&role](i32 index)
    {
        const BattleRole* checked = role(index);
        return checked != nullptr && checked->AutoControl() != 1 && checked->GuestControl() != 1 &&
               !checked->IsGuarding() && checked->Ghost() != 1;
    };

    auto escort = [&role](i32 index)
    {
        const BattleRole* checked = role(index);
        return checked != nullptr && !checked->IsDefeated() && checked->AutoControl() == 1;
    };

    const auto roles = static_cast<i32>(battle.roleCount);
    const i32 others = roles - battle.unk5770D4 - battle.guestCount - battle.escortCount;
    UnitRect rect;
    int tries = 0;

    auto spin = [&tries]
    {
        // The original searches for ever when no role can be taken.
        if (++tries > static_cast<int>(BattleState::RoleUnits) * kHangTries)
        {
            Hang();
        }
    };

    if (battle.menuCommand == kFuseCommand)
    {
        i32 index = step > 0 ? battle.aimTarget : roles - 2;

        if (step > 0 ? index < roles : index > 0)
        {
            while (!escort(index))
            {
                index += step;

                if (step > 0 ? index > roles : index < 0)
                {
                    index = -1;
                    break;
                }
            }

            if (index >= 0)
            {
                battle.aimTarget = index + 1;
            }
        }
    }
    else if (static_cast<u32>(others) < static_cast<u32>(kFewAimRoles))  // unsigned (0x0046D4D7)
    {
        do
        {
            StepWrapped(battle.aimTarget, step, roles);
            spin();
        } while (!aimable(battle.aimTarget - 1));

        this->battleRoles[static_cast<std::size_t>(battle.aimTarget - 1)].GetShadowRect(services, rect);
    }
    else
    {
        const i32 cycle = roles - battle.guestCount - battle.escortCount;

        do
        {
            StepWrapped(battle.orderStep, step, cycle);
            // Past RoleCycle the original reads on (RoleCycleRead); the string bytes beyond are no role.
            battle.actingEnemy = battle.RoleCycleRead(battle.orderStep).value_or(kPastRoleCycle);
            spin();
        } while (!aimable(battle.actingEnemy));

        this->battleRoles[static_cast<std::size_t>(battle.actingEnemy)].GetShadowRect(services, rect);
        battle.aimTarget = battle.actingEnemy + 1;
        battle.actingEnemy = 0;
    }

    for (std::size_t panel = 0; panel < static_cast<std::size_t>(Party::Roles); ++panel)
    {
        this->battleRoles[panel].SetAC0(0);
        this->battleHud.portraitHighlight[panel] = 0;
    }

    // 融合 without a standing escort keeps the aim past the roles (aimTarget 11); the original marks a unit past the
    // role table (deviations.md).
    if (static_cast<u32>(battle.aimTarget - 1) < this->battleRoles.size())
    {
        this->battleRoles[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(1);
    }

    battle.unk5772B4 = 1;
    battle.unk5777F0 = 1;
}

// The aim through the standing enemies by `step` (inline in MenuUp and MenuDown): enemyOrder stepped (wrapping) past
// the defeated, the shadow of the aimed enemy into `rect`, the enemies unmarked and the aimed one marked.
void Game::AimEnemyStep(int step, UnitRect& rect)
{
    BattleState& battle = this->battleState;
    StepWrapped(battle.orderStep, step, static_cast<i32>(battle.enemyCount));
    int tries = 0;

    for (battle.actingEnemy = battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)];
         this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated();
         battle.actingEnemy = battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)])
    {
        StepWrapped(battle.orderStep, step, static_cast<i32>(battle.enemyCount));

        // The original loops for ever when every enemy is down.
        if (++tries > BattleState::Enemies)
        {
            Hang();
        }
    }

    this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].GetShadowRect(this->battleServices, rect);

    for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
    {
        this->battleEnemies[enemy].SetAC0(0);
    }

    this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].SetAC0(1);
    battle.unk5772B4 = 1;
    battle.unk5777F0 = 1;
    battle.aimTarget = battle.actingEnemy + 1;
}

// Battle state 9 of MenuUp and MenuDown (swd3e, inline in each): the talisman ghost's target (鎖定目標) steps through
// the standing enemies as the aim does, and the pointer goes to the middle of its shadow.
void Game::AimGhostStep(int step)
{
    BattleState& battle = this->battleState;
    UnitRect rect;
    this->AimEnemyStep(step, rect);
    battle.aiTarget = battle.aimTarget;
    this->input.SetMousePosition(rect.x + (rect.width >> 1), rect.y + (rect.height >> 1));
}

// swd3e.exe 0x00468480: the debug keys of the battle (with the debug mode). With a Ctrl key: F3 stops the music, F4
// toggles showRoleBlocks, F1 and X toggle unk537A18 and unk576084, K starts a strong attack (強攻擊), Z refills the
// roles (vitals, fury and strong attributes), T draws the enemies' gauges again, D and R hurt the roles, V the enemies,
// F9 toggles skipping the talk, E ends the battle (0), C ends the spell and the acting unit (the original then resets
// the turn of role actingUnit - 20 and enemy actingUnit with actingUnit just set to -1: not done here), F5 slows the
// frames or restores them, F2 toggles the absolute spirit pact, W defeats the enemies. Without Ctrl: H and J move
// every unit and the background 10 pixels, T and Y set the slide mode. P toggles the snapshot in any mode. 1
// otherwise.
int Game::BattleDebugKeys()
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    Input& input = this->input;

    auto pressed = [&input](int key)
    {
        return input.IsKeyDown(key) != 0;
    };

    auto pause = []
    {
        SleepMilliseconds(kDebugPause);
    };

    if (this->config.debugMode == 1)
    {
        if (pressed(Input::LeftControl) || pressed(Input::RightControl))
        {
            if (pressed(Input::F3))
            {
                this->audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
            }

            if (pressed(Input::F4))
            {
                SleepMilliseconds(kDebugShortPause);
                battle.showRoleBlocks = (battle.showRoleBlocks + 1) % kToggleStates;
            }

            if (pressed(Input::F1))
            {
                pause();
                battle.unk537A18 = battle.unk537A18 == 0 ? 1 : 0;
            }

            if (pressed(Input::X))
            {
                pause();
                battle.unk576084 = battle.unk576084 == 0 ? 1 : 0;
            }

            if (pressed(Input::K))
            {
                if (battle.criticalAttack == 0)
                {
                    battle.criticalAttack = 1;
                }

                this->battleMessages
                    .Add(kDebugMessageX, kDebugMessageY, kDebugMessageFrames, kStrongAttack, BattleMessage::Centered);
            }

            if (pressed(Input::Z))
            {
                pause();

                for (u32 index = 0; index < battle.roleCount; ++index)
                {
                    BattleRole& role = this->battleRoles[index];
                    role.SetVitals(kRefill, kRefill, kRefill);
                    role.AddFury(services, kFillFury);
                }

                for (u32 index = 0; index < battle.roleCount; ++index)
                {
                    this->battleRoles[index].SetAttributes(kDebugAttack, kDebugDefense, kDebugAgility);
                }
            }

            if (pressed(Input::T))
            {
                pause();

                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    this->battleEnemies[index].RandomizeGauge(services, 1);
                }
            }

            auto hurtRoles = [this, &battle, &services](int hitPoints, i16 drain)
            {
                for (u32 index = 0; index < battle.roleCount; ++index)
                {
                    BattleRole& role = this->battleRoles[index];

                    if (role.AutoControl() != 1 && role.GuestControl() != 1)
                    {
                        role.Damage(services, hitPoints, drain, drain);
                    }
                }
            };

            if (pressed(Input::D))
            {
                pause();
                hurtRoles(kSmallHurt, 0);
            }

            if (pressed(Input::R))
            {
                SleepMilliseconds(kDebugShortPause);
                hurtRoles(kBigHurt, kBigDrain);
            }

            if (pressed(Input::V))
            {
                pause();

                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    this->battleEnemies[index].Damage(services, kEnemyHurt, 0, 0);
                }
            }

            if (pressed(Input::F9))
            {
                pause();
                this->talk.skipAll = (this->talk.skipAll + 1) % kToggleStates;
            }

            if (pressed(Input::E))
            {
                this->audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
                this->audio.Queue().Switch(battle.musicBeforeBattle, MusicQueue::RepeatList, 0);
                this->EndBattle();
                return 0;
            }

            if (pressed(Input::C))
            {
                battle.spellStage = static_cast<u16>(battle.spellStage | BattleState::SpellOver);
                battle.unk577270 = 0;
                battle.ClearActing();
                battle.unk5772BC = 0;
                battle.unk5772C0 = 0;
                battle.actingUnit = -1;

                if (battle.unk577260 == 1)
                {
                    battle.unk577260 = 0;
                }

                if (battle.hitsAllRoles == 1)
                {
                    battle.hitsAllRoles = 0;
                }
            }

            if (pressed(Input::F5))
            {
                if (battle.slowFrames != 0)
                {
                    this->state.SetFrameInterval(Game::FrameInterval);
                    battle.slowFrames = 0;
                }
                else
                {
                    this->state.SetFrameInterval(kSlowFrameInterval);
                    battle.slowFrames = 1;
                }
            }

            if (pressed(Input::F2))
            {
                pause();

                if (battle.unk577330 == 1)
                {
                    battle.panelFlags &= ~BattleState::UnkPanel2;
                    battle.unk577330 = 0;
                    this->battleMessages
                        .Add(kDebugMessageX, kDebugMessageY, kDebugMessageFrames, kNormalPact, BattleMessage::Centered);
                }
                else
                {
                    battle.panelFlags |= BattleState::UnkPanel2;
                    battle.unk577330 = 1;
                    this->battleMessages.Add(
                        kDebugMessageX,
                        kDebugMessageY,
                        kDebugMessageFrames,
                        kAbsolutePact,
                        BattleMessage::Centered
                    );
                }
            }

            if (pressed(Input::W))
            {
                for (u32 index = 0; index < battle.enemyCount; ++index)
                {
                    BattleEnemy& enemy = this->battleEnemies[index];

                    if (!enemy.IsDefeated())
                    {
                        battle.unk513664[index] = static_cast<i32>(index);
                        battle.slotDamage[index] = 0;
                        enemy.Damage(services, kDefeatHurt, 0, 0);
                    }
                }

                battle.unk577064 = static_cast<i32>(battle.enemyCount);
                battle.roleOrder = {};
                std::fill_n(battle.turnTaken.begin() + kClearedTurnFirst, kClearedTurnUnits, 0);
                battle.slotRecords.fill(BattleSlotRecord{});
                battle.unk4BF1CA = kNoTalkRole;
                battle.unk57727C = 1;
                battle.unk577050 = 0;
                battle.unk577260 = 0;
                battle.unk5772BC = 1;
                battle.unk5772C0 = 1;
                battle.unk577258 = 0;
                battle.turnUnit = 0;
                battle.currentUnit = 0;
                battle.actingUnit = -1;
                battle.result = 0;
            }
        }

        auto moveAll = [this, &battle](i16 step)
        {
            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                this->battleRoles[index].Move(step, 0);
            }

            for (u32 index = 0; index < battle.enemyCount; ++index)
            {
                this->battleEnemies[index].Move(step, 0);
            }

            battle.shake = step;
        };

        if (pressed(Input::H))
        {
            moveAll(kDebugStep);
        }

        if (pressed(Input::J))
        {
            moveAll(-kDebugStep);
        }

        if (pressed(Input::T))
        {
            battle.slideMode = BattleState::SlideToUnit;
        }

        if (pressed(Input::Y))
        {
            battle.slideMode = BattleState::SlideBack;
        }
    }

    if (pressed(Input::P))
    {
        battle.snapNext = battle.snapNext != 1 ? 1 : 0;
    }

    return 1;
}

// swd3e.exe 0x00468B20: the debug overlay (unk537A18): each enemy's hit points, lock (GetWord2A74), command and
// level, each role's lock and command, the slot records (攻擊順序), the party order (我方等待順序) and the draw order
// (排列順序), the acting and current units, each enemy's action gauge as a two-row bar of kGaugePixel at its place
// with its second gauge as a green bar, and the state values (無敵, UM, 戰鬥, the menu, the command, the aim, the
// locks, the battle number, the enemies gone, the frame). With showRoleBlocks the experience values of the four party
// battle blocks follow.
void Game::DrawBattleDebug()
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawBattleDebugAdvance(scene);
    scene.Show(this->battleServices);
}

// swd3e.exe 0x0046BFB0: the pointer over the battle (only when it moved, or while unk5772B8): the click regions of
// a talk choose Talk::choice; then by battle state: 0 and 1 (outside the menu) find the party panel under it
// (anyStateRole); 1 picks the row of an open choice (ghost's orders, 煉妖, elements), the command of the role that
// took 蓄勁, or the enabled menu command under it (a grid of three a row); 2, 4, 0x1B, 0x29 and 0x50 pick a page tab
// and a list row, or the scroll arrows, track and thumb (hoverScrollPart 1..4); 3 aims at the unit whose picture is
// under it (or the party portrait), 9 the ghost's target; 5, 0xA, 8 and 0x1E pick a row or a cell. 1 when something
// is under the pointer.
int Game::MouseHover()
{
    BattleState& battle = this->battleState;
    const int x = this->state.pointerX;
    const int y = this->state.pointerY;

    if (battle.pointerX == x && battle.pointerY == y)
    {
        if (battle.unk5772B8 == 0)
        {
            return 0;
        }
    }
    else
    {
        battle.unk5772B8 = 1;
        battle.promptFrames = 0;
    }

    battle.pointerX = x;
    battle.pointerY = y;

    if (!this->state.clickRegions.regions.empty())
    {
        const int index = this->state.clickRegions.Find(x, y, battle.hoverRegion);

        if (battle.hoverRegion != nullptr)
        {
            this->talk.choice = index;
        }
    }

    // The pointer is compared unsigned throughout (jbe/jnb in the original).
    auto inside = [](int value, int low, int high)
    {
        return static_cast<u32>(value) > static_cast<u32>(low) && static_cast<u32>(value) < static_cast<u32>(high);
    };

    // The step of a hit test over bands (inline in the original): the band of `value` among `bands` bands of `size`
    // ending at `first`, `first` + `size`, ...; -1 outside them.
    auto band = [&inside](int value, int first, int size, int end)
    {
        int found = 0;

        for (int edge = first; edge < end; edge += size)
        {
            if (inside(value, edge - size, edge))
            {
                return found;
            }

            ++found;
        }

        return -1;
    };

    auto panelUnder = [&battle, x, y, &inside](bool lastWins)
    {
        if (!inside(y, kPanelsTop, Display::Height))
        {
            return false;
        }

        bool found = false;

        for (u32 index = 0; index < battle.roleCount; ++index)
        {
            // The roles after the party read on past partyRoles and PortraitX (the ghost takes the panel of party
            // role 1, at 0x5D), as the original does.
            const std::optional<i32> role = battle.PartyRoleRead(index);
            const std::optional<int> left = role ? BattleState::PortraitXRead(*role) : std::nullopt;

            if (left && inside(x, *left - kPanelLeftMargin, *left + kPanelWidth))
            {
                battle.anyStateRole = static_cast<u16>(index + BattleState::FirstRoleUnit);
                found = true;

                if (!lastWins)
                {
                    return true;
                }
            }
        }

        return found;
    };

    auto scrollParts = [this, &battle, x, y, &inside](int left, int right, int downTop, int downBottom)
    {
        const BattleHud& hud = this->battleHud;

        if (!inside(x, left, right))
        {
            battle.hoverScrollPart = 0;
            battle.unk577078 = 0;
            battle.unk5772B4 = 0;
            return 0;
        }

        int part = 0;

        if (inside(y, kScrollUpTop, kScrollUpBottom))
        {
            part = kScrollUp;
        }
        else if (inside(y, downTop, downBottom))
        {
            part = kScrollDown;
        }
        else if (inside(y, hud.scrollTrackTop, hud.scrollThumbTop))
        {
            part = kScrollAbove;
        }
        else if (
            static_cast<u32>(x) < static_cast<u32>(right) && inside(y, hud.scrollThumbBottom, hud.scrollTrackBottom)
        )
        {
            part = kScrollBelow;
        }

        if (part == 0)
        {
            battle.hoverScrollPart = 0;
            battle.unk577078 = 0;
            battle.unk5772B4 = 0;
            return 0;
        }

        battle.unk5772B4 = 1;
        battle.hoverScrollPart = part;
        return 1;
    };

    auto pickRow = [this, &battle](i32& cursor, int row)
    {
        if (cursor != row + 1)
        {
            this->audio.PlaySound(kBackSound, this->config.soundVolume);
        }

        cursor = row + 1;
        battle.hoverScrollPart = 0;
        battle.unk5772B4 = 1;
        battle.unk5772B8 = 1;
        return 1;
    };

    auto leaveList = [this, &battle]
    {
        battle.unk5772B4 = 0;

        if (this->controls.buttons[static_cast<std::size_t>(Button::MouseLeft)].heldFrames >= 1)
        {
            return false;
        }

        battle.unk577078 = 0;
        battle.unk57703C = 0;
        battle.unk577040 = 0;
        battle.unk5772FC = 0;
        return battle.roleListEntries > kListRowsShown;
    };

    switch (battle.result)
    {
        case 0:
            if (battle.enemiesGone >= static_cast<i32>(battle.enemyCount))
            {
                return 0;
            }

            if (panelUnder(false))
            {
                return 1;
            }

            battle.anyStateRole = kNoPanelRole;
            return 0;

        case kMenuState:
        {
            if (battle.currentUnit == 0)
            {
                return 0;
            }

            // A choice under the pointer takes its row (kBackSound when it changes); 0 either way, as in the
            // original.
            auto choose = [this, &battle](i32& row, int found)
            {
                if (row != found + 1)
                {
                    this->audio.PlaySound(kBackSound, this->config.soundVolume);
                }

                row = found + 1;
                battle.unk5772B4 = 1;
                return 0;
            };

            battle.unk5772B4 = 0;

            if (battle.ghostOrdersOpen == 1)
            {
                const int row = inside(x, kGhostOrdersLeft, kGhostOrdersRight)
                                    ? band(y, kGhostOrdersFirst, kMenuRowHeight, kGhostOrdersEnd)
                                    : -1;
                return row >= 0 ? choose(battle.ghostOrderRow, row) : 0;
            }

            if (battle.refineOptionsOpen == 1)
            {
                const int row = inside(x, kRefineOptionsLeft, kRefineOptionsRight)
                                    ? band(y, kRefineOptionsFirst, kMenuRowHeight, kRefineOptionsEnd)
                                    : -1;
                return row >= 0 ? choose(battle.refineOptionRow, row) : 0;
            }

            if (battle.elementsOpen == 1)
            {
                const int cell = inside(y, kElementsHitTop, kElementsHitBottom)
                                     ? band(x, kElementsHitFirst, kMenuRowHeight, kElementsHitEnd)
                                     : -1;
                return cell >= 0 ? choose(battle.elementRow, cell) : 0;
            }

            const int left = battle.menuX;
            const int top = battle.menuY;

            if (battle.chargingRole == battle.currentUnit)
            {
                const int cell = inside(y, top + kGatherHitTop, top + kGatherHitBottom)
                                     ? band(
                                           x,
                                           left + kGatherHitFirst,
                                           kMenuColumnWidth,
                                           left + kGatherHitFirst + kGatheringCommands * kMenuColumnWidth
                                       )
                                     : -1;
                return cell >= 0 ? choose(battle.menuCommand, cell) : 0;
            }

            if (inside(x, left + kMenuHitLeft, left + kMenuHitRight) &&
                inside(y, top + kMenuHitTop, top + kMenuHitBottom))
            {
                const auto index = static_cast<int>(
                    static_cast<u32>(x - left - kMenuHitLeft) / kMenuHitColumn +
                    kMenuColumns * (static_cast<u32>(y - top - kMenuHitTop) / kMenuRowHeight)
                );

                if (index < battle.listedCommands + kMenuFixedCommands)
                {
                    if (battle.commandEnabled[static_cast<std::size_t>(index)] == 0)
                    {
                        return 0;
                    }

                    if (battle.menuCommand != index + 1)
                    {
                        this->audio.PlaySound(kBackSound, this->config.soundVolume);
                    }

                    battle.menuCommand = std::min(index + 1, battle.listedCommands + kMenuFixedCommands);
                    battle.unk5772B8 = 1;
                    battle.unk5772B4 = 1;
                    return 1;
                }
            }

            battle.anyStateRole = kNoPanelRole;
            panelUnder(true);
            return 0;
        }

        case kListState2:
        {
            battle.hoverPageTab = -1;

            if (inside(y, kTabsHitTop, kTabsHitBottom))
            {
                const int tab = band(x, kTabsHitFirst, kTabWidth, kSpellTabsEnd);

                if (tab >= 0)
                {
                    battle.hoverPageTab = tab;
                    battle.unk5772B8 = 1;
                }
            }

            if (inside(x, kListHitLeft, kSpellListRight))
            {
                const int row = band(y, kListHitFirst, kListRowHeight, kListHitEnd);

                if (row >= 0 && battle.listScroll + row + 1 <= static_cast<i8>(battle.listEntries))
                {
                    return pickRow(battle.pageListRow, row);
                }
            }

            return scrollParts(kSpellScrollLeft, kSpellScrollRight, kSpellScrollDownTop, kSpellScrollDownBottom);
        }

        case kTargetState:
            return this->HoverTarget();

        case kListState4:
        {
            battle.hoverRoleTab = -1;

            if (inside(y, kTabsHitTop, kTabsHitBottom))
            {
                const int tab = band(x, kTabsHitFirst, kTabWidth, kItemTabsEnd);

                if (tab >= 0)
                {
                    if (battle.listRole != tab)
                    {
                        battle.hoverRoleTab = tab;
                    }

                    battle.unk5772B8 = 1;
                }
            }

            if (inside(x, kListHitLeft, kItemListRight))
            {
                const int row = band(y, kListHitFirst, kListRowHeight, kListHitEnd);

                if (row >= 0 && battle.roleListScroll + row + 1 <= battle.roleListEntries)
                {
                    return pickRow(battle.roleListRow, row);
                }
            }

            if (!leaveList())
            {
                return 0;
            }

            return scrollParts(kItemScrollLeft, kItemScrollRight, kItemScrollDownTop, kItemScrollDownBottom);
        }

        case kWaitState5:
        case kWaitStateA:
        {
            if (!inside(x, kEscortHitLeft, kEscortHitRight))
            {
                battle.unk5772B4 = 0;
                return 0;
            }

            const int row = band(y, kEscortHitFirst, kSpecialRowHeight, kEscortHitEnd);

            if (row < 0)
            {
                battle.unk5772B4 = 0;
                return 0;
            }

            if (battle.guardCursor != row + 1)
            {
                this->audio.PlaySound(kBackSound, this->config.soundVolume);
            }

            battle.guardCursor = row + 1;
            battle.unk5772B4 = 1;
            battle.unk5772B8 = 1;
            return 1;
        }

        case kSkillListState:
        {
            if (!inside(x, kListHitLeft, kSpecialHitRight))
            {
                battle.unk5772B4 = 0;
                return 0;
            }

            const int row = band(y, kSpecialHitFirst, kListBarHeight, kSpecialHitEnd);

            if (row < 0 || row + 1 > static_cast<i8>(battle.specialCount))
            {
                battle.unk5772B4 = 0;
                return 0;
            }

            if (battle.specialRow != row + 1)
            {
                this->audio.PlaySound(kBackSound, this->config.soundVolume);
            }

            battle.specialRow = row + 1;
            battle.unk5772B4 = 1;
            battle.unk5772B8 = 1;
            return 1;
        }

        case kTurnState9:
            return this->HoverGhostTarget();

        case kListState1B:
        case kWaitState29:
        case kWaitState50:
        {
            if (inside(x, kListHitLeft, kThrowListRight))
            {
                const int row = band(y, kListHitFirst, kListRowHeight, kListHitEnd);

                if (row >= 0 && battle.roleListScroll + row + 1 <= battle.roleListEntries)
                {
                    return pickRow(battle.roleListRow, row);
                }
            }

            if (!leaveList())
            {
                return 0;
            }

            return scrollParts(kSpellScrollLeft, kSpellScrollRight, kItemScrollDownTop, kThrowScrollDownBottom);
        }

        case kWaitState1E:
            return this->HoverRefineCell();

        default:
            return 0;
    }
}

// The 煉符 grid of MouseHover (inline in the original): the column whose right edge is the first past the pointer
// (from kRefineHitLeft), then the row; a column without a row under the pointer passes to the next. The cell is
// 5 * column + row + 1, at most 10.
int Game::HoverRefineCell()
{
    BattleState& battle = this->battleState;
    const int x = this->state.pointerX;
    const int y = this->state.pointerY;
    int column = 0;

    for (int right = kRefineHitFirst;; right += kRefineColumnWidth, ++column)
    {
        if (right >= kRefineHitEnd)
        {
            battle.unk5772B4 = 0;
            battle.anyStateRole = kNoPanelRole;
            return 0;
        }

        if (static_cast<u32>(x) <= static_cast<u32>(kRefineHitLeft) || static_cast<u32>(x) >= static_cast<u32>(right))
        {
            continue;
        }

        int row = 0;
        int bottom = kRefineRowFirst;

        while (static_cast<u32>(y) <= static_cast<u32>(bottom - kListRowHeight) ||
               static_cast<u32>(y) >= static_cast<u32>(bottom))
        {
            bottom += kListRowHeight;
            ++row;

            if (bottom >= kRefineRowEnd)
            {
                row = -1;
                break;
            }
        }

        if (row < 0)
        {
            continue;
        }

        const int cell = kRefineRows * column + row + 1;

        if (battle.roleListRow != cell)
        {
            this->audio.PlaySound(kBackSound, this->config.soundVolume);
        }

        battle.roleListRow = std::min(cell, kMenuStepMost);
        battle.unk5772B8 = 1;
        battle.unk5772B4 = 1;
        return 1;
    }
}

// The aim of MouseHover in battle state 3 (inline in the original; none with a whole side, unk577A50 or unk577212).
// With an order on the party: the party members from the last whose idle picture is under the pointer (sampled every
// pixel of an 8 x 8 square; the current role only for menu commands 2 and 3, its other samples still taken), or
// whose portrait is (PortraitX by place). Otherwise the standing enemies from the last whose picture is under it
// (every second pixel; 煉妖 and 誘餌 clear turnPointer over an enemy that is not a small machine). Each sample off
// the picture clears unk5772B4; the unit aimed at is marked.
int Game::HoverTarget()
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const int x = this->state.pointerX;
    const int y = this->state.pointerY;

    if (battle.unk577284 == 1 || battle.unk577A50 == 1 || battle.unk577212 != 0)
    {
        return 0;
    }

    // Samples the picture of `unit` under the pointer by `step` over kHitSquare (inline in the original): `onHit` for
    // each sample on it (true stops: the unit is taken), unk5772B4 cleared for each one off it. A missing picture is
    // read through a null pointer in the original: every sample is off it here.
    auto sample =
        [&battle, x, y](BattleUnit& unit, const UnitRect& rect, const Picture* picture, int step, auto&& onHit)
    {
        for (int down = 0; down < kHitSquare; down += step)
        {
            for (int across = 0; across < kHitSquare; across += step)
            {
                bool hit = false;

                if (picture != nullptr && !picture->data.empty())
                {
                    const int pointX = unit.Mirrored() == 1 ? picture->width + 2 * rect.x - x - across : across + x;
                    hit = PictureHitTest(*picture, pointX, down + y, rect.x, rect.y) != 0;
                }

                if (!hit)
                {
                    battle.unk5772B4 = 0;
                }
                else if (onHit())
                {
                    return true;
                }
            }
        }

        return false;
    };

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);

    if (battle.pendingOrders[unit].unk00 == 0)
    {
        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            this->battleEnemies[index].SetAC0(0);
        }

        for (i32 index = static_cast<i32>(battle.enemyCount) - 1; index >= 0; --index)
        {
            BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(index)];

            if (enemy.IsDefeated())
            {
                continue;
            }

            UnitRect rect;
            enemy.GetShadowRect(services, rect);
            const Picture* picture = enemy.StepIdleAction(services);

            // A missing picture is read through a null pointer in the original (deviations.md).
            if (picture == nullptr || picture->data.empty())
            {
                continue;
            }

            battle.turnPointer = 1;

            auto aim = [&battle, &enemy, index]
            {
                battle.aimTarget = index + 1;
                battle.hoverUnit = index;
                enemy.SetAC0(1);

                if ((battle.menuCommand == kRefineMenuCommand || battle.menuCommand == kWaitState50) &&
                    !enemy.IsSmallMachine())
                {
                    battle.turnPointer = 0;
                }

                battle.unk5772B4 = 1;
                battle.unk5772B8 = 1;
                return true;
            };

            if (sample(enemy, rect, picture, kEnemyHitStep, aim))
            {
                return 1;
            }
        }

        return 0;
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        this->battleRoles[index].SetAC0(0);
    }

    // An enemy aimed at before (hoverUnit up to 19) writes past the role table in the original (deviations.md).
    if (static_cast<u32>(battle.hoverUnit) < this->battleRoles.size())
    {
        this->battleRoles[static_cast<std::size_t>(battle.hoverUnit)].SetAC0(0);
    }

    auto& portraits = this->battleHud.portraitHighlight;

    for (i32 role = static_cast<i32>(battle.roleCount) - 1; role >= 0; --role)
    {
        BattleRole& checked = this->battleRoles[static_cast<std::size_t>(role)];

        if (checked.PartyMember() == 0)
        {
            continue;
        }

        auto aim = [&battle, &checked, &portraits, role]
        {
            if (battle.currentUnit - BattleState::FirstRoleUnit == role && battle.menuCommand != kSpellCommand &&
                battle.menuCommand != kItemCommand)
            {
                return false;
            }

            battle.aimTarget = role + 1;
            battle.hoverUnit = role;
            checked.SetAC0(1);
            battle.unk5772B4 = 1;
            battle.unk5772B8 = 1;
            std::fill_n(portraits.begin(), Party::Roles, u8{0});
            return true;
        };

        UnitRect rect;
        checked.GetShadowRect(services, rect);
        const Picture* picture = checked.StepIdleAction(services);

        if (sample(checked, rect, picture, kRoleHitStep, aim))
        {
            return 1;
        }
    }

    // An enemy or a guest aimed at clears a byte past the four portraits (hitFading) in the original (deviations.md).
    if (static_cast<u32>(battle.hoverUnit) < portraits.size())
    {
        portraits[static_cast<std::size_t>(battle.hoverUnit)] = 0;
    }

    // The pointer is compared unsigned (jb/ja in the original).
    if (static_cast<u32>(y) < static_cast<u32>(kPortraitRowTop) ||
        static_cast<u32>(y) > static_cast<u32>(Display::Height))
    {
        battle.unk5772B4 = 0;
        return 0;
    }

    for (u32 place = 0; place < battle.roleCount; ++place)
    {
        // Only the party roles set up at the battle start (the first places, at most four) are party members.
        if (this->battleRoles[place].PartyMember() == 0)
        {
            continue;
        }

        const int left = BattleState::PortraitX[place];

        if (static_cast<u32>(x) < static_cast<u32>(left - kPortraitHitMargin) ||
            static_cast<u32>(x) > static_cast<u32>(left + kPortraitHitRight))
        {
            continue;
        }

        battle.aimTarget = static_cast<i32>(place) + 1;
        battle.hoverUnit = battle.partyRoles[place];
        portraits[static_cast<std::size_t>(battle.hoverUnit)] = 1;
        battle.unk5772B4 = 1;
        battle.unk5772B8 = 1;
        return 1;
    }

    battle.unk5772B4 = 0;
    return 0;
}

// The aim of MouseHover in battle state 9 (swd3e, inline in the original): the ghost's target, as HoverTarget over
// the standing enemies, kept as aiTarget too.
int Game::HoverGhostTarget()
{
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    const int x = this->state.pointerX;
    const int y = this->state.pointerY;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].SetAC0(0);
    }

    for (i32 index = static_cast<i32>(battle.enemyCount) - 1; index >= 0; --index)
    {
        BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(index)];

        if (enemy.IsDefeated())
        {
            continue;
        }

        UnitRect rect;
        enemy.GetShadowRect(services, rect);
        const Picture* picture = enemy.StepIdleAction(services);

        // A missing picture is read through a null pointer in the original (deviations.md).
        if (picture == nullptr || picture->data.empty())
        {
            continue;
        }

        for (int down = 0; down < kHitSquare; down += kEnemyHitStep)
        {
            for (int across = 0; across < kHitSquare; across += kEnemyHitStep)
            {
                const int pointX = enemy.Mirrored() == 1 ? picture->width + 2 * rect.x - x - across : across + x;

                if (PictureHitTest(*picture, pointX, down + y, rect.x, rect.y) == 0)
                {
                    battle.unk5772B4 = 0;
                    continue;
                }

                battle.aimTarget = index + 1;
                battle.aiTarget = index + 1;
                battle.hoverUnit = index;
                enemy.SetAC0(1);
                battle.unk5772B4 = 1;
                battle.unk5772B8 = 1;
                return 1;
            }
        }
    }

    return 0;
}

// swd3e.exe 0x0046F920: the confirm key of the battle menu (while unk577258), by battle state: 1 takes the menu
// command or an open choice; 2, 4, 8, 0x1B, 0x1E, 0x29 (融合) and 0x50 (誘餌) take a list entry; 3 takes the aimed
// unit and ends the role's choice; 5 places an escort, 0xA fuses with one; 7 answers the talk; 9 locks the ghost on
// the aimed enemy; 0x62..0x71 step the end of the battle (the messages wait out endFrames frames); 0xC8 closes the
// menu.
void Game::MenuConfirm()
{
    BattleState& battle = this->battleState;

    if (battle.unk577258 != 1)
    {
        return;
    }

    battle.unk5772AC = 0;

    switch (battle.result)
    {
        case kMenuState:
            this->ConfirmCommand();
            return;

        case kListState2:
            this->ConfirmSpell();
            return;

        case kTargetState:
            this->ConfirmTarget();
            return;

        case kListState4:
            this->ConfirmItem();
            return;

        case kWaitState5:
            this->ConfirmEscort();
            return;

        case kTalkState:
            this->ConfirmTalk();
            return;

        case kSkillListState:
            this->ConfirmSpecial();
            return;

        case kListState1B:
            this->ConfirmThrow();
            return;

        case kWaitState1E:
            this->ConfirmRefine();
            return;

        case kTurnState9:
            this->ConfirmGhostTarget();
            return;

        case kWaitStateA:
            this->ConfirmFusePlace();
            return;

        case kWaitState29:
            this->ConfirmFuse();
            return;

        case kWaitState50:
            this->ConfirmBait();
            return;

        default:
            this->ConfirmEnd();
            return;
    }
}

// The order of the current role as its command sets it (inline in MenuConfirm, 0x0046F920): the speed of its act, the
// act's flag 1 (unk5772F0 and the order's unk08); 1 when the act's flag 0 is set (the command aims at the party).
bool Game::PrepareOrder()
{
    BattleState& battle = this->battleState;
    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    BattleRole& role = this->battleRoles[unit];
    PendingOrder& order = battle.pendingOrders[unit];
    order.unk0C = static_cast<u16>(role.ActSpeed());
    order.unk0E = 0;
    battle.unk5772F0 = 0;

    if (role.ActFlag1() == 1)
    {
        battle.unk5772F0 = 1;
        order.unk08 = 1;
    }

    battle.unk577284 = 0;
    return role.ActFlag0() == 1;
}

// The aim at the party of a chosen command (inline in states 2 and 4 of MenuConfirm, 0x0046F920): fewer than
// four party roles (an unsigned count) aim at the role itself (marked, unless the pointer aims), otherwise the next
// role in RoleCycle.
void Game::AimAtParty()
{
    BattleState& battle = this->battleState;
    const u32 partyRoles = battle.roleCount - battle.escortCount - battle.guestCount;

    // An unsigned compare (0x00470461, 0x00470D0E).
    if (partyRoles >= static_cast<u32>(Party::Roles))
    {
        this->NextRoleTurn();
        return;
    }

    battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);

    if (battle.unk5772B8 == 0)
    {
        BattleRole& role = this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)];
        role.SetAC0(1);
        UnitRect rect;
        role.GetShadowRect(this->battleServices, rect);
        battle.unk5772B4 = 1;
    }
}

// Battle state 3 opened from a list (inline in MenuConfirm, 0x0046F920): the panels closed and the confirm held.
void Game::OpenTargets()
{
    BattleState& battle = this->battleState;
    battle.result = kTargetState;
    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    battle.listRows = kOpenListRows;
    this->controls.PressConfirm();
}

// The end of a role's choice (inline in MenuConfirm, 0x0046F920): the choosing role becomes turnUnit and the menu
// closes.
void Game::EndChoice()
{
    BattleState& battle = this->battleState;
    battle.turnUnit = battle.currentUnit;
    battle.commandNames = {};
    battle.currentUnit = 0;
    battle.unk57725C = 1;
    battle.result = 0;
    battle.unk5772B4 = 0;
    battle.listedCommands = 0;
}

// Battle state 1 of MenuConfirm (once the menu has opened, menuGrow at least 6). An open choice takes its row: an
// order of the ghost (鎖定目標: state 9 aims it), a 煉妖 choice (收化 aims at an enemy, keeping the choices shown;
// 誘餌 opens state 0x50), an element of 改變 (the role acts at once). Otherwise a disabled command refuses (the role
// that took 蓄勁 may still take its second one); the command (a listed one its kind; for the role that took 蓄勁 the
// first goes on gathering while its treasure's experience allows, the second attacks) opens its list or choice, aims,
// or acts at once. The panels close unless a choice opened.
void Game::ConfirmCommand()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0 || battle.menuGrow < kMenuOpened)
    {
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    BattleRole& role = this->battleRoles[unit];

    if (battle.ghostOrdersOpen == 1)
    {
        battle.ghostLockTarget = 0;

        if (battle.ghostOrderRow == kLockOrder)
        {
            battle.result = kTurnState9;
        }
        else
        {
            this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)].SetGhostOrder(
                static_cast<u16>(battle.ghostOrderRow)
            );
        }

        battle.ghostOrdersOpen = 0;
        battle.menuCommand = kGhostMenuEntry;
        return;
    }

    if (battle.refineOptionsOpen == 1)
    {
        if (battle.refineOptionRow == kCaptureOption)
        {
            battle.result = kTargetState;
            battle.refineOptionsOpen = kRefineOptionTaken;
        }
        else if (battle.refineOptionRow == kBaitOption)
        {
            battle.result = kWaitState50;
            battle.refineOptionsOpen = kRefineOptionTaken;
            return;
        }
    }

    if (battle.elementsOpen == 1)
    {
        role.SetElement(this->ProficiencyList(), static_cast<i8>(battle.elementRow));
        battle.turnCommand = kChangeCommand;
        battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = kChangeCommand;
        role.SetTarget(1);
        battle.unk57725C = 1;
        battle.turnUnit = battle.currentUnit;
        battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
        battle.currentUnit = 0;
        battle.commandNames = {};
        battle.pendingOrders[unit].unk00 = 1;
        battle.result = 0;
        battle.elementsOpen = 0;
        battle.unk5772B4 = 0;
        battle.listedCommands = 0;
        return;
    }

    i32 command = battle.menuCommand;
    const bool charging = battle.chargingRole == battle.currentUnit;

    // Commands past the table read the bytes beside it in the original (deviations.md): taken as disabled.
    const auto at = static_cast<std::size_t>(command - 1);
    const bool enabled = command >= 1 && at < battle.commandEnabled.size() && battle.commandEnabled[at] != 0;

    if (!enabled && (!charging || command == kAttackCommand))
    {
        this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
        return;
    }

    battle.unk5772B4 = 0;
    battle.unk577284 = 0;
    battle.unk5772F0 = 0;
    battle.unk5777F0 = 1;
    battle.listReturnCommand = command;

    if (command >= kFirstAccessoryMenuCommand && battle.MenuKind(command) != 0)
    {
        command = battle.MenuKind(command);
        battle.menuCommand = command;
    }

    if (charging)
    {
        if (command == kAttackCommand)
        {
            if (!role.Unk2EB2BelowProficiency(this->ProficiencyList()))
            {
                this->battleMessages.Add(kNoticeX, kNoticeY, kNoticeFrames, kGatherRefused, kNoticeFlags);
                this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
                return;
            }

            command = kGatherCommand;
            battle.menuCommand = command;
        }
        else if (command == kSpellCommand)
        {
            command = kAttackCommand;
            battle.menuCommand = command;
        }
    }

    auto aimedEnemy = [&battle]
    {
        return battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)];
    };

    auto openList = [this, &battle, &aimedEnemy](i32 state)
    {
        battle.result = state;
        battle.unk5772B4 = 1;
        this->display.WarpPointer(kListPointerX, kListPointerY);
        battle.actingEnemy = aimedEnemy();
        this->input.SetMousePosition(kListMouseX, kListMouseY);
    };

    auto actAtOnce = [this, &battle, &role, unit](i32 order)
    {
        battle.turnCommand = order;
        battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = order;
        role.SetTarget(1);
        battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
        battle.pendingOrders[unit].unk00 = 1;
        this->EndChoice();
    };

    switch (command)
    {
        case kAttackCommand:
            battle.result = kTargetState;
            battle.actingEnemy = aimedEnemy();
            break;

        case kSpellCommand:
            battle.listPanelFrames = 0;
            battle.menuGrow = 0;
            battle.listRows = kOpenListRows;
            battle.listScroll = 0;
            openList(kListState2);
            break;

        case kItemCommand:
            battle.roleListRow = 1;
            battle.roleListScroll = 0;
            openList(kListState4);
            break;

        case kSkillCommand:
            battle.specialRow = 1;
            openList(kSkillListState);
            break;

        case kGuardCommand:
            battle.turnCommand = command;
            battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = command;
            role.ResetAfterGuard();
            role.SetTarget(1);
            battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
            battle.pendingOrders[unit].unk00 = 1;
            this->EndChoice();
            break;

        case kRefineMenuCommand:
            battle.refineOptionsOpen = 1;
            battle.refineOptionRow = 1;
            battle.menuSlide = kRefineOptionsSlide;
            break;

        case kGhostMenuEntry:
            battle.ghostOrdersOpen = 1;
            battle.ghostOrderRow = this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)].GhostOrder();
            battle.menuSlide = kGhostOrdersSlide;
            break;

        case kLeaveCommand:
            battle.turnUnit = battle.currentUnit;
            battle.currentUnit = 0;
            battle.result = kLeaveState;
            break;

        case kAimedCommand15:
        case kAimedCommand17:
        case kAimedCommand1D:
        case kAimedCommand1F:
        case kAimedCommand21:
        case kAimedCommand25:
            battle.result = kTargetState;
            break;

        case kSelfCommand:
        case kPartyAimCommand:
            battle.result = kTargetState;
            battle.unk5772B4 = 1;
            battle.unk577284 = 1;
            break;

        case kControlCommand:
            battle.unk576F9C |= kControlMark;
            battle.result = kTargetState;
            break;

        case kStealCommand:
            battle.result = kTargetState;
            battle.unk577080 = 0;
            break;

        case kListState1B:
            battle.result = kListState1B;
            battle.roleListRow = 1;
            battle.roleListScroll = 0;
            this->input.SetMousePosition(kListMouseX, kListMouseY);
            break;

        case kOnceCommand1C:
        case kOnceCommand20:
        case kOnceCommand22:
        case kOnceCommand23:
        case kOnceCommand24:
            actAtOnce(command);
            break;

        case kWaitState1E:
            battle.result = kWaitState1E;
            battle.roleListRow = 1;
            this->input.SetMousePosition(kListMouseX, kListMouseY);
            break;

        case kChangeCommand:
            battle.elementsOpen = 1;
            battle.menuCommand = battle.listReturnCommand;
            battle.elementRow = 1;
            battle.menuSlide = kElementsSlide;
            break;

        case kGatherCommand:
            battle.chargingRole = battle.currentUnit;
            actAtOnce(command);
            break;

        case kStrikeBackCommand:
            battle.turnCommand = command;
            battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = command;

            if (this->battleHud.meterShown != 0)
            {
                battle.result = kTargetState;
            }
            else
            {
                role.SetTarget(1);
                battle.meterRole = battle.currentUnit;
                battle.turnUnit = battle.currentUnit;
                battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
                battle.currentUnit = 0;
                battle.unk57725C = 1;
                battle.pendingOrders[unit].unk00 = 1;
                battle.result = 0;
                battle.unk5772B4 = 0;
            }

            battle.listedCommands = 0;
            battle.commandNames = {};
            break;

        case kFuseCommand:
            battle.result = kWaitState29;
            battle.roleListRow = 1;
            battle.menuSlide = kListSlideStart;
            this->input.SetMousePosition(kListMouseX, kListMouseY);
            break;

        default:
            break;
    }

    if (battle.unk5772B8 == 0 && battle.result == kTargetState)
    {
        this->NextEnemyTurn();
    }

    this->controls.PressConfirm();

    if (battle.menuCommand != kGhostMenuEntry && battle.menuCommand != kRefineMenuCommand && battle.elementsOpen != 1)
    {
        battle.listRows = kOpenListRows;
        battle.listPanelFrames = 0;
        battle.menuGrow = 0;
    }
}

// Battle state 2 of MenuConfirm: a page tab under the pointer opens its page; otherwise the chosen spell (affordable)
// becomes the command and its target is chosen: the party (a spell of the act's flag 0 aims at the whole party),
// or the enemies.
void Game::ConfirmSpell()
{
    BattleState& battle = this->battleState;

    if (battle.hoverPageTab != -1)
    {
        battle.listPage = battle.hoverPageTab;
        battle.hoverPageTab = -1;
        battle.pageListRow = 1;
        battle.listScroll = 0;
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
        return;
    }

    if (battle.unk5772B4 == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.unk577078 != 0 || battle.hoverScrollPart != 0 || battle.specialCursor == 0 || battle.currentUnit == 0)
    {
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    BattleRole& role = this->battleRoles[unit];

    if (role.CanPaySkill(this->battleServices, battle.listPage, battle.specialCursor) == 0)
    {
        return;
    }

    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const u16 kind = role.SelectSkill(this->database, battle.listPage, 0, battle.specialCursor, act);
    battle.unk50D88C[unit] = static_cast<i32>(act);
    PendingOrder& order = battle.pendingOrders[unit];

    const bool aimsEnemies = this->PrepareOrder();
    battle.unk57735C = role.ActFlag17() == 1 ? 1 : 0;

    if (aimsEnemies)
    {
        battle.unk577284 = 1;
        battle.unk5772B4 = 1;
        battle.aimTarget = static_cast<i32>(battle.enemyCount);

        if (battle.listPage == kHealPage || kind == 0)
        {
            battle.unk5772D0 = 1;
            order.unk00 = 1;
        }

        this->OpenTargets();
        return;
    }

    if (battle.listPage != 0)
    {
        order.unk00 = kind == 0 ? 1 : 0;

        if (battle.unk57735C == 1)
        {
            order.unk00 = 0;
        }

        if (order.unk00 == 1)
        {
            this->AimAtParty();
            this->OpenTargets();
            return;
        }
    }

    if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    this->OpenTargets();
}

// Battle state 3 of MenuConfirm: the aimed unit taken (kept as aiTarget; refused with 煉妖 and state 0x50 while
// turnPointer is off): an ActFlag17 skill turns on the role itself, its enemy kept in unk34C0; the order is the menu
// command, the role's choice ends (蓄勁 and, once the meter is gone, 逆攻 no longer the role's), the party's hits on
// the aimed enemy (on each standing enemy for a whole side) counted, every unit unmarked and the lists reset, the
// command back on 1. Command 0x1E throws the chosen object (state 0xD when it lands, a talisman paper back otherwise);
// a skill takes its stamina; 奇術 fills the second gauge; the auto battle remembers the aim.
void Game::ConfirmTarget()
{
    BattleState& battle = this->battleState;

    if (battle.aimTarget == -1 || (battle.currentUnit == 0 && battle.autoBattle == 0))
    {
        return;
    }

    // In the auto battle without a current unit the original works on the role 20 before the first and its order
    // before the orders: those calls and accesses are left out, the rest done.
    auto roleOf = [this](i32 unitNumber) -> BattleRole*
    {
        return unitNumber >= BattleState::FirstRoleUnit
                   ? &this->battleRoles[static_cast<std::size_t>(unitNumber - BattleState::FirstRoleUnit)]
                   : nullptr;
    };

    auto orderOf = [&battle](i32 unitNumber) -> PendingOrder*
    {
        return unitNumber >= BattleState::FirstRoleUnit
                   ? &battle.pendingOrders[static_cast<std::size_t>(unitNumber - BattleState::FirstRoleUnit)]
                   : nullptr;
    };

    battle.aiTarget = battle.aimTarget;
    PendingOrder* order = orderOf(battle.currentUnit);

    if (order != nullptr && order->unk00 == 1)
    {
        battle.orderStep = 1;
    }

    if ((battle.menuCommand == kRefineMenuCommand || battle.menuCommand == kWaitState50) && battle.turnPointer == 0)
    {
        this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
        return;
    }

    if (battle.unk5772B4 == 0)
    {
        battle.unk5772B4 = battle.unk5772B8 == 0 ? 1 : 0;
        return;
    }

    if (battle.unk57735C == 1)
    {
        if (order != nullptr)
        {
            order->unk00 = 1;
        }

        if (BattleRole* aiming = roleOf(battle.currentUnit); aiming != nullptr)
        {
            aiming->SetUnk34C0(static_cast<i16>(battle.aimTarget - 1));
        }

        battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
    }

    battle.unk57735C = 0;
    const i32 chooser = battle.currentUnit;
    battle.turnCommand = battle.menuCommand;
    battle.turnTaken[static_cast<std::size_t>(chooser)] = battle.menuCommand;
    battle.turnUnit = chooser;
    battle.currentUnit = 0;

    if (battle.chargingRole == chooser)
    {
        battle.chargingRole = -1;
    }

    if (battle.meterRole == chooser && this->battleHud.meterShown == 0)
    {
        battle.meterRole = -1;
    }

    battle.unk57725C = 1;
    BattleRole* role = roleOf(battle.turnUnit);
    order = orderOf(battle.turnUnit);

    if (role != nullptr)
    {
        role->SetTarget(1);
    }

    const i32 wholeSide = battle.unk577284;
    battle.result = 0;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 0;

    if (wholeSide == 1)
    {
        if (order != nullptr)
        {
            order->unk04 = 1;
            order->targetCount = 0;
        }

        battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = battle.turnCommand;
    }

    if (order != nullptr && order->unk00 == 0)
    {
        if (wholeSide == 1)
        {
            for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
            {
                if (!this->battleEnemies[enemy].IsDefeated())
                {
                    ++battle.enemyHits[enemy];
                }
            }
        }
        else
        {
            ++battle.enemyHits[static_cast<std::size_t>(battle.aimTarget - 1)];
        }
    }

    if (battle.menuCommand == kWaitState1E)
    {
        battle.turnCommand = kItemCommand;
        battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = kItemCommand;

        if (role != nullptr)
        {
            role->UseSelectedItem(battle.listRole);
        }

        if (role != nullptr && role->Word2A86Bit13() == 1)
        {
            battle.turnCommand = kThrowOrder;
            battle.unk577320 = 1;
            battle.turnTaken[static_cast<std::size_t>(battle.turnUnit)] = kThrowOrder;
            ++battle.unk577994;
        }
        else
        {
            AddOneItem(this->party.inventory, kThrownObject, 0, this->database);
        }
    }
    else if (battle.turnCommand == kSkillCommand && role != nullptr)
    {
        role->TakeCommand2F18();
    }

    if (battle.menuCommand == kListState1B || battle.menuCommand == kWaitState50)
    {
        battle.roleListRow = 1;
        battle.roleListScroll = 0;
    }

    battle.refineOptionsOpen = 0;

    for (u32 index = 0; index < battle.enemyCount; ++index)
    {
        this->battleEnemies[index].SetAC0(0);
    }

    for (std::size_t panel = 0; panel < static_cast<std::size_t>(Party::Roles); ++panel)
    {
        this->battleRoles[panel].SetAC0(0);
        this->battleHud.portraitHighlight[panel] = 0;
    }

    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    this->battleHud.scrollCount = 0;
    battle.roleListRow = 1;

    if (battle.menuCommand == kSpellCommand)
    {
        battle.unk5772BC = 0;
        battle.unk5772C0 = 0;

        if (role != nullptr)
        {
            role->FillSecondGauge();
        }
    }

    battle.unk5772C4 = 0;
    this->controls.PressConfirm();

    if (battle.autoBattle == 1)
    {
        battle.unk5772BC = 0;
        battle.unk5772C0 = 0;
        battle.autoTarget = battle.aimTarget;
    }

    battle.menuCommand = 1;
    battle.commandNames = {};
    battle.listedCommands = 0;
}

// Battle state 4 of MenuConfirm: a role tab under the pointer opens its list; otherwise the chosen object becomes the
// command: an escort object calls its escort (refused while one is being called; with escorts out it opens the 佈置護駕
// panel; else the escort comes at once), others choose their target as ConfirmSpell; a refused object shows
// 體力點數不夠!! (categories 0, 2, 3) with the refusal sound.
void Game::ConfirmItem()
{
    BattleState& battle = this->battleState;

    if (battle.hoverRoleTab != -1)
    {
        battle.listRole = battle.hoverRoleTab;
        battle.hoverRoleTab = -1;
        battle.roleListRow = 1;
        battle.roleListScroll = 0;
        this->audio.PlaySound(kBackSound, this->config.soundVolume);
        return;
    }

    if (battle.unk5772B4 == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.unk577078 != 0 || battle.hoverScrollPart != 0 || battle.currentUnit == 0)
    {
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    BattleRole& role = this->battleRoles[unit];
    u16 kind = kNoObjectKind;
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const int chosen =
        role.SelectObject(this->database, this->battleState, battle.listRole, battle.listCursor, act, kind);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (chosen == 0)
    {
        const i32 category = battle.listRole;

        if (category == kItemCategory3 || category == 0 || category == kHealPage)
        {
            if (kind == kNoObjectKind)
            {
                return;
            }

            this->battleMessages
                .Add(kRefusalMessageX, kRefusalMessageY, kRefusalMessageFrames, kStaminaShort, kRefusalMessageFlags);
        }

        this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
        return;
    }

    if (role.Word2A86Bit12() == 1)
    {
        this->CallEscort(role);
        return;
    }

    PendingOrder& order = battle.pendingOrders[unit];

    if (this->PrepareOrder())
    {
        battle.aimTarget = static_cast<i32>(battle.enemyCount);
        battle.unk577284 = 1;
        battle.unk5772B4 = 1;
        order.unk00 = 0;

        if (battle.listRole == kHealPage)
        {
            order.unk00 = 1;
            battle.unk5772D0 = 1;
        }

        if (kind == 0)
        {
            order.unk00 = 1;
        }

        battle.result = kTargetState;
        this->controls.PressConfirm();
        return;
    }

    if (battle.listRole != 0)
    {
        order.unk00 = kind == 0 ? 1 : 0;

        if (order.unk00 == 1)
        {
            this->AimAtParty();
            battle.result = kTargetState;
            this->controls.PressConfirm();
            return;
        }
    }

    if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    battle.result = kTargetState;
    this->controls.PressConfirm();
}

// An escort object of ConfirmItem (inline in the original): with escorts out the 佈置護駕 panel opens (state 5);
// otherwise the escort comes at once: the order 0xF and the role's choice ends, its object used (counted, held back
// as returned item 1) and the escort placed after the roles.
void Game::CallEscort(BattleRole& role)
{
    BattleState& battle = this->battleState;

    if (battle.escortCount != 0)
    {
        battle.result = kWaitState5;
        battle.guardCursor = battle.escortCount == kEscortRows ? 1 : kEscortRows;
        return;
    }

    battle.turnCommand = kEscortOrder;
    battle.turnUnit = battle.currentUnit;
    battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = kEscortOrder;
    battle.currentUnit = 0;
    battle.unk57725C = 1;
    this->battleRoles[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].SetTarget(1);
    battle.result = 0;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 0;
    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    battle.pendingOrders[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].unk00 = 1;
    this->controls.PressConfirm();

    if (battle.autoBattle == 1)
    {
        battle.autoTarget = battle.aimTarget;
    }

    battle.commandNames = {};
    battle.listedCommands = 0;
    battle.callingEscort = role.UseSelectedItem(battle.listRole);
    battle.CountItemUse(battle.callingEscort);
    battle.SetReturnedItem(1, battle.callingEscort);

    // With every place taken the original writes the escort past the places.
    if (battle.roleCount >= battle.rolePlaces.size())
    {
        battle.callingEscort = static_cast<u16>(battle.roleCount);
        battle.escortCalled[static_cast<std::size_t>(battle.turnUnit)] = static_cast<u16>(battle.roleCount);
        return;
    }

    UnitPlacement& place = battle.rolePlaces[battle.roleCount];
    place.x = kEscortX;
    place.y = kFirstEscortY;
    place.actId = battle.callingEscort;
    place.kind = 1;

    if (battle.mirrored == 1)
    {
        place.x = static_cast<i16>(Display::Width - place.x);
    }

    battle.callingEscort = static_cast<u16>(battle.roleCount);
    battle.escortCalled[static_cast<std::size_t>(battle.turnUnit)] = static_cast<u16>(battle.roleCount);
}

// Battle state 5 of MenuConfirm: the escort object used (counted) takes the place of escort row guardCursor (an
// escort there gives its object back and marks UnkPanel4): row 1 the first place, row 2 the second; the order 0xF ends
// the role's choice and the units are measured again.
void Game::ConfirmEscort()
{
    BattleState& battle = this->battleState;

    if (battle.currentUnit == 0)
    {
        return;
    }

    BattleRole& role = this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)];
    battle.callingEscort = role.UseSelectedItem(battle.listRole);
    battle.turnUnit = battle.currentUnit;
    battle.CountItemUse(battle.callingEscort);
    battle.turnCommand = kEscortOrder;
    battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = kEscortOrder;
    battle.currentUnit = 0;
    battle.unk57725C = 1;
    role.SetTarget(1);
    battle.result = 0;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 0;
    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    battle.pendingOrders[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].unk00 = 1;
    this->controls.PressConfirm();
    const auto row =
        static_cast<std::size_t>(battle.guardCursor - battle.escortCount + static_cast<i32>(battle.roleCount) - 1);
    UnitPlacement& place = battle.rolePlaces[row];
    u16 placed = battle.callingEscort;

    auto replace = [this, &battle, &place](i16 x, i16 y)
    {
        if (place.actId != 0)
        {
            AddOneItem(this->party.inventory, place.actId, 0, this->database);
            battle.panelFlags |= BattleState::UnkPanel4;
        }

        place.x = x;
        place.y = y;
        place.actId = battle.callingEscort;
        place.kind = 1;

        if (battle.mirrored == 1)
        {
            place.x = static_cast<i16>(Display::Width - place.x);
        }
    };

    if (battle.guardCursor == 1)
    {
        replace(kEscortX, kFirstEscortY);
        battle.SetReturnedItem(1, battle.callingEscort);
        placed = static_cast<u16>(row);
        battle.callingEscort = static_cast<u16>(row);
    }

    if (battle.guardCursor == kEscortRows)
    {
        replace(kSecondEscortX, kSecondEscortY);
        battle.SetReturnedItem(kEscortRows, battle.callingEscort);
        placed = static_cast<u16>(row);
    }

    battle.commandNames = {};
    battle.callingEscort = 0;
    battle.escortCalled[static_cast<std::size_t>(battle.turnUnit)] = placed;
    battle.listedCommands = 0;
    this->MeasureUnits();
}

// Battle state 7 of MenuConfirm: the last answer (controlRows) ends the role's choice with order 0x63; the others aim
// at the enemies with command 0x19 (state 1 when every enemy is down).
void Game::ConfirmTalk()
{
    BattleState& battle = this->battleState;

    if (battle.controlCursor == battle.controlRows)
    {
        battle.turnCommand = kTalkOrder;
        this->controls.PressConfirm();
        this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)].SetTarget(1);
        battle.commandNames = {};
        battle.unk57725C = 1;
        battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
        battle.turnUnit = battle.currentUnit;
        battle.currentUnit = 0;
        battle.unk5772B4 = 0;
        battle.result = 0;
        battle.listPanelFrames = 0;
        battle.menuGrow = 0;
        battle.listedCommands = 0;
        battle.listRows = kOpenListRows;
        return;
    }

    battle.result = kTargetState;
    battle.menuCommand = kControlCommand;

    if (this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
    {
        int tries = 0;

        while (true)
        {
            if (++battle.orderStep > static_cast<i32>(battle.enemyCount))
            {
                battle.orderStep = 1;
            }

            battle.actingEnemy = battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)];

            if (++tries >= static_cast<int>(battle.enemyCount))
            {
                break;
            }

            if (!this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
            {
                battle.unk5772B4 = 1;
                return;
            }
        }

        battle.result = kMenuState;
    }

    battle.unk5772B4 = 1;
}

// Battle state 8 of MenuConfirm: the chosen special skill becomes the command (aiming at the party with the act's
// flag 0, otherwise at the enemies).
void Game::ConfirmSpecial()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0 || battle.currentUnit == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;
    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const u16 kind = this->battleRoles[unit].SelectSkill(this->database, 0, 1, battle.specialCursor, act);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (kind == kNoObjectKind)
    {
        return;
    }

    if (this->PrepareOrder())
    {
        battle.unk577284 = 1;
        battle.unk5772B4 = 1;
        battle.aimTarget = static_cast<i32>(battle.enemyCount);
    }
    else if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    this->OpenTargets();
}

// Battle state 0x1B of MenuConfirm: the chosen object of the 投擲 list becomes the command, aiming at the enemies.
void Game::ConfirmThrow()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.unk577078 != 0 || battle.hoverScrollPart != 0 || battle.currentUnit == 0)
    {
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    u16 kind = 0;
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const int chosen =
        this->battleRoles[unit]
            .SelectObject(this->database, this->battleState, kThrowCategory, battle.listCursor, act, kind);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (chosen == 0)
    {
        return;
    }

    if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    battle.result = kTargetState;
    this->controls.PressConfirm();
}

// Battle state 0x1E of MenuConfirm: a 煉符 cell past the objects refuses; otherwise the chosen throwable becomes the
// command (as ConfirmSpecial); a refused one sounds the refusal.
void Game::ConfirmRefine()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0 || battle.currentUnit == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.roleListRow > battle.roleListEntries)
    {
        this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    u16 kind = 0;
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const int chosen =
        this->battleRoles[unit]
            .SelectObject(this->database, this->battleState, kRefineCategory, battle.listCursor, act, kind);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (chosen == 0)
    {
        this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
        return;
    }

    if (this->PrepareOrder())
    {
        battle.unk577284 = 1;
        battle.unk5772B4 = 1;
        battle.aimTarget = static_cast<i32>(battle.enemyCount);
        battle.result = kTargetState;
        this->controls.PressConfirm();
        return;
    }

    if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    battle.result = kTargetState;
    this->controls.PressConfirm();
}

// Battle state 9 of MenuConfirm (swd3e): the ghost locks on the aimed enemy (鎖定目標): the order taken, the menu
// back on 符鬼 with the enemy unmarked.
void Game::ConfirmGhostTarget()
{
    BattleState& battle = this->battleState;
    battle.ghostLockTarget = battle.aiTarget;
    battle.result = kMenuState;
    battle.menuCommand = kGhostMenuEntry;
    battle.ghostOrdersOpen = 0;
    this->battleRoles[static_cast<std::size_t>(battle.ghostSlot)].SetGhostOrder(static_cast<u16>(battle.ghostOrderRow));
    this->battleEnemies[static_cast<std::size_t>(battle.aimTarget - 1)].SetAC0(0);
}

// Battle state 0xA of MenuConfirm (swd3e): the escort of row guardCursor takes the chosen 融合 object: refused without
// an escort there, for an escort already fused, or when the role's 融合 proficiency does not reach the object's level;
// otherwise the object is used and the role's choice ends with order 0x29 on the escort.
void Game::ConfirmFusePlace()
{
    BattleState& battle = this->battleState;
    const i32 row = battle.guardCursor - battle.escortCount + static_cast<i32>(battle.roleCount) - 1;

    if (battle.givenBack[static_cast<std::size_t>(battle.guardCursor)] == 0)
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kNoticeFrames, kNoEscortToFuse, kNoticeFlags);
        return;
    }

    BattleRole& escort = this->battleRoles[static_cast<std::size_t>(row)];

    if (escort.Flag26B8Bit17() == 1)
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kNoticeFrames, kFusedAlready, kNoticeFlags);
        return;
    }

    BattleRole& role = this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)];
    const auto proficiency = static_cast<u16>(role.AccessoryProficiency(kFuseCommand, this->ProficiencyList()));

    if (escort.LevelInReach(this->database, role.SelectedItemId(), proficiency) == 0)
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kNoticeFrames, kFuseRefused, kNoticeFlags);
        return;
    }

    battle.usedUpItem = role.UseSelectedItem(kFuseCategory);
    battle.turnUnit = battle.currentUnit;
    battle.turnTaken[static_cast<std::size_t>(battle.currentUnit)] = kFuseCommand;
    battle.turnCommand = kFuseCommand;
    battle.currentUnit = 0;
    battle.aimTarget = row + 1;
    battle.unk57725C = 1;
    role.SetTarget(1);
    battle.result = 0;
    battle.unk5772B8 = 0;
    battle.unk5772B4 = 0;
    battle.listPanelFrames = 0;
    battle.menuGrow = 0;
    battle.pendingOrders[static_cast<std::size_t>(battle.turnUnit - BattleState::FirstRoleUnit)].unk00 = 1;
    this->controls.PressConfirm();
}

// Battle state 0x29 of MenuConfirm (swd3e): the chosen 融合 object (category 3) opens state 0xA on the first escort
// row, aiming at the first standing escort; refused without escorts.
void Game::ConfirmFuse()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.unk577078 != 0 || battle.hoverScrollPart != 0 || battle.currentUnit == 0)
    {
        return;
    }

    if (battle.escortCount == 0)
    {
        this->battleMessages.Add(kNoticeX, kNoticeY, kNoticeFrames, kNoEscortToFuse, kNoticeFlags);
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    u16 kind = 0;
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const int chosen =
        this->battleRoles[unit]
            .SelectObject(this->database, this->battleState, kFuseCategory, battle.listCursor, act, kind);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (chosen == 0)
    {
        return;
    }

    // The original searches past the roles when none is a standing escort (deviations.md): the search ends there.
    i32 escort = 0;

    while (escort < static_cast<i32>(BattleState::RoleUnits) &&
           (this->battleRoles[static_cast<std::size_t>(escort)].IsDefeated() ||
            this->battleRoles[static_cast<std::size_t>(escort)].AutoControl() != 1))
    {
        ++escort;
    }

    battle.aimTarget = escort + 1;
    battle.unk5772D0 = 1;
    battle.menuCommand = kFuseCommand;
    battle.result = kWaitStateA;
    battle.guardCursor = 1;
    battle.pendingOrders[unit].unk00 = 1;
    this->controls.PressConfirm();
}

// Battle state 0x50 of MenuConfirm (swd3e): the chosen 誘餌 object (category 6) aims at the enemies with the menu
// command 0x50.
void Game::ConfirmBait()
{
    BattleState& battle = this->battleState;

    if (battle.unk5772B4 == 0)
    {
        return;
    }

    battle.unk5772B4 = 0;

    if (battle.unk577078 != 0 || battle.hoverScrollPart != 0 || battle.currentUnit == 0 || battle.roleListEntries == 0)
    {
        return;
    }

    const auto unit = static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit);
    u16 kind = 0;
    u32 act = static_cast<u32>(battle.unk50D88C[unit]);
    const int chosen =
        this->battleRoles[unit]
            .SelectObject(this->database, this->battleState, kBaitCategory, battle.listCursor, act, kind);
    battle.unk50D88C[unit] = static_cast<i32>(act);

    if (chosen == 0)
    {
        return;
    }

    if (battle.unk5772B8 == 0)
    {
        this->NextEnemyTurn();
    }

    battle.menuCommand = kWaitState50;
    battle.result = kTargetState;
    this->controls.PressConfirm();
}

// The end states of MenuConfirm (0x62..0x71, 0xC8): the messages of the end wait out endFrames frames (0x64 not for a
// victory without its panel; a defeat, 0x67 once waited and 0x68 at once, stops the music), the level-up steps (0x65:
// a level-up shows 0x6E, a learned skill 0x6F; none ends), 0xC8 closes the menu and unmarks every unit.
void Game::ConfirmEnd()
{
    BattleState& battle = this->battleState;
    BattleMessages& messages = this->battleMessages;

    switch (battle.result)
    {
        case kEndCraft:
            battle.pendingActionRow = 0;
            battle.pendingAction = 0;
            messages.panelHeight = 0;
            battle.endFrames = 0;
            messages.craftSucceeded = 0;
            battle.result = kBattleWon;
            battle.levelUpRole = kNoLevelUp;
            return;

        case kEndVictory:
            if (battle.victoryWithoutPanel != 1 && battle.endFrames >= kEndMessageFrames)
            {
                battle.endFrames = 0;
                messages.panelHeight = 0;
                battle.result = kEndLevelUps;
            }

            return;

        case kEndLevelUps:
            battle.levelUpFrames = 0;

            if (battle.levelUpPending != 0 && battle.endFrames >= kEndLevelUpFrames)
            {
                messages.panelHeight = 0;

                if (battle.levelUpRole == kNoLevelUp)
                {
                    if (messages.lootCount != 0)
                    {
                        this->audio.PlaySound(kLearnedSound, this->config.soundVolume);
                    }

                    battle.endFrames = 0;
                    battle.unk577258 = 0;
                    battle.levelUpPending = 0;
                    battle.result = kEndLoot;
                }
                else
                {
                    this->audio.PlaySound(kLevelUpPanelSound, this->config.soundVolume);
                    battle.levelUpPending = 0;
                    battle.endFrames = 0;
                    battle.result = kEndStats;
                }
            }

            return;

        case kEndLoot:
            if (battle.endFrames >= kEndMessageFrames)
            {
                battle.endFade = 1;
                battle.result = 0;
                battle.endFrames = 0;
                battle.unk577258 = 0;
                messages.panelHeight = 0;
            }

            return;

        case kBattleLost:
            if (battle.endFrames >= kEndMessageFrames)
            {
                this->audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
                battle.endFade = 1;
                messages.panelHeight = 0;
                battle.result = 0;
                battle.endFrames = 0;
            }

            return;

        case kLostQuietly:
            this->audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, 0);
            battle.endFade = 1;
            messages.panelHeight = 0;
            return;

        case kEndStats:
            messages.panelHeight = 0;

            if (battle.learnedPending == 1)
            {
                if (!this->battleRoles[static_cast<std::size_t>(static_cast<i8>(battle.levelUpRole))].IsDefeated())
                {
                    this->battleRoles[static_cast<std::size_t>(static_cast<i8>(battle.levelUpRole))].RestartMainAct(
                        kLearnedPose,
                        1
                    );
                }

                this->audio.PlaySound(kLearnedSound, this->config.soundVolume);
                battle.endFrames = 0;
                battle.result = kEndNewSpell;
            }
            else
            {
                battle.levelUpPending = 0;
                battle.endFrames = 0;
                battle.levelUpRole = kNoLevelUp;
                battle.result = kEndLevelUps;
            }

            return;

        case kEndNewSpell:
            if (battle.endFrames > kEndMessageFrames)
            {
                messages.panelHeight = 0;
                battle.levelUpPending = 0;
                battle.learnedPending = 0;
                battle.endFrames = 0;
                battle.levelUpRole = kNoLevelUp;
                battle.result = kEndLevelUps;
            }

            return;

        case kEndSpecials:
        case kEndMastery:
            if (battle.endFrames > kEndMessageFrames)
            {
                messages.panelHeight = 0;
                battle.learnedPending = 0;
                battle.endFrames = 0;
                battle.levelUpRole = kNoLevelUp;
            }

            return;

        case kCloseMenuState:
        {
            battle.turnCommand = 0;
            battle.menuCommand = 1;
            battle.currentUnit = 0;
            battle.turnUnit = 0;
            battle.result = 0;
            battle.unk5772B8 = 0;
            battle.unk5772B4 = 0;

            for (u32 index = 0; index < battle.enemyCount; ++index)
            {
                this->battleEnemies[index].SetAC0(0);
            }

            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                this->battleRoles[index].SetAC0(0);
            }

            battle.listPanelFrames = 0;
            battle.menuGrow = 0;
            battle.unk5772BC = 0;
            battle.unk5772C0 = 0;
            battle.unk577258 = 0;
            this->controls.PressConfirm();

            if (battle.autoBattle == 1)
            {
                battle.autoTarget = battle.aimTarget;
            }

            battle.commandNames = {};
            battle.listedCommands = 0;
            return;
        }

        default:
            return;
    }
}

// swd3e.exe 0x0046F100: the confirm key of the battle (not while unk577204, unk577328, the fade out or a negative
// unk577088): with one standing enemy left and a talk role, the menu closes (unless won); the level-up stats skip to
// their end (0x1D, then all at once); a talk window moves on; outside the menu MenuConfirm takes it; a role taking
// the panel (not held) opens the menu (kOpenSound): the command menu, or the talk menu of the talk role with its
// answers (攻擊, the enemy's objects and skills, 釋放).
void Game::ConfirmKey()
{
    BattleState& battle = this->battleState;

    if (battle.unk577204 != 0 || battle.unk577328 == 1 || battle.endFade == 1 || battle.unk577088 < 0)
    {
        return;
    }

    if (battle.unk577A50 == 0 && static_cast<i32>(battle.enemyCount) - battle.enemiesGone <= 1 &&
        battle.unk4BF1CA != kNoTalkRole && battle.result != kBattleWon)
    {
        battle.result = 0;
        return;
    }

    if (battle.result == kEndStats)
    {
        if (battle.levelUpFrames < kRollStart)
        {
            battle.levelUpFrames = kRollStart - 1;
            return;
        }

        if (battle.levelUpFrames == kRollStart)
        {
            battle.levelUpFrames = kShowNow;
            return;
        }
    }

    // swd3e: with click regions shown the talk moves on only for a click on one of them.
    if (this->scene.talkWindows != nullptr)
    {
        if (this->state.clickRegions.regions.empty())
        {
            this->talk.advance = 1;
        }
        else
        {
            this->state.clickRegions.Find(this->state.pointerX, this->state.pointerY, battle.hoverRegion);
            this->talk.advance = battle.hoverRegion != nullptr ? 1 : 0;
        }

        return;
    }

    if (battle.unk577258 != 1 || battle.currentUnit == 0 || (battle.result != 0 && battle.anyStateRole == kNoPanelRole))
    {
        this->MenuConfirm();
        return;
    }

    battle.anyStateRole = kNoPanelRole;
    BattleRole& role = this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)];

    if (role.IsHeld() == 1)
    {
        return;
    }

    this->audio.PlaySound(kOpenSound, this->config.soundVolume);
    battle.unk5772B4 = 1;
    battle.unk5777F0 = 1;
    battle.listRows = kListRows;
    role.GetPosition(battle.unk577240, battle.unk577244);

    if (battle.controlledEnemy == kNoTalkRole || battle.talkRole != battle.currentUnit - BattleState::FirstRoleUnit)
    {
        battle.result = kMenuState;
        battle.menuCommand = 1;
        this->BuildCommandMenu();
        return;
    }

    battle.result = kTalkState;
    battle.controlRows = kTalkFixedAnswers;
    BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(static_cast<i16>(battle.controlledEnemy))];
    std::string name;

    for (int choice = 0; choice < kControlObjects; ++choice)
    {
        u32 id = static_cast<u32>(battle.orderedSkill);

        if (enemy.ItemName(this->database, choice, name, id) == 1)
        {
            ++battle.controlRows;
        }

        battle.orderedSkill = static_cast<i32>(id);
    }

    for (int choice = 0; choice < kControlSkills; ++choice)
    {
        if (enemy.SkillName(this->database, choice, name) == 1)
        {
            ++battle.controlRows;
        }
    }

    battle.orderedSkill = 0;
}

// A key that repeats (inline in the original): the first frame held, then every third frame from the fifteenth.
bool Game::KeyRepeats(const ButtonState& button)
{
    return button.heldFrames == 1 || (button.heldFrames >= kRepeatDelay && button.heldFrames % kRepeatStep == 1);
}

// swd3e.exe 0x0046B3D0: the keys of the battle. In the menu (battle state 0 or 1 with a current unit, no talk window,
// no open choice, not the role that took 蓄勁) the digit keys 1..9 and the keypad (7 8 9 / 4 5 6 / 1 2 3, as the grid)
// take that command when enabled (5 guards at once), and G retreats (無法撤退!! when the role is marked or the party
// cannot; not while aiming). Then: right Shift moves the panel to the previous role, A starts the auto battle (once
// the start pose is over); the right mouse button and right Ctrl press Escape; the left button takes the role under
// the pointer or the scroll part under it and presses Space; Enter presses Space; Space confirms (every sixth frame
// when held). The arrow keys move the menu, the panel and the talk choice, Page Up and Page Down scroll the lists,
// Escape cancels.
void Game::BattleKeys()
{
    BattleState& battle = this->battleState;
    Controls& controls = this->controls;
    battle.unk577078 = 0;

    i32 unit = battle.currentUnit;

    if (battle.result <= kMenuState && battle.currentUnit != 0 && this->scene.talkWindows == nullptr &&
        battle.ghostOrdersOpen == 0 && battle.refineOptionsOpen == 0 && battle.elementsOpen == 0 &&
        battle.chargingRole != battle.currentUnit)
    {
        for (std::size_t at = 0; at < kCommandKeys.size(); ++at)
        {
            const auto command = static_cast<i32>(at) + 1;

            if (this->input.IsKeyDown(kDigitKey1 + static_cast<int>(at)) == 0 &&
                this->input.IsKeyDown(kCommandKeys[at]) == 0)
            {
                continue;
            }

            if (command == kGuardCommand)
            {
                if (battle.menuCommand != kGuardCommand)
                {
                    if (battle.result == 0)
                    {
                        battle.result = kMenuState;
                    }

                    battle.menuGrow = kMenuOpened;
                    battle.unk5772B4 = 1;
                    battle.unk5772B8 = 0;
                    battle.menuCommand = kGuardCommand;
                    this->ConfirmKey();
                }

                battle.result = 0;
                battle.unk5772B4 = 0;
                battle.menuCommand = 1;
                return;
            }

            if (battle.result == 0)
            {
                battle.result = kMenuState;
            }

            this->BuildCommandMenu();

            if (battle.commandEnabled[at] == 1)
            {
                battle.menuGrow = kMenuOpened;
                battle.unk5772B4 = 1;
                battle.unk5772B8 = 0;
                battle.menuCommand = command;
                this->ConfirmKey();
            }

            return;
        }

        if (controls.At(Button::KeyG).press != 0 && Game::KeyRepeats(controls.At(Button::KeyG)))
        {
            if (battle.unk577204 != 0 || battle.hitsAllRoles == 1 || battle.unk577260 == 1 ||
                battle.result == kTalkOrder || battle.result == kEndVictory || battle.result == kTargetState ||
                this->scene.talkWindows != nullptr)
            {
                return;
            }

            BattleRole& role =
                this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)];

            if (role.IsHeld() == 1 ||
                (static_cast<i32>(battle.enemyCount) - battle.enemiesGone == 1 && battle.unk4BF1CA != kNoTalkRole))
            {
                return;
            }

            if (!role.MarkBit11Clear() || (battle.panelFlags & BattleState::NoRetreat) != 0)
            {
                SleepMilliseconds(kRefusedPause);
                this->battleMessages.Add(
                    kRefusalMessageX,
                    kRefusalMessageY,
                    kRetreatMessageFrames,
                    kNoRetreatText,
                    kRefusalMessageFlags
                );
                this->audio.PlaySound(kRefusedSound, this->config.soundVolume);
                return;
            }

            SleepMilliseconds(kRetreatPause);
            battle.unk57725C = 1;
            battle.turnUnit = battle.currentUnit;
            battle.result = 0;
            battle.sweptUnits = 0;
            battle.aimTarget = battle.currentUnit - (BattleState::FirstRoleUnit - 1);
            unit = 0;
            battle.unk577204 |= kRetreatMark;
            battle.currentUnit = 0;
            battle.unk5772B4 = 0;
            battle.unk5772BC = 1;
            battle.unk5772C0 = 1;
            battle.listPanelFrames = 0;
            battle.menuGrow = 0;
            battle.listRows = kOpenListRows;
        }
    }

    const ButtonState& space = controls.At(Button::Space);

    if (battle.unk577328 == 1 && space.heldFrames == 1 && space.press != 0)
    {
        battle.unk577080 |= kSpaceMark;
    }

    const ButtonState& rightControl = controls.At(Button::RightControl);

    if (rightControl.press != 0 && rightControl.heldFrames >= 1)
    {
        controls.At(Button::Escape).press = 1;
        controls.At(Button::Escape).heldFrames = rightControl.heldFrames;
    }

    auto panelTaken = [this, &battle]
    {
        if (battle.result == kMenuState)
        {
            this->ConfirmKey();
        }

        battle.anyStateRole = kNoPanelRole;
    };

    if (controls.At(Button::RightShift).press != 0 && Game::KeyRepeats(controls.At(Button::RightShift)))
    {
        if (battle.result > kMenuState)
        {
            return;
        }

        battle.anyStateRole = 0;
        battle.menuCommand = 1;
        this->SelectPreviousRole();
        panelTaken();
        return;
    }

    if (controls.At(Button::KeyA).press != 0 && Game::KeyRepeats(controls.At(Button::KeyA)))
    {
        if (battle.result == kMenuState && this->scene.talkWindows == nullptr &&
            (battle.startPoseRole & BattleState::StartPoseDone) != 0)
        {
            battle.unk5772B8 = 0;
            battle.autoBattle = 1;
            battle.turnCommand = 1;
            battle.turnTaken[static_cast<std::size_t>(unit)] = 1;
            battle.unk5772B4 = 1;
            battle.menuCommand = 1;
            battle.result = kTargetState;
            return;
        }
    }

    const ButtonState& mouseRight = controls.At(Button::MouseRight);

    if (mouseRight.press != 0 && mouseRight.heldFrames >= 1)
    {
        battle.promptFrames = 0;
        battle.unk5772B8 = 1;
        controls.At(Button::Escape).press = 1;
        controls.At(Button::Escape).heldFrames = mouseRight.heldFrames;
    }

    ButtonState& mouseLeft = controls.At(Button::MouseLeft);

    if (mouseLeft.press != 0 && Game::KeyRepeats(mouseLeft))
    {
        battle.unk5772B8 = 1;

        if (battle.anyStateRole != kNoPanelRole)
        {
            this->SelectRoleUnit(static_cast<i16>(battle.anyStateRole));
            panelTaken();
            return;
        }

        battle.promptFrames = 0;

        if (battle.hoverScrollPart == kScrollUp)
        {
            battle.unk577078 = kScrollUp;
            this->MenuUp();
        }

        if (battle.hoverScrollPart == kScrollDown)
        {
            battle.unk577078 = kScrollDown;
            this->MenuDown();
        }

        if (battle.hoverScrollPart == kScrollAbove)
        {
            battle.unk577078 = kScrollAbove;
            this->ScrollListBack();
        }

        if (battle.hoverScrollPart == kScrollBelow)
        {
            battle.unk577078 = kScrollBelow;
            this->ScrollListForward();
        }

        const i32 held = mouseLeft.heldFrames;
        const auto pointerY = static_cast<u32>(this->state.pointerY);

        if (pointerY > static_cast<u32>(this->battleHud.scrollThumbTop) &&
            pointerY < static_cast<u32>(this->battleHud.scrollThumbBottom) && held == 1)
        {
            battle.unk57703C = this->state.pointerY;
            battle.unk577040 = 0;
        }

        if (battle.unk5772FC == 1 && held >= 1)
        {
            battle.unk577078 = kThumbPart;
        }

        controls.At(Button::Space).press = 1;
        controls.At(Button::Space).heldFrames = held;
        battle.unk5772B8 = 1;
    }

    const ButtonState& enter = controls.At(Button::Enter);
    bool confirm = true;

    if (enter.press != 0 && enter.heldFrames % kRepeatStep == 1)
    {
        controls.At(Button::Space).press = 1;
        controls.At(Button::Space).heldFrames = enter.heldFrames;
    }
    else if (controls.At(Button::Space).press == 0)
    {
        confirm = false;
    }

    const i32 spaceHeld = controls.At(Button::Space).heldFrames;

    if (confirm && (spaceHeld == 1 || (spaceHeld >= kRepeatDelay && spaceHeld % kConfirmRepeatStep == 1)))
    {
        if (battle.unk577088 < 0)
        {
            battle.unk577088 |= 1;
        }

        if ((battle.unk577088 & kUnk577088Bit30) != 0)
        {
            battle.unk577088 |= 1;
        }

        if (battle.hitsAllRoles == 1)
        {
            const auto hit = static_cast<std::size_t>(
                this->battleEnemies[static_cast<std::size_t>(battle.actingUnit)].GetWord29A2()
            );
            this->battleRoles[hit].CancelHitNumber(this->battleServices);
        }

        if ((battle.unk5772B8 == 1 && battle.hoverRegion != nullptr) ||
            (battle.unk5772B8 != 1 && !this->state.clickRegions.regions.empty()))
        {
            this->state.clickRegions.Free();
        }

        if (mouseLeft.press == 0)
        {
            if (battle.menuCommand == kRefineMenuCommand || battle.menuCommand == kWaitState50)
            {
                if (battle.unk5772B8 == 0)
                {
                    battle.unk5772B4 = 1;
                    battle.promptFrames = kConfirmIdle;
                }
                else
                {
                    battle.unk5772B8 = 0;
                }
            }
            else
            {
                battle.unk5772B4 = 1;
                battle.unk5772B8 = 0;
            }

            battle.hoverScrollPart = 0;
        }

        battle.panelFlags &= ~BattleState::ResistancePanel;
        this->ConfirmKey();
    }

    if (controls.At(Button::Up).press != 0 && Game::KeyRepeats(controls.At(Button::Up)) &&
        this->scene.talkWindows == nullptr)
    {
        this->MenuUp();
        this->SelectPreviousRole();
        battle.unk577078 = kScrollUp;
    }

    if (controls.At(Button::Down).press != 0 && Game::KeyRepeats(controls.At(Button::Down)) &&
        this->scene.talkWindows == nullptr)
    {
        this->MenuDown();
        this->SelectNextRole();
        battle.unk577078 = kScrollDown;
    }

    if (controls.At(Button::Left).press != 0 && Game::KeyRepeats(controls.At(Button::Left)))
    {
        ClickRegions& regions = this->state.clickRegions;

        if (!regions.regions.empty())
        {
            --this->talk.choice;

            if ((this->talk.choice & kChoiceSignBit) != 0)
            {
                this->talk.choice = regions.Count() - 1;
            }
        }

        if (this->scene.talkWindows == nullptr)
        {
            if (battle.result == kTargetState)
            {
                this->MenuUp();
            }

            this->SelectNextRole();
            this->MenuLeft();
        }
    }

    if (controls.At(Button::Right).press != 0 && Game::KeyRepeats(controls.At(Button::Right)))
    {
        ClickRegions& regions = this->state.clickRegions;

        if (!regions.regions.empty())
        {
            ++this->talk.choice;

            // An unsigned compare (0x0046BEEC).
            if (static_cast<u32>(this->talk.choice) > static_cast<u32>(regions.Count() - 1))
            {
                this->talk.choice = 0;
            }
        }

        if (this->scene.talkWindows == nullptr)
        {
            if (battle.result == kTargetState)
            {
                this->MenuDown();
            }

            this->MenuRight();
            this->SelectPreviousRole();
        }
    }

    if (controls.At(Button::PageUp).press != 0 && controls.At(Button::PageUp).heldFrames % kRepeatStep == 1)
    {
        this->ScrollListBack();
    }

    if (controls.At(Button::PageDown).press != 0 && controls.At(Button::PageDown).heldFrames % kRepeatStep == 1)
    {
        this->ScrollListForward();
    }

    if (controls.At(Button::Escape).press != 0 && Game::KeyRepeats(controls.At(Button::Escape)))
    {
        this->MenuCancel();
        battle.autoBoxStep = 0;
        battle.autoBoxX = 0;
    }
}

// swd3e.exe 0x00471E80: the battle menu of a frame (not in battle state 0x67; without a current unit only in the auto
// battle). A current role that may not choose (StatusBit5) acts by chance at once (RandomRoleAct); another opens the
// menu. By battle state: 1 the command menu (the role that took 蓄勁 sees its charge panel until it holds ailment
// 0x200), 2, 4, 8, 0x1B, 0x1E, 0x29 and 0x50 the lists (2, 0x1B and 0x50 with a scroll bar past seven rows), 3 the aim
// pointers, 5 and 0xA the 佈置護駕 panels, 6 the retreat, 7 the 控制 menu, 9 the ghost's target.
void Game::DrawBattleMenu()
{
    BattleState& battle = this->battleState;
    BattleHud& hud = this->battleHud;

    if (battle.result == kBattleLost || (battle.currentUnit == 0 && battle.autoBattle != 1))
    {
        return;
    }

    i32 unit = battle.currentUnit;

    if (battle.currentUnit != 0)
    {
        if (this->battleRoles[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)].StatusBit5() ==
            1)
        {
            this->RandomRoleAct(battle.currentUnit);
            unit = 0;
            battle.currentUnit = 0;
        }
        else if (battle.result == 0)
        {
            battle.result = kMenuState;
        }
    }

    battle.unk577258 = 1;

    auto listDrawn = [&battle]
    {
        battle.unk5772C0 = 1;
        battle.unk5772BC = 1;
    };

    auto scrollBar = [&battle, &hud](i32 count, i32 first, int x)
    {
        hud.scrollCount = count;
        hud.scrollFirst = first;

        if (count > kListRowsShown)
        {
            hud.DrawScrollBar(kListScrollBar, x, kListScrollY, kListScrollRows, battle.unk577040, battle.unk577078);
        }
    };

    switch (battle.result)
    {
        case kMenuState:
            if (unit == 0)
            {
                return;
            }

            if (battle.chargingRole != -1 && unit == battle.chargingRole)
            {
                if (this->battleRoles[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)].HasEffectBits(
                        kChargeEndsAilment
                    ) != 1)
                {
                    this->DrawChargePanel(battle.menuX + kChargePanelX, battle.menuY, battle.menuCommand);
                    battle.menuGrow = kMenuOpened;
                    return;
                }

                battle.chargingRole = -1;
            }

            this->DrawCommandPanel(unit - BattleState::FirstRoleUnit);
            return;

        case kListState2:
            battle.unk5772C4 = 1;
            this->DrawListTabs(kListX, kListY);

            if (battle.menuGrow == kListGrown && battle.listPanelFrames == kListOpened)
            {
                this->DrawSpellList(kSpellListX, kSpellListY, battle.pageListRow, battle.listScroll);
                scrollBar(static_cast<i8>(battle.listEntries), battle.listScroll, kSpellScrollX);
            }

            listDrawn();
            return;

        case kTargetState:
            this->DrawAimPointers();
            return;

        case kListState4:
            battle.unk5772C4 = 1;
            this->DrawItemList(kListX, kListY, battle.roleListRow, battle.roleListScroll);
            listDrawn();
            return;

        case kWaitState5:
            this->DrawEscortList(kEscortListX, kEscortListY);
            return;

        case kLeaveState:
            if (battle.unk577260 == 0 && battle.hitsAllRoles == 0)
            {
                battle.unk577204 |= kRetreatMark;
                battle.result = 0;
                battle.currentUnit = 0;
                battle.unk5772BC = 1;
                battle.unk5772C0 = 1;
            }

            return;

        case kTalkState:
            this->DrawControlMenu(battle.menuX + kControlMenuX, battle.menuY + kControlMenuY, battle.controlCursor);
            listDrawn();
            return;

        case kSkillListState:
            battle.unk5772C4 = 1;
            this->DrawSpecialList(kListX, kListY, battle.specialRow);
            listDrawn();
            return;

        case kTurnState9:
            this->DrawGhostAimPointer();
            return;

        case kWaitStateA:
            this->DrawEscortFusionList(kEscortListX, kEscortListY);
            return;

        case kListState1B:
            battle.unk5772C4 = 1;
            this->DrawThrowList(kListX, kListY, battle.roleListRow, battle.roleListScroll);
            scrollBar(battle.roleListEntries, battle.roleListScroll, kThrowScrollX);
            listDrawn();
            return;

        case kWaitState1E:
            battle.unk5772C4 = 1;
            this->DrawRefineList(kListX, kListY, battle.roleListRow);
            listDrawn();
            return;

        case kWaitState29:
            battle.unk5772C4 = 1;
            this->DrawTitledItemList(
                kListX,
                kListY,
                battle.roleListRow,
                battle.roleListScroll,
                kFuseCategory,
                BattleState::AccessoryCommandNames[kFuseCommand - kFirstAccessoryCommand]
            );
            listDrawn();
            return;

        case kWaitState50:
            battle.unk5772C4 = 1;
            this->DrawTitledItemList(
                kListX,
                kListY,
                battle.roleListRow,
                battle.roleListScroll,
                kBaitCategory,
                BattleState::RefineOptionNames[kBaitOption - 1]
            );
            scrollBar(battle.roleListEntries, battle.roleListScroll, kThrowScrollX);
            listDrawn();
            return;

        default:
            return;
    }
}

// Battle state 9 of DrawBattleMenu (swd3e, inline in the original): unless the pointer moved, the first standing
// enemy is aimed at (as the ghost's target too) and marked, with the pointer over it and the name tag.
void Game::DrawGhostAimPointer()
{
    BattleState& battle = this->battleState;
    battle.unk5772BC = 1;
    battle.unk5772C0 = 1;
    this->mediumFont.SetAdvance(TextFont::DefaultGlyphSize);
    battle.actingEnemy = 0;

    if (battle.unk5772B8 == 0)
    {
        // The original searches past the enemies when every one is down (deviations.md): the search ends there.
        while (battle.actingEnemy < static_cast<i32>(battle.enemyCount) - 1 &&
               this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
        {
            ++battle.actingEnemy;
        }

        battle.unk5772B4 = 1;
        battle.unk577244 = 0;
        battle.unk577240 = 0;
        BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)];
        enemy.SetAC0(1);
        UnitRect rect;
        enemy.GetShadowRect(this->battleServices, rect);
        enemy.GetOffset(battle.unk577240, battle.unk577244);
        battle.aimTarget = battle.actingEnemy + 1;
        battle.aiTarget = battle.actingEnemy + 1;
        this->battleHud.DrawSlotAnimation(kTurnPointer, 0, rect.x + (rect.width >> 1), rect.y + (rect.height >> 1));
    }

    if (battle.unk5772B4 == 1)
    {
        this->DrawTargetTag(kTargetTagX, kTargetTagY);
    }
}

// Battle state 1 of DrawBattleMenu (inline in the original): the tall gauge grows for six frames (menuGrow) from
// the pointer, then the name of party role nameIndex (the role whose panel it is, or the step of the panel handed
// on) and the command menu.
void Game::DrawCommandPanel(i32 nameIndex)
{
    smooth::Scene scene;
    smooth::TurnFrame(*this).DrawCommandPanelAdvance(nameIndex, scene);
    scene.Show(this->battleServices);
}

// Battle state 3 of DrawBattleMenu (inline in the original). Aiming at every unit (unk577284), a pointer over each
// role standing that is not the ghost (for a command at the party) or each enemy standing (at its anchor when it has
// one). Otherwise a defeated aimed enemy passes the aim on through enemyOrder (state 1 when none stands), and the
// pointer marks the aimed role or enemy (turnPointer 0: the refusing pointer; 煉妖 and 誘餌 refuse an enemy not a small
// machine), with the name tag while unk5772B4.
void Game::DrawAimPointers()
{
    BattleState& battle = this->battleState;
    BattleHud& hud = this->battleHud;
    battle.unk5772C0 = 1;
    battle.unk5772BC = 1;
    this->mediumFont.SetAdvance(TextFont::DefaultGlyphSize);

    // Without a current unit (the auto battle) the original reads the order before the first: taken as aiming at
    // the enemies.
    const bool atParty =
        battle.currentUnit >= BattleState::FirstRoleUnit &&
        battle.pendingOrders[static_cast<std::size_t>(battle.currentUnit - BattleState::FirstRoleUnit)].unk00 != 0;
    UnitRect rect;

    if (battle.unk577284 == 1)
    {
        if (atParty)
        {
            for (u32 index = 0; index < battle.roleCount; ++index)
            {
                BattleRole& role = this->battleRoles[index];

                if (role.AutoControl() != 1 && role.GuestControl() != 1 && role.Ghost() != 1 && role.StatusBit13() == 0)
                {
                    role.GetShadowRect(this->battleServices, rect);
                    role.SetAC0(1);
                    hud.DrawSlotAnimation(
                        kTurnPointer,
                        static_cast<int>(index),
                        rect.x + (rect.width >> 1),
                        rect.y + (rect.height >> 1)
                    );
                }
            }

            return;
        }

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];

            if (enemy.IsDefeated())
            {
                continue;
            }

            enemy.GetShadowRect(this->battleServices, rect);
            enemy.SetAC0(1);
            enemy.GetOffset(battle.unk577240, battle.unk577244);

            if (battle.unk577240 != 0 || battle.unk577244 != 0)
            {
                hud.DrawSlotAnimation(
                    kTurnPointer,
                    static_cast<int>(index),
                    rect.x + static_cast<i16>(battle.unk577240),
                    rect.y + static_cast<i16>(battle.unk577244)
                );
            }
            else
            {
                hud.DrawSlotAnimation(
                    kTurnPointer,
                    static_cast<int>(index),
                    rect.x + (rect.width >> 1),
                    rect.y + (rect.height >> 1)
                );
            }
        }

        return;
    }

    if (!atParty && this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
    {
        u32 tries = 0;

        while (true)
        {
            if (++battle.orderStep > static_cast<i32>(battle.enemyCount))
            {
                battle.orderStep = 1;
            }

            battle.actingEnemy = battle.enemyOrder[static_cast<std::size_t>(battle.orderStep - 1)];
            battle.aimTarget = battle.actingEnemy + 1;

            if (++tries >= battle.enemyCount)
            {
                battle.result = kMenuState;
                break;
            }

            if (!this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)].IsDefeated())
            {
                break;
            }
        }
    }

    if (battle.aimTarget == -1)
    {
        return;
    }

    if (battle.unk5772B8 == 0)
    {
        battle.unk577244 = 0;
        battle.unk577240 = 0;

        if (atParty)
        {
            const auto aimed = static_cast<std::size_t>(battle.aimTarget - 1);

            if (aimed < this->battleRoles.size())
            {
                BattleRole& role = this->battleRoles[aimed];
                role.GetShadowRect(this->battleServices, rect);
                role.GetOffset(battle.unk577240, battle.unk577244);
                role.SetAC0(1);
                battle.unk577244 = static_cast<u16>(battle.unk577244 + kRoleAimDrop);
            }
        }
        else
        {
            BattleEnemy& enemy = this->battleEnemies[static_cast<std::size_t>(battle.actingEnemy)];
            enemy.SetAC0(1);
            enemy.GetShadowRect(this->battleServices, rect);
            enemy.GetOffset(battle.unk577240, battle.unk577244);
            battle.aimTarget = battle.actingEnemy + 1;

            if (battle.menuCommand == kRefineMenuCommand || battle.menuCommand == kWaitState50)
            {
                battle.turnPointer = enemy.IsSmallMachine() ? 1 : 0;
            }
        }

        hud.DrawSlotAnimation(
            battle.turnPointer == 0 ? kNoTurnPointer : kTurnPointer,
            0,
            rect.x + (rect.width >> 1),
            rect.y + (rect.height >> 1)
        );
    }

    if (battle.unk5772B4 == 1)
    {
        this->DrawTargetTag(kTargetTagX, kTargetTagY);
    }
}

// swd3e.exe 0x00475CB0: the frame of the battle end states (not while a talk window or a battle message shows):
// 0x60 back to state 0; 0x61 the glow of the unit after the roles (a level-up role 1 once over); 0x62 the 煉符 result;
// 0x63 the end checked (every enemy down, unless the talk or unk57734C ended it), then 0x64; 0x64 the victory panel
// (without it the fade after kEndMessageFrames frames); 0x65 the level-ups one role at a time (0x70 once none is left);
// 0x66 the loot; 0x67 the defeat; 0x68 a pause; 0x6E the level-up stats; 0x6F the new spell; 0x70 the new special
// skills (0x71 after); 0x71 the masteries (0x66 after). The panels confirm themselves after kEndPanelFrames.
void Game::BattleEndFrame()
{
    BattleState& battle = this->battleState;
    BattleMessages& messages = this->battleMessages;

    if (this->scene.talkWindows != nullptr || messages.messages != nullptr)
    {
        return;
    }

    auto levelRole = [this, &battle]() -> BattleRole&
    {
        return this->battleRoles[static_cast<std::size_t>(static_cast<i8>(battle.levelUpRole))];
    };

    auto noLevelRole = [&battle]
    {
        return battle.levelUpRole == kNoLevelUp;
    };

    switch (battle.result)
    {
        case kEndBack:
            battle.unk5772C0 = 0;
            battle.unk5772B4 = 0;
            battle.result = 0;
            return;

        case kEndGlow:
        {
            // The original reads the unit after the roles even with every place taken.
            if (battle.roleCount >= this->battleRoles.size())
            {
                return;
            }

            const UnitPlacement& place = battle.rolePlaces[battle.roleCount];

            if (this->battleRoles[battle.roleCount].GlowAct(this->battleServices, place.x, place.y) == 1)
            {
                battle.levelUpRole = 1;
            }

            return;
        }

        case kEndCraft:
            battle.unk5772BC = 1;
            messages.DrawCraftResultPanel();
            return;

        case kEndCheck:
            this->EndCheckFrame();
            return;

        case kEndVictory:
            if (battle.victoryWithoutPanel == 1)
            {
                if (++battle.endFrames >= kEndMessageFrames)
                {
                    battle.endFrames = 0;
                    battle.endFade = 1;
                }

                return;
            }

            this->DrawVictoryPanel();
            battle.unk577260 = 0;
            battle.unk5772BC = 1;
            battle.unk5772C0 = 1;
            battle.unk577270 = 1;
            battle.currentUnit = 0;

            if (++battle.endFrames >= kEndPanelFrames)
            {
                this->ConfirmKey();
            }

            return;

        case kEndLevelUps:
            if (noLevelRole())
            {
                this->CheckLevelUps();

                if (noLevelRole())
                {
                    battle.endFrames = 0;
                    battle.result = kEndSpecials;
                    return;
                }
            }

            if (battle.levelUpPending == 0)
            {
                if (!levelRole().IsDefeated())
                {
                    battle.levelUpPending = levelRole().RestartMainAct(kLevelUpPose, 0) ? 1 : 0;
                }

                if (battle.levelUpPending == 0)
                {
                    return;
                }
            }

            if (noLevelRole())
            {
                battle.endFrames = 0;
                battle.result = kEndSpecials;
            }
            else
            {
                this->DrawLevelUpPanel();
                ++battle.endFrames;
            }

            return;

        case kEndLoot:
            if (messages.lootCount == 0)
            {
                battle.endFade = 1;
                return;
            }

            if (++battle.endFrames >= kEndPanelFrames)
            {
                this->ConfirmKey();
            }

            messages.DrawLootPanel();
            return;

        case kBattleLost:
            battle.unk5772BC = 1;
            battle.unk5772C0 = 1;

            if ((battle.panelFlags & BattleState::UnkPanel8) != 0)
            {
                if (++battle.endFrames >= kEndQuietFrames)
                {
                    battle.endFrames = 0;
                    battle.endFade = 1;
                }

                return;
            }

            messages.DrawDefeatPanel();

            if (++battle.endFrames >= kEndPanelFrames)
            {
                this->ConfirmKey();
            }

            return;

        case kLostQuietly:
            battle.unk5772BC = 1;
            battle.unk5772C0 = 1;

            if (++battle.endFrames > kEndMessageFrames)
            {
                this->ConfirmKey();
            }

            return;

        case kEndStats:
            this->DrawLevelUpStats();
            return;

        case kEndNewSpell:
            this->DrawNewSpell();
            ++battle.endFrames;
            return;

        case kEndSpecials:
            if (noLevelRole())
            {
                this->LearnSpecials();

                if (!noLevelRole())
                {
                    this->audio.PlaySound(kLearnedSound, this->config.soundVolume);

                    if (!levelRole().IsDefeated())
                    {
                        levelRole().RestartMainAct(kLearnedPose, 1);
                    }
                }

                if (noLevelRole())
                {
                    battle.endFrames = 0;
                    battle.result = kEndMastery;
                    return;
                }
            }

            this->DrawNewSpecial();
            ++battle.endFrames;
            return;

        case kEndMastery:
            if (noLevelRole())
            {
                this->AnnounceMastery();

                if (!noLevelRole())
                {
                    this->audio.PlaySound(kLearnedSound, this->config.soundVolume);

                    if (!levelRole().IsDefeated())
                    {
                        levelRole().RestartMainAct(kLearnedPose, 1);
                    }
                }

                if (noLevelRole())
                {
                    battle.result = kEndLoot;
                    battle.endFrames = 0;

                    if (messages.lootCount != 0)
                    {
                        this->audio.PlaySound(kLearnedSound, this->config.soundVolume);
                    }

                    return;
                }
            }

            messages.DrawMastery();
            ++battle.endFrames;
            return;

        default:
            return;
    }
}

// Battle state 0x63 of BattleEndFrame (inline in the original); see there. Unless the talk or unk57734C ended the
// battle, every enemy is unmarked and one still standing goes back to state 0; otherwise every role is unmarked and
// its effects expire (1), and the victory follows (0x64).
void Game::EndCheckFrame()
{
    BattleState& battle = this->battleState;
    battle.unk577204 = 0;
    battle.autoBattle = 0;
    battle.unk577A50 = 1;

    if (this->fightTalk.wonByTalk == 0 && battle.unk57734C == 0)
    {
        u32 defeated = 0;

        for (u32 index = 0; index < battle.enemyCount; ++index)
        {
            BattleEnemy& enemy = this->battleEnemies[index];
            enemy.SetAC0(0);

            if (enemy.IsDefeated())
            {
                ++defeated;
            }
        }

        if (defeated != battle.enemyCount)
        {
            battle.result = 0;
            return;
        }
    }

    for (u32 index = 0; index < battle.roleCount; ++index)
    {
        BattleRole& role = this->battleRoles[index];
        role.SetAC0(0);
        role.ExpireEffects(this->battleServices, 1);
    }

    battle.result = kEndVictory;
}
}  // namespace swd3e
