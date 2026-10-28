#include <algorithm>
#include <climits>

#include "core/format.h"
#include "game/databases.h"
#include "game/game.h"
#include "ghost/talisman_ghost.h"
#include "menu/menu.h"
#include "pic/rle.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

constexpr std::u8string_view kNone = u8"無";                     // 0x004AF630
constexpr std::u8string_view kFeedDirectly = u8"直接餵食";       // 0x004BEB9C
constexpr std::u8string_view kTooFull = u8"太飽了 無法餵食";     // 0x004BEB84
constexpr std::u8string_view kUnsuited = u8"屬性不合 無法餵食";  // 0x004BEB70
constexpr std::string_view kCountFormat = "%-02d";

// 0x004AACD0: the icon of a skill by its script's unk10 (the first word belongs to the table before it in the
// original; read as it is)
constexpr std::array<i32, 10> kSkillIcons = {0x10002000, 0, 1, 2, 3, 4, 5, 6, -1, -1};
// 0x004AACF8: the object category of each tab; read on by category too (0x0044B670), into the food values after it
constexpr std::array<i32, 12> kTabCategories = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0xA, 0x12, 0};
// 0x004AAD34..0x004AAD93: the food value of a category, and on into the icon offsets that follow it (0x0044B670
// reads index 0x12 for category 0xA)
constexpr std::array<i32, 0x18> kFoodValues = {
    0x12, 0xF, 0xC, 0xA, 8, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 3, 0, 0, 0,
};

constexpr std::array<i32, 10> kIconX = {0, 0, 0, 0, 1, 4, 3, 0, 0, 0};  // 0x004AAD6C
constexpr std::array<i32, 9> kIconY = {0, 0, 0, 1, 1, 4, 3, 0, 0};      // 0x004AAD94
// 0x004AADB8: by the ghost chosen, the icon of the skills it refuses (屬性不合)
constexpr std::array<i32, 6> kUnsuitedIcons = {0, 3, 0, 4, 2, 1};
constexpr i32 kDoubledIcon = 6;  // a skill of this icon only goes into the third slot

constexpr int kChartSide = 0x78;
constexpr int kChartCenter = 0x3C;
constexpr float kChartSlope = 0.4472135901451111f;  // 0x004AADD8 (and its negation at 0x004AADD0)
constexpr float kChartHalf = 0.5f;                  // 0x004AADD4
constexpr int kChartTipY = 0xA;
constexpr int kChartLeftX = 0x10;
constexpr int kChartRightX = 0x68;
constexpr int kChartBottomY = 0x52;
constexpr u8 kChartChannelMax = 0xFF;
constexpr u32 kChannelBits = CHAR_BIT;

constexpr i32 kSatietyLimit = 0x64;
constexpr i32 kGrowthSatietyLimit = 0xC8;
constexpr i32 kStrengthLimit = 0x1F4;
constexpr i32 kStatLimit = 0x64;
constexpr i32 kSpiritLimit = 0x270F;
constexpr i32 kSpiritPerEssence = 5;
constexpr u32 kSpiritRange = 5;
constexpr i32 kFoodLevelScale = 2;

constexpr int kRowHeight = 0x18;
constexpr int kOptionHeight = 0x16;
constexpr u16 kGaugeFile = 0x23B3;
constexpr u16 kMarkerFile = 0x23BA;
constexpr u16 kTabIconFile = 0x2346;
constexpr u16 kTabChosenFile = 0x2362;
constexpr u16 kSkillIconFile = 0x23AC;
constexpr int kGaugeStep = 0x32;
constexpr int kGaugeScale = 7;  // the gauges are 128 pixels long
constexpr i32 kTickWrap = 0xC8;
constexpr u32 kPressedMask = 0xF;
constexpr u32 kUpArrowFrames = 0xF0;
constexpr u32 kUpArrowFrame = 0x10;
constexpr u32 kArrowShift = 4;
constexpr u32 kArrowsByte = UCHAR_MAX;  // the byte of `arrows` the two counts share
constexpr i32 kTextRed = 0x19;
constexpr i32 kTextGreen = 0x17;
constexpr i32 kTextBlue = 0x11;
constexpr int kTextStyle = TextFont::BoldShadow;

// The highlight of the list, the options and the slots: lit when the stage is theirs.
struct Highlight
{
    int red = 0;
    int green = 0;
    int blue = 0;
};

constexpr Highlight kLit = {0x14, 0xD, 0};
constexpr Highlight kDim = {0xC, 5, -8};

i32 Clamp(i32 value, i32 low, i32 high)
{
    return value > high ? high : value < low ? low : value;
}

}  // namespace

// swd3e.exe 0x00447F60: with a ghost chosen, the page opens on the first tab: the chart buffer, the list of the tab,
// the ghost's slots and the description; no category compared
void Menu::GhostEnter()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    page.stage = GhostPage::Choosing;
    page.option = 0;
    page.slot = 0;
    page.tab = 0;
    this->GhostMakeChartBuffer();
    page.slots = GhostSlots{};
    this->GhostBuildList(page.tab);
    this->GhostRow();
    this->GhostMakeSlots();
    this->GhostDescribe();
    page.compared = -1;
    page.strengthChange = 0;
    page.essenceChange = 0;
    page.wisdomChange = 0;
    page.agilityChange = 0;
}

// swd3e.exe 0x00448010: the page left: the slots, the list and the charts freed
void Menu::GhostLeave()
{
    GhostPage& page = this->ghost;
    page.stage = GhostPage::Choosing;
    this->GhostFreeSlots();
    this->GhostFreeList();
    page.slots.reset();
    this->GhostFreeChart();
}

