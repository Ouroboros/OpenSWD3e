#include "game/game.h"
#include "menu/menu.h"

namespace swd3e
{

namespace
{

constexpr u16 kLastPage = 0x10;             // the main page's titles run from the first role page to here
constexpr u16 kBestiaryPage = 0x11;         // keeps the background of the page before it
constexpr i32 kFirstPageBackground = 0x29;  // Menu::background shows page p with action p + this
constexpr int kTitleSound = 0x8B;
constexpr int kPageSound = 0xBB;
constexpr u32 kMouseSuppressed = 0xC;  // Controls::mouseSuppressed after a right click goes back

// The main page's titles (the mouse): seven pixels a step, ten steps a title.
constexpr int kTitlesLeft = 0xDC;
constexpr int kTitlesRight = 0x276;
constexpr int kTitleStep = 7;
constexpr int kTitleSteps = 0xA;
constexpr int kTitleGap = 8;  // the steps past this are the gap between titles
constexpr u32 kTitlesTop = 0xA;
constexpr u32 kTitlesBottom = 0x2A;
constexpr u32 kLeftClick = 1;
constexpr u32 kRightClicks = 0xC;
constexpr u16 kFirstClickedPage = 0xC;  // the title clicked counts from here (MainLeft steps it back)

}  // namespace

// The background of role page `page` (inline in the original).
void Menu::ShowPageBackground(int page)
{
    this->background.action = page + kFirstPageBackground;
}

// swd3e.exe 0x0044DC60: the opening, leaving and drawing handlers of the role pages 0xB..0x11
void Menu::FillPageTables()
{
    this->drawTable[FirstRolePage] = &Menu::ItemsDraw;
    this->drawTable[FirstRolePage + 1] = &Menu::EquipDraw;
    this->drawTable[FirstRolePage + 2] = &Menu::SkillsDraw;
    this->drawTable[FirstRolePage + 3] = &Menu::AltarDraw;
    this->drawTable[FirstRolePage + 4] = &Menu::GhostDraw;
    this->drawTable[FirstRolePage + 5] = &Menu::OptionsDraw;
    this->drawTable[FirstRolePage + 6] = &Menu::BestiaryDraw;
    this->enterTable[FirstRolePage] = &Menu::ItemsEnter;
    this->enterTable[FirstRolePage + 1] = &Menu::EquipEnter;
    this->enterTable[FirstRolePage + 2] = &Menu::SkillsEnter;
    this->enterTable[FirstRolePage + 3] = &Menu::AltarEnter;
    this->enterTable[FirstRolePage + 4] = &Menu::GhostEnter;
    this->enterTable[FirstRolePage + 5] = &Menu::OptionsEnter;
    this->enterTable[FirstRolePage + 6] = &Menu::BestiaryEnter;
    this->leaveTable[FirstRolePage] = &Menu::ItemsLeave;
    this->leaveTable[FirstRolePage + 1] = &Menu::EquipLeave;
    this->leaveTable[FirstRolePage + 2] = &Menu::SkillsLeave;
    this->leaveTable[FirstRolePage + 3] = &Menu::AltarLeave;
    this->leaveTable[FirstRolePage + 4] = &Menu::GhostLeave;
    this->leaveTable[FirstRolePage + 5] = &Menu::OptionsLeave;
    this->leaveTable[FirstRolePage + 6] = &Menu::BestiaryLeave;
}

// swd3e.exe 0x0044DD40: the mouse on the role pages' titles (run before every frame of them): a click on a title opens
// its page (the page open first left); a right click on the main page closes the menu
void Menu::MainFrame()
{
    Game& owner = this->game;
    const auto x = static_cast<u32>(owner.state.pointerX);
    const auto y = static_cast<u32>(owner.state.pointerY);
    const u32 step = (x - kTitlesLeft) / kTitleStep;
    const u32 within = step % kTitleSteps;

    if (x <= static_cast<u32>(kTitlesLeft) || x >= static_cast<u32>(kTitlesRight))
    {
        return;
    }

    if (y < kTitlesBottom && y > kTitlesTop && (this->clicks & kLeftClick) != 0 && within > 0 && within < kTitleGap)
    {
        if (this->code != MainCode)
        {
            this->Call(this->leaveTable[this->page]);
        }

        this->page = static_cast<u16>(step / kTitleSteps + kFirstClickedPage);
        this->MainLeft();
        this->MainSelect();
        return;
    }

    if ((this->clicks & kRightClicks) != 0 && this->code == MainCode)
    {
        owner.controls.mouseSuppressed = static_cast<i32>(kMouseSuppressed);
        this->MainCancel();
    }
}

// swd3e.exe 0x0044DE10: left on the main page: the title before (from the first role page on)
void Menu::MainLeft()
{
    Game& owner = this->game;
    // The page is a dword in the original, compared signed after the decrement.
    const i32 before = static_cast<i32>(this->page) - 1;
    u16 page = static_cast<u16>(before);
    this->page = page;

    if (before <= FirstRolePage - 1)
    {
        page = FirstRolePage;
        this->page = FirstRolePage;
    }

    this->mode = static_cast<u16>(PageModeStep * page - (PageModeStep * FirstRolePage - FirstPageMode));
    this->lastMode = this->mode;
    this->ShowPageBackground(page);
    owner.audio.PlaySound(kTitleSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044DE70: right on the main page: the next title (up to page 0x10)
void Menu::MainRight()
{
    Game& owner = this->game;
    u16 page = static_cast<u16>(this->page + 1);
    this->page = page;

    if (this->page > kLastPage)
    {
        page = kLastPage;
        this->page = kLastPage;
    }

    this->mode = static_cast<u16>(PageModeStep * page - (PageModeStep * FirstRolePage - FirstPageMode));
    this->lastMode = this->mode;
    this->ShowPageBackground(page);
    owner.audio.PlaySound(kTitleSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044DEC0: Enter on the main page: the page of the title opens (its background, its handlers, its opening
// handler)
void Menu::MainSelect()
{
    Game& owner = this->game;
    this->code = PageCode;

    if (this->page != kBestiaryPage)
    {
        this->ShowPageBackground(this->page);
    }

    this->SetPage(this->code);
    this->Call(this->enterTable[this->page]);
    owner.audio.PlaySound(kPageSound, owner.config.soundVolume);
}

// swd3e.exe 0x0044DF20: Escape on the main page: the menu closes
void Menu::MainCancel()
{
    // The code is a dword in the original, compared signed after the decrement.
    const i32 code = static_cast<i32>(this->code) - 1;
    this->code = static_cast<u16>(code);

    if (code <= 0)
    {
        this->game.menuState = Game::NoMenu;
    }

    this->SetPage(this->code);
}

// swd3e.exe 0x0044DF50: the main page: the page of the title chosen drawn
void Menu::MainDraw()
{
    this->Call(this->drawTable[this->page]);
}

}  // namespace swd3e
