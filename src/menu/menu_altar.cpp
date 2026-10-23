#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <string>

#include "core/error_box.h"
#include "core/format.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game/databases.h"
#include "game/game.h"
#include "menu/menu.h"
#include "menu/menu_list.h"
#include "org/database.h"
#include "org/fame.h"
#include "pic/rle.h"
#include "platform/display.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

// AltarPage::stage.
constexpr i32 kChoosing = 1;
constexpr i32 kOffered = 2;  // the result shown, to be made
constexpr i32 kMorphing = 3;
constexpr i32 kMade = 4;
constexpr i32 kNeedTwo = 5;         // "請將兩個欄位選擇物品"
constexpr i32 kOfferedAgain = 0xA;  // drawn and answered as kOffered (nothing sets it)

constexpr int kAltarOpenFlag = 0x49;     // the altar works ("目前無法使用" otherwise)
constexpr int kSlotAArrowFlag = 0x1BA9;  // swd32's east altar flag: slot A's pass also draws the result's arrow
constexpr int kLastTab = 2;
constexpr int kTooHighLevels = 6;  // a result this many levels above the first party role cannot be made
constexpr u32 kTooHigh = 3;        // AltarPage::tooHigh
constexpr u32 kTooHighShown = 1;

// The result (0x00444630).
constexpr u16 kDefaultResult = 0x12A;      // without a recipe or an object of the category
constexpr std::size_t kMatrixSide = 0x10;  // maps.dat table 0x18: the result category by the two categories
constexpr u16 kFoldedCategory = 0x15;      // looked up as category 0xF
constexpr u16 kFoldedTo = 0xF;
constexpr std::size_t kRecipesAt = 0x200;  // then three words a recipe (two objects and the result), ended by 0xFFFF
constexpr u16 kRecipesEnd = 0xFFFF;
constexpr std::size_t kRecipeWords = 3;
constexpr int kLevelBonus = 3;  // the result aims three levels above the average
constexpr i32 kNoBest = 0x7FFFFFFF;
constexpr int kFirstResult = 0x65;
constexpr int kLastMonster = 0x1F4;
constexpr int kFirstLateObject = 0xBB9;  // the objects 0x1F5..0xBB8 are not loaded
constexpr u32 kKeptKind = 0x800;         // AltarPage::kinds: a monster of 0x65..0x1F4 without it is never a result
constexpr i32 kNeverResult = 1;          // AltarPage::altars

// The tabs (0x00444950).
constexpr u16 kLastTabCategory = 0xE;  // categories 0..0xE and 0x15 are listed
constexpr u16 kExtraTabCategory = 0x15;
constexpr u32 kKindMask = 0x0FFF7FFF;
constexpr u32 kFirstTabKinds = 0x101F;  // tested on the whole kind
constexpr u32 kSecondTabKinds = 0x700;
constexpr u32 kThirdTabKinds = 0x800;

constexpr i32 kPageBackground = 0x3B;  // Menu::background actions
constexpr i32 kOfferBackground = 0x39;
constexpr i32 kMorphBackground = 0x46;
constexpr i32 kLeaveBackground = 0x37;
constexpr i32 kFaceAct = 0x232A;
constexpr i32 kFaceLeaveAction = 3;
constexpr i32 kAltarAct = 0x232C;
constexpr i32 kPictureAction = 0x3B;
constexpr u16 kOpenSound = 0x2BF;
constexpr u16 kMoveSound = 0x2E;
constexpr u16 kSwitchSound = 0x107;
constexpr u16 kRefusedSound = 0x8C;
constexpr u16 kMorphStartSound = 0x2BD;
constexpr u16 kDoneSound = 0x208;

// The morph (AltarMorph): the counter runs from -40.
constexpr i32 kMorphStart = -0x28;
constexpr i32 kMorphShown = -0x23;  // from here the pictures morph
constexpr i32 kTintEnd = -0x1E;
constexpr i32 kSkipTo = 0x23;
constexpr i32 kSettleStart = 0x78;
constexpr u32 kMorphLast = 0x8C;
constexpr i32 kMorphDone = 0xC8;
constexpr i32 kMixStart = 0x32;
constexpr i32 kMixEnd = 0x6E;
constexpr i32 kMixPerFrame = 8;
constexpr i32 kMixBase = 0x190;
constexpr u32 kMixPairs = 0x3354;
constexpr i32 kDrawRange = 0xB4;
constexpr i32 kDrawLast = 0x28;
constexpr i32 kStepSpan = 0x28;
constexpr std::array<int, 3> kSteps = {3, 5, 7};
constexpr int kSettle = 9;  // the offsets shrink to nine tenths a frame
constexpr int kTenths = 0xA;
constexpr int kPictureTop = 0xB4;
constexpr int kLeftPictureX = 0x8C;
constexpr int kRightPictureX = 0x17C;
constexpr int kTintStep = 6;
constexpr int kTintBase = 0x5A;
constexpr int kWaveRows = 0x7F;  // AltarPage::wave: offsets of -126..126 rows are set

constexpr u16 kRolePagesCode = 2;  // Menu::code on a role page
constexpr u16 kScrollUp = 3;       // AltarPage::scrollArrows
constexpr u16 kScrollDown = 0x30;
constexpr u16 kScrollUpMask = 0xF;
constexpr u16 kScrollDownMask = 0xF0;
constexpr int kNibbleShift = 4;
constexpr int kSpiritItemA = 2;  // AltarResultVitals: the level times these
constexpr int kSpiritItemB = 3;
constexpr int kSpiritItemC = 5;
constexpr int kSpiritHealA = 2;
constexpr int kSpiritHealB = 4;
constexpr int kStaminaSkillA = 3;
constexpr int kStaminaSkillB = 5;

// The clicks (Menu::clicks).
constexpr u32 kLeftClicks = 3;
constexpr u32 kRightClicks = 0xC;
constexpr u32 kAnyClick = 0xF;

// The mouse areas.
constexpr u32 kTabsTop = 0x32;
constexpr u32 kTabsBottom = 0x49;
constexpr u32 kTabsLeft = 8;
constexpr u32 kTabsRight = 0x74;
constexpr u32 kTabWidth = 0x24;
constexpr u32 kRowsTop = 0x50;
constexpr u32 kRowsBottom = 0x1D0;
constexpr u32 kRowsLeft = 2;
constexpr u32 kRowsRight = 0xBC;
constexpr u32 kRowHeight = 0x18;
constexpr u32 kSlotsTop = 0x46;
constexpr u32 kSlotsBottom = 0x151;
constexpr u32 kFirstSlotLeft = 0xFE;
constexpr u32 kFirstSlotRight = 0x189;
constexpr u32 kSecondSlotLeft = 0x1D1;
constexpr u32 kSecondSlotRight = 0x25B;
constexpr u32 kRunTop = 0x151;
constexpr u32 kRunBottom = 0x166;
constexpr u32 kRunLeft = 0x19C;
constexpr u32 kRunRight = 0x1C4;
constexpr u32 kBarLeft = 0xC6;
constexpr u32 kBarRight = 0xD6;
constexpr i32 kBarUpTop = 0x4C;
constexpr i32 kBarUpBottom = 0x5A;
constexpr i32 kBarDownTop = 0x1C6;
constexpr i32 kBarDownBottom = 0x1D4;