// swd3e.exe 0x00448060: the mouse on the page: the tab icons step the tab, a click on a row of the list chooses it (a
// double click opens it), the scroll bar's arrows and track page, the options and the slots choose (a double click on
// a slot feeds), a right click goes back. Without a ghost only the right click acts.
void Menu::GhostFrame()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    const ButtonState& left = owner.controls.At(Button::MouseLeft);
    const ButtonState& right = owner.controls.At(Button::MouseRight);
    const bool rightClick = right.heldFrames == 1 && (static_cast<u32>(right.press) & kPressedMask) == 1;

    if (owner.party.variables[Party::GhostVariable] == 0)
    {
        if (rightClick)
        {
            this->GhostCancel();
        }

        return;
    }

    if (page.stage == GhostPage::TooFull || page.stage == GhostPage::Unsuited)
    {
        if (rightClick || (left.heldFrames == 1 && (static_cast<u32>(left.press) & kPressedMask) == 1))
        {
            this->GhostCancel();
        }

        return;
    }

    constexpr u32 kTabsTop = 0x5A;
    constexpr u32 kTabsBottom = 0x69;
    constexpr u32 kTabsLeft = 0xCB;
    constexpr u32 kTabsRight = 0x198;
    constexpr i32 kTabsMiddle = 0x134;
    constexpr i32 kTabWidth = 0x16;
    const auto x = static_cast<u32>(owner.state.pointerX);
    const auto y = static_cast<u32>(owner.state.pointerY);

    if (y < kTabsBottom && y > kTabsTop && x < kTabsRight && x > kTabsLeft &&
        owner.controls.Repeating(Button::MouseLeft))
    {
        page.stage = GhostPage::Choosing;
        const i32 step = (static_cast<i32>(x) - kTabsMiddle) / kTabWidth;

        if (step > 0)
        {
            this->GhostRight();
        }

        if (step < 0)
        {
            this->GhostLeft();
        }

        return;
    }

    constexpr u32 kListTop = 0x74;
    constexpr u32 kListBottom = 0x132;
    constexpr u32 kListLeft = 0xCD;
    constexpr u32 kListRight = 0x192;
    const bool leftPressed = (static_cast<u32>(left.press) & kPressedMask) != 0;
    const bool leftDouble = left.heldFrames == 1 && (static_cast<u32>(left.press) & kPressedMask) == 2;

    if (y < kListBottom && y > kListTop && x < kListRight && x > kListLeft && leftPressed)
    {
        const i32 before = page.row;
        page.row = static_cast<i32>((y - kListTop) / kRowHeight);
        page.option = 0;  // 0x0044AF80 looks the row up and clears the option

        if (page.foods[static_cast<std::size_t>(page.row + page.top)].id != 0)
        {
            page.stage = GhostPage::Choosing;
            this->GhostMove(0, 0);
            return;
        }

        page.row = before;

        if (leftDouble)
        {
            this->GhostSelect();
        }

        return;
    }

    constexpr u32 kBarLeft = 0x198;
    constexpr u32 kBarRight = 0x1A8;
    constexpr i32 kUpArrowTop = 0x6E;
    constexpr i32 kUpArrowBottom = 0x7A;
    constexpr i32 kDownArrowTop = 0x130;
    constexpr i32 kDownArrowBottom = 0x140;

    if (owner.controls.Repeating(Button::MouseLeft) && page.count > GhostPage::Rows && x < kBarRight && x > kBarLeft)
    {
        if (page.stage != GhostPage::Choosing)
        {
            page.stage = GhostPage::Choosing;
            this->GhostMove(0, 0);
        }

        const i32 pointerY = owner.state.pointerY;

        if (pointerY < kUpArrowBottom && pointerY > kUpArrowTop)
        {
            this->GhostUp();
            return;
        }

        if (pointerY < kDownArrowBottom && pointerY > kDownArrowTop)
        {
            this->GhostDown();
            return;
        }

        if (pointerY < this->scrollBar.thumbTop && pointerY > this->scrollBar.top)
        {
            this->GhostPageUp();
            return;
        }

        if (pointerY < this->scrollBar.bottom && pointerY > this->scrollBar.thumbBottom)
        {
            this->GhostPageDown();
            return;
        }
    }

    constexpr u32 kOptionsTop = 0x71;
    constexpr u32 kOptionsBottom = 0xDE;
    constexpr u32 kPanelLeft = 0x1AC;
    constexpr u32 kPanelRight = 0x266;
    constexpr u32 kSlotsTop = 0xF2;
    constexpr u32 kSlotsBottom = 0x16A;
    const GhostFood& food = page.foods[page.current];
    const i32 firstSkill = food.skills[0];
    const bool anyPressed = leftPressed || (static_cast<u32>(right.press) & kPressedMask) != 0;

    if (firstSkill != 0 && y < kOptionsBottom && y > kOptionsTop && x < kPanelRight && x > kPanelLeft && anyPressed)
    {
        page.stage = GhostPage::Options;
        page.option = static_cast<i32>((y - kOptionsTop) / kOptionHeight) - 1;
        this->GhostDown();

        if (leftDouble)
        {
            this->GhostSelect();
        }

        return;
    }

    if (y >= kSlotsBottom || y <= kSlotsTop || x >= kPanelRight || x <= kPanelLeft)
    {
        if (rightClick)
        {
            this->GhostCancel();
        }

        return;
    }

    const auto within = static_cast<i32>(y - kSlotsTop);
    const i32 icon = food.icons[static_cast<std::size_t>(page.option)];

    if ((static_cast<u32>(left.press) & kPressedMask) == 1)
    {
        if (icon == kUnsuitedIcons[static_cast<std::size_t>(owner.party.variables[Party::GhostVariable])])
        {
            page.stage = GhostPage::Unsuited;
        }
        else
        {
            page.stage = GhostPage::Slots;
            page.slot = icon == kDoubledIcon ? 2 : within / kRowHeight % 2;
        }
    }
    else if (rightClick)
    {
        const i32 before = page.stage;
        page.slot = within / kRowHeight;
        page.stage = GhostPage::Slots;
        this->GhostDescribe();
        page.stage = before;
    }
    else if (left.heldFrames == 1 && (static_cast<u32>(left.press) & kPressedMask) == 2)
    {
        if (page.foods.front().id != 0 && firstSkill != 0)
        {
            page.stage = GhostPage::Slots;
            owner.controls.At(Button::MouseLeft) = ButtonState{};
            this->GhostSelect();
        }
        else
        {
            page.stage = GhostPage::Choosing;
        }
    }
}

// swd3e.exe 0x00448520: down: the next row, the next option (to the last one present when the next is empty), or the
// other of the first two slots (the third for a skill of the doubled icon)
void Menu::GhostDown()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        this->GhostMove(0, 1);
        this->GhostDescribe();
        return;
    }

    const GhostFood& food = page.foods[page.current];

    if (page.stage != GhostPage::Options)
    {
        if (page.stage == GhostPage::Slots)
        {
            page.slot = food.icons[static_cast<std::size_t>(page.option)] == kDoubledIcon ? 2 : (page.slot + 1) % 2;
        }

        this->GhostDescribe();
        return;
    }

    i32 next = page.option + 1;

    if (next < static_cast<i32>(GhostFood::Skills))
    {
        if (food.skills[static_cast<std::size_t>(next)] == 0)
        {
            next = static_cast<i32>(GhostFood::Skills) - 1;

            while (food.skills[static_cast<std::size_t>(next)] == 0)
            {
                if (--next < 0)
                {
                    this->GhostSlotFor(food.icons[static_cast<std::size_t>(page.option)]);
                    this->GhostDescribe();
                    return;
                }
            }
        }

        page.option = next;
    }

    this->GhostSlotFor(food.icons[static_cast<std::size_t>(page.option)]);
    this->GhostDescribe();
}

// swd3e.exe 0x00448630: up: the row before, the option before, or the other of the first two slots
void Menu::GhostUp()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        this->GhostMove(0, -1);
        this->GhostDescribe();
        return;
    }

    const GhostFood& food = page.foods[page.current];

    if (page.stage == GhostPage::Options)
    {
        if (page.option - 1 >= 0)
        {
            --page.option;
        }

        this->GhostSlotFor(food.icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Slots)
    {
        if (food.icons[static_cast<std::size_t>(page.option)] == kDoubledIcon)
        {
            page.slot = 2;
        }
        else if (--page.slot < 0)
        {
            page.slot = 1;
        }
    }

    this->GhostDescribe();
}

// swd3e.exe 0x00448700: page down: the next page of rows, the last option present, or the second slot (the third for
// a skill of the doubled icon)
void Menu::GhostPageDown()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        this->GhostMove(1, GhostPage::Rows);
        this->GhostDescribe();
        return;
    }

    const GhostFood& food = page.foods[page.current];

    if (page.stage == GhostPage::Options)
    {
        for (i32 option = static_cast<i32>(GhostFood::Skills) - 1; option >= 0; --option)
        {
            if (food.skills[static_cast<std::size_t>(option)] != 0)
            {
                page.option = option;
                break;
            }
        }

        this->GhostSlotFor(food.icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Slots)
    {
        page.slot = (food.icons[static_cast<std::size_t>(page.option)] == kDoubledIcon ? 1 : 0) + 1;
    }

    this->GhostDescribe();
}

// swd3e.exe 0x004487C0: page up: the page of rows before, the first option, or the first slot (the third for a skill
// of the doubled icon)
void Menu::GhostPageUp()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        this->GhostMove(1, -GhostPage::Rows);
        this->GhostDescribe();
        return;
    }

    const GhostFood& food = page.foods[page.current];

    if (page.stage == GhostPage::Options)
    {
        page.option = 0;
        this->GhostSlotFor(food.icons[0]);
    }
    else if (page.stage == GhostPage::Slots)
    {
        page.slot = food.icons[static_cast<std::size_t>(page.option)] != kDoubledIcon ? 0 : 2;
    }

    this->GhostDescribe();
}

