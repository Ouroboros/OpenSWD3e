#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>

#include "core/format.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game/databases.h"
#include "game/game.h"
#include "menu/menu.h"
#include "menu/menu_list.h"
#include "org/database.h"
#include "platform/display.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

// SkillsPage::stage.
constexpr i32 kSkillList = 1;
constexpr i32 kChooseRole = 2;
constexpr i32 kDescribing = 5;
constexpr i32 kChooseTown = 0xF;
constexpr i32 kNoTownWarp = 0x11;  // the town warp cannot be used here
constexpr i32 kNoExitWarp = 0x12;
constexpr u16 kTownCode = 0xF;  // Menu::code with the town list open as a subpage (as the original tests)

constexpr int kRows = 0x18;  // the cells shown: two columns of twelve
constexpr int kColumns = 2;
constexpr int kTownRows = 8;
constexpr int kBaseTabs = 3;
constexpr int kFirstExtraTabFlag = 0x15;  // two flags a role add to SkillsPage::tabs
constexpr int kTabFlagsPerRole = 2;
constexpr u16 kSkillKindA = 0x11;  // ObjectData::category of the skills the page lists
constexpr u16 kSkillKindB = 0x14;
constexpr u32 kHeadingKind = 0x80;  // ObjectData::kind: a heading across both columns
constexpr int kOddCell = 1;
constexpr u32 kEvenCells = 0xFFFE;   // the original clears bit 0 of the cell as a word
constexpr u16 kSpiritCost = 0x8000;  // ObjectData::costBits: the cost in spirit (bit 15) or stamina (bit 14)
constexpr u16 kSpiritMask = 0x7FFF;
constexpr u16 kStaminaCost = 0x4000;
constexpr u16 kStaminaMask = 0x3FFF;
constexpr u16 kCostBits = 0xC000;
constexpr u16 kUseMask = 0xF;  // ObjectData::usableBy's low nibble: 0 never, 1 in battle only, 2 and up also here
constexpr u16 kFieldUse = 2;
constexpr u16 kBattleOnly = 1;
constexpr u16 kExitWarp = 0x17B5;
constexpr u16 kTownWarp = 0x17B4;
constexpr int kExitWarpBlocked = GameState::FileNameB;
constexpr int kTownWarpBlocked = GameState::FileNameA;
constexpr u32 kWholeParty = 1;  // SkillScript::flags: the skill works on every party role
constexpr int kTownFlags = 0x1388;
constexpr i16 kWarpEvent = 0x52;
constexpr u16 kWarpLeader = 1;
constexpr u16 kTownNameEnd = 0x5125;  // "%Q"
constexpr u16 kTableEnd = 0xFFFF;
constexpr std::size_t kTownNameBytes = 0x40;
constexpr std::size_t kTownsMost = 0x200;
constexpr std::size_t kTownWords = 3;
constexpr i32 kDescriptionSlide = 0x80;
constexpr i32 kDescriptionHidden = 0x1E0;
constexpr i32 kDescriptionUp = 0x168;
constexpr i32 kLeaveBackground = 0x36;
constexpr i32 kPressedOffset = -1;
constexpr int kMessageLines = 3;
constexpr std::size_t kMessageLineBytes = 0x10;

constexpr u16 kMoveSound = 0x2E;
constexpr u16 kRoleSound = 0x107;
constexpr u16 kUsedSound = 0x8B;
constexpr u16 kRefusedSound = 0x8C;
constexpr u16 kScrollUp = 3;
constexpr u16 kScrollDown = 0x30;
constexpr u16 kTownScrollUp = 0x300;
constexpr u16 kTownScrollDown = 0x3000;
constexpr int kNibbleShift = 4;
constexpr u16 kNibble = 0xF;

// The clicks (Menu::clicks).
constexpr u32 kLeftClicks = 3;
constexpr u32 kDoubleClick = 2;
constexpr u32 kRightClick = 4;
constexpr u32 kLeftOrRight = 5;

// The texts.
constexpr std::u8string_view kCannotHere = u8"此地無法使用";  // 0x004BEBA8
constexpr std::u8string_view kEmptyName = u8"無";             // 0x004AF630
constexpr std::string_view kNameFormat = "%-14s";
constexpr std::string_view kHeadingFormat = "~~~~~~~~%-14s~~~~~~~~";
constexpr std::string_view kCostFormat = "%3d";
// The element picture of a skill by SkillScript::unk10 in pictures of 0x23AC (-1: none). The first entry is the
// grey of the unusable rows, which the original keeps in the same stack slot.
constexpr std::size_t kElementPictures = 10;
constexpr std::array<i32, kElementPictures - 1> kElementPictureTail = {0, 1, 2, 3, 4, 5, 6, -1, -1};
constexpr u16 kElementPictureFile = 0x23AC;

// The layout.
constexpr u32 kTextRed = 0x19;
constexpr u32 kTextGreen = 0x17;
constexpr u32 kTextBlue = 0x11;
constexpr u32 kNeverRed = 0xD;
constexpr u32 kNeverGreen = 0xD;
constexpr u32 kNeverBlue = 9;
constexpr u32 kBattleRed = 0x18;
constexpr u32 kBattleGreen = 0xA;
constexpr u32 kBattleBlue = 0xB;
constexpr u32 kRoleRed = 0x15;
constexpr u32 kRoleGreen = 0xF;
constexpr u32 kRoleBlue = 8;
constexpr u32 kRoleChosenRed = 0x19;
constexpr u32 kRoleChosenGreen = 0x13;
constexpr u32 kRoleChosenBlue = 0xC;
constexpr int kDim = -4;
constexpr int kPanelStyle = 4;
constexpr int kFrameBorder = static_cast<int>(0x80000008);
constexpr int kListBoxX = 0xD4;
constexpr int kListBoxY = 0x6A;
constexpr int kListBoxWidth = 0x18C;
constexpr int kListBoxHeight = 0x134;
constexpr int kListFrameLeft = 0xD8;
constexpr int kListFrameTop = 0x70;
constexpr int kListFrameRight = 0x258;
constexpr int kListFrameBottom = 0x198;
constexpr int kHelpBoxX = 0xD0;
constexpr int kHelpBoxY = 0x1B5;
constexpr int kHelpBoxWidth = 0x190;
constexpr int kHelpBoxHeight = 0x24;
constexpr int kHelpFrameLeft = 0xD8;
constexpr int kHelpFrameTop = 0x1BD;
constexpr int kHelpFrameRight = 0x258;
constexpr int kHelpFrameBottom = 0x1D1;
constexpr int kListX = 0xF0;
constexpr int kListY = 0x72;
constexpr int kColumnWidth = 0xC6;
constexpr int kRowHeight = 0x19;
constexpr int kCostX = 0x178;
constexpr int kCostDown = 6;
constexpr int kElementColumn = 0xC8;
constexpr int kElementX = 0xDB;
constexpr int kElementDown = 1;
constexpr int kHighlightColumn = 0xC5;
constexpr int kHighlightX = 0xD3;
constexpr int kHighlightY = 0x70;
constexpr int kHighlightWidth = 0xC5;
constexpr int kHighlightHeight = 0x18;
constexpr int kHeadingHighlightX = 0x198;  // the second half of a heading's highlight
constexpr int kHighlightRed = 0x14;
constexpr int kHighlightGreen = 0xD;
constexpr int kNoneX = 0xD8;
constexpr int kNoneY = 0x70;
constexpr Pixel kNoneColor = 0xFFFF;
constexpr i32 kButtonAct = 0x232A;
constexpr i32 kButtonAction = 0x20;
constexpr int kButtonX = 0x21C;
constexpr int kButtonY = 0x1D4;
constexpr int kBarX = 0x264;
constexpr int kBarY = 0x7A;
constexpr int kBarHeight = 0x118;
constexpr int kRolesBoxX = 0x154;
constexpr int kRolesBoxY = 0xD0;
constexpr int kRolesBoxWidth = 0x74;
constexpr int kRolesBoxPad = 0xC;
constexpr int kRolesBoxRed = 0xC0;
constexpr int kRolesBoxGreen = 0x40;
constexpr int kRolesBoxBlue = 0x40;
constexpr int kRolesFrameLeft = 0x15C;
constexpr int kRolesFrameTop = 0xD8;
constexpr int kRolesFrameRight = 0x1C4;
constexpr int kRoleTextX = 0x160;
constexpr int kRoleTextY = 0xDC;
constexpr int kRoleHighlightX = 0x157;
constexpr int kRoleHighlightY = 0xDA;
constexpr int kRoleHighlightWidth = 0x72;
constexpr int kRoleHighlightHeight = 0x14;
constexpr int kHelpTextClipX = 0xD4;
constexpr int kHelpTextClipY = 0x1BD;
constexpr int kHelpTextClipWidth = 0x188;
constexpr int kHelpTextClipHeight = 0x1E;
constexpr int kHelpTextX = 0xDA;
constexpr int kHelpTextY = 0x1C0;
constexpr int kTownsBoxX = 0x190;
constexpr int kTownsBoxY = 0xC4;
constexpr int kTownsBoxWidth = 0xA2;
constexpr int kTownsBoxHeight = 0xD0;
constexpr int kTownsBoxMode = 4;
constexpr int kTownsBoxBlue = 4;
constexpr int kTownsX = 0x190;
constexpr int kTownsY = 0xC2;
constexpr int kTownHighlightX = 0x18B;
constexpr int kTownHighlightWidth = 0xAC;
constexpr int kTownsBarX = 0x226;
constexpr int kTownsBarY = 0xD0;
constexpr int kTownsBarHeight = 0xB8;
constexpr int kMessageX = 0x12C;
constexpr int kMessageY = 0xF0;
constexpr int kMessageWidth = 0xB0;
constexpr int kMessageHeight = 0x48;
constexpr int kMessageBoxMode = 4;
constexpr int kMessageLineHeight = 0x18;
constexpr int kDescriptionBoxX = 0xD4;
constexpr int kDescriptionBoxUp = 8;
constexpr int kDescriptionBoxWidth = 0x188;
constexpr int kDescriptionBoxBottom = 0x1E6;
constexpr int kDescriptionShade = 0x10;
constexpr int kDescriptionBlue = 0x6C;
constexpr int kDescriptionTextX = 0xDC;
constexpr int kDescriptionFrameRight = 0x254;
constexpr int kDescriptionFrameBottom = 0x1D6;
constexpr int kDescriptionLines = 5;
constexpr int kDescriptionWidth = 0x168;