// The texts.
constexpr std::u8string_view kClosed = u8"目前無法使用";              // 0x004BE994
constexpr std::u8string_view kChooseTwo = u8"請將兩個欄位選擇物品";   // 0x004BE954
constexpr std::u8string_view kTooHighText = u8"等級不足 無法訂契約";  // 0x004BE9A4
constexpr std::u8string_view kCannotMake = u8"無法練化";              // 0x004BE96C
constexpr std::u8string_view kSlotFormat = u8"%-8s  %2d級";           // 0x004BE97C
constexpr std::u8string_view kResultLevelFormat = u8"%d 級";          // 0x004BEA00
constexpr std::u8string_view kHitPointsFormat = u8" 生命  %4d";       // 0x004BE9F4
constexpr std::u8string_view kSpiritFormat = u8" 靈力  %4d";          // 0x004BE9E8
constexpr std::u8string_view kStaminaFormat = u8" 體力  %4d";         // 0x004BE9DC
constexpr std::u8string_view kAttackFormat = u8" 攻擊  %4d";          // 0x004BE9D0
constexpr std::u8string_view kDefenseFormat = u8" 防禦  %4d";         // 0x004BE9C4
constexpr std::u8string_view kAgilityFormat = u8" 敏捷  %4d";         // 0x004BE9B8
constexpr std::string_view kRowFormat = "%-12s%3d";
constexpr std::string_view kNameFormat = "%-12s";

// The layout.
constexpr u32 kTextRed = 0x19;
constexpr u32 kTextGreen = 0x17;
constexpr u32 kTextBlue = 0x11;
constexpr u32 kGoldRed = 0x15;
constexpr u32 kGoldGreen = 0xF;
constexpr u32 kGoldBlue = 8;
constexpr u32 kDimRed = 0xA;
constexpr u32 kDimGreen = 7;
constexpr u32 kDimBlue = 4;
constexpr u32 kWarnRed = 0x18;
constexpr u32 kWarnGreen = 0xA;
constexpr u32 kWarnBlue = 0xB;
constexpr int kPanelStyle = 4;
constexpr int kFrameBorder = static_cast<int>(0x80000008);
constexpr int kClosedX = 0x118;
constexpr int kClosedY = 0xDC;
constexpr int kClosedCharWidth = 0xB;
constexpr int kClosedHeight = 0x16;
constexpr int kClosedBoxMode = 2;
constexpr int kAltarBoxMode = 4;
constexpr int kChosenBoxMode = 2;
constexpr i32 kRunLitAction = 0xF;
constexpr int kRunLitX = 0x198;
constexpr int kRunLitY = 0x150;
constexpr int kRunLitRight = 0x280;
constexpr int kRunLitBottom = 0x164;
constexpr i32 kFirstTabAction = 0x14;
constexpr int kTabStepX = 0x1F;
constexpr int kTabX = 8;
constexpr int kTabY = 0x34;
constexpr u16 kMarkerFile = 0x235B;  // the slot marker, bobbing over five steps of two frames
constexpr int kMarkerSteps = 0xA;
constexpr int kMarkerStepX = 0xD2;
constexpr int kMarkerX = 0x134;
constexpr int kMarkerY = 0x38;
constexpr int kBarX = 0xC5;
constexpr int kBarY = 0x5C;
constexpr int kBarHeight = 0x168;
constexpr int kListTop = 0x4F;
constexpr int kListEnd = 0x1CF;
constexpr int kListX = 0x10;
constexpr int kHighlightX = 4;
constexpr int kHighlightWidth = 0xBE;
constexpr int kHighlightHeight = 0x18;
constexpr int kHighlightRed = 0x14;
constexpr int kHighlightGreen = 0xD;
constexpr int kSlotPictureStep = 0xD4;
constexpr int kSlotPictureX = 0x108;
constexpr int kSlotPictureY = 0x5E;
constexpr int kSlotBoxStep = 0xEC;
constexpr int kSlotBoxX = 0xF8;
constexpr int kSlotBoxY = 0x164;
constexpr int kSlotBoxWidth = 0x7E;
constexpr int kSlotBoxHeight = 0x24;
constexpr int kSlotNameY = 0x176;
constexpr int kArrowX = 0x1C5;
constexpr int kArrowY = 0x1A9;
constexpr u16 kUpArrow = 0x235E;  // the result's level at least the average
constexpr u16 kDownArrow = 0x235F;
constexpr int kResultX = 0xAF;
constexpr int kResultY = 0x28;
constexpr u32 kChosenResult = 1;  // AltarDrawResult flags
constexpr u32 kTooHighFlag = 0x1000;
constexpr int kResultBoxWidth = 0x118;
constexpr int kResultBoxHeight = 0xFA;
constexpr int kResultPictureX = 0xC;
constexpr int kResultPictureY = 0x10;
constexpr int kShadeX = 0x10;
constexpr int kShadeY = 0x18;
constexpr int kShadeWidth = 0x78;
constexpr int kShadeHeight = 0xDC;
constexpr int kResultValuesX = 0x92;
constexpr int kResultValuesY = 0xA;
constexpr int kResultLine = 0x19;
constexpr int kTooHighX = 0x40;
constexpr int kTooHighY = 0x112;
constexpr int kDescriptionBoxX = 0x7E;
constexpr int kDescriptionBoxY = 0x138;
constexpr int kDescriptionBoxWidth = 0x184;
constexpr int kDescriptionBoxHeight = 0x9D;
constexpr int kDescriptionLeft = 0x82;
constexpr int kDescriptionTop = 0x140;
constexpr int kDescriptionRight = 0x1FA;
constexpr int kDescriptionBottom = 0x1CD;
constexpr int kDescriptionLines = 5;
constexpr int kDescriptionWidth = 0x168;
constexpr int kCannotMakeBoxX = 0xDC;
constexpr int kCannotMakeX = 0xE5;
constexpr int kCannotMakeY = 0xFA;
constexpr int kCannotMakeWidth = 0x5A;
constexpr int kCannotMakeHeight = 0x12;
constexpr int kMadeX = 0x104;
constexpr int kMadeY = 0xB4;
constexpr int kNeedTwoCharWidth = 0xC;
constexpr int kNeedTwoY = 0xE4;
constexpr int kNeedTwoHeight = 0x18;

// The word at byte `at` of maps.dat table 0x18 (Scene::recipesTable, set by the new game)
u16 RecipeWord(const std::vector<u8>& maps, u32 table, std::size_t at)
{
    return MapsWord(maps, table + at);
}

}  // namespace