// swd3e.exe 0x00448860: left: the tab before (its list made again), the option before, or the first slot
void Menu::GhostLeft()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        if (--page.tab <= 0)
        {
            page.tab = 0;
        }

        this->GhostBuildList(page.tab);
        this->GhostRow();
        this->GhostCompare();
        this->GhostSlotFor(page.foods[page.current].icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Options)
    {
        this->GhostUp();
        this->GhostSlotFor(page.foods[page.current].icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Slots)
    {
        const i32 icon = page.foods[page.current].icons[static_cast<std::size_t>(page.option)];
        page.slot = icon != kDoubledIcon ? 0 : 2;
    }

    this->GhostDescribe();
}

// swd3e.exe 0x00448940: right: the next tab, the next option, or the second slot
void Menu::GhostRight()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        if (++page.tab >= GhostPage::Tabs - 1)
        {
            page.tab = GhostPage::Tabs - 1;
        }

        this->GhostBuildList(page.tab);
        this->GhostRow();
        this->GhostCompare();
        this->GhostSlotFor(page.foods[page.current].icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Options)
    {
        this->GhostDown();
        this->GhostSlotFor(page.foods[page.current].icons[static_cast<std::size_t>(page.option)]);
    }
    else if (page.stage == GhostPage::Slots)
    {
        const i32 icon = page.foods[page.current].icons[static_cast<std::size_t>(page.option)];
        page.slot = (icon == kDoubledIcon ? 1 : 0) + 1;
    }

    this->GhostDescribe();
}

// swd3e.exe 0x00448A20: shift: the description of the next slot while choosing, of the slot while choosing an option
void Menu::GhostShift()
{
    GhostPage& page = this->ghost;

    if (this->game.party.variables[Party::GhostVariable] == 0)
    {
        return;
    }

    if (page.stage == GhostPage::Choosing)
    {
        page.stage = GhostPage::Slots;
        page.slot = (page.slot + 1) % 3;
        this->GhostDescribe();
        page.stage = GhostPage::Choosing;
    }
    else if (page.stage == GhostPage::Options)
    {
        page.stage = GhostPage::Slots;
        this->GhostDescribe();
        page.stage = GhostPage::Options;
    }
}

// swd3e.exe 0x00448AA0: Enter: a row opens its options; 直接餵食 feeds the object (the list made again) or tells it
// is too full; a skill goes on to the slots unless refused (屬性不合); a slot feeds the object and puts its skill in
// (the list and the slots made again); a message is closed.
void Menu::GhostSelect()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    const i32 chosen = owner.party.variables[Party::GhostVariable];

    if (chosen == 0)
    {
        return;
    }

    auto stepOn = [this, &page]()
    {
        ++page.stage;
        this->GhostDescribe();
    };

    switch (page.stage)
    {
        case GhostPage::Choosing:
            if (page.foods.front().id != 0)
            {
                page.option = 0;
                this->GhostDescribe();
                stepOn();
            }

            break;

        case GhostPage::Options:
        {
            page.slot = 0;
            const GhostFood& food = page.foods[page.current];

            if (food.skills[static_cast<std::size_t>(page.option)] == GhostFood::SkillsEnd)
            {
                const int fed = this->GhostFeed(food.id, 0);

                if (fed != 0 && fed != -1)
                {
                    this->GhostBuildList(page.tab);
                    this->GhostMoveRow(0);
                    this->GhostRow();
                    this->GhostDescribe();
                    page.stage = GhostPage::Fed;
                    stepOn();
                    break;
                }

                page.stage = GhostPage::TooFull;
                break;
            }

            const i32 icon = food.icons[static_cast<std::size_t>(page.option)];

            if (icon == kUnsuitedIcons[static_cast<std::size_t>(chosen)])
            {
                page.stage = GhostPage::Unsuited;
                break;
            }

            this->GhostDescribe();

            if (icon == kDoubledIcon)
            {
                page.slot = 2;
            }
            else if (page.slot == 2)
            {
                page.slot = 0;
            }

            stepOn();
            break;
        }

        case GhostPage::Slots:
        {
            const GhostFood& food = page.foods[page.current];
            const int fed = this->GhostFeed(food.id, food.skills[static_cast<std::size_t>(page.option)]);

            if (fed != 0 && fed != -1)
            {
                this->GhostBuildList(page.tab);
                this->GhostMakeSlots();
                this->GhostMoveRow(0);
                this->GhostRow();
                page.stage = GhostPage::Fed;
                stepOn();
                break;
            }

            page.stage = GhostPage::TooFull;
            break;
        }

        case GhostPage::TooFull:
        case GhostPage::Unsuited:
            page.stage = GhostPage::Choosing;
            break;

        default:
            stepOn();
            break;
    }
}

// swd3e.exe 0x00448CB0: Escape: back a stage (a message closes to the slots or the list); from the list the page is
// left for the main page (the menu closes from it)
void Menu::GhostCancel()
{
    GhostPage& page = this->ghost;

    if (page.stage == GhostPage::Choosing)
    {
        // The code is a dword in the original, compared signed after the decrement.
        const i32 code = static_cast<i32>(this->code) - 1;
        this->code = static_cast<u16>(code);

        if (code <= 0)
        {
            this->game.menuState = Game::NoMenu;
        }

        this->GhostLeave();
        this->SetPage(this->code);
        return;
    }

    if (page.stage == GhostPage::TooFull)
    {
        page.stage = GhostPage::Slots;
        return;
    }

    if (page.stage == GhostPage::Unsuited)
    {
        page.stage = GhostPage::Choosing;
        return;
    }

    --page.stage;
    this->GhostDescribe();
}

// swd3e.exe 0x00448D20: the page: the chart of the ghost's balance, attack and defence, the gauges, the list, the
// options, the slots, the description and a message when one is up
void Menu::GhostDraw()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;

    if (!page.slots.has_value() || page.foods.empty())
    {
        return;
    }

    constexpr u32 kTopColor = 0x1011F;
    constexpr u32 kLeftColor = 0x11F01;
    constexpr u32 kRightColor = 0x1F0101;
    constexpr int kMessageX = 0x134;
    constexpr int kMessageTop = 0x80;
    constexpr int kMessageTextX = 0x13C;
    constexpr int kTooFullWidth = 0xBE;
    constexpr int kUnsuitedWidth = 0xC8;
    constexpr int kMessageHeight = 0x14;
    constexpr int kMessageShade = -0x10;
    const TalismanGhost& talisman = owner.talismanGhost;
    this->GhostMakeChart(talisman.balance, talisman.attack, talisman.defense, kTopColor, kLeftColor, kRightColor);
    this->GhostDrawGauges();
    this->GhostDrawList();
    this->GhostDrawOptions();
    this->GhostDrawSlots();
    this->GhostDrawDescription();

    auto message = [this, &owner, &page](int width, std::u8string_view text)
    {
        const int y = kRowHeight * page.row + kMessageTop;
        this->DrawBox(kMessageX, y, width, kMessageHeight, 1, kMessageShade, kMessageShade, kMessageShade);
        const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
        owner.largeFont.DrawText(owner.drawSurface, kMessageTextX, y, text, color, kTextStyle);
    };

    if (page.stage == GhostPage::TooFull)
    {
        message(kTooFullWidth, kTooFull);
    }

    if (page.stage == GhostPage::Unsuited)
    {
        message(kUnsuitedWidth, kUnsuited);
    }
}