// The mouse areas.
constexpr i32 kTownsLeft = 0x190;
constexpr i32 kTownsRight = 0x226;
constexpr i32 kTownsTop = 0xC4;
constexpr i32 kTownsBottom = 0x18C;
constexpr u32 kTownsBottomInside = 0x18B;
constexpr u32 kTownsInnerRight = 0x222;
constexpr u32 kTownsBarLeft = 0x228;
constexpr u32 kTownsBarRight = 0x236;
constexpr i32 kTownsUpTop = 0xC2;
constexpr i32 kTownsUpBottom = 0xD0;
constexpr i32 kTownsDownTop = 0x18A;
constexpr i32 kTownsDownBottom = 0x198;
constexpr u32 kDescribeTop = 0x1D0;
constexpr u32 kDescribeBottom = 0x1E0;
constexpr u32 kDescribeLeft = 0x216;
constexpr u32 kDescribeRight = 0x260;
constexpr i32 kRowsTop = 0x72;
constexpr i32 kRowsBottom = 0x19E;
constexpr i32 kRowsLeft = 0xD4;
constexpr i32 kRowsRight = 0x25A;
constexpr i32 kRowsColumnWidth = 0xC5;
constexpr u32 kBarLeft = 0x264;
constexpr u32 kBarRight = 0x274;
constexpr i32 kBarUpTop = 0x6A;
constexpr i32 kBarUpBottom = 0x7A;
constexpr i32 kBarDownTop = 0x194;
constexpr i32 kBarDownBottom = 0x1A2;
constexpr i32 kRoleRowsTop = 0xD7;
constexpr i32 kRoleRowsBottom = 0x13B;
constexpr i32 kRoleRowsLeft = 0x158;
constexpr i32 kRoleRowsRight = 0x1C6;
constexpr u32 kTabsTop = 0x10;  // the role tabs above the page
constexpr u32 kTabsBottom = 0x30;
constexpr i32 kFirstTabLeft = 0x28;
constexpr i32 kTabStep = 0x32;
constexpr i32 kTabWidth = 0x20;

// Whether `role` can pay `cost` (inline in 0x0044C7A0)
bool CanPay(const RoleRecord& role, u16 cost)
{
    int costs = 0;
    int met = 0;

    if ((cost & kSpiritCost) != 0)
    {
        costs = 1;
        met = role.spirit >= (cost & kSpiritMask) ? 1 : 0;
    }

    if ((cost & kStaminaCost) != 0)
    {
        ++costs;

        if (role.stamina >= (cost & kStaminaMask))
        {
            ++met;
        }
    }

    return met == costs && costs != 0;
}

// The costs of `cost` that `role` can pay taken from it (inline in 0x0044C7A0): true when every cost it has was met
// (the costs met are taken even when another is not)
bool PayCost(RoleRecord& role, u16 cost)
{
    int costs = 0;
    int paid = 0;

    if ((cost & kSpiritCost) != 0)
    {
        ++costs;

        if (role.spirit >= (cost & kSpiritMask))
        {
            ++paid;
            role.spirit = static_cast<i16>(role.spirit - (cost & kSpiritMask));
        }
    }

    if ((cost & kStaminaCost) != 0)
    {
        ++costs;

        if (role.stamina >= (cost & kStaminaMask))
        {
            ++paid;
            role.stamina = static_cast<i16>(role.stamina - (cost & kStaminaMask));
        }
    }

    return paid == costs && costs != 0;
}

// A filler node (0xB4 bytes in the original, of which only the id and count are set)
std::unique_ptr<ItemNode> NewFiller()
{
    auto filler = std::make_unique<ItemNode>();
    filler->id = SkillsPage::FillerId;
    filler->count = 1;
    return filler;
}

u16 TableWord(const std::vector<u8>& data, std::size_t at)
{
    return MapsWord(data, at);
}

}  // namespace

// swd3e.exe 0x0044B9C0: the skills page opened (on the first tab when none is raised): the cell map allocated once,
// the list of the tab's role made, its target's tabs counted, the cell chosen described
void Menu::SkillsEnter()
{
    SkillsPage& page = this->skills;

    if (page.cells.empty())
    {
        page.cells.assign(SkillsPage::CellBytes, 0);
    }

    if (this->roleTab == Menu::NoTab)
    {
        this->roleTab = 0;
    }

    this->mode = static_cast<u16>(Menu::FirstPageMode + 2 * Menu::PageModeStep);
    page.target = 0;
    page.stage = kSkillList;
    page.unk50BAF4 = 0;
    this->SkillsFillList();
    this->SkillsCountTabs();
    this->SkillsDescribe();
    page.buttonOffset = 0;
    page.descriptionSlide = 0;
    page.descriptionTop = kDescriptionHidden;
    page.skill = SkillScript{};
    page.scrollArrows = 0;
}

// swd3e.exe 0x0044BA70: the skills page left: the list's skills back with the role, the cell map freed
void Menu::SkillsLeave()
{
    SkillsPage& page = this->skills;
    this->SkillsReturn();
    page.stage = 0;
    page.skill = SkillScript{};
    this->background.action = kLeaveBackground;
    page.cells.clear();
}