// swd3e.exe 0x00442E10: the altar page opened: the category, level, kind and altar of the objects 0..0x1F4 and
// 0xBB9..0xD47 (-1 for the others); the result emptied; the inventory's counts free to use; the objects of the first
// tab moved over, both slots empty; the morph's wave table made from the row table
void Menu::AltarEnter()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    altar.categories.assign(AltarPage::Objects, -1);
    altar.levels.assign(AltarPage::Objects, -1);
    altar.kinds.assign(AltarPage::Objects, ~0u);
    altar.altars.assign(AltarPage::Objects, -1);

    for (int id = 0; id < AltarPage::Objects; ++id)
    {
        if (id > kLastMonster && id < kFirstLateObject)
        {
            continue;
        }

        ObjectRecord record;

        if (owner.database.LoadObject(record, static_cast<u16>(id)))
        {
            const auto at = static_cast<std::size_t>(id);
            altar.categories[at] = record.data.category;
            altar.levels[at] = record.data.level;
            altar.kinds[at] = record.data.kind;
            altar.altars[at] = static_cast<i8>(record.data.altar);
        }
    }

    altar.result = ItemNode{};

    for (ItemNode* node = owner.party.inventory.get(); node != nullptr; node = node->next.get())
    {
        node->unk06 = static_cast<i16>(node->count + node->pending);
    }

    this->background.id = kFaceAct;
    this->background.action = kPageBackground;
    ActState& panel = owner.battleMessages.panelAct;
    panel.id = BattleMessages::MenuPanelAct;
    panel.action = 0;
    altar.top = 0;
    altar.row = 0;
    altar.stage = kChoosing;
    altar.slot = 0;
    altar.tab = 0;
    this->AltarFillTab();
    altar.firstShown = altar.objects.get();
    altar.count = CountItems(altar.objects);
    CountShown(altar.firstShown, altar.shown, AltarPage::Rows);
    altar.slotA.id = ItemNode::Empty;
    altar.slotB.id = ItemNode::Empty;
    owner.audio.PlaySound(kOpenSound, owner.config.soundVolume);
    altar.columnsA.assign(AltarPage::MorphColumns, 0);
    altar.columnsB.assign(AltarPage::MorphColumns, 0);
    altar.rowsA.assign(AltarPage::MorphRows, 0);
    altar.rowsB.assign(AltarPage::MorphRows, 0);

    // The row table's first 127 entries, halved, upwards from the centre and negated downwards (the centre takes the
    // negated one); the three entries at the ends are left unset by the original (0 here).
    altar.wave.assign(AltarPage::WaveSize, 0);
    const i32 pitch = owner.painter.ScreenPitch();

    for (int rows = 0; rows < kWaveRows; ++rows)
    {
        const i32 offset = rows * pitch * static_cast<i32>(sizeof(u16));
        altar.wave[AltarPage::WaveCenter + static_cast<std::size_t>(rows)] = offset / 2;
        altar.wave[AltarPage::WaveCenter - static_cast<std::size_t>(rows)] = offset / -2;
    }

    altar.phase = 0;
    altar.scrollArrows = 0;
    this->code = kRolePagesCode;
}

// swd3e.exe 0x00443150: the altar page left: the backgrounds, the moved objects back in the inventory, the slots and
// the result emptied, the tables freed, the main tabs back
void Menu::AltarLeave()
{
    AltarPage& altar = this->altar;
    this->background.action = kLeaveBackground;
    this->background.id = kFaceAct;
    this->face.id = kFaceAct;
    this->face.action = kFaceLeaveAction;
    this->AltarReturnObjects();
    altar.slotA = ItemNode{};
    altar.slotB = ItemNode{};
    altar.result = ItemNode{};
    FreeItemList(altar.objects);
    altar.categories.clear();
    altar.levels.clear();
    altar.kinds.clear();
    altar.altars.clear();
    altar.columnsA.clear();
    altar.columnsB.clear();
    altar.rowsA.clear();
    altar.rowsB.clear();
    altar.wave.clear();
    this->code = Menu::MainCode;
}

// swd3e.exe 0x00444450: the objects of the tab moved over from the inventory (the page's empty entry when none)
void Menu::AltarFillTab()
{
    AltarPage& altar = this->altar;
    this->AltarReturnObjects();
    this->AltarTakeObjects(this->game.party.inventory, altar.tab);

    if (altar.objects == nullptr)
    {
        altar.objects = NewEmptyItem();
    }

    altar.count = CountItems(altar.objects);
    altar.top = 0;
    altar.row = 0;
    altar.firstShown = altar.objects.get();
    // The original counts from the list's head itself, one more than its objects.
    altar.shown = std::min(altar.count + 1, AltarPage::Rows);
}

// swd3e.exe 0x004444D0: the page's objects back at the front of the inventory one by one (their order turns around);
// the empty entry is freed
void Menu::AltarReturnObjects()
{
    AltarPage& altar = this->altar;
    ItemLink& inventory = this->game.party.inventory;

    while (altar.objects != nullptr)
    {
        std::unique_ptr<ItemNode> node = std::move(altar.objects);
        altar.objects = std::move(node->next);

        if (node->id != ItemNode::Empty)
        {
            node->next = std::move(inventory);
            inventory = std::move(node);
        }
    }
}

// swd3e.exe 0x00444520: the objects of `source` the tab takes moved into the page's list in the order of their ids
void Menu::AltarTakeObjects(ItemLink& source, int tab)
{
    AltarPage& altar = this->altar;
    altar.objects.reset();
    ItemLink* link = &source;

    while (*link != nullptr)
    {
        if (!AltarTabTakes(**link, tab))
        {
            link = &(*link)->next;
            continue;
        }

        std::unique_ptr<ItemNode> node = std::move(*link);
        *link = std::move(node->next);
        InsertItemById(altar.objects, std::move(node));
    }
}

// swd3e.exe 0x004445B0: the page's list sorted again by id
void Menu::AltarSortObjects()
{
    AltarPage& altar = this->altar;
    ItemLink sorted;

    while (altar.objects != nullptr)
    {
        std::unique_ptr<ItemNode> node = std::move(altar.objects);
        altar.objects = std::move(node->next);
        InsertItemById(sorted, std::move(node));
    }

    altar.objects = std::move(sorted);
}

// swd3e.exe 0x00444950: whether tab `tab` lists the object: categories 0..0xE and 0x15 only; tab 0 an object of any of
// kinds 0x101F, tab 1 of 0x700, tab 2 of 0x800 (bits 15 and 28..31 ignored for the last two)
bool AltarTabTakes(const ItemNode& node, int tab)
{
    const u16 category = node.record.data.category;
    const u32 kind = node.record.data.kind;

    if (category > kLastTabCategory && category != kExtraTabCategory)
    {
        return false;
    }

    switch (tab)
    {
        case 0:
            return (kind & kFirstTabKinds) != 0;

        case 1:
            return (kind & kKindMask & kSecondTabKinds) != 0;

        case 2:
            return (kind & kKindMask & kThirdTabKinds) != 0;

        default:
            return false;
    }
}

// swd3e.exe 0x004449B0: on the list, the object of the row chosen put into the slot chosen; the list counted, sorted
// and fitted again
void Menu::AltarPick()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        return;
    }

    if (altar.slot == 0)
    {
        this->AltarPut(altar.slotA, altar.top + altar.row);
    }
    else if (altar.slot == 1)
    {
        this->AltarPut(altar.slotB, altar.top + altar.row);
    }

    if (altar.objects == nullptr)
    {
        altar.objects = NewEmptyItem();
    }

    altar.count = CountItems(altar.objects);
    this->AltarSortObjects();
    SkipItems(altar.top, altar.objects, altar.firstShown);
    CountShown(altar.firstShown, altar.shown, AltarPage::Rows);
    FitListRow(altar.count, altar.top, altar.row, altar.shown);
}

