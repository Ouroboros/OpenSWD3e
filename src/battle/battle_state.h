#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "battle/battle_file.h"
#include "battle/battle_hud.h"
#include "battle/battle_unit.h"
#include "core/types.h"
#include "game/color_fade.h"
#include "item/item_list.h"
#include "org/level.h"
#include "party/party.h"
#include "pic/picture.h"
#include "pic/projector.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "res/act_state.h"

namespace swd3e
{

struct ClickRegion;
struct ScriptObject;

// A record of 0x1C bytes, eighteen of them from 0x00524788 (cleared by the battle start): a unit with a kind of 1 or
// 2 (AddSlotRecord), the free records after the used ones.
struct BattleSlotRecord
{
    i32 unit = -1;        // +0x00 -1: a free record
    i32 targetCount = 0;  // +0x04 the units its order hits (BattleState::sweepTotal)
    u16 kind = 0;         // +0x08
    u16 unk0A = 0;        // +0x0A
    i32 unk0C = 0;        // +0x0C
    i32 unk10 = 0;        // +0x10
    i32 unk14 = 0;        // +0x14
    i32 unk18 = 0;        // +0x18
};

// The order a party unit chose (0x14 bytes, ten at 0x00520E90), taken into its slot record by InsertSlotRecord.
struct PendingOrder
{
    i32 unk00 = 0;  // +0x00 to the record's unk0C
    i32 unk04 = 0;  // +0x04 to unk14
    i32 unk08 = 0;  // +0x08 to unk18
    u16 unk0C = 0;  // +0x0C to unk0A
    u16 unk0E = 0;
    i32 targetCount = 0;  // +0x10 the units it hits, counted as they are gathered (to targetCount)
};

// What an object bought in the shop changes for a party role (0x10 bytes, four of them at 0x004FD3C8): the attack,
// defence (each with its equipment bonus) and agility against the equipment worn, and whether the role can use it.
struct ShopComparison
{
    i32 attack = 0;   // +0x00
    i32 defense = 0;  // +0x04
    i32 agility = 0;  // +0x08
    i32 usable = 0;   // +0x0C
};

// The state of the battle and of the shop and item lists it shares with the menus.
// swd3e: a picture of a second-tier spell (a 0xB4-byte list node at 0x00577818, AddHitPicture): an act played at its
// place until it waits, its target bits gathered for the hit (hitBits); bit 5 adds its effect as another such picture.
struct HitPicture
{
    std::unique_ptr<HitPicture> next;  // +0x00
    ActState act;                      // +0x04
    i32 mirrored = 0;                  // +0xA8 1: drawn mirrored
    i16 x = 0;                         // +0xAC
    i16 y = 0;                         // +0xAE
    u8 done = 0;                       // +0xB0 1: its act waited (freed after the frame)
};

class BattleState
{
public:
    static constexpr i32 InitialGaugeFull = 900;
    static constexpr std::size_t ItemUseIds = 0xFF;  // itemUses entries
    static constexpr u8 StartPoseDone = 0x80;        // startPoseRole: the start pose is over
    static constexpr u8 CostlyUses = 2;  // past this many uses (itemUses + 1 >= 3) an object costs a third more
    static constexpr int QueueRows = 4;
    static constexpr int QueueColumns = 10;
    static constexpr int Enemies = 20;                 // 0x00539590: 20 enemy units of 0x3078 bytes
    static constexpr int RoleUnits = 10;               // the party roles and the guests
    static constexpr int Units = Enemies + RoleUnits;  // the enemies, then the roles
    static constexpr int FirstRoleUnit = Enemies;
    static constexpr i32 SlideToUnit = 1;  // slideMode (SlideUnits)
    static constexpr i32 SlideBack = 2;
    static constexpr int Guests = 8;
    static constexpr std::size_t LootSlots = 10;  // as BattleMessages::LootSlots
    static constexpr std::size_t TextBytes = 64;
    static constexpr u16 DefaultTimingWindow = 0x3C;
    static constexpr int FieldHeight = 0x180;  // the rows of the battle field above the panels
    static constexpr int BandRow = 0xC0;       // the grey band opens from this row
    static constexpr u16 BandFull = 0xC0;
    static constexpr u16 BandFastFrom = 0x14;  // below it the band doubles a frame, from it it grows by BandStep
    static constexpr u16 BandStep = 0x16;
    static constexpr i8 FlashStart = 0x10;
    static constexpr i8 FlashStep = 4;
    static constexpr i16 MaxDarkLevel = 2;
    static constexpr i32 DarkLevelFrames = 1;  // darkFrames past it step to the next level
    static constexpr std::array<u8, 10> QuakeSteps = {4, 3, 4, 3, 4, 3, 2, 3, 2, 3};  // 0x004BF190 by quakePhase
    static constexpr std::array<i32, 5> InitialRoleCycle = {11, 2, 1, 0, 3};          // 0x004BF578 (exe)
    static constexpr std::array<i32, 4> PanelUnits = {10, 9, 8, 11};             // 0x004BF56C the role units by panel
    static constexpr std::array<int, 4> PortraitX = {0xFD, 0x5D, 0x19D, 0x19D};  // 0x004BF13C by place (swd3e)
    // 0x004BF0C4: the five commands of the menu (pointers in the original, kinds 0x2A..0x2E of the accessory names)
    static constexpr std::array<std::u8string_view, 5> FixedCommandNames =
        {u8"攻擊", u8"奇術", u8"物品", u8"絕技", u8"防禦"};
    static constexpr std::u8string_view RefineCommandName = u8"煉妖";   // 0x004BF614 battle role 0's first command
    static constexpr std::u8string_view GhostCommandName = u8"符鬼";    // 0x004B12B4 its second (game flag 0x5A)
    static constexpr std::u8string_view ReleaseCommandName = u8"釋放";  // 0x004BF60C kind 0x28 while the meter shows
    // 0x004BF01C [0x15..0x29]: the names of the accessory commands by kind (pointers in the original)
    static constexpr std::array<std::u8string_view, 21> AccessoryCommandNames = {
        u8"連擊", u8"天雷", u8"偷竊", u8"群攻", u8"控制", u8"命中", u8"投擲", u8"隱形", u8"斷玉", u8"煉符", u8"鼠槌",
        u8"護罩", u8"分解", u8"氣合", u8"回氣", u8"運功", u8"龍颿", u8"改變", u8"蓄勁", u8"逆攻", u8"融合",
    };

    // 0x004BF630: the orders of the talisman ghost the 符鬼 command offers (ghostOrdersOpen)
    static constexpr std::array<std::u8string_view, 8> GhostOrderNames = {
        u8"自由攻擊",
        u8"保存靈力",
        u8"靈力攻擊",
        u8"保護主人",
        u8"攻擊弱者",
        u8"攻擊強者",
        u8"援助夥伴",
        u8"鎖定目標",
    };