// The description of the cell chosen (0x00441200 of the node, inline in the skills page)
void Menu::SkillsDescribe()
{
    const SkillsPage& page = this->skills;
    const ItemNode* chosen = ItemAfter(page.top + page.row, page.list);
    this->description =
        chosen != nullptr ? ObjectDescription(this->game.databases.mapsData, chosen->id) : std::string();
}

// The kind of cell `cell` (SkillsPage::Cell). The original reads the map from 0x80 bytes in without a bound; outside
// the map it is taken as no cell.
int Menu::SkillsCell(int cell) const
{
    const std::vector<u8>& cells = this->skills.cells;
    const i32 at = static_cast<i32>(SkillsPage::CellBase) + cell;
    return at >= 0 && static_cast<std::size_t>(at) < cells.size() ? cells[static_cast<std::size_t>(at)]
                                                                  : SkillsPage::NoCell;
}

// The skill list of the tab's role (0x004C2B68 by party role, inline in the skills page)
ItemLink& Menu::SkillsRoleList()
{
    return this->game.party.skills[static_cast<std::size_t>(this->TabRole())];
}

// The record of the tab's role (inline in the skills page)
RoleRecord& Menu::SkillsRole()
{
    return this->game.party.roles[static_cast<std::size_t>(this->TabRole())];
}

// swd3e.exe 0x0044D940: the skills of `source` of kinds 0x11 and 0x14 moved into the page's list in the order of their
// ids
void Menu::SkillsTake(ItemLink& source)
{
    SkillsPage& page = this->skills;
    page.list.reset();
    ItemLink* link = &source;

    while (*link != nullptr)
    {
        const u16 kind = (*link)->record.data.category;

        if (kind != kSkillKindA && kind != kSkillKindB)
        {
            link = &(*link)->next;
            continue;
        }

        std::unique_ptr<ItemNode> moved = std::move(*link);
        *link = std::move(moved->next);
        InsertItemById(page.list, std::move(moved));
    }
}

// swd3e.exe 0x0044D9D0: the cells shown from the first one shown (at most 24)
void Menu::SkillsCountShown()
{
    SkillsPage& page = this->skills;
    page.shown = 0;

    for (const ItemNode* node = page.firstShown; node != nullptr && page.shown < kRows; node = node->next.get())
    {
        ++page.shown;
    }
}

// swd3e.exe 0x0044DA00: the list of the tab's role made (the empty entry when it has none) and laid out in cells: a
// heading starts in the first column (a filler before it otherwise) and has a filler after it; the first skill cell of
// the first column chosen
void Menu::SkillsFillList()
{
    SkillsPage& page = this->skills;
    ItemLink& head = this->SkillsRoleList();

    if (head != nullptr)
    {
        this->SkillsTake(head->next);
    }

    if (page.list == nullptr)
    {
        page.list = NewEmptyItem();
    }

    page.cells.assign(SkillsPage::CellBytes, SkillsPage::NoCell);
    page.hasSkills = 0;
    int cell = 0;
    ItemLink* link = &page.list;

    // The original writes past its map for lists of more cells; the cells past it are not kept here.
    auto mark = [&page](int at, SkillsPage::Cell kind)
    {
        const std::size_t index = SkillsPage::CellBase + static_cast<std::size_t>(at);

        if (index < page.cells.size())
        {
            page.cells[index] = kind;
        }
    };

    for (ItemNode* node = page.list.get(); node != nullptr; node = node->next.get())
    {
        if ((node->record.data.kind & kHeadingKind) == 0)
        {
            if (node->id != SkillsPage::FillerId)
            {
                mark(cell, SkillsPage::SkillCell);
                page.hasSkills = 1;
            }
            else
            {
                mark(cell, SkillsPage::FillerCell);
            }
        }
        else
        {
            if ((cell & kOddCell) != 0)
            {
                std::unique_ptr<ItemNode> before = NewFiller();
                before->next = std::move(*link);
                *link = std::move(before);
                mark(cell, SkillsPage::FillerCell);
                link = &(*link)->next;
                ++cell;
            }

            mark(cell, SkillsPage::HeadingCell);
            std::unique_ptr<ItemNode> after = NewFiller();
            after->next = std::move(node->next);
            node->next = std::move(after);
        }

        link = &(*link)->next;
        ++cell;
    }

    page.column = 0;
    page.count = CountItems(page.list);
    page.top = 0;
    page.row = 0;
    page.firstShown = page.list.get();
    this->SkillsCountShown();

    if (page.hasSkills != 0)
    {
        while (this->SkillsCell(page.top + page.row) != SkillsPage::SkillCell &&
               static_cast<std::size_t>(page.row) < SkillsPage::CellBytes)
        {
            page.row += kColumns;
        }
    }
}

// swd3e.exe 0x0044DB90: the list's skills back at the front of the role's list one by one (their order turns around);
// the empty entry and the fillers freed
void Menu::SkillsReturn()
{
    SkillsPage& page = this->skills;
    ItemLink& head = this->SkillsRoleList();

    while (page.list != nullptr)
    {
        std::unique_ptr<ItemNode> node = std::move(page.list);
        page.list = std::move(node->next);

        if (node->id == ItemNode::Empty || node->id == SkillsPage::FillerId || head == nullptr)
        {
            continue;
        }

        node->next = std::move(head->next);
        head->next = std::move(node);
    }
}

// swd3e.exe 0x0044DC10: three, and one for each of the two extra flags of the target slot (SkillsPage::tabs, never
// read)
void Menu::SkillsCountTabs()
{
    SkillsPage& page = this->skills;
    page.tabs = kBaseTabs;

    if (this->game.state.GameFlag(kTabFlagsPerRole * page.target + kFirstExtraTabFlag))
    {
        ++page.tabs;
    }

    if (this->game.state.GameFlag(kTabFlagsPerRole * page.target + kFirstExtraTabFlag + 1))
    {
        ++page.tabs;
    }
}

// swd3e.exe 0x0044BF50: down: on the list the cell two on (a line down, scrolling a line at the bottom), in the column
// left or right chose when that cell is a skill; on the roles the next one; on the towns the next town
void Menu::SkillsDown()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    if (page.stage == kSkillList)
    {
        if (page.hasSkills == 0)
        {
            return;
        }

        int cell = page.row + kColumns;

        if (cell >= page.shown)
        {
            if (page.top + page.shown < page.count)
            {
                page.top += kColumns;
            }

            cell = page.row;
        }

        page.row = cell & ~kOddCell;

        if (this->SkillsCell((cell | kOddCell) + page.top) == SkillsPage::SkillCell && page.column != 0)
        {
            page.row = cell | kOddCell;
        }

        SkipItems(page.top, page.list, page.firstShown);
        CountShown(page.firstShown, page.shown, kRows);
        this->SkillsDescribe();
        page.scrollArrows = kScrollDown;
        owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
        return;
    }

    if (page.stage == kChooseRole)
    {
        if (++page.target >= this->VisibleCount())
        {
            page.target = this->VisibleCount() - 1;
        }

        return;
    }

    if (page.stage == kChooseTown)
    {
        ListDown(static_cast<i32>(this->towns.size()), this->townTop, this->townRow, this->townShown);
        page.scrollArrows |= kTownScrollDown;
    }
}