// swd3e.exe 0x00444A70: the object of list place `index` put into `slot` (one of it taken; with none left the slot
// empties); the object the slot held goes back to the list or the inventory (the original reads through a null node
// when it is in neither, or when the place is past the list)
void Menu::AltarPut(ItemNode& slot, int index)
{
    AltarPage& altar = this->altar;
    ItemNode* chosen = ItemAfter(index, altar.objects);
    const bool listed = AltarTabTakes(slot, altar.tab);
    const u16 previous = slot.id;

    if (chosen != nullptr && chosen->unk06 != 0)
    {
        slot.id = chosen->id;
        slot.record = chosen->record;
        slot.count = 0;
        slot.pending = 0;
        slot.unk06 = 1;
        --chosen->unk06;
    }
    else
    {
        slot.record.description.reset();
        slot.id = ItemNode::Empty;
        slot.count = 0;
        slot.pending = 0;
        slot.unk06 = 1;
    }

    if (previous == ItemNode::Empty)
    {
        return;
    }

    ItemNode* back = FindItemOfKind(listed ? altar.objects : this->game.party.inventory, static_cast<i16>(previous));

    if (back != nullptr)
    {
        ++back->unk06;
    }
}

// swd3e.exe 0x00444630: the result of the two slots (none unless both hold an object left): a recipe of maps.dat
// table 0x18 that names both; otherwise among the objects 0x65..0x1F4 and 0xBB9..0xD47 of the category the table's
// matrix gives for (first, second; category 0x15 as 0xF), the one nearest the level aimed at (the average plus three):
// at it or above when the half sum of the categories is even, at it or below when odd, the first of the nearest; a
// monster of 0x65..0x1F4 needs kind 0x800 and an object of altar 1 never results; 0x12A when none does. The result is
// loaded from mon.dat.
bool Menu::AltarCombine(const ItemNode& first, const ItemNode& second)
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    const std::vector<u8>& maps = owner.databases.mapsData;
    const u32 table = owner.scene.recipesTable;
    ItemNode& result = altar.result;
    result = ItemNode{};
    result.id = ItemNode::Empty;

    if (first.id == ItemNode::Empty || first.unk06 == 0 || second.id == ItemNode::Empty || second.unk06 == 0)
    {
        return false;
    }

    result.id = kDefaultResult;
    result.count = 0;
    result.pending = 0;
    result.unk06 = 1;

    for (std::size_t at = kRecipesAt; RecipeWord(maps, table, at) != kRecipesEnd; at += kRecipeWords * sizeof(u16))
    {
        const u16 one = RecipeWord(maps, table, at);
        const u16 other = RecipeWord(maps, table, at + sizeof(u16));

        if ((one == first.id && other == second.id) || (one == second.id && other == first.id))
        {
            result.id = RecipeWord(maps, table, at + 2 * sizeof(u16));
            result.count = 0;
            result.pending = 0;
            result.unk06 = 1;
            owner.database.LoadObject(result.record, result.id);
            return true;
        }
    }

    const u16 firstCategory = first.record.data.category;
    const u16 secondCategory = second.record.data.category;
    const std::size_t row = firstCategory == kFoldedCategory ? kFoldedTo : firstCategory;
    const std::size_t column = secondCategory == kFoldedCategory ? kFoldedTo : secondCategory;
    const auto category = static_cast<i32>(RecipeWord(maps, table, sizeof(u16) * (column + kMatrixSide * row)));
    const int aim = (second.record.data.level + first.record.data.level) / 2 + kLevelBonus;
    const bool below = (secondCategory + firstCategory) / 2 % 2 != 0;
    int best = 0;
    i32 bestGap = kNoBest;

    for (int id = kFirstResult; id < AltarPage::Objects; ++id)
    {
        const auto at = static_cast<std::size_t>(id);

        if ((id > kLastMonster && id < kFirstLateObject) || altar.categories[at] != category)
        {
            continue;
        }

        const i32 gap = altar.levels[at] - aim;
        bool nearer = false;

        if (below)
        {
            nearer = gap <= 0 && -gap < bestGap;
        }
        else
        {
            nearer = gap >= 0 && gap < bestGap;
        }

        if (id >= kFirstResult && id <= kLastMonster && (altar.kinds[at] & kKeptKind) == 0)
        {
            nearer = false;
        }

        if (altar.altars[at] == kNeverResult || !nearer)
        {
            continue;
        }

        best = id;
        bestGap = below ? -gap : gap;
    }

    result.id = kDefaultResult;
    result.count = 0;
    result.pending = 0;
    result.unk06 = 1;

    if (best != 0)
    {
        result.id = static_cast<u16>(best);
    }

    owner.database.LoadObject(result.record, result.id);
    return true;
}

// The list moved (inline in the altar page's keys): the first row shown, the object put into the slot, the result made
// again
void Menu::AltarMoved(u16 arrows)
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    SkipItems(altar.top, altar.objects, altar.firstShown);
    CountShown(altar.firstShown, altar.shown, AltarPage::Rows);
    this->AltarPick();
    this->AltarCombine(altar.slotA, altar.slotB);
    altar.scrollArrows = static_cast<u16>(altar.scrollArrows | arrows);
    owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
}

// The morph skipped to its end (the keys during it, inline in the altar page's keys)
void Menu::AltarSkipMorph()
{
    AltarPage& altar = this->altar;

    if (altar.stage == kMorphing)
    {
        altar.counter = kMorphDone;
    }
}

// swd3e.exe 0x00443520: down: the next object (into the slot); during the morph its end
void Menu::AltarDown()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    ListDown(altar.count, altar.top, altar.row, altar.shown);
    this->AltarMoved(kScrollDown);
}

// swd3e.exe 0x004435C0: up: the object before (into the slot); during the morph its end
void Menu::AltarUp()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    ListUp(altar.top, altar.row);
    this->AltarMoved(kScrollUp);
}

// swd3e.exe 0x00443660: Page Down: a page of sixteen on; during the morph its end
void Menu::AltarPageDown()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    ListPageDown(altar.count, altar.top, altar.row, altar.shown, AltarPage::Rows);
    this->AltarMoved(kScrollDown);
}

// swd3e.exe 0x00443700: Page Up: a page of sixteen back; during the morph its end
void Menu::AltarPageUp()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    ListPageUp(altar.top, altar.row, AltarPage::Rows);
    this->AltarMoved(kScrollUp);
}

// Another tab chosen (inline in 0x004437A0 and 0x00443840): its list made and fitted, the object put into the slot, the
// result made again
void Menu::AltarTabChanged()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    this->AltarFillTab();
    altar.top = 0;
    altar.row = 0;
    altar.shown = AltarPage::Rows;
    MenuList view{altar.count, altar.top, altar.row, altar.shown, altar.firstShown};
    this->description = FitMenuList(view, altar.objects, AltarPage::Rows, owner.database, owner.databases.mapsData);
    altar.count = view.count;
    altar.top = view.top;
    altar.row = view.row;
    altar.shown = view.shown;
    altar.firstShown = view.first;
    this->AltarPick();
    this->AltarCombine(altar.slotA, altar.slotB);
    owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
}

// swd3e.exe 0x004437A0: left: the tab before (wrapping); during the morph its end
void Menu::AltarLeft()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    if (--altar.tab < 0)
    {
        altar.tab = kLastTab;
    }

    this->AltarTabChanged();
}

// swd3e.exe 0x00443840: right: the next tab (wrapping); during the morph its end
void Menu::AltarRight()
{
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    if (++altar.tab > kLastTab)
    {
        altar.tab = 0;
    }

    this->AltarTabChanged();
}