// swd3e.exe 0x00448E60: the day gauge (the growth days marked) and the satiety gauge with their markers
void Menu::GhostDrawGauges()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    const TalismanGhost& talisman = owner.talismanGhost;
    constexpr int kDayBoxX = 0xF0;
    constexpr int kSatietyBoxX = 0x1CC;
    constexpr int kBoxY = 0x40;
    constexpr int kBoxWidth = 0x78;
    constexpr int kBoxHeight = 8;
    constexpr int kBoxMode = 4;
    constexpr int kDayLeft = 0xEC;
    constexpr int kSatietyLeft = 0x1C8;
    constexpr int kSatietyRight = 0x248;
    constexpr int kGaugeTop = 0x3C;
    constexpr int kGaugeBottom = 0x4C;
    constexpr int kSatietyGood = 0x40;
    constexpr int kSatietyEnd = 0x80;
    constexpr int kMarkerY = 0x36;
    constexpr int kDayMarkerX = 0xE4;
    constexpr int kSatietyMarkerX = 0x1C0;
    constexpr int kLabelY = 0x4A;
    constexpr int kDayLabelX = 0x15A;
    constexpr int kSatietyLabelX = 0x236;

    enum GaugePicture : u16
    {
        BeforeGrowth = 0,
        Growth = 1,
        AfterGrowth = 2,
        Hungry = 3,
        Fed = 4,
    };

    enum MarkerPicture : u16
    {
        SatietyLabel = 0,
        DayLabel = 1,
        Marker = 2,
    };

    // The original divides by the cycle unchecked (0 only without a ghost, when the page draws nothing); 0 then.
    auto scaled = [&talisman](i32 days)
    {
        if (talisman.cycleDays == 0)
        {
            return 0;
        }

        return (days << kGaugeScale) / talisman.cycleDays;
    };

    const i32 growthFrom = scaled(talisman.growthFrom);
    const i32 growthTo = scaled(talisman.growthTo);
    const i32 end = scaled(talisman.cycleDays);
    page.dayMarker = scaled(talisman.day);
    this->DrawBox(kDayBoxX, kBoxY, kBoxWidth, kBoxHeight, kBoxMode, 0, 0, 0);
    owner.painter.SetClipClamped(kDayLeft, kGaugeTop, end + kDayLeft, kGaugeBottom);

    for (int at = 0; at < growthFrom; at += kGaugeStep)
    {
        owner.actDrawer.DrawPicture(kGaugeFile, BeforeGrowth, at + kDayLeft, kGaugeTop);
    }

    for (int at = growthFrom; at < growthTo; at += kGaugeStep)
    {
        owner.actDrawer.DrawPicture(kGaugeFile, Growth, at + kDayLeft, kGaugeTop);
    }

    for (int at = growthTo; at < end; at += kGaugeStep)
    {
        owner.actDrawer.DrawPicture(kGaugeFile, AfterGrowth, at + kDayLeft, kGaugeTop);
    }

    // The original counts a tick from 0 to 0xC7 and wraps it to 0 (read by nothing).
    if (++page.tick >= kTickWrap)
    {
        page.tick = 0;
    }

    page.satietyMarker = (talisman.satiety << kGaugeScale) / kGrowthSatietyLimit;
    this->DrawBox(kSatietyBoxX, kBoxY, kBoxWidth, kBoxHeight, kBoxMode, 0, 0, 0);
    owner.painter.SetClipClamped(kSatietyLeft, kGaugeTop, kSatietyRight, kGaugeBottom);

    for (int at = 0; at < kSatietyGood; at += kGaugeStep)
    {
        owner.actDrawer.DrawPicture(kGaugeFile, Hungry, at + kSatietyLeft, kGaugeTop);
    }

    for (int at = kSatietyGood; at < kSatietyEnd; at += kGaugeStep)
    {
        owner.actDrawer.DrawPicture(kGaugeFile, Fed, at + kSatietyLeft, kGaugeTop);
    }

    owner.painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    owner.actDrawer.DrawPicture(kMarkerFile, Marker, page.dayMarker + kDayMarkerX, kMarkerY);
    owner.actDrawer.DrawPicture(kMarkerFile, Marker, page.satietyMarker + kSatietyMarkerX, kMarkerY);
    owner.actDrawer.DrawPicture(kMarkerFile, DayLabel, kDayLabelX, kLabelY);
    owner.actDrawer.DrawPicture(kMarkerFile, SatietyLabel, kSatietyLabelX, kLabelY);
}

// swd3e.exe 0x00449080: the tab icons (the tab's in the middle), the list box with its scroll bar beyond eight rows and
// the rows: name and count, the row chosen lit (dim off the list); "無" for an empty tab, which compares nothing
void Menu::GhostDrawList()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
    const Highlight light = page.stage == GhostPage::Choosing ? kLit : kDim;
    int arrows = 0;
    constexpr u32 kDownArrowFrames = 0xF;

    if (page.count > GhostPage::Rows)
    {
        if ((page.arrows & kDownArrowFrames) != 0)
        {
            page.arrows = (page.arrows & ~kDownArrowFrames) | ((page.arrows & kDownArrowFrames) - 1);
            arrows = 1;
        }

        if ((page.arrows & kUpArrowFrames) != 0)
        {
            const u32 frames = (page.arrows >> kArrowShift) & kDownArrowFrames;
            arrows |= 2;
            page.arrows = (page.arrows & ~kArrowsByte) | (page.arrows & kDownArrowFrames) |
                          (kUpArrowFrame * frames - kUpArrowFrame);
        }
    }

    constexpr int kIconsClipLeft = 0xA;
    constexpr int kIconsClipTop = 1;
    constexpr int kIconsClipBottom = 0x6B;
    constexpr int kIconsX = 0xCA;
    constexpr int kIconsY = 0x5A;
    constexpr int kIconStep = 0x28;
    constexpr int kIconsShown = 2;
    constexpr int kChosenX = 0x11A;
    constexpr int kChosenY = 0x57;
    constexpr int kListClipBottom = 0x1DE;
    owner.painter.SetClipClamped(kIconsClipLeft, kIconsClipTop, Display::Width, kIconsClipBottom);
    int x = kIconsX;

    for (int icon = page.tab - kIconsShown; icon <= page.tab + kIconsShown; ++icon)
    {
        if (static_cast<u32>(icon) < static_cast<u32>(GhostPage::Tabs) && icon != page.tab)
        {
            owner.actDrawer.DrawPicture(kTabIconFile, static_cast<u16>(icon), x, kIconsY);
        }

        x += kIconStep;
    }

    owner.actDrawer.DrawPicture(kTabChosenFile, static_cast<u16>(page.tab), kChosenX, kChosenY);
    owner.painter.SetClipClamped(0, kIconsClipTop, Display::Width, kListClipBottom);
    constexpr int kListBoxX = 0xD2;
    constexpr int kListBoxY = 0x71;
    constexpr int kListBoxWidth = 0xBE;
    constexpr int kListBoxHeight = 0xC8;
    this->DrawBox(kListBoxX, kListBoxY, kListBoxWidth, kListBoxHeight, 2, 0, 0, 0);

    if (page.count > GhostPage::Rows)
    {
        constexpr int kBarX = 0x198;
        constexpr int kBarY = 0x7D;
        constexpr int kBarHeight = 0xB0;
        const auto total = static_cast<double>(page.count);
        const auto to = static_cast<float>(static_cast<double>(page.lastRow + page.top + 1) / total);
        const auto from = static_cast<float>(static_cast<double>(page.top) / total);
        this->DrawScrollBar(kBarX, kBarY, kBarHeight, arrows, from, to, this->scrollBar);
    }

    constexpr int kNameX = 0xDC;
    constexpr int kCountX = 0x17C;
    constexpr int kCountDown = 7;
    constexpr int kRowsTop = 0x74;
    constexpr int kRowsBottom = 0x134;
    constexpr int kLightX = 0xCD;
    constexpr int kLightWidth = 0xC8;

    if (page.foods.front().id == 0)
    {
        page.compared = -1;
        owner.largeFont.DrawText(owner.drawSurface, kNameX, kRowsTop, kNone, color, kTextStyle);
        FillBox(
            owner.painter,
            kLightX,
            kRowsTop,
            kLightWidth,
            kRowHeight,
            kLit.red,
            kLit.green,
            kLit.blue,
            FillMode::Gradient
        );
        return;
    }

    int index = 0;

    for (int y = kRowsTop; y < kRowsBottom; y += kRowHeight)
    {
        const GhostFood& food = page.foods[static_cast<std::size_t>(index + page.top)];

        if (food.id != 0)
        {
            owner.largeFont.DrawText(owner.drawSurface, kNameX, y, food.name, color, kTextStyle);
            const std::string count = WinFormat(kCountFormat, {food.count});
            owner.smallFont.DrawText(owner.drawSurface, kCountX, y + kCountDown, count, color, kTextStyle);

            if (index == page.row)
            {
                FillBox(
                    owner.painter,
                    kLightX,
                    y,
                    kLightWidth,
                    kRowHeight,
                    light.red,
                    light.green,
                    light.blue,
                    FillMode::Gradient
                );
            }
        }

        ++index;
    }
}