// swd3e.exe 0x0044C090: up: on the list the cell two back (a line up, scrolling a line at the top), in the column left
// or right chose when that cell is a skill; on the roles the one before; on the towns the town before
void Menu::SkillsUp()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    if (page.stage == kSkillList)
    {
        if (page.hasSkills == 0)
        {
            return;
        }

        int cell = page.row - kColumns;

        if (cell < 0)
        {
            cell = page.row;

            if (page.top > 0)
            {
                page.top -= kColumns;
            }
        }

        page.row = cell & ~kOddCell;

        if (this->SkillsCell((cell | kOddCell) + page.top) == SkillsPage::SkillCell && page.column != 0)
        {
            page.row = cell | kOddCell;
        }

        SkipItems(page.top, page.list, page.firstShown);
        CountShown(page.firstShown, page.shown, kRows);
        this->SkillsDescribe();
        page.scrollArrows = kScrollUp;
        owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
        return;
    }

    if (page.stage == kChooseRole)
    {
        if (--page.target < 0)
        {
            page.target = 0;
        }

        return;
    }

    if (page.stage == kChooseTown)
    {
        ListUp(this->townTop, this->townRow);
        page.scrollArrows |= kTownScrollUp;
    }
}

// swd3e.exe 0x0044C1B0: Page Down: on the list to the last line shown (first column; quietly), from there a page of
// 24 cells on (the last line when there is none); on the roles the slot before the last (a loop kept from swd32 steps
// back over tabs without a role: -1 with one role); on the towns a page of 8 on
void Menu::SkillsPageDown()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    if (page.stage != kSkillList)
    {
        if (page.stage == kChooseRole)
        {
            // Past the first tab the original reads the word before the tabs (0x004BE5B8, never written: 0).
            int slot = this->VisibleCount() - 1;

            do
            {
                --slot;
            } while (slot >= 0 && this->tabs[static_cast<std::size_t>(slot)].role == Menu::NoRole);

            page.target = slot;
        }
        else if (page.stage == kChooseTown)
        {
            ListPageDown(
                static_cast<i32>(this->towns.size()),
                this->townTop,
                this->townRow,
                this->townShown,
                kTownRows
            );
            page.scrollArrows |= kTownScrollDown;
        }

        return;
    }

    page.row &= ~kOddCell;
    const int last = page.shown - ((page.shown & kOddCell) == 0 ? 1 : 0);

    if (page.row < last - 1)
    {
        const int cell = (page.row & kOddCell) + last - 1;
        page.row = cell;

        if (cell >= page.shown)
        {
            page.row = cell - kColumns;
        }

        return;
    }

    page.top += kRows;

    if (page.top >= page.count)
    {
        page.top -= kRows;
        page.row = last - 1;
    }
    else
    {
        SkipItems(page.top, page.list, page.firstShown);
        this->SkillsCountShown();

        if (page.row >= page.shown)
        {
            page.row = (page.row & kOddCell) - ((page.shown & kOddCell) == 0 ? 1 : 0) + page.shown - 1;

            if (page.row >= page.shown)
            {
                page.row -= kColumns;
            }

            return;
        }
    }

    SkipItems(page.top, page.list, page.firstShown);
    CountShown(page.firstShown, page.shown, kRows);

    if (page.row >= page.shown)
    {
        page.row -= kColumns;
    }

    this->SkillsDescribe();
    page.scrollArrows = kScrollDown;
    owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044C350: Page Up: on the list to the first line shown (quietly), from there a page of 24 cells back
// (the column kept); on the roles the first; on the towns a page of 8 back
void Menu::SkillsPageUp()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    if (page.stage == kSkillList)
    {
        page.row &= ~kOddCell;

        if (page.row > 1)
        {
            page.row &= kOddCell;
            return;
        }

        page.top -= kRows;

        if (page.top >= 0)
        {
            SkipItems(page.top, page.list, page.firstShown);
            this->SkillsCountShown();
            page.row &= kOddCell;
        }
        else
        {
            page.row &= kOddCell;
            page.top = 0;
            SkipItems(0, page.list, page.firstShown);
            this->SkillsCountShown();
        }

        this->SkillsDescribe();
        page.scrollArrows = kScrollUp;
        owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
        SkipItems(page.top, page.list, page.firstShown);
        CountShown(page.firstShown, page.shown, kRows);
        this->SkillsDescribe();
        return;
    }

    if (page.stage == kChooseRole)
    {
        page.target = 0;
        return;
    }

    if (page.stage == kChooseTown)
    {
        ListPageUp(this->townTop, this->townRow, kTownRows);
        page.scrollArrows |= kTownScrollUp;
    }
}

// The other column of the line (the last cell shown at most; the first column unless that cell is a skill), which
// down and up then keep (inline in 0x0044C4A0 and 0x0044C590)
void Menu::SkillsOtherColumn(u16 arrows)
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;
    page.row += (page.row & kOddCell) != kOddCell ? 1 : -1;

    if (page.row >= page.shown)
    {
        page.row = page.shown - 1;
    }

    if (this->SkillsCell(page.top + page.row) != SkillsPage::SkillCell)
    {
        page.row = static_cast<i32>(static_cast<u32>(page.row) & kEvenCells);
    }

    page.column = page.row & kOddCell;
    this->SkillsDescribe();
    owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
    page.scrollArrows = arrows;
}

// swd3e.exe 0x0044C4A0: left: on the list the other column; on the roles the one before; on the towns the town before
void Menu::SkillsLeft()
{
    SkillsPage& page = this->skills;

    if (page.stage == kSkillList)
    {
        this->SkillsOtherColumn(kScrollUp);
        return;
    }

    if (page.stage == kChooseRole)
    {
        if (--page.target < 0)
        {
            page.target = 0;
        }

        return;
    }

    if (page.stage == kChooseTown)
    {
        ListUp(this->townTop, this->townRow);
        page.scrollArrows |= kTownScrollUp;
    }
}

// swd3e.exe 0x0044C590: right: on the list the other column; on the roles the next one; on the towns the next town
void Menu::SkillsRight()
{
    SkillsPage& page = this->skills;

    if (page.stage == kSkillList)
    {
        this->SkillsOtherColumn(kScrollDown);
        return;
    }

    if (page.stage == kChooseRole)
    {
        if (++page.target >= this->VisibleCount())
        {
            page.target = this->VisibleCount() - 1;
        }

        return;
    }

    if (page.stage == kChooseTown)
    {
        ListDown(static_cast<i32>(this->towns.size()), this->townTop, this->townRow, this->townShown);
        page.scrollArrows |= kTownScrollDown;
    }
}