// swd3e.exe 0x004438F0: Shift: the other slot; during the morph its end
void Menu::AltarShift()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;

    if (altar.stage != kChoosing)
    {
        this->AltarSkipMorph();
        return;
    }

    if (++altar.slot > 1)
    {
        altar.slot = 0;
    }

    owner.audio.PlaySound(kSwitchSound, owner.config.soundVolume);
}

// swd3e.exe 0x00443990: Space or Enter. On the list (with the altar open; Escape otherwise) the result is offered when
// both slots hold an object (refused when more than six levels above the first party role), else
// "請將兩個欄位選擇物品"; on the offer the morph starts unless refused; during the morph it ends at once; once made the
// result is kept; the notice closes.
void Menu::AltarSelect()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;

    switch (altar.stage)
    {
        case kChoosing:
            if (!owner.state.GameFlag(kAltarOpenFlag))
            {
                this->AltarCancel();
                return;
            }

            if (!this->AltarCombine(altar.slotA, altar.slotB))
            {
                altar.stage = kNeedTwo;
                return;
            }

            this->AltarResultVitals();
            ++altar.stage;
            this->background.id = kFaceAct;
            this->background.action = kOfferBackground;
            altar.tooHigh = 0;

            if (altar.result.record.data.level - owner.party.roles[0].level > kTooHighLevels)
            {
                altar.tooHigh = kTooHigh;
            }

            owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
            return;

        case kOffered:
            if (altar.tooHigh != 0)
            {
                owner.audio.PlaySound(kRefusedSound, owner.config.soundVolume);
                return;
            }

            altar.stage = kMorphing;
            altar.counter = kMorphStart;
            this->AltarPrepareMorph();
            owner.audio.PlaySound(kMorphStartSound, owner.config.soundVolume);
            return;

        case kMorphing:
            this->AltarFinishMorph();

            if (altar.counter < kMorphShown)
            {
                altar.counter = kSkipTo;
                this->background.id = kFaceAct;
                this->background.action = kMorphBackground;
            }

            owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
            return;

        case kMade:
            this->AltarFinish();
            return;

        case kNeedTwo:
        case kOfferedAgain:
            altar.stage = kChoosing;
            return;

        default:
            return;
    }
}

// The result made (stage 4 of 0x00443990): the two slots' objects spent (from the list when the tab lists them, else
// from the inventory), the result counted in the first counter list, one of it added (pending) to the list or the
// inventory, free to use; the list fitted again, back to the list
void Menu::AltarFinish()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    this->tint = 0;

    for (ItemNode* slot : {&altar.slotA, &altar.slotB})
    {
        const bool listed = AltarTabTakes(*slot, altar.tab);

        if (slot->id == ItemNode::Empty)
        {
            continue;
        }

        AddItem(listed ? altar.objects : owner.party.inventory, slot->id, -1, ItemCountMode::Pending, owner.database);
        slot->record.description.reset();
        slot->id = ItemNode::Empty;
    }

    ItemNode& result = altar.result;
    AddCounterCapped(owner.party.fame.lists[Fame::FameList], static_cast<i16>(result.id), 1);
    const bool listed = AltarTabTakes(result, altar.tab);

    if (result.id != ItemNode::Empty)
    {
        ItemLink& list = listed ? altar.objects : owner.party.inventory;

        if (ItemNode* added = AddItem(list, result.id, 1, ItemCountMode::PendingOnly, owner.database); added != nullptr)
        {
            ++added->unk06;
        }
    }

    altar.count = CountItems(altar.objects);
    SkipItems(altar.top, altar.objects, altar.firstShown);
    CountShown(altar.firstShown, altar.shown, AltarPage::Rows);
    MenuList view{altar.count, altar.top, altar.row, altar.shown, altar.firstShown};
    this->description = FitMenuList(view, altar.objects, AltarPage::Rows, owner.database, owner.databases.mapsData);
    altar.count = view.count;
    altar.top = view.top;
    altar.row = view.row;
    altar.shown = view.shown;
    altar.firstShown = view.first;
    altar.counter = 0;
    altar.unk50B534 = 0;
    altar.stage = kChoosing;
    this->background.id = kFaceAct;
    this->background.action = kPageBackground;
}

// swd3e.exe 0x00443CB0: Escape: from the list the page closes (the menu too when no page is left); from the offer back
// to the list; during the morph and once made as Enter; the notice closes
void Menu::AltarCancel()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;

    switch (altar.stage)
    {
        case kChoosing:
            if (static_cast<i16>(--this->code) <= 0)
            {
                owner.menuState = Game::NoMenu;
            }

            this->SetPage(this->code);
            this->AltarLeave();
            return;

        case kOffered:
            altar.stage = kChoosing;
            this->background.id = kFaceAct;
            this->background.action = kPageBackground;
            return;

        case kMorphing:
        case kMade:
            this->AltarSelect();
            return;

        case kNeedTwo:
            altar.stage = kChoosing;
            return;

        default:
            return;
    }
}

// swd3e.exe 0x004432E0: the mouse on the altar page. On the list: a click on a tab chooses it, on a row picks it, on a
// slot chooses that slot (during the morph: its end); the run button lights under the pointer and offers on a click;
// the held left button on the scroll bar; a right click goes back. On the offer a left click starts the morph (goes
// back when refused), a right click goes back; during the morph and once made any click answers; on the notice any
// click closes it.
void Menu::AltarFrame()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    const u32 clicks = this->clicks;
    altar.hover = 0;

    if (altar.stage != kChoosing)
    {
        switch (altar.stage)
        {
            case kOffered:
                if ((clicks & kLeftClicks) != 0)
                {
                    if ((altar.tooHigh & kTooHighShown) == 0)
                    {
                        this->AltarSelect();
                        return;
                    }
                }
                else if ((clicks & kRightClicks) == 0)
                {
                    return;
                }

                this->AltarCancel();
                return;

            case kMorphing:
            case kMade:
                if ((clicks & kAnyClick) != 0)
                {
                    this->AltarSelect();
                }

                return;

            default:
                if (altar.stage == kNeedTwo && (clicks & kAnyClick) != 0)
                {
                    this->AltarCancel();
                }

                return;
        }
    }

    const auto x = static_cast<u32>(owner.state.pointerX);
    const auto y = static_cast<u32>(owner.state.pointerY);

    auto inside = [](u32 value, u32 low, u32 high)
    {
        return value < high && value > low;
    };

    if ((clicks & kLeftClicks) != 0 && inside(y, kTabsTop, kTabsBottom) && inside(x, kTabsLeft, kTabsRight))
    {
        altar.tab = static_cast<i32>((x - kTabsLeft) / kTabWidth) + 1;
        this->AltarLeft();
        return;
    }

    if ((clicks & kLeftClicks) != 0)
    {
        if (inside(y, kRowsTop, kRowsBottom) && inside(x, kRowsLeft, kRowsRight))
        {
            const auto row = static_cast<i32>((y - kRowsTop) / kRowHeight);

            if (row < altar.shown)
            {
                altar.row = row + 1;
                this->AltarUp();
            }

            return;
        }

        bool onSlot = false;

        if (inside(y, kSlotsTop, kSlotsBottom) && inside(x, kFirstSlotLeft, kFirstSlotRight))
        {
            altar.slot = 1;
            onSlot = true;
        }
        else if (inside(y, kSlotsTop, kSlotsBottom) && inside(x, kSecondSlotLeft, kSecondSlotRight))
        {
            altar.slot = 0;
            onSlot = true;
        }

        if (onSlot)
        {
            // The slot clicked is the one after the one set (the first box sets 1, stepped on to 0).
            if (altar.stage == kChoosing)
            {
                if (++altar.slot > 1)
                {
                    altar.slot = 0;
                }

                owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
            }
            else
            {
                this->AltarSkipMorph();
            }

            return;
        }
    }

    altar.hover = 0;

    if (inside(y, kRunTop, kRunBottom) && inside(x, kRunLeft, kRunRight))
    {
        altar.hover = 1;

        if ((clicks & kLeftClicks) != 0)
        {
            this->AltarSelect();
            return;
        }
    }

    if (altar.count <= AltarPage::Rows || !owner.controls.Repeating(Button::MouseLeft) ||
        !inside(x, kBarLeft, kBarRight))
    {
        if ((clicks & kRightClicks) != 0)
        {
            this->AltarCancel();
        }

        return;
    }

    const MenuScrollBar& bar = this->scrollBar;
    i32 pointerY = owner.state.pointerY;

    if (pointerY < kBarUpBottom && pointerY > kBarUpTop)
    {
        this->AltarUp();
        pointerY = owner.state.pointerY;
    }

    if (pointerY < kBarDownBottom && pointerY > kBarDownTop)
    {
        this->AltarDown();
        pointerY = owner.state.pointerY;
    }

    if (pointerY < bar.thumbTop && pointerY > bar.top)
    {
        this->AltarPageUp();
        pointerY = owner.state.pointerY;
    }

    if (pointerY < bar.bottom && pointerY > bar.thumbBottom)
    {
        this->AltarPageDown();
    }
}