    // 0x004BF650: the choices of the 煉妖 command (refineOptionsOpen)
    static constexpr std::array<std::u8string_view, 2> RefineOptionNames = {u8"收化", u8"誘餌"};

    // BattleState::mode bits.
    enum Mode : i32
    {
        SellMode = 1,  // prices at 60 percent
        UnkMode2 = 2,
        ListModes = SellMode | UnkMode2,
    };

    // BattleState::panelFlags bits.
    enum PanelFlag : i32
    {
        UnkPanel2 = 2,        // set when game flag 0xC9 is on at the battle start
        UnkPanel4 = 4,        // an escort was sent back (ConfirmEscort)
        UnkPanel8 = 8,        // the battle ends with frameResult 0 even when lost (CheckBattleEnd)
        UnkPanel100 = 0x100,  // the screen quakes all the time (QuakeFrame)
        ResistancePanel = 0x20,
        NoRetreat = 0x200,    // the party cannot retreat
        UnkPanel800 = 0x800,  // swd3e: no hit number is taken back (CancelHitNumber)
        MusicGiven = 0x400,   // swd3e: a battle talk set the battle music (BattleIntro keeps it, then clears this)
    };

    // Bits of spellStage.
    enum SpellStage : u16
    {
        SpellOver = 0x1,      // the spell is over (MagicFrame ends)
        UnkSpell400 = 0x400,  // swd3e: set with SpellCast once the coin toss of command 0x16 closes the grey band
        SpellCast = 0x8000,   // the magic act followed up: the spell takes effect
    };

    BattleState();
    ~BattleState();
    BattleState(const BattleState&) = delete;
    BattleState& operator=(const BattleState&) = delete;

    void SortDrawOrder();
    void ListEnemyOrder();
    void AddSlotRecord(int kind, i32 unit);
    void RemoveSlotRecord(i32 unit);
    void InsertSlotRecord(int kind, i32 unit, int at);
    void StartFieldFade(int red, int green, int blue, int toRed, int toGreen, int toBlue, int frames);
    i16 StartSlideTo(i16 x);
    i16 StartSlideBack(i16 x);
    u8 ItemUses(u16 id) const;
    void CountItemUse(u16 id);
    u8 MostHitEnemy() const;
    void ClearHitBits();
    HitPicture& AddHitPicture(ErrorReporter& errors, i32 id, i32 action, i16 x, i16 y, i32 mirrored);
    bool HitPicturesDone() const;
    u16 HitBits() const;
    void WipeSlotRecords();
    void SortDrawOrderAround(i32 acting, i32 target);
    void SetReturnedItem(int index, u16 id);
    void TakeSlotRecordAt(std::size_t at, BattleSlotRecord& taken);
    void ClearActing();
    u16 MenuKind(int command) const;

    i32 listBCount = 0;         // 0x0050BF18 the rows of the list shown from listB, at most 13
    ItemNode* listB = nullptr;  // 0x0050C034 the first item shown (itemsHead itself until the list is made)
    i32 mode = 0;               // 0x0050C0EC bits 0..1: the shop and sell modes of the item lists
    ItemNode itemsHead;         // 0x0050C0F8 the node heading the sorted item list (its count at 0x004FD588)

    // The shop (talk command ItemMenu, Game::ItemChoiceMenu).
    i32 shopStage = 0;    // 0x0050BF7C 0: closed, 1: buy, sell or leave, 2: the list, 3: the total, 4: equip now?, 0xA:
                          // too little money
    u32 shopFlags = 0;    // 0x0050BF78 bit 1: the list was opened (its sound played), bit 2: equipment was bought
    i32 listTop = 0;      // 0x0050BF88 the first item shown
    i32 listRow = 0;      // 0x0050C1AC the row chosen
    i32 itemCount = 0;    // 0x0050BF70 the items of the list
    i16 lessPressed = 0;  // 0x0050BF1C frames the minus arrow of the row chosen shows pressed
    i16 morePressed = 0;  // 0x0050BF74 the plus arrow
    std::array<i16, Party::Roles> faceSlides = {};              // 0x0050BF20 the roles' faces slide out (8..16)
    std::array<ShopComparison, Party::Roles> comparisons = {};  // swd32 0x004FD3C8
    u16 scrollArrows = 0;              // 0x0050BF80 frames the scroll arrows show pressed (bits 0..3: up, 4..7: down)
    std::vector<u16> shopScreen;       // 0x0050BF84 the darkened grey map under the shop
    std::vector<u16> shopScreenSaved;  // 0x0050C0F0 a copy of it
    ActState shopTitle;                // 0x0050BF90 act 0x232A, action 0x16
    i32 gaugeFull = InitialGaugeFull;  // 0x004BEFF8 the full action gauge
    std::array<i16, 3> darkCopyTint = {-1, -1, -1};  // 0x004BF1D6 the darkRed, darkGreen, darkBlue of the copies
    // 0x00578474 1: a role's critical attack is under way (not evaded)
    i32 criticalAttack = 0;  // 0x00578474 1: the next attack of a role is critical (battle talk command 0x43)
    // 0x00578470 (low byte) the attacks since the last critical one
    u8 attacksSinceCritical = 0;
    i32 lastCriticalRole = 0;   // 0x00578478 the act of the role with the last rolled critical attack
    i32 fadingField = 0;        // 0x00577334 1: an act started the field fade (the background waits for it)
    u8 evadeStreak = 0;         // 0x005785EC failed evasion rolls in a row of the party roles
    u8 evadeJustSucceeded = 0;  // 0x005785ED
    u8 enemyEvadeStreak = 0;    // 0x005785EE (swd3e) failed evasion rolls in a row of the weak enemies
    // 0x005134A0 (swd3e) the uses of each object id in this battle (0xFF of them: cleared by StartBattle, raised by
    // MenuConfirm); from the third use an object costs a third more (ListObject, SelectObject)
    std::array<u8, ItemUseIds> itemUses = {};
    std::array<std::unique_ptr<Surface>, 2> surfaces;  // 0x005379F4 screen sized off-screen surfaces
    // swd32 0x0053477C 4 rows of 10 (the queued refines of the roles)
    std::array<i16, QueueRows * QueueColumns> actionQueue = {};
    i32 panelFlags = 0;        // 0x00576D3C bit 5: the resistance panel
    i16 pendingActionRow = 0;  // 0x00577206
    i16 pendingAction = 0;     // 0x00577208
    i32 paused = 0;            // swd32 0x0053C00C messages do not slide while set
    // swd32 0x0053D02C the formatted text of the battle messages
    std::array<char, TextBytes> text = {};