// swd3e.exe 0x0044C680: Shift on the list: the tab after SkillsPage::roleCycle (which it keeps), its list made
void Menu::SkillsShift()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    if (page.stage != kSkillList)
    {
        return;
    }

    page.roleCycle = (page.roleCycle + 1) % this->VisibleCount();
    this->SkillsReturn();
    this->roleTab = static_cast<u16>(page.roleCycle);

    if (static_cast<i16>(this->roleTab) >= this->VisibleCount())
    {
        this->roleTab = 0;
    }

    this->SkillsCountTabs();
    this->SkillsFillList();
    FitListRow(page.count, page.top, page.row, page.shown);
    SkipItems(page.top, page.list, page.firstShown);
    CountShown(page.firstShown, page.shown, kRows);
    this->SkillsDescribe();
    owner.audio.PlaySound(kRoleSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044C750: End on the list: the list made again
void Menu::SkillsEnd()
{
    Game& owner = this->game;

    if (this->skills.stage != kSkillList)
    {
        return;
    }

    this->SkillsReturn();
    this->SkillsFillList();
    this->SkillsDescribe();
    owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044C7A0: Space or Enter. On the list a skill usable here (the role not defeated) whose cost the role
// can pay is used: the exit warp warps out at once (its spirit paid), the town warp opens the towns, one working on
// every party role works on each (its costs paid), another asks for a role (Menu::code 3); on the roles the costs are
// paid and the skill works on the role chosen; the description closes; on the towns the party warps (the spirit
// paid); a refusal closes.
void Menu::SkillsSelect()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    switch (page.stage)
    {
        case kSkillList:
        {
            const ItemNode* chosen = ItemAfter(page.top + page.row, page.list);

            if (chosen == nullptr || chosen->id == ItemNode::Empty)
            {
                return;
            }

            RoleRecord& role = this->SkillsRole();
            const u16 cost = chosen->record.data.costBits;

            if ((role.condition & RoleRecord::Defeated) != 0 || (chosen->record.data.usableBy & kUseMask) < kFieldUse ||
                !CanPay(role, cost))
            {
                owner.audio.PlaySound(kRefusedSound, owner.config.soundVolume);
                return;
            }

            if (chosen->id == kExitWarp)
            {
                const std::size_t warp =
                    owner.state.GameFlag(kExitWarpBlocked) ? 0 : this->FindExitWarp(owner.currentMap);

                if (warp == 0)
                {
                    page.stage = kNoExitWarp;
                    owner.audio.PlaySound(kRefusedSound, owner.config.soundVolume);
                    return;
                }

                role.spirit = static_cast<i16>(role.spirit - (cost & kSpiritMask));
                const std::vector<u8>& maps = owner.databases.mapsData;
                this->Warp(
                    MapsWord(maps, warp),
                    MapsWord(maps, warp + sizeof(u16)),
                    MapsWord(maps, warp + 2 * sizeof(u16)),
                    kWarpEvent
                );
                this->SkillsLeave();
                owner.menuState = Game::NoMenu;
                return;
            }

            if (chosen->id == kTownWarp)
            {
                if (owner.state.GameFlag(kTownWarpBlocked) || owner.scene.townWarpAllowed != 1)
                {
                    page.stage = kNoTownWarp;
                    owner.audio.PlaySound(kRefusedSound, owner.config.soundVolume);
                    return;
                }

                page.stage = kChooseTown;
                this->ListTowns();
                this->townShown = kTownRows;
                this->townTop = 0;
                this->townRow = 0;

                if (static_cast<i32>(this->towns.size()) < kTownRows)
                {
                    this->townShown = static_cast<i32>(this->towns.size());
                }

                return;
            }

            page.skill = SkillScript{};

            if (!owner.database.LoadSkill(page.skill, chosen->record.data.skillScript))
            {
                return;
            }

            if ((page.skill.flags & kWholeParty) == 0)
            {
                ++page.stage;
                this->code = static_cast<u16>(page.stage + 1);
                return;
            }

            if ((cost & kSpiritCost) != 0 && role.spirit >= (cost & kSpiritMask))
            {
                role.spirit = static_cast<i16>(role.spirit - (cost & kSpiritMask));
            }

            if ((cost & kStaminaCost) != 0 && role.stamina >= (cost & kStaminaMask))
            {
                role.stamina = static_cast<i16>(role.stamina - (cost & kStaminaMask));
            }

            owner.audio.PlaySound(kUsedSound, owner.config.soundVolume);

            for (int member = 0; member < Party::Roles; ++member)
            {
                if (owner.state.GameFlag(Party::FirstRoleFlag + member))
                {
                    ApplyObject(
                        owner.party.roles[static_cast<std::size_t>(member)],
                        chosen->record.data,
                        owner.database
                    );
                }
            }

            return;
        }

        case kChooseRole:
        {
            const ItemNode* chosen = ItemAfter(page.top + page.row, page.list);

            if (chosen == nullptr || !PayCost(this->SkillsRole(), chosen->record.data.costBits))
            {
                owner.audio.PlaySound(kRefusedSound, owner.config.soundVolume);
                return;
            }

            // A target of -1 (Page Down with one role) reads the flag before the party's; its record is not
            // written here.
            const int target = this->VisibleRole(page.target);

            if (owner.state.GameFlag(target + Party::FirstRoleFlag) && target >= 0)
            {
                ApplyObject(owner.party.roles[static_cast<std::size_t>(target)], chosen->record.data, owner.database);
                owner.audio.PlaySound(kUsedSound, owner.config.soundVolume);
            }

            return;
        }

        case kDescribing:
            page.stage = kSkillList;
            page.descriptionSlide = -kDescriptionSlide;
            return;

        case kChooseTown:
        {
            if (const ItemNode* chosen = ItemAfter(page.top + page.row, page.list); chosen != nullptr)
            {
                RoleRecord& role = this->SkillsRole();
                role.spirit = static_cast<i16>(role.spirit - (chosen->record.data.costBits & kSpiritMask));
            }

            const auto index = static_cast<std::size_t>(this->townTop + this->townRow);

            if (index < this->towns.size())
            {
                const MenuTown town = this->towns[index];
                this->Warp(town.map, town.x, town.y, kWarpEvent);
            }

            this->ItemsFreeTowns();
            this->SkillsLeave();
            owner.menuState = Game::NoMenu;
            return;
        }

        case kNoTownWarp:
        case kNoExitWarp:
            page.stage = kSkillList;
            return;

        default:
            return;
    }
}

// swd3e.exe 0x0044CDC0: Escape: from the list the page closes (the menu too when no page is left); from the roles back
// to the list (Menu::code one less); the description closes; the towns close; a refusal closes
void Menu::SkillsCancel()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;

    switch (page.stage)
    {
        case kSkillList:
            if (static_cast<i16>(--this->code) <= 0)
            {
                owner.menuState = Game::NoMenu;
            }

            this->SetPage(this->code);
            this->SkillsLeave();
            return;

        case kChooseRole:
            page.stage = kSkillList;
            page.target = 0;
            --this->code;
            return;

        case kDescribing:
            page.stage = kSkillList;
            page.descriptionSlide = -kDescriptionSlide;
            return;

        case kChooseTown:
            this->ItemsFreeTowns();
            page.stage = kSkillList;
            return;

        case kNoTownWarp:
        case kNoExitWarp:
            page.stage = kSkillList;
            return;

        default:
            return;
    }
}