// swd3e.exe 0x00443D40: the altar page ("目前無法使用" while the altar is closed). The run button lit under the
// pointer; on the list and its notice the tab, the bobbing slot marker, the list with its scroll bar, the two slots and
// the result's arrow; on the offer the result with its description ("無法練化" when too high); the morph (the result
// shown first while the screen tints); the result made; "請將兩個欄位選擇物品".
void Menu::AltarDraw()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));

    if (!owner.state.GameFlag(kAltarOpenFlag))
    {
        const std::string text = Big5(kClosed);
        this->DrawBox(
            kClosedX,
            kClosedY,
            kClosedCharWidth * static_cast<int>(text.size()),
            kClosedHeight,
            kClosedBoxMode,
            0,
            0,
            0
        );
        owner.largeFont.DrawText(owner.drawSurface, kClosedX, kClosedY, text, color, kPanelStyle);
        return;
    }

    if (altar.hover == 1)
    {
        this->face.id = kAltarAct;
        this->face.action = kRunLitAction;
        owner.painter.SetClipClamped(kRunLitX, kRunLitY, kRunLitRight, kRunLitBottom);
        owner.actDrawer.Draw(this->face, kRunLitX, kRunLitY);
        owner.painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    }

    if (altar.stage == kChoosing || altar.stage == kNeedTwo)
    {
        this->AltarDrawList(color);
    }

    if (altar.stage == kOffered || altar.stage == kOfferedAgain)
    {
        this->AltarDrawOffer(color);
    }

    if (altar.stage == kMorphing)
    {
        if (altar.counter > kMorphShown)
        {
            this->background.id = kFaceAct;
            this->background.action = kMorphBackground;
            this->tint = 0;

            if (altar.counter <= kTintEnd)
            {
                this->tint = 2 * (3 * altar.counter + kTintBase);
            }

            this->AltarMorph();

            if (++altar.counter > static_cast<i32>(kMorphLast))
            {
                altar.counter = kMorphDone;
                this->AltarFinishMorph();
            }
            else if (altar.counter >= kMorphDone)
            {
                this->AltarFinishMorph();
            }

            return;
        }

        this->AltarDrawResult(altar.result, kResultX, kResultY, kChosenResult);
        this->tint = kTintStep * (kMorphStart - altar.counter);

        if (altar.counter == kMorphShown)
        {
            this->background.id = kFaceAct;
            this->background.action = kMorphBackground;
        }

        ++altar.counter;
        return;
    }

    if (altar.stage == kMade)
    {
        this->gauge.id = altar.result.record.data.act;
        this->gauge.action = kPictureAction;
        this->gauge.direction = 0;
        owner.actDrawer.Draw(this->gauge, kMadeX, kMadeY);
    }

    if (altar.stage == kNeedTwo)
    {
        const std::string text = Big5(kChooseTwo);
        const int width = kNeedTwoCharWidth * static_cast<int>(text.size());
        const int x = Display::Width / 2 - width / 2;
        this->DrawBox(x, kNeedTwoY, width, kNeedTwoHeight, kAltarBoxMode, 0, 0, 0);
        owner.largeFont.DrawText(owner.drawSurface, x, kNeedTwoY, text, color, kPanelStyle);
    }
}

// The list, the tab, the slot marker and the two slots (inline in 0x00443D40, stages 1 and 5)
void Menu::AltarDrawList(Pixel color)
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    this->face.id = kAltarAct;
    this->face.action = altar.tab + kFirstTabAction;
    owner.actDrawer.Draw(this->face, kTabStepX * altar.tab + kTabX, kTabY);

    if (altar.slot <= 1)
    {
        altar.bob = (altar.bob + 1) % kMarkerSteps;
        owner.actDrawer.DrawPicture(kMarkerFile, 0, kMarkerStepX * altar.slot + kMarkerX, altar.bob / 2 + kMarkerY);
    }

    if (altar.count > AltarPage::Rows)
    {
        int arrows = 0;

        if ((altar.scrollArrows & kScrollUpMask) != 0)
        {
            altar.scrollArrows =
                static_cast<u16>((altar.scrollArrows & ~kScrollUpMask) | ((altar.scrollArrows & kScrollUpMask) - 1));
            arrows = 1;
        }

        if ((altar.scrollArrows & kScrollDownMask) != 0)
        {
            const int down = (altar.scrollArrows & kScrollDownMask) >> kNibbleShift;
            arrows |= 2;
            altar.scrollArrows =
                static_cast<u16>((altar.scrollArrows & ~kScrollDownMask) | ((down - 1) << kNibbleShift));
        }

        const auto count = static_cast<double>(altar.count);
        const auto to = static_cast<float>(static_cast<double>(altar.top + altar.shown) / count);
        const auto from = static_cast<float>(static_cast<double>(altar.top) / count);
        this->DrawScrollBar(kBarX, kBarY, kBarHeight, arrows, from, to, this->scrollBar);
    }

    int rowY = kListTop;
    int row = 0;

    for (const ItemNode* node = altar.firstShown; node != nullptr && rowY < kListEnd;
         node = node->next.get(), ++row, rowY += static_cast<int>(kRowHeight))
    {
        const std::string text =
            WinFormat(kRowFormat, {ObjectName(node->record), static_cast<i32>(static_cast<u16>(node->unk06))});
        owner.largeFont.DrawText(owner.drawSurface, kListX, rowY + 1, text, color, kPanelStyle);

        if (altar.row == row)
        {
            FillBox(
                owner.painter,
                kHighlightX,
                rowY,
                kHighlightWidth,
                kHighlightHeight,
                kHighlightRed,
                kHighlightGreen,
                0,
                FillMode::Gradient
            );
        }
    }

    const int average = static_cast<u16>(altar.slotA.record.data.level + altar.slotB.record.data.level) / 2;

    for (int side = 0; side < 2; ++side)
    {
        const ItemNode& slot = side == 1 ? altar.slotB : altar.slotA;

        if (slot.id == ItemNode::Empty)
        {
            continue;
        }

        if (slot.record.data.act != 0)
        {
            this->gauge.id = slot.record.data.act;
            this->gauge.action = kPictureAction;
            this->gauge.direction = 0;
            owner.actDrawer.Draw(this->gauge, kSlotPictureStep * side + kSlotPictureX, kSlotPictureY);
        }

        const int boxX = kSlotBoxStep * side + kSlotBoxX;
        this->DrawBox(boxX, kSlotBoxY, kSlotBoxWidth, kSlotBoxHeight, kAltarBoxMode, 0, 0, 0);
        owner.mediumFont.DrawText(
            owner.drawSurface,
            boxX,
            kSlotBoxY,
            WinFormat(kSlotFormat, {CategoryName(slot.record.data.category), static_cast<i32>(slot.record.data.level)}),
            color,
            kPanelStyle
        );
        owner.mediumFont.DrawText(
            owner.drawSurface,
            boxX,
            kSlotNameY,
            WinFormat(kNameFormat, {ObjectName(slot.record)}),
            color,
            kPanelStyle
        );

        // Both passes draw the arrow at one place (slot A's only with flag 0x1BA9).
        if ((side == 1 || owner.state.GameFlag(kSlotAArrowFlag)) && altar.result.id != ItemNode::Empty)
        {
            const u16 arrow = altar.result.record.data.level >= average ? kUpArrow : kDownArrow;
            owner.actDrawer.DrawPicture(arrow, 0, kArrowX, kArrowY);
        }
    }
}