// swd3e.exe 0x00449390: the options of the object chosen: its skills with their icons and 直接餵食 after them (not
// shown when it has five skills), the option chosen lit (dim off the options)
void Menu::GhostDrawOptions()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;

    if (page.foods.empty())
    {
        return;
    }

    constexpr int kBoxX = 0x1B0;
    constexpr int kBoxY = 0x71;
    constexpr int kBoxWidth = 0xB4;
    constexpr int kBoxHeight = 0x6C;
    constexpr int kTextX = 0x1C6;
    constexpr int kLightX = 0x1AC;
    constexpr int kLightWidth = 0xBC;
    const Highlight light = page.stage == GhostPage::Options ? kLit : kDim;
    const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
    this->DrawBox(kBoxX, kBoxY, kBoxWidth, kBoxHeight, 2, 0, 0, 0);
    const GhostFood& food = page.foods[page.current];
    int y = kBoxY;

    for (std::size_t option = 0; option < GhostFood::Skills; ++option)
    {
        if (food.skills[option] > 0)
        {
            if (food.names[option].has_value())
            {
                owner.largeFont.DrawText(owner.drawSurface, kTextX, y, *food.names[option], color, kTextStyle);
            }

            const i32 icon = food.icons[option];

            // The original reads the offset tables past their end for an icon beyond them (see deviations.md).
            if (icon >= 0 && static_cast<std::size_t>(icon) < kIconY.size())
            {
                owner.actDrawer.DrawPicture(
                    kSkillIconFile,
                    static_cast<u16>(icon),
                    kIconX[static_cast<std::size_t>(icon)] + kBoxX,
                    y + kIconY[static_cast<std::size_t>(icon)]
                );
            }

            if (static_cast<i32>(option) == page.option)
            {
                FillBox(
                    owner.painter,
                    kLightX,
                    y,
                    kLightWidth,
                    kRowHeight,
                    light.red,
                    light.green,
                    light.blue,
                    FillMode::Gradient
                );
            }
        }

        if (food.skills[option] == GhostFood::SkillsEnd)
        {
            const int feedY = kOptionHeight * static_cast<int>(option) + kBoxY;
            owner.largeFont.DrawText(owner.drawSurface, kTextX, feedY, kFeedDirectly, color, kTextStyle);

            if (static_cast<i32>(option) == page.option)
            {
                FillBox(
                    owner.painter,
                    kLightX,
                    feedY,
                    kLightWidth,
                    kRowHeight,
                    light.red,
                    light.green,
                    light.blue,
                    FillMode::Gradient
                );
            }

            return;
        }

        y += kOptionHeight;
    }
}

// swd3e.exe 0x00449560: the ghost's first three skill slots with their icons, the slot chosen lit (dim off the slots)
void Menu::GhostDrawSlots()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    constexpr int kBoxX = 0x1B0;
    constexpr int kBoxY = 0xF2;
    constexpr int kBoxWidth = 0xB4;
    constexpr int kBoxHeight = 0x47;
    constexpr int kTextX = 0x1C6;
    constexpr int kIconX0 = 0x1B4;
    constexpr int kLightX = 0x1AC;
    constexpr int kLightWidth = 0xBC;
    constexpr int kShown = 3;
    const Highlight light = page.stage == GhostPage::Slots ? kLit : kDim;
    const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
    this->DrawBox(kBoxX, kBoxY, kBoxWidth, kBoxHeight, 2, 0, 0, 0);

    if (!page.slots.has_value())
    {
        return;
    }

    const GhostSlots& slots = *page.slots;

    for (int slot = 0; slot < kShown; ++slot)
    {
        const int y = kRowHeight * slot + kBoxY;
        const auto at = static_cast<std::size_t>(slot);
        owner.largeFont.DrawText(owner.drawSurface, kTextX, y, slots.names[at], color, kTextStyle);
        const i32 icon = slots.icons[at];

        if (icon >= 0 && static_cast<std::size_t>(icon) < kIconY.size())
        {
            owner.actDrawer.DrawPicture(
                kSkillIconFile,
                static_cast<u16>(icon),
                kIconX[static_cast<std::size_t>(icon)] + kIconX0,
                kIconY[static_cast<std::size_t>(icon)] + y
            );
        }

        if (slot == page.slot)
        {
            FillBox(
                owner.painter,
                kLightX,
                y,
                kLightWidth,
                kRowHeight,
                light.red,
                light.green,
                light.blue,
                FillMode::Gradient
            );
        }
    }
}

// swd3e.exe 0x004496B0: the description line and the note box (wrapped in five lines)
void Menu::GhostDrawDescription()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    constexpr int kLineX = 0xD2;
    constexpr int kLineY = 0x1BB;
    constexpr int kLineWidth = 0x19E;
    constexpr int kLineHeight = 0x12;
    constexpr int kShadeX = 0xCA;
    constexpr int kShadeY = 0x144;
    constexpr int kShadeWidth = 0x1A2;
    constexpr int kShadeHeight = 0x6C;
    constexpr int kFrameLeft = 0xD2;
    constexpr int kFrameTop = 0x14C;
    constexpr int kFrameRight = 0x264;
    constexpr int kFrameBottom = 0x1A8;
    constexpr int kFrameBorder = static_cast<int>(0x80000008);
    constexpr int kNoteX = 0xDE;
    constexpr int kNoteY = 0x14A;
    constexpr int kNoteLines = 5;
    constexpr int kNoteWidth = 0x168;
    const auto color = static_cast<Pixel>(owner.painter.MakeColor(kTextRed, kTextGreen, kTextBlue));
    this->DrawBox(kLineX, kLineY, kLineWidth, kLineHeight, 2, 0, 0, 0);

    if (page.description.has_value())
    {
        owner.mediumFont.DrawText(owner.drawSurface, kLineX, kLineY, *page.description, color, kTextStyle);
    }

    FillBox(owner.painter, kShadeX, kShadeY, kShadeWidth, kShadeHeight, 0, 0, 0, FillMode::Eighth);
    DrawWindowFrame(
        owner.painter,
        owner.archive,
        owner.battleMessages.panelAct.frame,
        kFrameLeft,
        kFrameTop,
        kFrameRight,
        kFrameBottom,
        0,
        kFrameBorder
    );

    if (page.note.has_value())
    {
        owner.DrawWrappedText(*page.note, kNoteX, kNoteY, kNoteLines, kNoteWidth);
    }
}

// swd3e.exe 0x00449780: the chart as last made, at (x, y) (shown on the role panel of the ghost page too)
void Menu::DrawGhostChart(int x, int y)
{
    Game& owner = this->game;

    if (this->ghost.chart.empty())
    {
        return;
    }

    owner.painter.SetSource(this->ghost.chart.data());
    owner.painter.DrawPicture(x, y, kChartSide, kChartSide, Painter::Copy);
}