// swd3e.exe 0x0044BAC0: the mouse on the skills page: a refusal closes on a click; the towns opened as a subpage
// (Menu::code 0xF) choose on a click; the description button presses the description key; on the list a click
// chooses a cell (a double click on the chosen one uses it); the held left button on the list's or the towns' scroll
// bar; a click on a town chooses it (on the chosen one, a double click warps); on the roles a click chooses (on the
// chosen one, uses); a click on a role tab steps to it; a right click goes back
void Menu::SkillsFrame()
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;
    u32 clicks = this->clicks;
    i32 stage = page.stage;
    page.buttonOffset = 0;

    if ((clicks & kLeftOrRight) != 0 && (stage == kNoTownWarp || stage == kNoExitWarp))
    {
        this->SkillsSelect();
        return;
    }

    i32 x = owner.state.pointerX;
    i32 y = owner.state.pointerY;

    if (this->code == kTownCode && x < kTownsRight && x > kTownsLeft && y < kTownsBottom && y > kTownsTop &&
        (clicks & kLeftClicks) != 0)
    {
        const auto row = static_cast<i32>(static_cast<u32>(y - kTownsTop) / kRowHeight);

        if (this->townRow == row)
        {
            this->SkillsSelect();
            return;
        }

        this->townRow = row;
        return;
    }

    bool cells = true;  // LABEL_20 in the original

    if (stage == kSkillList && static_cast<u32>(y) < kDescribeBottom && static_cast<u32>(y) > kDescribeTop &&
        static_cast<u32>(x) < kDescribeRight && static_cast<u32>(x) > kDescribeLeft)
    {
        page.buttonOffset = kPressedOffset;

        if ((clicks & kLeftClicks) != 0)
        {
            owner.input.PressKey(owner.config.keys[static_cast<std::size_t>(Button::F1)]);
            clicks = this->clicks;
            stage = page.stage;
            x = owner.state.pointerX;
            y = owner.state.pointerY;
        }
        else
        {
            cells = false;
        }
    }

    if (cells && (clicks & kLeftClicks) != 0 && stage == kSkillList && y < kRowsBottom && y > kRowsTop &&
        x < kRowsRight && x > kRowsLeft)
    {
        const int cell = (x - kRowsLeft) / kRowsColumnWidth + kColumns * ((y - kRowsTop) / kRowHeight);

        if (cell < page.shown)
        {
            if (cell == page.row)
            {
                if ((clicks & kDoubleClick) != 0)
                {
                    this->SkillsSelect();
                }
            }
            else
            {
                page.row = cell;

                if (this->SkillsCell(page.top + cell) != SkillsPage::SkillCell)
                {
                    page.row = cell & ~kOddCell;
                }

                this->SkillsDescribe();
                owner.audio.PlaySound(kMoveSound, owner.config.soundVolume);
            }
        }

        return;
    }

    stage = page.stage;

    if (owner.controls.Repeating(Button::MouseLeft))
    {
        const auto pointerX = static_cast<u32>(owner.state.pointerX);

        if (stage == kSkillList && page.count > kRows && pointerX < kBarRight && pointerX > kBarLeft)
        {
            const MenuScrollBar& bar = this->scrollBar;
            i32 pointerY = owner.state.pointerY;

            if (pointerY < kBarUpBottom && pointerY > kBarUpTop)
            {
                this->SkillsUp();
                pointerY = owner.state.pointerY;
            }

            if (pointerY < kBarDownBottom && pointerY > kBarDownTop)
            {
                this->SkillsDown();
                pointerY = owner.state.pointerY;
            }

            if (pointerY < bar.thumbTop && pointerY > bar.top)
            {
                this->SkillsPageUp();
                pointerY = owner.state.pointerY;
            }

            if (pointerY < bar.bottom && pointerY > bar.thumbBottom)
            {
                this->SkillsPageDown();
            }

            return;
        }

        if (stage == kChooseTown)
        {
            if (static_cast<i32>(this->towns.size()) > kTownRows && pointerX < kTownsBarRight &&
                pointerX > kTownsBarLeft)
            {
                const MenuScrollBar& bar = this->townBar;
                i32 pointerY = owner.state.pointerY;

                if (pointerY < kTownsUpBottom && pointerY > kTownsUpTop)
                {
                    this->SkillsUp();
                    pointerY = owner.state.pointerY;
                }

                if (pointerY < kTownsDownBottom && pointerY > kTownsDownTop)
                {
                    this->SkillsDown();
                    pointerY = owner.state.pointerY;
                }

                if (pointerY < bar.thumbTop && pointerY > bar.top)
                {
                    this->SkillsPageUp();
                    pointerY = owner.state.pointerY;
                }

                if (pointerY < bar.bottom && pointerY > bar.thumbBottom)
                {
                    this->SkillsPageDown();
                }

                return;
            }

            const auto pointerY = static_cast<u32>(owner.state.pointerY);

            if (pointerX < kTownsInnerRight && pointerX > static_cast<u32>(kTownsLeft) &&
                pointerY < kTownsBottomInside && pointerY > static_cast<u32>(kTownsTop))
            {
                auto row = static_cast<i32>((pointerY - kTownsTop) / kRowHeight);

                if (row >= this->townShown)
                {
                    row = this->townShown - 1;
                }

                if (row != this->townRow)
                {
                    if ((this->clicks & kLeftClicks) != 0)
                    {
                        this->townRow = row;
                    }

                    return;
                }

                if ((this->clicks & kDoubleClick) != 0)
                {
                    this->SkillsSelect();
                }
            }

            return;
        }
    }

    clicks = this->clicks;
    x = owner.state.pointerX;
    y = owner.state.pointerY;

    if ((clicks & kLeftClicks) != 0 && page.stage == kChooseRole)
    {
        if (y < kRoleRowsBottom && y > kRoleRowsTop && x < kRoleRowsRight && x > kRoleRowsLeft)
        {
            const auto slot = static_cast<i32>(static_cast<u32>(y - kRoleRowsTop) / kRowHeight);

            if (slot < this->VisibleCount())
            {
                if (page.target == slot)
                {
                    this->SkillsSelect();
                }

                page.target = slot;
            }

            return;
        }
    }
    else if (
        page.stage == kSkillList && (clicks & kLeftClicks) != 0 && static_cast<u32>(y) < kTabsBottom &&
        static_cast<u32>(y) > kTabsTop
    )
    {
        int chosen = -1;

        for (int slot = 0, left = kFirstTabLeft; slot < this->VisibleCount(); ++slot, left += kTabStep)
        {
            if (x < left + kTabWidth && x > left &&
                owner.state.GameFlag(this->VisibleRole(slot) + Party::FirstRoleFlag))
            {
                chosen = slot;
            }
        }

        if (chosen != -1)
        {
            page.roleCycle = chosen - 1;
            this->SkillsShift();
            return;
        }
    }

    if ((clicks & kRightClick) != 0)
    {
        this->SkillsCancel();
    }
}