// The result offered with its description, "無法練化" when it is too high (inline in 0x00443D40, stages 2 and 0xA)
void Menu::AltarDrawOffer(Pixel color)
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    this->AltarDrawResult(altar.result, kResultX, kResultY, kChosenResult);
    FillBox(
        owner.painter,
        kDescriptionBoxX,
        kDescriptionBoxY,
        kDescriptionBoxWidth,
        kDescriptionBoxHeight,
        0,
        0,
        0,
        FillMode::Eighth
    );
    DrawWindowFrame(
        owner.painter,
        owner.archive,
        owner.battleMessages.panelAct.frame,
        kDescriptionLeft,
        kDescriptionTop,
        kDescriptionRight,
        kDescriptionBottom,
        0,
        kFrameBorder
    );
    owner.DrawWrappedText(
        altar.result.record.description.value_or(std::string()),
        kDescriptionLeft,
        kDescriptionTop,
        kDescriptionLines,
        kDescriptionWidth
    );

    if ((altar.tooHigh & kTooHighShown) != 0)
    {
        this->DrawBox(kCannotMakeBoxX, kCannotMakeY, kCannotMakeWidth, kCannotMakeHeight, kChosenBoxMode, 0, 0, 0);
        owner.mediumFont.DrawText(owner.drawSurface, kCannotMakeX, kCannotMakeY, kCannotMake, color, kPanelStyle);
    }
}

// swd3e.exe 0x00444BA0: a result in its box at (x, y): the picture, the category, name, level and values (the spirit
// and stamina of AltarPage::vitals, the pair of the right half past the screen's middle); darker and the picture shaded
// when not chosen or too high, then "等級不足 無法訂契約"
void Menu::AltarDrawResult(const ItemNode& result, int x, int y, u32 flags)
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    Pixel text = static_cast<u16>(owner.painter.MakeColor(kGoldRed, kGoldGreen, kGoldBlue));
    int mode = kChosenBoxMode;
    const bool dim = (flags & kChosenResult) == 0 || (flags & kTooHighFlag) != 0;

    if (dim)
    {
        text = static_cast<u16>(owner.painter.MakeColor(kDimRed, kDimGreen, kDimBlue));
        mode = kAltarBoxMode;
    }

    this->DrawBox(x, y, kResultBoxWidth, kResultBoxHeight, mode, 0, 0, 0);
    const ObjectData& data = result.record.data;
    this->gauge.id = data.act;
    this->gauge.action = kPictureAction;
    this->gauge.direction = 0;
    owner.actDrawer.Draw(this->gauge, x + kResultPictureX, y + kResultPictureY);
    const int valuesX = x + kResultValuesX;
    const std::size_t side = x <= Display::Width / 2 ? 0 : 2;
    const std::array<std::string, 9> lines = {
        Big5(CategoryName(data.category)),
        ObjectName(result.record),
        WinFormat(kResultLevelFormat, {static_cast<i32>(data.level)}),
        WinFormat(kHitPointsFormat, {data.hitPoints}),
        WinFormat(kSpiritFormat, {static_cast<i32>(static_cast<u16>(altar.vitals[side]))}),
        WinFormat(kStaminaFormat, {static_cast<i32>(static_cast<u16>(altar.vitals[side + 1]))}),
        WinFormat(kAttackFormat, {static_cast<i32>(data.attack)}),
        WinFormat(kDefenseFormat, {static_cast<i32>(data.defense)}),
        WinFormat(kAgilityFormat, {static_cast<i32>(data.agility)}),
    };

    for (std::size_t line = 0; line < lines.size(); ++line)
    {
        owner.largeFont.DrawText(
            owner.drawSurface,
            valuesX,
            y + kResultValuesY + kResultLine * static_cast<int>(line),
            lines[line],
            text,
            kPanelStyle
        );
    }

    if (!dim)
    {
        return;
    }

    FillBox(owner.painter, x + kShadeX, y + kShadeY, kShadeWidth, kShadeHeight, 0, 0, 0, FillMode::Quarter);

    if ((flags & kTooHighFlag) != 0)
    {
        owner.mediumFont.DrawText(
            owner.drawSurface,
            x + kTooHighX,
            y + kTooHighY,
            kTooHighText,
            static_cast<Pixel>(owner.painter.MakeColor(kWarnRed, kWarnGreen, kWarnBlue)),
            kPanelStyle
        );
    }
}

// swd3e.exe 0x00444EC0: the morph prepared: its offsets cleared (with four buffers of the original never read), the
// pictures of the result and of the two slots unpacked; a failed act is reported with the title of its step
void Menu::AltarPrepareMorph()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    std::fill(altar.columnsA.begin(), altar.columnsA.end(), i16{0});
    std::fill(altar.columnsB.begin(), altar.columnsB.end(), i16{0});
    std::fill(altar.rowsA.begin(), altar.rowsA.end(), i16{0});
    std::fill(altar.rowsB.begin(), altar.rowsB.end(), i16{0});

    auto unpack = [this, &owner](i32 act, std::vector<u16>& pixels, std::u8string_view failed)
    {
        this->gauge.action = kPictureAction;
        this->gauge.id = act;
        this->gauge.direction = 0;

        if (owner.acts.Run(this->gauge) == 0)
        {
            ErrorBox(
                owner.errors,
                failed,
                u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
                this->gauge.id,
                this->gauge.action,
                this->gauge.direction
            );
            return false;
        }

        const Picture* picture = owner.archive.GetPicture(this->gauge.frame, this->gauge.picture);
        pixels.clear();

        if (picture != nullptr)
        {
            const std::optional<RawPicture> raw = owner.codec.Decode(picture->data);

            if (raw.has_value())
            {
                pixels.assign(raw->pixels.size() / sizeof(u16), 0);
                std::memcpy(pixels.data(), raw->pixels.data(), pixels.size() * sizeof(u16));
            }
        }

        return true;
    };

    // The titles of 0x004BEA40, 0x004BEA24 and 0x004BEA08 with the format 0x004B0D58
    if (!unpack(altar.result.record.data.act, altar.resultPicture, u8"Act Err(Botl::GetOriginal2)"))
    {
        return;
    }

    if (!unpack(altar.slotA.record.data.act, altar.pictureA, u8"Act Err(Botl::GetOriginal3)"))
    {
        return;
    }

    unpack(altar.slotB.record.data.act, altar.pictureB, u8"Act Err(Botl::GetOriginal4)");
}