// swd3e.exe 0x004497B0: the charts freed and a cleared 120 x 120 buffer made
void Menu::GhostMakeChartBuffer()
{
    this->GhostFreeChart();
    this->ghost.chartPixels.assign(static_cast<std::size_t>(kChartSide) * kChartSide, 0);
}

// swd3e.exe 0x004497E0: the chart buffers freed
void Menu::GhostFreeChart()
{
    this->ghost.chartPixels.clear();
    this->ghost.chart.clear();
}

// swd3e.exe 0x00449840: the triangle of balance (top), attack (left) and defence (right) drawn into the buffer with
// colours graded between its corners, filled row by row, its axes drawn white over it, then encoded as an RLE picture
// with the fill colour 0 (no pixel is a fill run). The colours are channels in the bytes of `top`, `left` and `right`
// (red, green, blue from the low byte; MakeColor keeps their low five bits).
void Menu::GhostMakeChart(int balance, int attack, int defense, u32 top, u32 left, u32 right)
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    const Painter& painter = owner.painter;
    std::vector<u32> colors(static_cast<std::size_t>(kChartSide) * kChartSide, 0);
    std::fill(page.chartPixels.begin(), page.chartPixels.end(), u16{0});

    if (page.chartPixels.empty())
    {
        return;
    }

    auto channel = [](u32 color, u32 at)
    {
        return static_cast<int>((color >> (kChannelBits * at)) & UCHAR_MAX);
    };

    // swd3e.exe 0x00449BE0: a line from (x0, y0) to (x1, y1) graded from (r0, g0, b0) to (r1, g1, b1): each step writes
    // the pixel and its 24-bit colour; the cross steps follow an error term that compares with the wrong delta, so
    // lines bend unevenly as in the original.
    auto line =
        [&page, &colors, &painter](int x0, int y0, int x1, int y1, std::array<int, 3> from, std::array<int, 3> to)
    {
        int at = x0 + kChartSide * y0;
        int deltaY = y1 - y0;
        int rowStep = kChartSide;

        if (deltaY < 0)
        {
            deltaY = -deltaY;
            rowStep = -kChartSide;
        }

        int deltaX = x1 - x0;
        int columnStep = 1;

        if (deltaX < 0)
        {
            deltaX = x0 - x1;
            columnStep = -1;
        }

        int error = 0;
        auto red = static_cast<float>(from[0]);
        auto green = static_cast<float>(from[1]);
        auto blue = static_cast<float>(from[2]);
        const bool steep = deltaX <= deltaY;
        const int steps = (steep ? deltaY : deltaX) + 1;
        const auto count = static_cast<double>(steps);
        const auto redStep = static_cast<float>(static_cast<double>(to[0] - from[0]) / count);
        const auto greenStep = static_cast<float>(static_cast<double>(to[1] - from[1]) / count);
        const auto blueStep = static_cast<float>(static_cast<double>(to[2] - from[2]) / count);

        for (int step = 0; step < steps; ++step)
        {
            const auto r = static_cast<i32>(red);
            const auto g = static_cast<i32>(green);
            const auto b = static_cast<i32>(blue);

            if (at >= 0 && static_cast<std::size_t>(at) < page.chartPixels.size())
            {
                page.chartPixels[static_cast<std::size_t>(at)] =
                    static_cast<u16>(painter.MakeColor(static_cast<u32>(r), static_cast<u32>(g), static_cast<u32>(b)));
                colors[static_cast<std::size_t>(at)] =
                    static_cast<u32>(b) |
                    ((static_cast<u32>(g) | (static_cast<u32>(r) << kChannelBits)) << kChannelBits);
            }

            red = red + redStep;
            green = green + greenStep;
            blue = blue + blueStep;

            if (steep)
            {
                error += deltaX;
                at += rowStep;

                if (error > deltaX)
                {
                    error -= deltaY;
                    at += columnStep;
                }
            }
            else
            {
                error += deltaY;
                at += columnStep;

                if (error > deltaX)
                {
                    error -= deltaX;
                    at += rowStep;
                }
            }
        }
    };

    auto rgb = [&channel](u32 color) -> std::array<int, 3>
    {
        return {channel(color, 0), channel(color, 1), channel(color, 2)};
    };

    const int tipY = kChartCenter - balance / 2;
    const int leftX = kChartCenter - static_cast<i32>(static_cast<double>(attack) * kChartSlope);
    const int leftY = kChartCenter - static_cast<i32>(static_cast<double>(attack) * kChartHalf * -kChartSlope);
    const int rightX = kChartCenter - static_cast<i32>(static_cast<double>(defense) * -kChartSlope);
    const int rightY = kChartCenter - static_cast<i32>(static_cast<double>(defense) * kChartHalf * -kChartSlope);
    line(kChartCenter, tipY, leftX, leftY, rgb(top), rgb(left));
    line(leftX, leftY, rightX, rightY, rgb(left), rgb(right));
    line(rightX, rightY, kChartCenter, tipY, rgb(right), rgb(top));

    // swd3e.exe 0x00449EB0: each row filled from its first to its last marked colour, graded between them; the
    // original steps one pixel more than the span, writing just past its last mark (kept within the buffer).
    for (int row = 0; row < kChartSide; ++row)
    {
        const std::size_t rowAt = static_cast<std::size_t>(row) * kChartSide;
        int first = 0;

        while (first < kChartSide && colors[rowAt + static_cast<std::size_t>(first)] == 0)
        {
            ++first;
        }

        if (first == kChartSide)
        {
            continue;
        }

        int trailing = 0;

        while (trailing < kChartSide && colors[rowAt + static_cast<std::size_t>(kChartSide - 1 - trailing)] == 0)
        {
            ++trailing;
        }

        if (trailing == kChartSide)
        {
            continue;
        }

        const int end = kChartSide - trailing;
        const u32 start = colors[rowAt + static_cast<std::size_t>(first)];
        const u32 stop = colors[rowAt + static_cast<std::size_t>(end - 1)];
        auto red = static_cast<float>(channel(start, 2));
        auto green = static_cast<float>(channel(start, 1));
        auto blue = static_cast<float>(channel(start, 0));
        const auto span = static_cast<double>(end - first + 1);
        const auto redStep = static_cast<float>((static_cast<double>(channel(stop, 2)) - red) / span);
        const auto greenStep = static_cast<float>((static_cast<double>(channel(stop, 1)) - green) / span);
        const auto blueStep = static_cast<float>((static_cast<double>(channel(stop, 0)) - blue) / span);

        for (int column = first; column <= end; ++column)
        {
            const std::size_t at = rowAt + static_cast<std::size_t>(column);

            if (at < page.chartPixels.size())
            {
                page.chartPixels[at] = static_cast<u16>(painter.MakeColor(
                    static_cast<u32>(static_cast<i32>(red)),
                    static_cast<u32>(static_cast<i32>(green)),
                    static_cast<u32>(static_cast<i32>(blue))
                ));
            }

            red = red + redStep;
            green = green + greenStep;
            blue = blue + blueStep;
        }
    }

    const std::array<int, 3> axisCenter = {0, kChartChannelMax, kChartChannelMax};
    const std::array<int, 3> white = {kChartChannelMax, kChartChannelMax, kChartChannelMax};
    line(kChartCenter, kChartCenter, kChartCenter, kChartTipY, axisCenter, white);
    line(kChartCenter, kChartCenter, kChartLeftX, kChartBottomY, axisCenter, white);
    line(kChartCenter, kChartCenter, kChartRightX, kChartBottomY, axisCenter, white);
    line(kChartRightX, kChartBottomY, kChartCenter, kChartTipY, axisCenter, axisCenter);
    line(kChartRightX, kChartBottomY, kChartLeftX, kChartBottomY, axisCenter, axisCenter);
    line(kChartLeftX, kChartBottomY, kChartCenter, kChartTipY, axisCenter, axisCenter);
    const u16 fill = owner.painter.SetRleFill(0);
    page.chart = owner.codec.Encode(page.chartPixels.data(), kChartSide, kChartSide, RleHeader::TrueColor);
    owner.painter.SetRleFill(fill);
}