// swd3e.exe 0x0044CED0: the skills page: the description key on the list opens the description of the cell chosen;
// the list in two columns of cells (a heading between tildes across both, the unusable grey, those of battle red,
// darker off the list; each skill's cost and element picture; the description button drawn with each cell) with its
// scroll bar; on the roles their box; the chosen skill's description below; the towns; a refusal; the description
// box sliding up or down.
void Menu::SkillsDraw()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    SkillsPage& page = this->skills;
    ActState& panel = owner.battleMessages.panelAct;
    const auto color = static_cast<Pixel>(painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
    Pixel textColor = color;
    const u32 never = painter.MakeColor(kNeverRed, kNeverGreen, kNeverBlue);
    const auto battle = static_cast<Pixel>(painter.MakeColor(kBattleRed, kBattleGreen, kBattleBlue));
    std::array<i32, kElementPictures> elementPictures = {};
    elementPictures[0] = static_cast<i32>(never);
    std::copy(kElementPictureTail.begin(), kElementPictureTail.end(), elementPictures.begin() + 1);

    if (page.stage == kSkillList && owner.input.IsKeyDown(owner.config.keys[static_cast<std::size_t>(Button::F1)]) != 0)
    {
        ItemNode* described = nullptr;
        SkipItems(page.top + page.row, page.list, described);

        if (described != nullptr && described->id != ItemNode::Empty)
        {
            page.descriptionSlide = kDescriptionSlide;
            page.stage = kDescribing;
        }
    }

    const ItemNode* chosen = nullptr;
    owner.acts.Run(panel);
    FillBox(painter, kListBoxX, kListBoxY, kListBoxWidth, kListBoxHeight, 0, 0, 0, FillMode::Quarter);
    DrawWindowFrame(
        painter,
        owner.archive,
        panel.frame,
        kListFrameLeft,
        kListFrameTop,
        kListFrameRight,
        kListFrameBottom,
        0,
        kFrameBorder
    );
    FillBox(painter, kHelpBoxX, kHelpBoxY, kHelpBoxWidth, kHelpBoxHeight, 0, 0, 0, FillMode::Quarter);
    DrawWindowFrame(
        painter,
        owner.archive,
        panel.frame,
        kHelpFrameLeft,
        kHelpFrameTop,
        kHelpFrameRight,
        kHelpFrameBottom,
        0,
        kFrameBorder
    );

    if (page.list != nullptr)
    {
        std::string cost;  // the original's file name buffer, kept from cell to cell
        const ItemNode* node = page.firstShown;

        for (int cell = 0; cell < page.shown && node != nullptr; ++cell, node = node->next.get())
        {
            if (node->id == SkillsPage::FillerId)
            {
                continue;
            }

            const bool heading = (node->record.data.kind & kHeadingKind) != 0;
            int shift = 0;
            Pixel rowColor = color;

            if (cell == page.row)
            {
                chosen = node;
                shift = -1;
            }

            const u16 use = node->record.data.usableBy & kUseMask;

            if (use == kBattleOnly)
            {
                shift = 0;
                rowColor = battle;
            }

            if (use == 0)
            {
                shift = 0;
                rowColor = static_cast<Pixel>(never);
            }

            if (page.stage > kSkillList)
            {
                painter.TintPixels(std::span<u16>(&rowColor, 1), kDim, kDim, kDim);
            }

            std::string text;

            if (heading)
            {
                text = WinFormat(kHeadingFormat, {ObjectName(node->record)});
                rowColor = color;
                shift = 0;
            }
            else
            {
                text = WinFormat(kNameFormat, {ObjectName(node->record)});
            }

            const int column = cell & kOddCell;
            const int line = kRowHeight * (cell / kColumns);
            owner.largeFont.DrawText(
                owner.drawSurface,
                shift + kColumnWidth * column + kListX,
                shift + line + kListY,
                text,
                rowColor,
                kPanelStyle
            );

            if (page.unk50BAF4 <= 1)
            {
                const u16 costBits = node->record.data.costBits;

                if ((costBits & kSpiritCost) != 0)
                {
                    cost = WinFormat(kCostFormat, {static_cast<i32>(costBits & kSpiritMask)});
                }

                if ((costBits & kStaminaCost) != 0)
                {
                    cost = WinFormat(kCostFormat, {static_cast<i32>(costBits & kStaminaMask)});
                }

                if ((costBits & kCostBits) != 0)
                {
                    owner.mediumFont.DrawText(
                        owner.drawSurface,
                        shift + kColumnWidth * column + kCostX,
                        shift + line + kListY + kCostDown,
                        cost,
                        rowColor,
                        kPanelStyle
                    );
                }
            }

            this->buttonAct.id = kButtonAct;
            this->buttonAct.action = kButtonAction;
            owner.actDrawer.Draw(this->buttonAct, page.buttonOffset + kButtonX, page.buttonOffset + kButtonY);

            if (page.unk50BAF4 <= 1 && node->id != ItemNode::Empty && !heading)
            {
                owner.database.LoadSkill(page.skill, node->record.data.skillScript);
                const auto element = static_cast<u16>(page.skill.unk10);

                // Past the table the original reads its stack; a grey picture number (element 0) is not found and
                // the original reads through a null picture there.
                if (element < kElementPictures && elementPictures[element] >= 0)
                {
                    owner.actDrawer.DrawPicture(
                        kElementPictureFile,
                        static_cast<u16>(elementPictures[element]),
                        shift + kElementColumn * column + kElementX,
                        shift + line + kListY + kElementDown
                    );
                }
            }

            if (cell == page.row && page.stage == kSkillList)
            {
                FillBox(
                    painter,
                    kHighlightColumn * column + kHighlightX,
                    line + kHighlightY,
                    kHighlightWidth,
                    kHighlightHeight,
                    kHighlightRed,
                    kHighlightGreen,
                    0,
                    FillMode::Gradient
                );

                if (heading)
                {
                    FillBox(
                        painter,
                        kHeadingHighlightX,
                        line + kHighlightY,
                        kHighlightWidth,
                        kHighlightHeight,
                        kHighlightRed,
                        kHighlightGreen,
                        0,
                        FillMode::Gradient
                    );
                }
            }
        }
    }
    else
    {
        owner.largeFont.DrawText(owner.drawSurface, kNoneX, kNoneY, kEmptyName, kNoneColor, kPanelStyle);
    }

    if (page.count > kRows)
    {
        int arrows = 0;

        if ((page.scrollArrows & kNibble) != 0)
        {
            page.scrollArrows = static_cast<u16>((page.scrollArrows & ~kNibble) | ((page.scrollArrows & kNibble) - 1));
            arrows = 1;
        }

        const u16 downMask = kNibble << kNibbleShift;

        if ((page.scrollArrows & downMask) != 0)
        {
            const int down = (page.scrollArrows & downMask) >> kNibbleShift;
            arrows |= 2;
            page.scrollArrows = static_cast<u16>((page.scrollArrows & ~downMask) | ((down - 1) << kNibbleShift));
        }

        const auto count = static_cast<double>(page.count);
        const auto to = static_cast<float>(static_cast<double>(page.top + page.shown) / count);
        const auto from = static_cast<float>(static_cast<double>(page.top) / count);
        this->DrawScrollBar(kBarX, kBarY, kBarHeight, arrows, from, to, this->scrollBar);
    }

    if (page.stage == kChooseRole)
    {
        const int roles = this->VisibleCount();
        FillBox(
            painter,
            kRolesBoxX,
            kRolesBoxY,
            kRolesBoxWidth,
            kRowHeight * roles + kRolesBoxPad,
            kRolesBoxRed,
            kRolesBoxGreen,
            kRolesBoxBlue,
            FillMode::Eighth
        );
        DrawWindowFrame(
            painter,
            owner.archive,
            panel.frame,
            kRolesFrameLeft,
            kRolesFrameTop,
            kRolesFrameRight,
            kRowHeight * roles + kRolesFrameTop,
            0,
            kFrameBorder
        );

        for (int slot = 0, line = 0; slot < this->VisibleCount(); ++slot, line += kRowHeight)
        {
            int shift = 0;
            textColor = static_cast<Pixel>(painter.MakeColor(kRoleRed, kRoleGreen, kRoleBlue));

            if (slot == page.target)
            {
                textColor = static_cast<Pixel>(painter.MakeColor(kRoleChosenRed, kRoleChosenGreen, kRoleChosenBlue));
                shift = -1;
            }

            const int role = std::clamp(this->VisibleRole(slot), 0, Party::Roles - 1);
            owner.mediumFont.DrawText(
                owner.drawSurface,
                shift + kRoleTextX,
                line + shift + kRoleTextY,
                owner.party.names[static_cast<std::size_t>(role)].FullName(),
                textColor,
                kPanelStyle
            );

            if (slot == page.target)
            {
                FillBox(
                    painter,
                    kRoleHighlightX,
                    line + kRoleHighlightY,
                    kRoleHighlightWidth,
                    kRoleHighlightHeight,
                    kHighlightRed,
                    kHighlightGreen,
                    0,
                    FillMode::Gradient
                );
            }
        }
    }

    if (chosen != nullptr && chosen->id != ItemNode::Empty)
    {
        owner.mediumFont.SetClip(kHelpTextClipX, kHelpTextClipY, kHelpTextClipWidth, kHelpTextClipHeight);
        owner.mediumFont.DrawText(
            owner.drawSurface,
            kHelpTextX,
            kHelpTextY,
            this->description,
            static_cast<Pixel>(painter.MakeColor(kTextRed, kTextGreen, kTextBlue)),
            kPanelStyle
        );
        owner.mediumFont.SetClip(0, 0, Display::Width, Display::Height);
    }

    if (page.stage == kChooseTown)
    {
        this->SkillsDrawTowns(textColor);
    }

    if (page.stage == kNoTownWarp || page.stage == kNoExitWarp)
    {
        const std::optional<std::string>& given =
            page.stage == kNoTownWarp ? owner.scene.noTownWarpText : owner.scene.noExitWarpText;
        std::string text = given.value_or(Big5(kCannotHere));
        text.resize(kMessageLineBytes * kMessageLines, '\0');
        this->DrawBox(kMessageX, kMessageY, kMessageWidth, kMessageHeight, kMessageBoxMode, 0, 0, 0);

        for (int line = 0; line < kMessageLines; ++line)
        {
            const char* start = text.data() + kMessageLineBytes * static_cast<std::size_t>(line);
            owner.largeFont.DrawText(
                owner.drawSurface,
                kMessageX,
                kMessageY + kMessageLineHeight * line,
                std::string_view(start, strnlen(start, kMessageLineBytes)),
                textColor,
                kPanelStyle
            );
        }
    }

    if (page.descriptionSlide != 0 || page.descriptionTop == kDescriptionUp)
    {
        const i32 topBefore = page.descriptionTop;
        const int half = page.descriptionSlide >> 1;
        page.descriptionSlide = half;
        page.descriptionTop -= half;

        if (half <= 0)
        {
            if (page.descriptionTop > kDescriptionHidden)
            {
                page.descriptionTop = kDescriptionHidden;
                page.descriptionSlide = 0;
            }
        }
        else if (page.descriptionTop < kDescriptionUp)
        {
            page.descriptionTop = kDescriptionUp;
            page.descriptionSlide = 0;
        }

        this->Move(SkillsDescriptionValue, page.descriptionTop, topBefore, page.descriptionTop);
        const i32 top = this->Shown(SkillsDescriptionValue, page.descriptionTop);
        const ItemNode* described = ItemAfter(page.top + page.row, page.list);
        FillBox(
            painter,
            kDescriptionBoxX,
            top - kDescriptionBoxUp,
            kDescriptionBoxWidth,
            kDescriptionBoxBottom - top,
            kDescriptionShade,
            kDescriptionShade,
            kDescriptionBlue,
            FillMode::Eighth
        );
        DrawWindowFrame(
            painter,
            owner.archive,
            panel.frame,
            kDescriptionTextX,
            top,
            kDescriptionFrameRight,
            kDescriptionFrameBottom,
            0,
            kFrameBorder
        );
        owner.DrawWrappedText(
            described != nullptr ? described->record.description.value_or(std::string()) : std::string(),
            kDescriptionTextX,
            top,
            kDescriptionLines,
            kDescriptionWidth
        );
    }
}