    // Set by the battle start (0x00451B10).
    u16 spellStage = 0;                 // 0x00577378 BattleState::SpellStage bits of the running spell
    u16 battle = 0;                     // swd32 0x0053C052 the battle number
    BattleFile file{&this->projector};  // 0x0050F620 battle.ffd
    BattleFormation formation;          // 0x0050EDE0
    Projector projector;                // 0x005761D0
    Picture background;                 // 0x00513638 picture `background` + 15000 of the formation, loaded directly
    // 0x0050CEE8 the animations over the background (animationCount of them)
    std::array<BattleAnimation, BattleFormation::Animations> backgroundAnimations;
    u32 enemyCount = 0;                                       // 0x00576FA0
    u32 roleCount = 0;                                        // 0x00576FA4 the party and the guests in battle
    u32 animationCount = 0;                                   // 0x005770B8 the formation's animationCount
    u16 scene = 0;                                            // 0x005771F8 1..4 at random
    std::array<i32, Party::Roles> partyRoles = {0, 1, 2, 3};  // swd32 0x004A75C8 the party role of each battle role
    // 0x0050ECB4 (swd3e) the names the battle shows for the party roles: the nonzero bytes of the first ten of each
    // RoleName (the surname and given name), set by StartBattle
    std::array<std::array<char, sizeof(RoleName)>, Party::Roles> roleNames = {};
    // swd32 0x00521398 the four roles are in the party (story flags 0x1E..)
    std::array<u8, Party::Roles> inParty = {};
    // 0x004BF578 the party roles by turn, from 1 (swd3e: a party of three rewrites [1..4], StartBattle)
    std::array<i32, 5> roleCycle = InitialRoleCycle;
    u16 partyMask = 0;  // 0x005770D2 (swd3e) bit per party role in the battle (StartBattle; not read)
    i32 unk577224 = 0;  // 0x00577224 (swd3e) cleared by StartBattle, not read
    u16 unk577228 = 0;  // 0x00577228 (swd3e) cleared by StartBattle, not read
    u8 unk5774E8 = 0;   // 0x005774E8 (swd3e) cleared by StartBattle, used by EnemyFrame
    i32 unk576FC4 = 0;  // 0x00576FC4 (swd3e) cleared by StartBattle, used by EnemyFrame
    i32 unk576FC8 = 0;  // 0x00576FC8 (swd3e)
    i32 unk576FD0 = 0;  // 0x00576FD0 (swd3e)
    i32 unk576FCC = 0;  // 0x00576FCC (swd3e) cleared by StartBattle, used by EnemyFrame
    // 0x0050D878 (swd3e) per enemy: 1 while the name of its skill is to show when it acts (EnemyFrame)
    std::array<u8, Enemies> skillNamePending = {};
    i32 unk57736C = 0;  // 0x0057736C (swd3e) 1: every party role standing is taunted (EnemyFrame aims at them all)
    // 0x005376A4 (swd3e) per enemy: the party's hits on it (MenuConfirm; cleared when it falls); the roles acting by
    // themselves aim at the most hit (MostHitEnemy)
    std::array<u8, Enemies> enemyHits = {};
    // swd3e: the units slide sideways with the field (SlideUnits): 1 to a unit's place, 2 back to the middle; never on
    // (0x00577364 is only read)
    i32 slideEnabled = 0;  // 0x00577364
    i32 slideMode = 0;     // 0x005777FC
    i16 slideTarget = 0;   // 0x00577804
    u16 guestCount = 0;    // 0x005771F4
    // 0x005770D4 low word: party roles the battle's checks leave out (read by 0x004612D0, 0x0045FA80 and
    // CheckBattleEnd; its writer is not rebuilt yet); the high word is a role slot
    u16 unk5770D4 = 0;
    i16 ghostSlot = 0;     // 0x005770D6 (swd3e) the role slot of the talisman ghost (StartBattle sets it to roleCount)
    i32 ghostRescued = 0;  // 0x00577358 (swd3e) 1 once the ghost healed its master this turn (ApplyHits; 0x0045FA80)
    i32 musicBeforeBattle = 0;               // 0x005770C8 the music queue's mode when the battle started (StartBattle)
    u8 defeatedRoles = 0;                    // 0x005771E7 the party roles entering defeated
    i32 result = 0;                          // 0x00576FAC 0x67: the battle is lost before it starts
    i32 mirrored = 0;                        // 0x00577304 the sides are swapped
    u16 timingWindow = DefaultTimingWindow;  // swd32 0x004A762A
    u16 lightColor = 0;                      // swd32 0x004FF104
    u16 darkColor = 0;                       // swd32 0x005240BC
    i32 pointerX = 0;                        // 0x00513428
    i32 pointerY = 0;                        // 0x0051342C
    std::array<UnitPlacement, Enemies> enemyPlaces;      // swd32 0x005213A0
    std::array<UnitPlacement, RoleUnits> rolePlaces;     // 0x00576088
    std::array<i32, 2 * Party::Roles> panelPlaces = {};  // 0x0050F3B8 the x and y of the four role panels
    std::array<i32, Party::Roles> roleListRows = {};     // swd32 0x004FF578 by role: its roleListRow
    std::array<i32, RoleUnits> hitPointBars = {};        // 0x00534750 and its copy 0x00502914
    std::array<i32, RoleUnits> hitPointBarsShown = {};
    std::array<i32, RoleUnits> hitPoints = {};   // 0x005347CC
    std::array<i32, RoleUnits> spiritBars = {};  // 0x00537430 and its copy 0x00524288
    std::array<i32, RoleUnits> spiritBarsShown = {};
    std::array<i32, RoleUnits> spirit = {};       // 0x00534728
    std::array<i32, RoleUnits> staminaBars = {};  // 0x005374FC and its copy 0x00524490
    std::array<i32, RoleUnits> staminaBarsShown = {};
    std::array<i32, RoleUnits> stamina = {};      // 0x00536884
    std::array<i32, RoleUnits> blockValues = {};  // 0x005377D4 and its copy 0x0052418C
    std::array<i32, RoleUnits> blockValuesShown = {};
    std::array<i32, Units> unitY = {};         // 0x0053694C the y of each unit
    std::array<i32, Units> drawOrder = {};     // 0x0053730C the units from the top down
    std::array<i32, Units> drawTaken = {};     // 0x00536808
    std::array<i32, Enemies> enemyOrder = {};  // 0x005368D4 the enemies from the top down
    ActState unk525470;
    ObjectRecord unk5246E0;
    std::array<i32, 0x14> unk4FF168 = {};
    std::array<i32, 0x3C> unk524324 = {};
    std::array<i32, 0xA> unk4FE5D4 = {};
    std::array<std::u8string_view, 4> commandNames;  // 0x0050EBF0 the listed commands of the menu (pointers)
    std::array<u16, 4> commandKinds = {};            // 0x0050D870 their kinds (swd3e: four)
    std::array<i32, Units> unk524268 = {};           // 0x005376B8 per unit (swd3e: 0x78 bytes)
    // 0x005378EC the five commands of the menu and the four listed ones (swd3e; the ninth byte is the low byte of
    // hitDamage in the original, kept apart here: deviations.md)
    std::array<u8, 9> commandEnabled = {};
    // 0x0050D86C (swd3e) a colour (0x19, 0x19, 0x1D) set by StartBattle and not drawn: the steps of MenuRight and
    // MenuLeft past an empty listed command read it as the kind of command 4 (MenuKind)
    u16 unk50D86C = 0;
    i32 chargingRole = -1;     // 0x004BF58C (swd3e) the role unit that took 蓄勁 (kind 0x27): its menu offers 1 and 2
    i32 meterRole = -1;        // 0x004BF590 (swd3e) the role unit that started the meter (逆攻, kind 0x28)
    u8 ghostOrdersOpen = 0;    // 0x00577999 (swd3e) 1: the 符鬼 command shows the ghost's orders
    u8 refineOptionsOpen = 0;  // 0x0057799A (swd3e) 1: the 煉妖 command shows its choices (0x80 once one is taken)
    u8 elementsOpen = 0;       // 0x0057799B (swd3e) 1: the 改變 command shows the five elements
    i32 ghostOrderRow = 1;     // 0x004BF0F8 (swd3e) the order under the cursor (1-based)
    i32 refineOptionRow = 0;   // 0x00577990 (swd3e) the choice under the cursor (1-based)
    i32 elementRow = 0;        // 0x0057798C (swd3e) the element under the cursor (1-based)
    std::array<i32, Party::Roles> roleListScrolls =  // 0x0052544C by role: its roleListScroll
        {};  // 0x005394B4..0x00525458 per party role: the scroll of its list (ScrollListBack)
    std::array<PendingOrder, RoleUnits> pendingOrders = {};  // 0x005369C8
    // 0x00536EE0 (swd3e) per enemy: its chosen order, taken into its slot record by AddSlotRecord (kind 2)
    std::array<PendingOrder, Enemies> enemyPendingOrders = {};
    i32 unk53BF80 = 0;  // cleared when a party unit's order is taken (InsertSlotRecord)
    i32 unk53BFD0 = 0;
    // 0x0057735C (swd3e) 1: the skill chosen (ActFlag17) is turned on the role itself once its enemy is aimed at, the
    // enemy kept in the role's unk34C0 (MenuConfirm states 2 and 3)
    i32 unk57735C = 0;
    // 0x0057722A (swd3e) the role whose start pose (Pose36Frame) plays at the battle start, StartPoseDone once over
    // (StartBattle, RoleFrame); the auto battle key waits for it
    u8 startPoseRole = 0;
    i32 ghostLockTarget = 0;  // 0x00576FBC (swd3e) the enemy (1-based) the ghost's 鎖定目標 aims at; 0: none
    i16 unk53BF46 = 0;        // cleared when a stagger ends (HurtFrame)
    i16 unk53BF48 = 0;
    u16 unk53BF28 = 0;  // non-zero: an enemy does not choose a command with UnkAction1000 (ChooseCommand)
    std::array<i32, Units> unk4FF0BC = {};  // 0x0050EBFC (swd3e: 30 slots, 0x78 bytes)
    i32 unk502944 = 0;
    i32 unk502948 = 0;
    i32 unk50294C = 0;
    i32 unk502950 = 0;
    i32 unk5244D8 = 0;
    i32 unk5244DC = 0;
    std::array<i32, Units> slotDamage = {};   // 0x00537758 per slot: the damage of the hit so far
    std::array<i32, Units> unk502984 = {};    // 0x00513664 per slot: the slot of a unit the hit felled (-1: none)
    std::array<i32, Units> slotTargets = {};  // 0x005378F8 per slot: the unit it hits (-1: none)
    std::array<i32, Units> unk53AE90 = {};    // 0x00575F38 (swd3e: 30 slots) set to -1 by StartBattle
    std::array<i32, 0xD2> unk5244E8 =
        {};  // 0x00537A20 (swd3e, 0x348 bytes) set to -1 by StartBattle and EndBattle only
    i32 unk53C048 = 0;
    i32 actingUnit = 0;      // 0x00575F18 the unit acting now (-1: none)
    u16 listedCommands = 0;  // 0x0057720A the commands the menu lists beyond its fixed ones
    i32 unk53C4B0 = 0;
    std::array<BattleSlotRecord, Units> slotRecords;  // 0x00537E10 the BattleSlotRecord of each slot
    u16 escortCount = 0;                              // 0x0057720E the high word of 0x0053BF24