// swd3e.exe 0x0044A080: the list of tab `tab`: every inventory object of its category (by id, ascending) with its
// count, note and the skills it teaches (the objects of its itemA, itemB, itemC, healItemA and healItemB, the repeats
// left out; each named, iconed and described when mon.dat has it); the first row chosen
void Menu::GhostBuildList(int tab)
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    this->GhostFreeList();
    page.current = 0;
    page.row = 0;
    page.top = 0;
    page.lastRow = 0;
    page.count = 0;
    page.foods.assign(GhostPage::Foods, GhostFood{});
    const i32 category = kTabCategories[static_cast<std::size_t>(tab)];

    auto teach = [&owner](GhostFood& food, std::size_t at, u16 id)
    {
        ObjectRecord record;

        if (owner.database.LoadObject(record, id))
        {
            food.names[at] = ObjectName(record);
            SkillScript skill;
            owner.database.LoadSkill(skill, record.data.skillScript);
            const auto icon = static_cast<std::size_t>(static_cast<u16>(skill.unk10));
            // The original reads the table past its end for a larger value (see deviations.md).
            food.icons[at] = icon < kSkillIcons.size() ? kSkillIcons[icon] : GhostFood::NoIcon;
            food.descriptions[at] = ObjectDescription(owner.databases.mapsData, id);
            food.notes[at] = record.description.value_or(std::string());
        }

        food.skills[at] = id;
    };

    for (const ItemNode* node = owner.party.inventory.get(); node != nullptr; node = node->next.get())
    {
        const ObjectData& data = node->record.data;

        if (data.category != category || static_cast<std::size_t>(page.count) >= GhostPage::Foods)
        {
            continue;
        }

        GhostFood& food = page.foods[static_cast<std::size_t>(page.count)];
        food.id = node->id;
        food.category = data.category;
        food.level = data.level;
        food.name = ObjectName(node->record);
        food.note = node->record.description.value_or(std::string());
        food.count = node->count + node->pending;
        food.icons.fill(GhostFood::NoIcon);
        std::size_t taught = 0;

        if (data.itemA != 0)
        {
            teach(food, taught++, data.itemA);
        }

        if (data.itemB != 0 && data.itemA != data.itemB)
        {
            teach(food, taught++, data.itemB);
        }

        if (data.itemC != 0 && data.itemA != data.itemC && data.itemB != data.itemC)
        {
            teach(food, taught++, data.itemC);
        }

        if (data.healItemA != 0)
        {
            teach(food, taught++, data.healItemA);
        }

        if (data.healItemB != 0 && data.healItemA != data.healItemB)
        {
            teach(food, taught++, data.healItemB);
        }

        // The end mark of an object teaching five skills falls on the first icon (the next word of the record).
        if (taught < GhostFood::Skills)
        {
            food.skills[taught] = GhostFood::SkillsEnd;
        }
        else
        {
            food.icons[0] = GhostFood::SkillsEnd;
        }

        ++page.count;
    }

    if (page.count > 1)
    {
        for (int first = 0; first < page.count - 1; ++first)
        {
            for (int other = first + 1; other < page.count; ++other)
            {
                if (page.foods[static_cast<std::size_t>(first)].id > page.foods[static_cast<std::size_t>(other)].id)
                {
                    std::swap(page.foods[static_cast<std::size_t>(first)], page.foods[static_cast<std::size_t>(other)]);
                }
            }
        }
    }

    page.lastRow = page.count - 1;

    if (page.lastRow >= GhostPage::Rows)
    {
        page.lastRow = GhostPage::Rows - 1;
    }

    this->GhostRow();
}

// swd3e.exe 0x0044AD70: the list freed
void Menu::GhostFreeList()
{
    this->ghost.foods.clear();
}

// swd3e.exe 0x0044AE10: object `id` fed to the ghost (GhostEat), skill `skill` put into the slot chosen when given and
// one of the object used up (its row removed with the last one): 1; 0 for an object not listed, -1 when the ghost
// is too full
int Menu::GhostFeed(i32 id, i32 skill)
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;

    if (page.foods.empty())
    {
        return 0;
    }

    const auto found = std::find_if(
        page.foods.begin(),
        page.foods.end(),
        [id](const GhostFood& food)
        {
            return food.id == id;
        }
    );

    if (found == page.foods.end())
    {
        return 0;
    }

    if (!this->GhostEat(found->level, found->category))
    {
        return -1;
    }

    if (skill > 0)
    {
        owner.talismanGhost.SetSkill(page.slot, skill);
    }

    if (AddItem(owner.party.inventory, static_cast<u16>(id), -1, ItemCountMode::Pending, owner.database) == nullptr)
    {
        for (int index = 0; index < page.count; ++index)
        {
            if (page.foods[static_cast<std::size_t>(index)].id == id)
            {
                page.foods.erase(page.foods.begin() + index);
                page.foods.emplace_back();
                break;
            }
        }

        --page.count;
    }

    return 1;
}

// swd3e.exe 0x0044AF80: the entry of the row chosen (its first option chosen)
void Menu::GhostRow()
{
    GhostPage& page = this->ghost;

    if (page.foods.empty())
    {
        return;
    }

    page.option = 0;
    page.current = static_cast<std::size_t>(page.row + page.top);
}

// swd3e.exe 0x0044AFB0: the description and note shown: of the slot chosen past the options, otherwise of the option
// chosen of the row chosen
void Menu::GhostDescribe()
{
    GhostPage& page = this->ghost;
    page.description.reset();
    page.note.reset();

    if (static_cast<u32>(page.stage) > GhostPage::Options)
    {
        if (page.slots.has_value())
        {
            page.description = page.slots->descriptions[static_cast<std::size_t>(page.slot)];
            page.note = page.slots->notes[static_cast<std::size_t>(page.slot)];
        }

        return;
    }

    if (page.foods.empty())
    {
        return;
    }

    const GhostFood& food = page.foods[static_cast<std::size_t>(page.top + page.row)];
    page.description = food.descriptions[static_cast<std::size_t>(page.option)];
    page.note = food.notes[static_cast<std::size_t>(page.option)];
}

// swd3e.exe 0x0044B020: the ghost's skill slots (TalismanGhost::Skill 0..4; index 4 reads its balance), each named,
// iconed and described when mon.dat has it ("無" otherwise)
void Menu::GhostMakeSlots()
{
    Game& owner = this->game;
    GhostPage& page = this->ghost;
    this->GhostFreeSlots();
    GhostSlots& slots = page.slots.emplace();

    for (std::size_t slot = 0; slot < GhostSlots::Slots; ++slot)
    {
        const i32 id = owner.talismanGhost.Skill(static_cast<int>(slot));
        slots.skills[slot] = id;
        slots.icons[slot] = GhostFood::NoIcon;
        slots.names[slot] = Big5(kNone);
        slots.descriptions[slot] = Big5(kNone);
        slots.notes[slot] = Big5(kNone);
        ObjectRecord record;

        if (owner.database.LoadObject(record, static_cast<u16>(id)))
        {
            slots.names[slot] = ObjectName(record);
            SkillScript skill;
            owner.database.LoadSkill(skill, record.data.skillScript);
            const auto icon = static_cast<std::size_t>(static_cast<u16>(skill.unk10));
            slots.icons[slot] = icon < kSkillIcons.size() ? kSkillIcons[icon] : GhostFood::NoIcon;
            slots.descriptions[slot] = ObjectDescription(owner.databases.mapsData, static_cast<u16>(id));
            slots.notes[slot] = record.description.value_or(std::string());
        }
    }
}