// The towns of the town warp with their bar (inline in 0x0044CED0).
void Menu::SkillsDrawTowns(Pixel color)
{
    Game& owner = this->game;
    SkillsPage& page = this->skills;
    this->DrawBox(kTownsBoxX, kTownsBoxY, kTownsBoxWidth, kTownsBoxHeight, kTownsBoxMode, 0, 0, kTownsBoxBlue);

    for (int row = 0, y = kTownsY; row < this->townShown; ++row, y += kRowHeight)
    {
        const auto index = static_cast<std::size_t>(row + this->townTop);

        if (index + 1 > this->towns.size())
        {
            break;
        }

        owner.largeFont.DrawText(owner.drawSurface, kTownsX, y + 2, this->towns[index].name, color, kPanelStyle);

        if (row == this->townRow)
        {
            FillBox(
                owner.painter,
                kTownHighlightX,
                y,
                kTownHighlightWidth,
                kHighlightHeight,
                kHighlightRed,
                kHighlightGreen,
                0,
                FillMode::Gradient
            );
        }
    }

    if (static_cast<i32>(this->towns.size()) <= this->townShown)
    {
        return;
    }

    int arrows = 0;
    const u16 upMask = kNibble << (2 * kNibbleShift);
    const u16 downMask = kNibble << (3 * kNibbleShift);

    if ((page.scrollArrows & upMask) != 0)
    {
        // As the original, the low byte (the list's arrows) comes out 0xFF.
        page.scrollArrows = static_cast<u16>((page.scrollArrows & ~upMask) | ((page.scrollArrows & upMask) - 1));
        arrows = 1;
    }

    if ((page.scrollArrows & downMask) != 0)
    {
        page.scrollArrows = static_cast<u16>(page.scrollArrows - (1u << (3 * kNibbleShift)));
        arrows |= 2;
    }

    const auto count = static_cast<double>(this->towns.size());
    const auto to = static_cast<float>(static_cast<double>(this->townShown + this->townTop) / count);
    const auto from = static_cast<float>(static_cast<double>(this->townTop) / count);
    this->DrawScrollBar(kTownsBarX, kTownsBarY, kTownsBarHeight, arrows, from, to, this->townBar);
}

// swd3e.exe 0x00441670: the exit warp of map `map` in maps.dat table 0x16 (its map, x and y, then the maps it serves,
// each list ended by 0xFFFF); 0 when the map has none
std::size_t Menu::FindExitWarp(int map) const
{
    const std::vector<u8>& maps = this->game.databases.mapsData;
    std::size_t entry = MapsTable(maps, MapsDataTable::ExitWarps);

    if (TableWord(maps, entry) == kTableEnd)
    {
        return 0;
    }

    while (entry + sizeof(u16) <= maps.size())
    {
        std::size_t at = entry + kTownWords * sizeof(u16);

        for (u16 word = TableWord(maps, at); word != kTableEnd; word = TableWord(maps, at))
        {
            if (static_cast<int>(word) == map)
            {
                return entry;
            }

            at += sizeof(u16);
        }

        entry = at + sizeof(u16);

        if (TableWord(maps, entry) == kTableEnd)
        {
            return 0;
        }
    }

    return 0;
}

// swd3e.exe 0x004416C0: the towns of maps.dat table 0x17 the party may warp to: each a name ended by %Q, its map, x
// and y, then the flags (from 0x1388) any of which opens it, ended by 0xFFFF (names past 0x40 bytes are cut, where the
// original writes past its buffer)
void Menu::ListTowns()
{
    const Game& owner = this->game;
    const std::vector<u8>& maps = owner.databases.mapsData;
    this->towns.clear();
    std::size_t at = MapsTable(maps, MapsDataTable::Towns);

    if (TableWord(maps, at) == kTableEnd)
    {
        return;
    }

    std::size_t flags = at;

    do
    {
        std::string name;

        while (at + 1 < maps.size() && TableWord(maps, at) != kTownNameEnd)
        {
            if (name.size() < kTownNameBytes)
            {
                name.push_back(static_cast<char>(maps[at]));
            }

            ++at;
        }

        const std::size_t place = at + sizeof(u16);
        flags = place + kTownWords * sizeof(u16);
        bool open = false;
        u16 flag = TableWord(maps, flags);

        if (flag != kTableEnd)
        {
            do
            {
                if (owner.state.GameFlag(flag + kTownFlags))
                {
                    open = true;
                }

                flag = TableWord(maps, flags + sizeof(u16));
                flags += sizeof(u16);
            } while (flag != kTableEnd);

            if (open && this->towns.size() < kTownsMost)
            {
                this->towns.push_back(
                    {TableWord(maps, place),
                     TableWord(maps, place + sizeof(u16)),
                     TableWord(maps, place + 2 * sizeof(u16)),
                     name}
                );
            }
        }

        at = flags + sizeof(u16);
    } while (flags + 2 * sizeof(u16) <= maps.size() && TableWord(maps, flags + sizeof(u16)) != kTableEnd);
}

// swd3e.exe 0x004417F0: the party warped to (map, x, y): the screen cleared, the map changed, the quake stopped, the
// leader set to run event `event` from the controlled role's place
void Menu::Warp(u16 map, u16 x, u16 y, i16 event)
{
    Game& owner = this->game;
    std::fill_n(owner.drawSurface, static_cast<std::size_t>(Display::Width) * Display::Height, u16{0});
    owner.ShowFrame();
    const Role& player = owner.state.roles[static_cast<std::size_t>(owner.state.playerRole)];
    owner.ChangeMap(map, x, y, player.act.id, 0, 1, 1);
    owner.scene.shake.fill(Scene::ShakeEnd);
    Role& leader = owner.state.leader;
    leader.event = static_cast<u16>(event);
    leader.eventOffset = 0;
    leader.eventCursor = 0;
    leader.id = kWarpLeader;
    leader.flags = player.flags;
    leader.x = player.x;
    leader.y = player.y;
}

}  // namespace swd3e