    // The background drawn each frame (0x0045B8D0): a quake shift, the grey band of the battle start, a flash of the
    // field, and the field darkened (by darkened copies in `surfaces` or by tint steps) while unit darkenUnit acts.
    i32 shake = 0;       // 0x0057702C the shift of the background this frame, cleared once drawn
    i32 quaking = 0;     // 0x0057730C 1: the screen quakes (read by 0x00453200, shaken by 0x0045E7D0)
    i32 quakePhase = 0;  // 0x00577030 the step of the quake in its offset table (0x004A75EC)
    i32 unk53BF94 = 0;   // 0x00577290 with unk53BF98 (0x00577294): the field is darkened
    i32 unk53BF98 = 0;
    i16 darkenUnit = -1;  // 0x004BF1D4 the unit darkening the field
    u16 unk53BF2C = 0;
    u16 grayBandState = 0;       // 0x00577214 (swd3e) 1 opens the grey band, 2 closes it (0x0045BEA0, inferred)
    u16 grayBand = 0;            // 0x00577216 the half height of the grey band around BandRow
    i32 grayBandShown = 0;       // 0x00577218
    i32 flashing = 0;            // 0x005772CC the field flashes, flashLevel falling by FlashStep a frame
    u16 sweptUnits = 1;          // 0x004BF1A0 the units a sweep has reached
    i8 flashLevel = FlashStart;  // 0x004BF1A2 tint steps of the flash
    u8 panelGlow = 1;            // 0x004BF1A3 the highlight of the current role's panel; bit 7: falling
    i32 darkenByTint = 0;        // 0x005777F4 the field is darkened by tint steps instead of the darkened copies
    i16 darkLevel = 0;           // 0x0057723A 0..MaxDarkLevel
    i16 darkRed = 0;             // 0x0057722C the tint steps of a darkened field per level, halved
    i16 darkGreen = 0;           // 0x0057722E
    i16 darkBlue = 0;            // 0x00577230
    i32 darkFrames = 0;          // 0x005369C4 frames counted to the next darker level
    i32 undarkening = 0;         // 0x005772EC the darkening is undone a level a frame
    // 0x005772C8 set by 0x0049B9D0 and 0x0049BE20, cleared by 0x0049C0D0: the undarkening waits while set
    i32 unk5772C8 = 0;
    std::unique_ptr<HitPicture> hitPictures;  // 0x00577818 (swd3e) the second-tier spell pictures (DrawHitPictures)
    u16 hitBits = 0;                          // 0x005778CC (swd3e) their target bits of the frame
    u16 experienceBonus = 0;  // 0x00577220 (swd3e) the percent the roulette adds to the battle's experience
    u16 moneyTimes = 0;       // 0x0057721E (swd3e) the times the roulette multiplies its money
    i32 menuSlide = 0;        // 0x00577A4C (swd3e) the menu panels slide in by it, halved each frame
    i32 enraging = 0;         // 0x00577310 (swd3e) 1 while an enemy enrages (Enrage sets it, 0x0049C0D0 clears it)
    i32 enrageDamage = 0;     // 0x005770C4 (swd3e) the damage of enrage 0x3F4 (1.4 times the maximum hit points)
    // 0x00536880 the ripple over the battle (a spell with ActFlag13 in rippleHits, the unit effect of 0x0049B9D0)
    std::unique_ptr<ScriptObject> ripple;
    // 0x005761C8 (swd3e) per role (the first three): 1 + the enemy whose hit holds ailment 0x400 on it (0x0045EE20);
    // cured when that enemy falls (CureHeldBy), all cleared when the ailment ends and by StartBattle
    std::array<u16, 3> ailment400From = {};
    i16 darkRedShown = 0;    // 0x00577232 the tint applied, a step a frame towards darkRed (or back to 0)
    i16 darkGreenShown = 0;  // 0x00577234
    i16 darkBlueShown = 0;   // 0x00577236
    i32 darkenedAt = -1;     // 0x004BF108 unk53BD0C when the darkened copies were made
    i32 unk53BD0C = 0;