// swd3e.exe 0x0044B270: the slots cleared and nothing described
void Menu::GhostFreeSlots()
{
    GhostPage& page = this->ghost;

    if (!page.slots.has_value())
    {
        return;
    }

    page.slots = GhostSlots{};
    page.description.reset();
    page.note.reset();
}

// swd3e.exe 0x0044B340: the row moved by `delta`, the list scrolling past its first or last row shown
void Menu::GhostMoveRow(int delta)
{
    GhostPage& page = this->ghost;
    page.row += delta;

    if (page.lastRow <= -1)
    {
        page.row = 0;
    }
    else
    {
        if (page.row < 0)
        {
            page.row = 0;

            if (--page.top < 0)
            {
                page.top = 0;
            }
        }

        if (page.row > page.lastRow)
        {
            page.row = page.lastRow;
            ++page.top;

            if (page.lastRow + page.top > page.count - 1)
            {
                page.top = page.count - page.lastRow - 1;
            }
        }
    }

    this->GhostCountShown();
}

// swd3e.exe 0x0044B3D0: a page of rows: up goes to the first row shown, then scrolls by `delta`; down goes to the last
// row shown, then scrolls by eight
void Menu::GhostMovePage(int delta)
{
    GhostPage& page = this->ghost;

    if (delta < 0)
    {
        if (page.row != 0)
        {
            page.row = 0;
            return;
        }

        page.top += delta;

        if (page.top < 0)
        {
            page.top = 0;
        }
    }

    if (delta > 0)
    {
        if (page.row == page.lastRow)
        {
            page.top += GhostPage::Rows;

            if (page.top + GhostPage::Rows - 1 > page.count)
            {
                page.top = page.count - GhostPage::Rows;

                if (page.top < 0)
                {
                    page.top = 0;
                }
            }
        }
        else
        {
            page.row = page.lastRow;
        }
    }

    this->GhostCountShown();
}

// swd3e.exe 0x0044B450: the last row shown (at most the eighth)
void Menu::GhostCountShown()
{
    GhostPage& page = this->ghost;
    page.lastRow = page.count - page.top - 1;

    if (page.lastRow > GhostPage::Rows - 1)
    {
        page.lastRow = GhostPage::Rows - 1;
    }
}

// swd3e.exe 0x0044B480: the slot a skill of icon `icon` goes into first (the third for the doubled icon)
void Menu::GhostSlotFor(i32 icon)
{
    this->ghost.slot = icon != kDoubledIcon ? 0 : 2;
}

// swd3e.exe 0x0044B4A0: the row moved (`kind` 0) or paged (1) by `delta`, then the entry, the changes compared, the
// slot of its first option and the description
void Menu::GhostMove(int kind, int delta)
{
    GhostPage& page = this->ghost;

    if (kind == 0)
    {
        this->GhostMoveRow(delta);
    }
    else if (kind == 1)
    {
        this->GhostMovePage(delta);
    }

    this->GhostRow();
    this->GhostCompare();
    this->GhostSlotFor(page.foods[page.current].icons[static_cast<std::size_t>(page.option)]);
    this->GhostDescribe();
}

// swd3e.exe 0x0044B530: the changes feeding the entry chosen brings to 力量, 精魄, 智慧 and 敏捷 by its category (the
// role panel shows them); none for other categories
void Menu::GhostCompare()
{
    GhostPage& page = this->ghost;
    page.compared = -1;
    const GhostFood& food = page.foods[page.current];

    if (food.id <= 0)
    {
        return;
    }

    struct Change
    {
        i32 category;
        i32 strength;
        i32 essence;
        i32 wisdom;
        i32 agility;
    };

    constexpr std::array<Change, 5> kChanges = {{
        {0, 3, 2, 5, -1},
        {2, 1, 5, 2, -2},
        {3, 7, 1, -1, -3},
        {7, -2, 1, -3, 5},
        {8, 3, -1, 2, 1},
    }};

    for (const Change& change : kChanges)
    {
        if (change.category == food.category)
        {
            page.compared = change.category;
            page.strengthChange = change.strength;
            page.essenceChange = change.essence;
            page.wisdomChange = change.wisdom;
            page.agilityChange = change.agility;
            return;
        }
    }
}

// swd3e.exe 0x0044B670: an object of `category` worth `level` fed: refused (false) when the satiety is at its limit
// (100, 200 in the growth days); otherwise the satiety grows (to at most 200) by the category's value and twice the
// level, and the triangle and the stats move by the category; the spirit becomes five times the essence and 0..4
bool Menu::GhostEat(i32 level, i32 category)
{
    Game& owner = this->game;
    TalismanGhost& talisman = owner.talismanGhost;
    // The original reads both tables by the category unchecked (see deviations.md); beyond them the value counts as 0.
    const auto categoryAt = static_cast<std::size_t>(category);
    const auto valueAt =
        categoryAt < kTabCategories.size() ? static_cast<std::size_t>(kTabCategories[categoryAt]) : kFoodValues.size();
    const i32 value = (valueAt < kFoodValues.size() ? kFoodValues[valueAt] : 0) + kFoodLevelScale * level;
    const i32 grown = talisman.satiety + value;
    i32 limit = kSatietyLimit;

    if (talisman.day >= talisman.growthFrom && talisman.day <= talisman.growthTo - 1)
    {
        limit = kGrowthSatietyLimit;
    }

    if (talisman.satiety >= limit)
    {
        return false;
    }

    talisman.satiety += value;

    if (grown > kGrowthSatietyLimit)
    {
        talisman.satiety = kGrowthSatietyLimit;
    }

    switch (category)
    {
        case 1:
            this->GhostShiftTriangle(-2, 5, 3);
            break;

        case 4:
            this->GhostShiftTriangle(0, 7, -2);
            break;

        case 5:
            this->GhostShiftTriangle(-2, 2, 5);
            break;

        case 6:
            this->GhostShiftTriangle(2, -3, 6);
            break;

        case 0xA:
            this->GhostShiftTriangle(5, -2, -3);
            break;

        default:
            break;
    }

    switch (category)
    {
        case 0:
            talisman.strength += 3;
            talisman.essence += 2;
            talisman.wisdom += 5;
            talisman.agility -= 1;
            break;

        case 2:
            talisman.strength += 1;
            talisman.essence += 5;
            talisman.wisdom += 2;
            talisman.agility -= 2;
            break;

        case 3:
            talisman.strength += 7;
            talisman.essence += 1;
            talisman.wisdom -= 1;
            talisman.agility -= 3;
            break;

        case 7:
            talisman.strength -= 2;
            talisman.essence += 1;
            talisman.wisdom -= 3;
            talisman.agility += 5;
            break;

        case 8:
            talisman.strength += 3;
            talisman.essence -= 1;
            talisman.wisdom += 2;
            talisman.agility += 1;
            break;

        default:
            break;
    }

    talisman.strength = Clamp(talisman.strength, 0, kStrengthLimit);
    talisman.essence = Clamp(talisman.essence, 0, kStatLimit);
    talisman.spirit = kSpiritPerEssence * talisman.essence + static_cast<i32>(owner.random.Below(kSpiritRange));

    if (talisman.spirit > kSpiritLimit)
    {
        talisman.spirit = kSpiritLimit;
    }

    talisman.wisdom = Clamp(talisman.wisdom, 0, kStatLimit);
    talisman.agility = Clamp(talisman.agility, 0, kStatLimit);
    return true;
}

// swd3e.exe 0x0044B940: the triangle's balance, attack and defence moved, each kept within 0..100
void Menu::GhostShiftTriangle(i32 balance, i32 attack, i32 defense)
{
    TalismanGhost& talisman = this->game.talismanGhost;
    talisman.balance = Clamp(talisman.balance + balance, 0, kStatLimit);
    talisman.attack = Clamp(talisman.attack + attack, 0, kStatLimit);
    talisman.defense = Clamp(talisman.defense + defense, 0, kStatLimit);
}

}  // namespace swd3e