// swd3e.exe 0x004450E0: a frame of the morph. Up to 0x8C: before 0x78 the columns and rows wander (more as the counter
// grows), pixel pairs of the result spread into both slot pictures from 0x32 (all of it at 0x6E, with its sound);
// from 0x78 the wandering settles. The two slot pictures are drawn displaced (a column's offset in rows through
// AltarPage::wave), closing in from the sides. The original reads the wave table past its ends for large offsets and
// writes past the screen; here those read 0 and those pixels are left out.
void Menu::AltarMorph()
{
    Game& owner = this->game;
    AltarPage& altar = this->altar;
    const i32 counter = altar.counter;
    const int pitch = owner.painter.ScreenPitch();

    // The first offsets of each pair are cut to 16 bits before the division, the second ones are not.
    auto settle = [](std::vector<i16>& values, bool cutFirst)
    {
        for (i16& value : values)
        {
            const int scaled = cutFirst ? static_cast<i16>(kSettle * value) : kSettle * value;
            value = static_cast<i16>(scaled / kTenths);
        }
    };

    if (static_cast<u32>(counter) <= kMorphLast)
    {
        if (counter >= kSettleStart)
        {
            settle(altar.columnsA, true);
            settle(altar.columnsB, false);
            settle(altar.rowsA, true);
            settle(altar.rowsB, false);
        }
        else
        {
            if (counter >= kMixStart && counter < kMixEnd && kMixPerFrame * counter - kMixBase > 0)
            {
                for (int pair = kMixPerFrame * counter - kMixBase; pair > 0; --pair)
                {
                    const auto at = static_cast<std::size_t>(2 * owner.random.Below(kMixPairs));

                    for (std::size_t half = 0; half < 2; ++half)
                    {
                        const u16 pixel =
                            at + half < altar.resultPicture.size() ? altar.resultPicture[at + half] : u16{0};

                        if (at + half < altar.pictureA.size())
                        {
                            altar.pictureA[at + half] = pixel;
                        }

                        if (at + half < altar.pictureB.size())
                        {
                            altar.pictureB[at + half] = pixel;
                        }
                    }
                }
            }

            if (counter == kMixEnd)
            {
                owner.audio.PlaySound(kDoneSound, owner.config.soundVolume);
                altar.pictureA = altar.resultPicture;
                altar.pictureB = altar.resultPicture;
            }

            const int step = kSteps[static_cast<std::size_t>(std::clamp(counter / kStepSpan, 0, 2))];
            const int half = step / 2;

            for (std::size_t column = 0; column < altar.columnsA.size(); ++column)
            {
                const auto change =
                    static_cast<i16>(static_cast<int>(owner.random.Below(static_cast<u32>(step))) - half);
                altar.columnsA[column] = static_cast<i16>(altar.columnsA[column] + change);
                altar.columnsB[column] = static_cast<i16>(altar.columnsB[column] - change);
            }

            for (std::size_t row = 0; row < altar.rowsA.size(); ++row)
            {
                const auto change =
                    static_cast<i16>(static_cast<int>(owner.random.Below(static_cast<u32>(step))) - half);
                altar.rowsA[row] = static_cast<i16>(altar.rowsA[row] + change);
                altar.rowsB[row] = static_cast<i16>(altar.rowsB[row] - change);
            }
        }
    }

    if (kDrawRange - counter < kDrawLast)
    {
        return;
    }

    const i32 shift = std::clamp(counter, 0, kSettleStart);
    const std::size_t screen = static_cast<std::size_t>(pitch) * Display::Height;

    auto wave = [&altar](i16 column)
    {
        const i32 at = static_cast<i32>(AltarPage::WaveCenter) + column;
        return at >= 0 && static_cast<std::size_t>(at) < altar.wave.size() ? altar.wave[static_cast<std::size_t>(at)]
                                                                           : i32{0};
    };

    auto draw = [&](const std::vector<u16>& picture, std::ptrdiff_t base)
    {
        std::size_t pixel = 0;

        for (std::size_t row = 0; row < altar.rowsA.size(); ++row, base += pitch)
        {
            for (std::size_t column = 0; column < altar.columnsA.size(); ++column, ++pixel)
            {
                const std::ptrdiff_t at =
                    base + static_cast<std::ptrdiff_t>(column) + altar.rowsA[row] - wave(altar.columnsA[column]);

                if (at >= 0 && static_cast<std::size_t>(at) < screen && pixel < picture.size())
                {
                    owner.drawSurface[at] = picture[pixel];
                }
            }
        }
    };

    draw(altar.pictureA, static_cast<std::ptrdiff_t>(kPictureTop) * pitch + shift + kLeftPictureX);
    draw(altar.pictureB, static_cast<std::ptrdiff_t>(kPictureTop) * pitch - shift + kRightPictureX);

    if (++altar.phase >= static_cast<i32>(AltarPage::MorphColumns))
    {
        altar.phase = 0;
    }
}

// swd3e.exe 0x004455D0: the morph over: the result made (stage 4), the pictures freed
void Menu::AltarFinishMorph()
{
    AltarPage& altar = this->altar;
    this->tint = 0;
    altar.stage = kMade;
    altar.resultPicture.clear();
    altar.pictureA.clear();
    altar.pictureB.clear();
    altar.phase = 0;
}

// swd3e.exe 0x004454E0: the spirit and stamina of the result from its level and the objects and skills it uses: the
// spirit twice, three and five times the level for its objects A..C and twice and four times for its two healing
// objects, the stamina three and five times for its skills A and B (as words); the other pair cleared
void Menu::AltarResultVitals()
{
    AltarPage& altar = this->altar;
    const ObjectData& data = altar.result.record.data;
    const int level = data.level;
    int spirit = 0;
    int stamina = 0;
    altar.vitals = {};

    for (const auto& [uses, times] : std::array<std::pair<u16, int>, 5>{{
             {data.itemA, kSpiritItemA},
             {data.itemB, kSpiritItemB},
             {data.itemC, kSpiritItemC},
             {data.healItemA, kSpiritHealA},
             {data.healItemB, kSpiritHealB},
         }})

    {
        if (uses != 0)
        {
            spirit += times * level;
            altar.vitals[0] = static_cast<i16>(spirit);
        }
    }

    for (const auto& [uses, times] : std::array<std::pair<u16, int>, 2>{{
             {data.skillA, kStaminaSkillA},
             {data.skillB, kStaminaSkillB},
         }})

    {
        if (uses != 0)
        {
            stamina += times * level;
            altar.vitals[1] = static_cast<i16>(stamina);
        }
    }
}

}  // namespace swd3e