    // The role panels (0x00459D10): the gauges move a step a frame towards their values and the numbers roll.
    i32 currentUnit = 0;                          // 0x00577024 the unit whose panel is highlighted
    i32 glowFrames = 0;                           // 0x00577034 frames to the next step of panelGlow
    i8 listGlow = 0;                              // swd32 0x0053BDF1 the glow of the last listed unit; bit 7: on
    std::array<i32, RoleUnits> nameFlashes = {};  // 0x00536A90
    std::array<u8, RoleUnits> panelGrown =
        {};  // 0x0050F410 (swd3e) per role: 1 once its name gauge grew in (DrawGrowing)
    std::array<i32, RoleUnits> hitPointsRolling = {};  // 0x0050EDB8 the change still rolling in the shown number
    std::array<i32, RoleUnits> hitPointSteps = {};     // 0x00536924
    std::array<i32, RoleUnits> spiritRolling = {};     // 0x00536B64
    std::array<i32, RoleUnits> spiritSteps = {};       // 0x00575EF0
    std::array<i32, RoleUnits> staminaRolling = {};    // 0x00536B08
    std::array<i32, RoleUnits> staminaSteps = {};      // 0x00537458
    i32 autoBattle = 0;                                // 0x0057731C
    i32 autoBoxX = 0;                                  // 0x005770AC the "自動攻擊" box slides in
    i32 autoBoxStep = 0;                               // 0x005770A8
    // 0x00520FB8 frames; levels 0x004FDFA4, 0x00521394, 0x00525468; targets 0x004FDF8C, 0x00521388, 0x00525448;
    // steps 0x0052151C, 0x00525430, 0x00520D58.
    ColorFade fieldFade;

    // The damage of the hit being dealt (0x0045BEA0, 0x0045D690).
    i32 hitDamage = 0;  // 0x005378F4
    // swd3e: the drains of a hit on many units summed (0x00577054, 0x00577058, 0x0057705C; ApplyDrain) and 1 while
    // the attacker's numbers of them wait (0x00577374, set by HitFrame)
    i32 drainedLife = 0;
    i32 drainedSpirit = 0;
    i32 drainedStamina = 0;
    i32 deferDrains = 0;
    i16 spiritDamage = 0;  // 0x005761CE
    // 0x004BF1DC (swd3e) the x of the middle of the field as it is pushed or slid (PushUnits, SlideUnits); the push
    // hitPush in the word after it
    u16 fieldX = Display::Width / 2;
    i16 staminaDamage = 0;     // 0x0050CE68
    i32 lastDamage = 0;        // 0x00575F34 the damage of the last hit with the proficiency bonus
    i16 proficiencyBonus = 0;  // 0x005373B4 the weapon proficiency step of the last strike

    // The state of the turns (0x0045BEA0, 0x00456680 and the commands).
    i32 unk53BFC0 = 0;
    i32 unk53BFC4 = 0;
    i32 unk53BF5C = 0;
    i32 turnUnit = 0;    // 0x00577020 the role unit taking the turn
    i32 endFade = 0;     // 0x005772E4 1: the field fades out once the battle is decided (CheckBattleEnd)
    u8 enemiesGone = 0;  // 0x005771E8 set to enemyCount by a retreat
    u8 unk53BF01 = 0;
    u8 unk53BF02 = 0;
    u8 unk53BF03 = 0;
    // The turns (0x00464CC0, 0x0045ADF0, 0x0045AA00).
    i32 turnCommand = 0;  // 0x00576FA8 the order of the turn under way
    i32 unk53BF60 = 0;
    i32 unk53BF64 = 0;
    i32 unk53BF74 = 0;
    // 0x00577314 set when a role turns to another enemy (RetargetRoles)
    i32 retargeted = 0;
    std::array<i32, Units> turnTaken = {};      // 0x00576024 per unit, set when it takes the turn
    std::array<i32, RoleUnits> roleOrder = {};  // 0x005368AC role units in order, a defeated one taken out
    i32 aimTarget = 1;                          // 0x004BF0DC the enemy a role turn aims at, 1-based
    u16 unk4A7626 = 0xFFFF;
    // The rewards of the defeated enemies (RemoveDefeated).
    u16 unk53BF12 = 0;
    u16 unk53BF14 = 0;
    u16 unk53BF16 = 0;
    const ObjectRecord* lastDefeatedObject = nullptr;  // 0x005135A0 the object of the last defeated enemy
    // The hit acts of the units (0x004582B0, 0x00458DE0, PlayHitAct, FadeHitAct).
    std::array<ActState, Units> hitActs;     // 0x005347F8 the hit act of each unit
    std::array<ActState, Units> followActs;  // swd32 0x004FE600 the follow act of each unit
    std::array<ActState, Units> fadeActs;    // the fading act of each unit (ending at 0x00538158)
    // 0x00538158 (swd3e) the step of the battle music volume: Config::musicVolume from BattleIntro, faded to 2 by
    // defeat pose 0x65
    i32 musicLevel = 0;  // 0x00538158 (swd3e) the battle music's volume while a boss sinks or an enemy casts (0..11)
    std::array<i8, Units> fadeActLevels = {};   // swd32 0x00521374 the fade of fadeActs
    std::array<u16, Units> walkLineCalls = {};  // 0x0050CED0 the step factor of WalkLine, doubling while it has none
    std::array<u16, Units> walkDelays = {};     // 0x005372FC (swd3e) the calls WalkLine has waited
    // 0x00575FB0 (swd3e) the mode of WalkLine: 1 rises to y 0x32 first, then 0x8000 (low bits 0: walking)
    std::array<u16, Units> walkModes = {};
    // The draw order around the acting unit (SortDrawOrderAround).
    u8 unk53C4C0 = 0;
    i32 unk53AE7C = 0;
    i32 unk53AE80 = 0;
    i32 orderSorted = 0;  // 0x00577344 set once the order is sorted around the acting unit
    // The push of a hit (Game::PushUnits).
    i16 hitPush = 0x1A4;       // 0x004BF1DE the push, halved each frame
    u16 pushFrames = 0;        // 0x005770CC frames since the push was halved
    i16 slideBackPicture = 0;  // 0x00575F22 the hit act picture after which the units slide back
    i32 quakeStepFrames = 0;   // 0x00577038 frames of a quake step
    i32 unk53AEDC = 0;         // the digits of the extra damage number (BattleUnit::DrawDamageNumbers)
    i32 unk53AEE0 = 0;
    i32 unk53AEE4 = 0;
    // The turns of the units and the lists (NextEnemyTurn, NextRoleTurn, ScrollListBack).
    i32 actingEnemy = 0;  // 0x00576FB4 the enemy taking the turn
    i32 orderStep = 1;    // 0x004BF0E0 1-based step through enemyOrder or RoleCycle
    i32 unk53BFB8 = 0;
    i32 damageShown = 0;  // 0x005772D8 set when a unit's damage numbers are done (ApplyHits)
    i32 unk53BD9C = 0;
    i32 pageListRow = 1;  // 0x004BF0E8 the row marked in the list of state 2 (1-based)
    // 0x004BF0EC the row marked in the lists of states 4 and 0x1B (1-based)
    i32 roleListRow = 1;
    i32 listScroll = 0;                       // swd32 0x0053BD04 the scroll of a list
    i32 roleListScroll = 0;                   // 0x00576FDC the scroll of the lists of states 4 and 0x1B
    i32 listRole = 2;                         // 0x004BF104 a party role
    u8 listEntries = 0;                       // 0x005770DC the entries of the list of state 2
    u16 roleListEntries = 0;                  // 0x005770DE the entries of the lists of states 4 and 0x1B
    i32 listCursor = 0;                       // 0x00576FE8 the list entry under the cursor
    i32 specialCursor = 0;                    // 0x00576FE4 the skill under the cursor of the 絕招 list
    u8 specialCount = 0;                      // 0x005770DD the skills of the 絕招 list
    i32 guardCursor = 0;                      // 0x00576FB8 the row under the cursor of the 佈置護駕 panel
    std::array<u16, LootSlots> lootIds = {};  // 0x0050EFD0 the objects of the loot list
    i32 accessoryGained = 0;                  // swd32 0x0053C4C4 set when an accessory gains proficiency at the victory
    i32 endFrames = 0;                        // 0x00576FF4 frames the end messages have waited
    RoleRecord levelUpBefore;                 // swd32 0x004FF108 the record of the role before its level-up
    i32 unk53C4C8 = 0;
    std::array<i16, 9> statRolls = {};  // swd32 0x005214A4..0x00525466 the rises of a level-up still to count
    // 0x005770CE the frames of the level-up stats (the high word of 0x0053BDE8)
    u16 levelUpFrames = 0;
    // The debug keys (Game::BattleDebugKeys) and the acting unit.
    i32 unk5244E0 = 0;   // 0x00537A18 (swd3e) the debug overlay (F1): DrawBattleDebug, the panels' second gauges
    i32 slowFrames = 0;  // 0x005777F8 (swd3e) the debug keys' F5: the frames are slowed to 100 ms
    i32 unk577360 = 0;   // 0x00577360 (swd3e) set when an enemy's step-in attack fells it (0x00487FE0); 0x0045EE20
    u8 itemStolen = 0;   // 0x00577255 (swd3e) 1 once an enemy stole (EnemyHitDamage, command flag 0x100000); 0x0045EE20
    i32 unk5772F8 = 0;  // 0x005772F8 (swd3e) set by 0x0045BEA0: the victory gives no loot, proficiency or battle counts
    i32 showRoleBlocks = 0;  // 0x00577808 (swd3e) the debug keys toggle it: the overlay shows the party battle blocks
    u8 unk5774E9 = 0;        // 0x005774E9 (swd3e) 0 or 1, set by FightTalkFrame (0x0047DF22, 0x0047DF46); debug overlay
    i32 unk53AF68 = 0;
    i32 hitsAllRoles = 0;  // 0x00577264 1: the hit falls on all roles at once
    i32 unk53BD88 = 0;
    i32 unk53BD80 = 0;
    i32 snapNext = 0;           // 0x005772E8 1: the next frame is saved as a screenshot
    std::u8string battleMusic;  // 0x005774EC the path of the battle music
    i32 unk53AF30 = 0;
    i32 unk53AF34 = 0;
    i32 sweepTotal = 0;  // swd32 0x0053AE74 the units a sweep is to hit
    u16 unk53AE78 = 0;
    i32 hitsAtOnce = 0;  // 0x00575F30 1: a sweep of the whole side hits it at once (HitFrame)
    i32 wholeSide = 0;   // 0x00575F2C 1: the hit or effect falls on a whole side (the effect messages name the side)
    u16 unk53BF4A = 0;   // a unit's place read for the debug gauge and the menu
    u16 unk53BF4E = 0;
    i32 hoverScrollPart = 0;    // 0x0057707C the part of the scroll bar under the pointer (1..4)
    i32 frameMilliseconds = 1;  // 0x004BF18C the milliseconds of a frame (the debug overlay shows 1000 / it)
    i32 frameNow = 0;           // 0x005770A0 (swd3e) the time of this frame (milliseconds)
    i32 frameLast = 0;          // 0x0057709C (swd3e) the time of the frame before
    u8 coinTossing = 0;         // 0x00577254 (swd3e) 1 while the coin toss runs (BattleFrame)
    // The pointer over the battle (Game::MouseHover).
    ClickRegion* hoverRegion = nullptr;  // 0x005379FC the click region under the pointer
    i32 menuX = 0;                       // 0x00576FF8 the place of the command menu
    i32 menuY = 0;                       // 0x00576FFC
    i32 hoverPageTab = -1;               // 0x004BF10C the page tab under the pointer (list of state 2)
    i32 hoverRoleTab = -1;               // 0x004BF110 the role tab under the pointer (list of state 4)
    i32 unk53BD6C = 0;
    i32 unk53BD70 = 0;
    i32 unk53BFF8 = 0;
    i32 hoverUnit = 0;  // 0x00577988 the unit aimed at under the pointer
    u16 unk53BF2A = 0;
    // The confirm key of the menu (Game::MenuConfirm).
    i32 unk53BDA4 = 0;
    i32 unk53BCDC = 0;
    i32 unk53C01C = 0;
    std::array<u16, Units> escortCalled = {};  // 0x00536EA8 per unit: the escort place it called
    u16 callingEscort = 0;                     // 0x0057720C the escort being called (its low word)
    i32 levelUpPending = 0;                    // 0x0057733C a level-up to show at the end
    u16 unk53BF1C = 0;
    i32 unk53C024 = 0;
    i32 victoryWithoutPanel = 0;  // swd32 0x0053C004 1: the victory ends without its panel (BattleEndFrame; inferred)
    // The hit act of a slot (Game::HitTargetFrame).
    i32 rippleHits = 0;                        // 0x00577340 1: hits run the ripple instead of their pictures
    std::array<i32, Units> rippleFrames = {};  // 0x00575FD4 per slot: the frames of the ripple
    std::array<u8, Units> hitWalked = {};   // 0x005379D4 (swd3e) per slot: 1 once the hit picture walked to its target
    std::array<ActState, Units> unk535B38;  // 0x00535B38 (swd3e) per slot: the follow act of the hit (only written)
    i32 hitDrawX = 0;     // 0x00537A04 (swd3e) where the hit picture was drawn last (its second-tier picture there)
    i32 hitDrawY = 0;     // 0x00537A08
    i32 unk577250 = 0;    // 0x00577250 (swd3e) cleared once a hit picture reaches its target (only written)
    i32 hitPictureX = 0;  // 0x005770BC the place the hit picture is drawn at
    i32 hitPictureY = 0;  // 0x005770C0
    std::array<i32, Units> lastSpiritDamage = {};   // 0x0053754C per slot: the spirit damage of the last hit
    std::array<i32, Units> lastStaminaDamage = {};  // 0x00537618 per slot: the stamina damage of the last hit
    std::array<i32, Units> hitFading = {};          // 0x00537484 per slot: 1 while the hit fades (FadeHitAct)
    i32 hitAllFrames = 0;  // 0x0050D868 the frames of a hit act on a whole side (Game::HitAllFrame)
    // The hits of the acting unit (Game::HitFrame).
    i32 hitLanded = 0;              // swd32 0x0053BFA8 1 once a hit landed
    i32 sweepHit = 0;               // swd32 0x0053BF7C 1 once a sweep hit a unit
    u16 sweepGap = 0;               // 0x005770D0 frames between the units a sweep reaches
    u16 unk4FDF7C = 0;              // the attacker's word 29A2 of the last hit
    u16 lastRoleHit = 0xFFFF;       // swd32 0x004A7620 the last role hit
    u16 lastEnemyHit = 0xFFFF;      // swd32 0x004A7622 the last enemy hit
    u16 lastRoleDefeated = 0xFFFF;  // 0x004BF1C8 the last role defeated by a hit
    i32 enemySweeps = 0;            // 0x00577280 1: an enemy's hit sweeps (only written in swd3e)
    i32 enemyHitsAll = 0;           // 0x005772F4 1: an enemy's hit falls on the party at once (only written in swd3e)
    i32 enemyHitAct = 0;            // swd32 0x0053BD40 the hit act of the acting enemy
    // 0x005135A4 (swd3e) per enemy: the hit act of its command (EnemyFrame); its spell runs it once to time the hits
    std::array<i32, Enemies> enemyHitActs = {};
    std::array<u8, 20> enemyCommandTurns = {};  // 0x00537690 (swd3e) per role: the turns of the casting enemy's command
    u8 unk575FD0 = 0;  // 0x00575FD0 (swd3e) half the round limit of the spell's hit act (only written)
    std::array<u8, Enemies> escapedEnemies = {};  // 0x005379C0 (swd3e) per enemy: 1 once it ran away (0x00464ED0)
    // 0x0050F41C (swd3e) per enemy: the items it stole (0x20 a enemy, the first free one taken; 0x00464ED0)
    std::array<std::array<u16, 0x20>, Enemies> stolenItems = {};
    i32 unk577370 = 0;  // 0x00577370 (swd3e) set when battle role 0 falls with the ghost in the battle
    std::array<i32, Enemies> unk50CE6C = {};  // 0x0050CE6C (swd3e) per enemy, set by EnemyFrame, cleared once it falls
    // 0x00576DE8 (swd3e) by battle number - 1: the times the battle was fought while a battle talk limits it
    // (FightTalk::repeatLimit, BattleIntro); past the limit its enemies give half their first two rewards
    std::array<u8, 0x1B4> battleRepeats = {};
    std::array<char, 0x40> noticeText = {};  // 0x00577494 (swd3e) the shared text of the notices (a stolen item)
    i32 slideUnit = 0;      // 0x00577800 (swd3e) the enemy attacking last: SlideUnits slides by it (never enabled)
    i16 slideX = 0x140;     // 0x004BF1E0 (swd3e) the x the units slide to
    i32 sweepTargets = 0;   // 0x0057704C the units an enemy's sweep hits
    i32 sweepDefeated = 0;  // swd32 0x0053BD84 the units an enemy's sweep defeated
    i32 enemyFaded = 0;     // 0x005772E0 1 once an enemy faded or left (Game::EnemyActionFrame)
    // The turn of an enemy (Game::EnemyFrame).
    std::array<u16, Enemies> enemyOrders = {};  // swd32 0x005028AC the order of the fight talk for each enemy
    i32 enemyGaugeBoost = 0;                    // 0x00577348 the boost of the enemies' gauges
    i32 heldEnemy = 0;                          // swd32 0x0053BD58 the enemy (1-based) whose turn ended held
    i32 gatheringSweep = 0;                     // swd32 0x0053BF8C 1 while an enemy gathers the targets of a sweep
    i32 enemyTargetPicked = 0;                  // 0x00577318 1 once the acting enemy picked its target
    i32 enemyTarget = 0;                        // 0x00576FC0 the target of the acting enemy
    i32 enemyAimsAtEnemies = 0;                 // swd32 0x0053BFD4 1: an ordered enemy aims at the enemies
    i32 unk53BF9C = 0;
    i32 enemiesHitAct = 0;  // swd32 0x0053BDBC the hit act of the enemies (unk53AE90)
    i32 unk53BFDC = 0;
    // The commands of a role (Game::RoleActionFrame).
    // 0x00577324 1: the taking (收化) succeeded; the enemy taken then gives no first reward and keeps its stolen
    // items (RemoveDefeated)
    i32 absorbSucceeded = 0;
    u16 usedUpItem = 0;  // 0x0057721C the item used up by the last command
    i32 unk53BDD4 = 0;
    i32 unk53BDD8 = 0;
    SkillScript accessoryEffect;  // swd32 0x004FF140 the buff or ailment of an accessory command
    // The frame of a role (Game::RoleFrame).
    i32 aiTarget = 1;  // 0x004BF0E4 the enemy (1-based) an AI role aims at
    u16 unk5240C4 = 0;
    // The frame of the battle (Game::BattleFrame).
    u16 slotRecordFrames = 0;  // swd32 0x0053BF1A frames since the last slot record was taken
    u16 snapNumber = 0;        // 0x005777EE the number of the last snapshot
    // Cleared by Game::EndBattle (no other use found).
    std::array<i32, 8> unk524200 = {};
    std::array<i32, 8> unk52100C = {};
    std::array<i32, 0x90> unk4FF318 = {};
    std::array<i32, 0xA> unk4FF588 = {};
    i32 unk53AED8 = 0;
    i32 unk4A7580 = 0x60;  // the opening places of unk4A7580..unk4A7594 (exe data)
    i32 unk4A7584 = 0x180;
    i32 unk4A7588 = 0xF0;
    i32 unk4A758C = 0xFA;
    i32 unk4A7590 = 0xF2;
    i32 unk4A7594 = 0x64;
    i32 unk53BD4C = 0;
    i32 unk53BD74 = 0;
    i32 unk53BD78 = 0;
    i32 unk520FBC = 0;
    i32 unk520FC0 = 0;
    u16 unk520FC4 = 0;
    i32 unk53BF04 = 0;
    i32 unk53BF08 = 0;
    u16 unk5242F8 = 0;
    u16 unk4A7628 = 6;  // passed to DrawRolePanels, which takes no argument
    u8 unk53BDEE = 0;
    i32 unk53BF6C = 0;
    i32 unk53BF70 = 0;
    i32 unk53BF78 = 0;
    i32 unk53BF90 = 0;
    i32 unk53BFA0 = 0;
    i32 unk53BFA4 = 0;
    i32 unk53BFAC = 0;
    i32 unk53BFB4 = 0;
    i32 unk4A763C = 1;
    i32 unk53C028 = 0;
    i32 autoTarget = -1;  // 0x004BF0F4 the enemy the auto battle aims at (1-based), -1: none
    // 0x004BF1EC a role unit that may take the panel in any battle state (SelectRoleUnit)
    u16 anyStateRole = 0xFFFF;
    i32 menuCommand = 1;  // 0x004BF0D8 the command chosen in the menu (1-based)
    i32 listPage = 0;     // 0x00576FEC the page of the list of state 2 (0..2)
    // The menu (MenuCancel).
    i32 unk53BFB0 = 0;
    // The rebuild (deviations.md): the unit the keys went to last frame; a new one spends the keys held.
    i32 keysUnit = 0;
    i32 listPanelFrames = 0;               // 0x0057706C the frames of the list panel act
    i32 menuGrow = 0;                      // 0x00577070 how far the command menu has opened
    i32 listRows = 0;                      // 0x00577074 the rows of the list open
    std::array<char, 0x14> listName = {};  // 0x005774D4 a name shown by the lists
    i32 unk53C49C = 0;
    i32 unk53BFC8 = 0;
    u16 controlledEnemy = 0xFFFF;  // swd32 0x004A762C the enemy under control (0xFFFF: none)
    u16 talkRole = 0xFFFF;         // 0x004BF1D2 the role with a battle talk (0xFFFF: none)
    i32 orderedSkill = 0;          // 0x00577014 the skill a controlled enemy is ordered to use
    i32 unk53BD48 = 0;
    i32 unk53C02C = 0;
    i32 unk53BF88 = 0;
    i32 unk53BFF0 = 0;
    i32 listReturnCommand = 0;  // swd32 0x0053BCF0 the menu command a list returns to
    i32 specialRow = 1;         // 0x004BF0F0 the row of the 絕招 list (1-based)
    i32 controlRows = 2;        // 0x004BF0FC the rows of the 控制 menu
    i32 controlCursor = 1;      // 0x004BF100 its row under the cursor (1-based)
    // The proficiency filled in the battle (AnnounceMastery).
    ObjectRecord lastObject;   // 0x00576D40 the object read last (stolen, or its proficiency filled)
    LearnedSkill lastLearned;  // 0x0057747C the name of the accessory or of the skill learned, for the messages
    std::string evolvedName;   // 0x00577A54 (swd3e, 0x18 bytes) the object a mastered accessory turned into
    i32 learnedPending = 0;    // 0x00577300 1: a proficiency filled or a special was learned, its message to show
    u8 levelUpRole = 0xFF;     // 0x004BF1EE the role
    // The end of the battle (Game::CheckBattleEnd, Game::DrawBattlePrompt).
    i32 blackFade = 0;    // 0x00577814 the fade of the field to black, 2 darker a frame
    i32 frameResult = 0;  // 0x005770A4 the result of the battle frame (1 runs on; the end CheckBattleEnd set)
    std::array<u16, 3> givenBack = {};  // 0x0050EFCA [1], [2]: objects given back at the end (set by 0x0045EC60)
    i32 promptFrames = 0;               // 0x00577084 frames of the prompt
    i32 unk53BDAC = 0;
    i32 unk53BFBC = 0;
    i32 turnPointer = 1;  // 0x004BF1E8 1: the pointer shows the turn mark
    i32 unk53BF58 = 0;
    i16 unk53BF50 = 0;
    i16 unk53BF52 = 0;
};

}  // namespace swd3e
