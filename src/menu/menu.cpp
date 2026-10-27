#include "menu/menu.h"

#include <cmath>
#include <span>
#include <string>
#include <utility>

#include "core/format.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game/game.h"
#include "pic/picture.h"
#include "platform/display.h"
#include "text/text_input.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

constexpr i32 kRepeatAfter = 7;  // frames a key slot is held before it repeats

constexpr int kFirstRoleFlag = GameState::InParty;
constexpr int kSwappedPagesFlag = GameState::SwappedPages;
constexpr int kHiddenPointerFlag = GameState::NoChoicePointer;
constexpr int kMenuSound = 0xBB;
constexpr int kSlideMaximum = 0x320;
constexpr int kTabGlowMaximum = 0x10;

// The modes of the role pages (Menu::mode) SetPage tells them by.
constexpr std::pair<u16, u16> kItemsModes = {0x1E, 0x20};
constexpr std::pair<u16, u16> kEquipmentModes = {0x24, 0x29};
constexpr std::pair<u16, u16> kSkillsModes = {0x2A, 0x2E};
constexpr std::pair<u16, u16> kAltarModes = {0x30, 0x34};
constexpr std::pair<u16, u16> kGhostModes = {0x36, 0x3A};  // swd3e: the talisman ghost's feeding page
constexpr std::pair<u16, u16> kOptionsModes = {0x3C, 0x3E};
constexpr std::pair<u16, u16> kBestiaryModes = {0x42, 0x47};

// The actions of the interface act (Scene::InterfaceAct) and of the effect act in the menus.
constexpr i32 kTabsPanelAction = 1;
constexpr i32 kFaceAction = 3;
constexpr i32 kHitPointBarAction = 4;
constexpr i32 kSpiritBarAction = 5;
constexpr i32 kStaminaBarAction = 6;
constexpr i32 kBarsTrackAction = 0x51;  // the track under the bars of the role tabs
constexpr i32 kFirstTabAction = 8;      // the animations of the four tabs
constexpr i32 kBarTrackAction = 0x18;
constexpr i32 kBarThumbAction = 0x19;
constexpr i32 kListUpArrowAction = 0x1A;
constexpr i32 kListDownArrowAction = 0x1B;
constexpr i32 kRoleBackgroundAction = 0x34;
constexpr i32 kSystemBackgroundAction = 0x4E;
constexpr i32 kFirstSystemItemAction = 0x2C;  // of the effect act: the four title entries

// Menu::frames: the frame counter of an opening menu.
constexpr i32 kOpeningFrame = 0x1E;
constexpr i32 kItemsOpeningFrame = 0x24;
constexpr i32 kSystemOpenedFrame = 0x40;
constexpr i32 kOpenedFrame = 0x41;  // the first frame shown: the tabs start sliding in
constexpr i32 kSlideFrames = 0x4B;  // the background slides until this frame
constexpr int kSlideGrowth = 2;     // sliding out, the tabs double their offset every frame
constexpr int kBackgroundX = 0xB4;

constexpr int kShadowRed = 6;  // the shadow of the menu fonts
constexpr int kShadowGreen = 4;
constexpr int kShadowBlue = 3;
constexpr i32 kEscapeDelay = 6;

// Menu::clicks.
constexpr u32 kLeftClick = 1;
constexpr u32 kLeftDoubleClick = 2;
constexpr u32 kRightClick = 4;
constexpr u32 kRightDoubleClick = 8;

// The title of a role page (0x0043F3B0).
constexpr i32 kTitleLit = 0x10;  // the glow of a fully lit title (counted, no longer drawn by swd3e)
constexpr i32 kTitleDim = 8;
constexpr int kTitleY = 9;
constexpr int kTitleStep = 0x46;
constexpr int kTitleBase = -0x22F;
constexpr i32 kTitleActions = 5;  // the title of page p is action p + kTitleActions of the interface act
constexpr i32 kTitleArrowDirection = ActState::Left;
// The title's offsets by page 0xB..0x10 (a local table of 0x0043F3B0; the other entries are never read).
constexpr std::array<int, Menu::Pages> kTitleOffsets = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -2, 2, 7, 0xB, 0};
// 0x004AAC88 by page: the role panel's mode (DrawRoleTabs); the items 1, the equipment 2, the ghost page 3. Only
// pages 0xB..0x10 read it (exe bytes 1, 2, 1, 1, 3, 1); the dwords before them are the end of a string.
constexpr std::array<int, Menu::Pages> kPanelModes = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 3, 1, 0};

// The role panel (0x0043F4A0).
enum PanelMode : int
{
    TabsPanel = 1,   // the three role tabs
    RolePanel = 2,   // the role chosen with its attributes and resistances
    GhostPanel = 3,  // the talisman ghost
};

constexpr int kPanelTextRed = 0xF;
constexpr int kPanelTextGreen = 0xD;
constexpr int kPanelTextBlue = 0xB;
constexpr int kPanelShadowRed = 0x1A;
constexpr int kPanelShadowGreen = 0x19;
constexpr int kPanelShadowBlue = 0x16;
constexpr int kTabGlowRise = 3;
constexpr int kVisibleRoles = 3;
constexpr int kFaceStep = 0x32;
constexpr int kFaceX = 0x38;
constexpr int kFaceY = 0x20;
constexpr i32 kFaceActions = 0x5C;      // the face of role r is action r + kFaceActions of the interface act
constexpr i32 kTabActions = 8;          // the tab of role r is action r + kTabActions
constexpr i32 kPortraitActions = 0x48;  // the portrait of role r is action r + kPortraitActions
constexpr i32 kGhostBackground = 0x5B;
constexpr i32 kGhostKindAct = 0x2333;
constexpr int kGhostKindX = 0x5C;
constexpr int kGhostKindY = 0x78;
constexpr int kBarX = 0x68;
constexpr int kBarLength = 0x4F;
constexpr int kBarClipX = 0x69;
constexpr int kHitPointBarDown = 0x4E;  // below the tab's top
constexpr int kSpiritBarDown = 0x64;
constexpr int kStaminaBarDown = 0x7A;
constexpr int kRoleBarsUp = 4;  // the role panel draws its bars and texts four pixels higher
constexpr int kNameX = 0x6C;
constexpr int kNameDown = 0x26;
constexpr int kLevelX = 0x5C;
constexpr int kLevelDown = 0x10;
constexpr int kRatioX = 0x6C;
constexpr int kHitPointsDown = 0x40;
constexpr int kSpiritDown = 0x56;
constexpr int kStaminaDown = 0x6C;
constexpr u32 kDefeatedMode = 0x28;  // ActDrawer::DrawWithMode of a defeated role's tab
constexpr int kDefeatedShadeX = 0xE;
constexpr int kDefeatedShadeY = 0x31;
constexpr int kDefeatedShadeWidth = 0xAE;
constexpr int kDefeatedShadeHeight = 0x189;
constexpr int kAttributesX = 0x18;
constexpr int kAttributeStep = 0x14;
constexpr int kElementY = 0xBE;
constexpr int kChangeX = 0x8C;
constexpr int kResistancesY = 0x14E;
constexpr std::size_t kRoleElementAt = 8;   // the element's character in kRoleElement
constexpr std::size_t kGhostElementAt = 7;  // in kGhostElement
constexpr int kGhostChangeX = 0x7E;
constexpr int kGhostChangeDown = 5;
constexpr int kGhostAttributesY = 0xAA;
constexpr int kGhostChartX = 0x24;
constexpr int kGhostChartY = 0x14A;
constexpr int kBalanceX = 0x54;
constexpr int kBalanceY = 0x144;
constexpr int kAttackX = 0x16;
constexpr int kDefenseX = 0x92;
constexpr int kTriangleY = 0x19A;
constexpr std::u8string_view kLevelFormat = u8"%2d 級/%d";        // 0x004BE76C
constexpr std::string_view kRatioFormat = "%4d/%4d";              // 0x004BE764
constexpr std::string_view kChangeFormat = "+%d";                 // 0x004BE70C
constexpr std::string_view kGhostChangeFormat = " %d";            // 0x004BE6B0
constexpr std::u8string_view kRoleElement = u8"屬性       ";      // 0x004BE758
constexpr std::u8string_view kGhostElement = u8"屬性          ";  // 0x004BE6C0
// 0x004BE650: the elements' names (a role's by RoleRecord::resistances[10], the ghost's by its kind)
constexpr std::array<std::u8string_view, 5> kElements = {u8"金", u8"木", u8"水", u8"火", u8"土"};
constexpr std::array<std::u8string_view, 4> kRoleAttributes = {
    u8"力量    %d",  // 0x004BE74C
    u8"耐力    %d",  // 0x004BE740
    u8"智慧    %d",  // 0x004BE734
    u8"敏捷    %d",  // 0x004BE728
};

constexpr std::u8string_view kNextLevel = u8"下一級  %d";   // 0x004BE71C
constexpr std::u8string_view kExperience = u8"經驗值  %d";  // 0x004BE710
constexpr std::array<std::u8string_view, 5> kResistances = {
    u8"[金]  %d%%",  // 0x004BE700
    u8"[木]  %d%%",  // 0x004BE6F4
    u8"[水]  %d%%",  // 0x004BE6E8
    u8"[火]  %d%%",  // 0x004BE6DC
    u8"[土]  %d%%",  // 0x004BE6D0
};

constexpr std::u8string_view kGhostStrength = u8"力量   %d";  // 0x004BE6B4
constexpr std::u8string_view kGhostSpirit = u8"靈力   %d";    // 0x004BE6A4
constexpr std::u8string_view kGhostEssence = u8"精魄   %d";   // 0x004BE698
constexpr std::u8string_view kGhostWisdom = u8"智慧   %d";    // 0x004BE68C
constexpr std::u8string_view kGhostAgility = u8"敏捷   %d";   // 0x004BE680
constexpr std::u8string_view kGhostInsight = u8"悟性   %d";   // 0x004BE674
constexpr std::u8string_view kBalance = u8"平衡";             // 0x004BE66C
constexpr std::u8string_view kAttack = u8"攻擊";              // 0x004BE664
constexpr std::u8string_view kDefense = u8"防禦";             // 0x004BE65C
// TalismanGhost::Stat numbers of the ghost panel
constexpr int kStatInsight = 3;
constexpr int kStatSpirit = 0xB;
constexpr int kStatStrength = 0xC;
constexpr int kStatEssence = 0xD;
constexpr int kStatWisdom = 0xE;
constexpr int kStatAgility = 0xF;
constexpr int kHideHudFlag = GameState::HideHud;

constexpr int kBoxMargin = 8;
constexpr int kBoxFrameThickness = 8;
constexpr int kBoxBorder = static_cast<int>(0x80000000u | kBoxFrameThickness);  // bit 31: the frame only

// A key slot acting once when it goes down.
bool Pressed(const ButtonState& button)
{
    return button.press != 0 && button.heldFrames == 1;
}

// A key slot acting when it goes down and every second frame after the seventh.
bool Repeating(const ButtonState& button, i32 parity)
{
    return button.press != 0 && (button.heldFrames == 1 || ((parity & 1) == 0 && button.heldFrames > kRepeatAfter));
}

// The length of a role tab's bar of `value` out of `maximum`. The rebuild (deviations.md): a maximum of 0 (the name
// code 燕赤霞 sets the stamina's) gives an empty bar; the original divides by it and fails.
int BarLength(i32 value, i32 maximum)
{
    return maximum != 0 ? kBarLength * value / maximum : 0;
}

}  // namespace

void Menu::Nothing()
{
}

void Menu::Call(Handler handler)
{
    if (handler != nullptr)
    {
        (this->*handler)();
    }
}

// swd3e.exe 0x0043EB10: the menu of the frame. On entering (bit 31 of the menu state) the roles in the party are
// listed and the page opens: the role pages (menus 1..2), the system page in its title mode (3, 6), or modes 1 and 2
// (4, 5) after the picture cache is flushed; then the keys, the handlers and the drawing of the page run.
void Menu::Run()
{
    Game& owner = this->game;
    const u32 state = owner.menuState;

    if (static_cast<i32>(state) < 0)
    {
        this->ListPartyRoles();
        this->pointerX = 0;
        this->pointerY = 0;
        this->tabsPanelAct.action = kTabsPanelAction;
        const u32 menu = state & Game::MenuNumberMask;

        if (menu <= Game::ItemChoiceMenu)
        {
            owner.menuMode = Menu::NoSlotsMode;
            this->background.id = Scene::InterfaceAct;
            this->background.action = kRoleBackgroundAction;
            this->upArrowAct.id = Scene::InterfaceAct;
            this->downArrowAct.id = Scene::InterfaceAct;
            this->upArrowAct.action = kListUpArrowAction;
            this->downArrowAct.action = kListDownArrowAction;

            for (std::size_t item = 0; item < this->items.size(); ++item)
            {
                this->items[item].id = Scene::InterfaceAct;
                this->items[item].action = static_cast<i32>(kFirstTabAction + item);
            }

            this->openedOnEquipment = 0;
            this->frames = kOpeningFrame;

            if ((state & Game::MenuOnEquipment) != 0)
            {
                this->openedOnEquipment = 1;
                this->frames = kItemsOpeningFrame;
                this->Open(Menu::EquipmentMode, Menu::PageCode);
                this->frames = kOpenedFrame;
                this->Call(this->enterTable[this->page]);
                this->beforeFrame = nullptr;
                this->frame = &Menu::EquipFrame;
            }
            else
            {
                this->Open(Menu::FirstPageMode, Menu::MainCode);
            }

            owner.audio.PlaySound(kMenuSound, owner.config.soundVolume);
        }

        if (menu == Game::TitleScreenMenu || menu == Game::NameEntryMenu)
        {
            owner.archive.Flush();

            for (ActState& item : this->items)
            {
                item = ActState{};
                ResetPlayback(item);
                item.id = Scene::EffectAct;
            }

            this->background.action = kSystemBackgroundAction;

            for (std::size_t item = 0; item < this->items.size(); ++item)
            {
                this->items[item].action = static_cast<i32>(kFirstSystemItemAction + item);
            }

            this->Open(menu == Game::NameEntryMenu ? Menu::LoadMode : Menu::TitleMode, Menu::SystemPage);
        }

        if (menu == Game::SystemMenuA)
        {
            owner.archive.Flush();
            this->Open(Menu::GameModeA, Menu::SystemPage);
        }

        if (menu == Game::SystemMenuB)
        {
            owner.archive.Flush();
            this->Open(Menu::GameModeB, Menu::SystemPage);
        }

        this->frames = kSystemOpenedFrame;
        this->flags = 0;
        owner.menuState &= Game::MenuEnteredMask;
    }

    ++this->frames;
    this->ReadMouse();
    this->Dispatch();

    if (owner.menuState != Game::NoMenu)
    {
        this->Present();
    }

    if (owner.menuState == Game::NoMenu)
    {
        this->flags &= ~2u;
    }
}

// swd3e.exe 0x0043EDE0: opens page `menuCode` in `menuMode` (the role pages count from 0x1E in steps of 6): its
// handlers, the tabs, cleared keys and the shadow of the three fonts
void Menu::Open(int menuMode, u16 menuCode)
{
    Game& owner = this->game;
    this->code = menuCode;
    this->roleTab = Menu::NoTab;
    this->page = static_cast<u16>((menuMode - Menu::FirstPageMode) / Menu::PageModeStep + Menu::FirstRolePage);
    this->mode = static_cast<u16>(menuMode);
    this->lastMode = static_cast<u16>(menuMode);
    this->openedMode = static_cast<u16>(menuMode);
    this->SetPage(menuCode);
    this->BuildTabs(this->roleTab);
    owner.controls.buttons = {};
    this->tint = 0;
    const auto shadow = static_cast<Pixel>(owner.painter.MakeColor(kShadowRed, kShadowGreen, kShadowBlue));

    for (TextFont* font : {&owner.largeFont, &owner.mediumFont, &owner.smallFont})
    {
        font->SetShadow(shadow);
    }

    for (TextFont* font : {&owner.largeFont, &owner.mediumFont, &owner.smallFont})
    {
        font->SetBackground(TextFont::NoBackground);
    }
}

// swd3e.exe 0x00441910: the roles in the party (flags 0x1E..0x21) listed in order; their count
int Menu::ListPartyRoles()
{
    this->visibleRoles.fill(Menu::NoVisibleRole);
    this->visibleCount = 0;

    for (int role = 0; role < Party::Roles; ++role)
    {
        if (this->game.state.GameFlag(role + kFirstRoleFlag))
        {
            this->visibleRoles[static_cast<std::size_t>(this->visibleCount++)] = role;
        }
    }

    return this->visibleCount;
}

// swd3e.exe 0x00441960: the role of tab `slot` (-1 past the third)
int Menu::VisibleRole(int slot) const
{
    if (static_cast<u32>(slot) > kVisibleRoles - 1)
    {
        return Menu::NoVisibleRole;
    }

    return this->visibleRoles[static_cast<std::size_t>(slot)];
}

// swd3e.exe 0x00441980: the roles with a tab (counted by ListPartyRoles)
int Menu::VisibleCount() const
{
    return this->visibleCount;
}

// swd3e.exe 0x00440570: `text` in `font` with a shadow two pixels right and one down, the text drawn twice a pixel
// apart (bold)
void Menu::DrawShadowText(int x, int y, Pixel color, Pixel shadow, std::string_view text, TextFont& font)
{
    Pixel* target = this->game.drawSurface;
    font.DrawText(target, x + 2, y + 1, text, shadow, TextFont::Plain);
    font.DrawText(target, x, y, text, color, TextFont::Plain);
    font.DrawText(target, x + 1, y, text, color, TextFont::Plain);
}

// swd3e.exe 0x0043EEB0: a tab for each of the first three roles listed (ListPartyRoles); the original then copies a
// word of the tab past the last one. `selected` is not read.
void Menu::BuildTabs([[maybe_unused]] int selected)
{
    for (MenuTab& tab : this->tabs)
    {
        tab.unk0A = 0;
        tab.state = 0;
        tab.stateCopy = 0;
    }

    this->unk4BE644 = Menu::NoRole;

    for (MenuTab& tab : this->tabs)
    {
        tab.role = Menu::NoRole;
    }

    int count = 0;

    for (int slot = 0; slot < kVisibleRoles; ++slot)
    {
        const int role = this->VisibleRole(slot);

        if (role >= 0)
        {
            this->tabs[static_cast<std::size_t>(count++)].role = static_cast<u16>(role);
        }
    }

    this->tabs[static_cast<std::size_t>(count)].state = this->tabs[static_cast<std::size_t>(count)].frameAct;
}

// swd3e.exe 0x00440CF0: the handlers of page `menuCode`: the system page; the main page (the page tables filled, its
// titles' mouse run before every frame); a role page by the mode (items, equipment, skills, the altar, the talisman
// ghost's feeding page, the system tab, the bestiary). A slot a page does not set keeps the handler of the page before
// it.
void Menu::SetPage(int menuCode)
{
    if (menuCode == Menu::SystemPage)
    {
        this->SystemEnter();
        this->beforeFrame = nullptr;
        this->frame = &Menu::SystemFrame;
        this->down = &Menu::SystemDown;
        this->up = &Menu::SystemUp;
        this->left = &Menu::SystemLeft;
        this->right = &Menu::SystemRight;
        this->pageDown = &Menu::SystemLast;
        this->pageUp = &Menu::SystemFirst;
        this->shift = &Menu::SystemShift;
        this->end = &Menu::SystemEnd;
        this->select = &Menu::SystemSelect;
        this->cancel = &Menu::SystemCancel;
        this->draw = &Menu::SystemDraw;
        return;
    }

    if (menuCode == Menu::MainCode)
    {
        this->FillPageTables();
        this->beforeFrame = &Menu::MainFrame;
        this->frame = &Menu::Nothing;
        this->down = &Menu::Nothing;
        this->up = &Menu::Nothing;
        this->left = &Menu::MainLeft;
        this->right = &Menu::MainRight;
        this->pageDown = &Menu::Nothing;
        this->pageUp = &Menu::Nothing;
        this->shift = &Menu::Nothing;
        this->end = &Menu::Nothing;
        this->select = &Menu::MainSelect;
        this->cancel = &Menu::MainCancel;
        this->draw = &Menu::MainDraw;
        return;
    }

    if (menuCode != Menu::PageCode)
    {
        return;
    }

    const u16 mode = this->mode;

    if (mode >= kItemsModes.first && mode <= kItemsModes.second)
    {
        this->frame = &Menu::ItemsFrame;
        this->down = &Menu::ItemsDown;
        this->up = &Menu::ItemsUp;
        this->left = &Menu::ItemsLeft;
        this->right = &Menu::ItemsRight;
        this->pageDown = &Menu::ItemsPageDown;
        this->pageUp = &Menu::ItemsPageUp;
        this->shift = &Menu::ItemsShift;
        this->end = &Menu::ItemsEnd;
        this->select = &Menu::ItemsSelect;
        this->cancel = &Menu::ItemsCancel;
        this->draw = &Menu::ItemsDraw;
    }
    else if (mode >= kEquipmentModes.first && mode <= kEquipmentModes.second)
    {
        this->frame = &Menu::EquipFrame;
        this->down = &Menu::EquipDown;
        this->up = &Menu::EquipUp;
        this->left = &Menu::EquipLeft;
        this->right = &Menu::EquipRight;
        this->pageDown = &Menu::EquipPageDown;
        this->pageUp = &Menu::EquipPageUp;
        this->shift = &Menu::EquipShift;
        this->end = &Menu::Nothing;
        this->select = &Menu::EquipSelect;
        this->cancel = &Menu::EquipCancel;
        this->draw = &Menu::EquipDraw;
    }
    else if (mode >= kSkillsModes.first && mode <= kSkillsModes.second)
    {
        this->frame = &Menu::SkillsFrame;
        this->down = &Menu::SkillsDown;
        this->up = &Menu::SkillsUp;
        this->left = &Menu::SkillsLeft;
        this->right = &Menu::SkillsRight;
        this->pageDown = &Menu::SkillsPageDown;
        this->pageUp = &Menu::SkillsPageUp;
        this->shift = &Menu::SkillsShift;
        this->end = &Menu::SkillsEnd;
        this->select = &Menu::SkillsSelect;
        this->cancel = &Menu::SkillsCancel;
        this->draw = &Menu::SkillsDraw;
    }
    else if (mode >= kAltarModes.first && mode <= kAltarModes.second)
    {
        this->frame = &Menu::AltarFrame;
        this->down = &Menu::AltarDown;
        this->up = &Menu::AltarUp;
        this->left = &Menu::AltarLeft;
        this->right = &Menu::AltarRight;
        this->pageDown = &Menu::AltarPageDown;
        this->pageUp = &Menu::AltarPageUp;
        this->shift = &Menu::AltarShift;
        this->select = &Menu::AltarSelect;
        this->cancel = &Menu::AltarCancel;
        this->draw = &Menu::AltarDraw;
    }
    else if (mode >= kGhostModes.first && mode <= kGhostModes.second)
    {
        this->frame = &Menu::GhostFrame;
        this->down = &Menu::GhostDown;
        this->up = &Menu::GhostUp;
        this->left = &Menu::GhostLeft;
        this->right = &Menu::GhostRight;
        this->pageDown = &Menu::GhostPageDown;
        this->pageUp = &Menu::GhostPageUp;
        this->shift = &Menu::GhostShift;
        this->select = &Menu::GhostSelect;
        this->cancel = &Menu::GhostCancel;
        this->draw = &Menu::GhostDraw;
    }
    else if (mode >= kOptionsModes.first && mode <= kOptionsModes.second)
    {
        this->frame = &Menu::OptionsFrame;
        this->down = &Menu::OptionsDown;
        this->up = &Menu::OptionsUp;
        this->left = &Menu::OptionsLeft;
        this->right = &Menu::OptionsRight;
        this->pageDown = &Menu::OptionsPageDown;
        this->pageUp = &Menu::OptionsPageUp;
        this->shift = &Menu::Nothing;
        this->select = &Menu::OptionsSelect;
        this->cancel = &Menu::OptionsCancel;
        this->draw = &Menu::OptionsDraw;
    }
    else if (mode >= kBestiaryModes.first && mode <= kBestiaryModes.second)
    {
        this->frame = &Menu::BestiaryFrame;
        this->down = &Menu::BestiaryDown;
        this->up = &Menu::BestiaryUp;
        this->left = &Menu::BestiaryLeft;
        this->right = &Menu::BestiaryRight;
        this->pageDown = &Menu::BestiaryPageDown;
        this->pageUp = &Menu::BestiaryPageUp;
        this->shift = &Menu::BestiaryShift;
        this->select = &Menu::Nothing;
        this->cancel = &Menu::BestiaryCancel;
        this->draw = &Menu::BestiaryDraw;
    }
}

// swd3e.exe 0x0043EF70: the handlers of the frame and of the keys going down (the cursor keys repeat). The repeats of
// Left and Right test the parity of PageUp's held frames.
void Menu::Dispatch()
{
    Controls& keys = this->game.controls;
    this->Call(this->beforeFrame);
    this->Call(this->frame);

    if (Pressed(keys.At(Button::Escape)))
    {
        this->Call(this->cancel);
    }

    if (this->escapeDelay == 0 && Pressed(keys.At(Button::RightControl)))
    {
        this->escapeDelay = kEscapeDelay;
        this->Call(this->cancel);
    }

    if (--this->escapeDelay < 0)
    {
        this->escapeDelay = 0;
    }

    if (Pressed(keys.At(Button::RightShift)))
    {
        this->Call(this->shift);
    }

    if (Pressed(keys.At(Button::End)))
    {
        this->Call(this->end);
    }

    const ButtonState& downKey = keys.At(Button::Down);

    if (Repeating(downKey, downKey.heldFrames))
    {
        this->Call(this->down);
    }

    const ButtonState& upKey = keys.At(Button::Up);

    if (Repeating(upKey, upKey.heldFrames))
    {
        this->Call(this->up);
    }

    const ButtonState& pageDownKey = keys.At(Button::PageDown);

    if (Repeating(pageDownKey, pageDownKey.heldFrames))
    {
        this->Call(this->pageDown);
    }

    const ButtonState& pageUpKey = keys.At(Button::PageUp);

    if (Repeating(pageUpKey, pageUpKey.heldFrames))
    {
        this->Call(this->pageUp);
    }

    if (Repeating(keys.At(Button::Left), keys.At(Button::PageUp).heldFrames))
    {
        this->Call(this->left);
    }

    if (Repeating(keys.At(Button::Right), keys.At(Button::PageUp).heldFrames))
    {
        this->Call(this->right);
    }

    if (Pressed(keys.At(Button::Space)) || Pressed(keys.At(Button::Enter)))
    {
        this->Call(this->select);
    }
}

// swd3e.exe 0x00441270: the clicks of the frame: bits 0..1 a single or double left click, bits 2..3 the right button
void Menu::ReadMouse()
{
    const Controls& keys = this->game.controls;
    this->clicks = 0;
    const ButtonState& leftButton = keys.At(Button::MouseLeft);

    if (leftButton.heldFrames == 1)
    {
        if (leftButton.press == 1)
        {
            this->clicks = kLeftClick;
        }
        else if (leftButton.press > 1)
        {
            this->clicks = kLeftDoubleClick;
        }
    }

    const ButtonState& rightButton = keys.At(Button::MouseRight);

    if (rightButton.heldFrames == 1)
    {
        if (rightButton.press == 1)
        {
            this->clicks |= kRightClick;
        }
        else if (rightButton.press > 1)
        {
            this->clicks |= kRightDoubleClick;
        }
    }
}

// swd3e.exe 0x0043F110: the page drawn over its background (sliding in on the role pages), then the role tabs, the
// gold on page 1, the pointer, a tint, the corner notices, and the frame shown; the last page and the pointer kept.
void Menu::Present()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    // The rebuild's continuous motion: the main menu of the map only (the title and the system menus as they were).
    const bool smooth = (owner.menuState & Game::MenuNumberMask) == Game::MainMenu;

    if (smooth)
    {
        this->KeepFrame();
    }

    owner.drawSurface = owner.painter.LockSurface(*owner.offscreen);
    owner.painter.SetTarget(owner.drawSurface);
    owner.painter.UnlockSurface(*owner.offscreen);

    if (!this->PaintPage(false))
    {
        return;
    }

    if (owner.menuMode == Menu::NoSlotsMode && smooth)
    {
        this->FrameDrawn();
        owner.ShowMenuFrame();
    }
    else if (owner.menuMode == Menu::NoSlotsMode)
    {
        owner.ShowFrame();
    }

    this->lastPage = this->page;
    this->pointerX = state.pointerX;
    this->pointerY = state.pointerY;
}

// The drawing of Present (0x0043F110) on the target set; `replay`: painted again between frames (the notices kept as
// they are). The background slides in from the right; the role tabs halve their slide every frame, except while the
// altar (pages swapped) or the first page's subpage codes slide them out, doubling it. The system page and the
// bestiary have no tabs; the gold shows on the main page unless flag 0x51 hides it.
bool Menu::PaintPage(bool replay)
{
    Game& owner = this->game;
    GameState& state = owner.state;
    const i32 slideBefore = this->frames == kOpenedFrame ? kSlideMaximum : this->slide;
    this->slide = slideBefore;

    // The slide doubles instead of halving on the altar's role page (swapped) and the role subpages; the tabs ask again
    // after the page drew itself, with the page, the code and the flag as they are then.
    auto doublingSlide = [this, &state]()
    {
        return (this->page == Menu::AltarRolePage && this->code > Menu::MainCode &&
                state.GameFlag(kSwappedPagesFlag)) ||
               (this->page == Menu::FirstRolePage && this->code >= Menu::SubpageCodes);
    };

    const bool swapped = state.GameFlag(kSwappedPagesFlag);

    if (!doublingSlide())
    {
        this->slide >>= 1;
        this->Move(SlideValue, this->slide, slideBefore, this->slide);
    }

    if (this->code != Menu::SystemPage)
    {
        int offset = this->frames < kSlideFrames ? this->Shown(SlideValue, this->slide) : 0;

        if (swapped && this->page == Menu::GhostFeedPage)
        {
            offset = 0;
        }

        owner.actDrawer.Draw(this->background, offset + kBackgroundX, 0);
    }

    if (this->code != Menu::MainCode)
    {
        this->Call(this->draw);

        if (owner.menuState == Game::NoMenu)
        {
            return false;
        }
    }

    if (owner.menuMode != Menu::NoSlotsMode)
    {
        return true;
    }

    if (this->code != Menu::SystemPage && this->page != Menu::BestiaryPage)
    {
        if (doublingSlide())
        {
            if (this->slide == 0)
            {
                this->slide = 1;
            }

            const i32 drawn = this->slide;
            const i32 shown = this->Shown(SlideValue, drawn);
            owner.actDrawer.Draw(this->tabsPanelAct, -shown, 0);
            this->DrawRoleTabs(shown, RolePanel);
            this->slide *= kSlideGrowth;

            if (this->slide > kSlideMaximum)
            {
                this->slide = kSlideMaximum;
            }

            this->Move(SlideValue, drawn, drawn, this->slide);
        }
        else
        {
            const i32 shown = this->Shown(SlideValue, this->slide);
            this->DrawPageTitle();
            owner.actDrawer.Draw(this->tabsPanelAct, -shown, 0);
            this->DrawRoleTabs(shown, this->code == Menu::MainCode ? TabsPanel : kPanelModes[this->page]);
        }
    }

    owner.scene.pointerRole.act.action = Scene::ChoicePointer;

    if (!state.GameFlag(kHiddenPointerFlag))
    {
        owner.RefreshCursor();
    }

    if (!state.GameFlag(kHideHudFlag) && this->code == Menu::MainCode)
    {
        owner.actDrawer.DrawCounter(
            ActDrawer::GoldX,
            ActDrawer::GoldY,
            static_cast<u32>(owner.party.variables[Party::Gold]),
            owner.counterAlpha
        );
    }

    if (this->tint != 0)
    {
        owner.painter
            .TintPixels(std::span<u16>(owner.drawSurface, Display::Pixels), this->tint, this->tint, this->tint);
    }

    owner.notices.Draw(owner.smallFont, owner.painter, state, owner.textColors, !replay);
    return true;
}

// swd3e.exe 0x0043F3B0: the title of the role page: the arrow (facing left) and the page's title picture; the glow
// still counts in (out on the other codes) over 8..16 steps, read by nothing
void Menu::DrawPageTitle()
{
    Game& owner = this->game;
    const int last = this->frames == kOpenedFrame ? 0 : this->titleGlow;

    if (this->code == Menu::MainCode)
    {
        this->titleGlow = last + 1 > kTitleLit ? kTitleLit : last + 1;
    }
    else
    {
        this->titleGlow = last - 1 < kTitleDim ? kTitleDim : last - 1;
    }

    this->Move(TitleGlowValue, this->titleGlow, last, this->titleGlow);
    this->titleArrowAct.direction = kTitleArrowDirection;
    owner.actDrawer.Draw(this->titleArrowAct, 0, kTitleY);
    this->titleArrowAct.flags &= Painter::RleSource | Painter::Mirror | Painter::Flip;
    this->titleAct.id = Scene::InterfaceAct;
    this->titleAct.action = this->page + kTitleActions;
    owner.actDrawer.Draw(this->titleAct, kTitleOffsets[this->page] + kTitleStep * this->page + kTitleBase, kTitleY);
}

// swd3e.exe 0x0043F4A0: the role panel shifted left by `offset`, by `mode`: the three role tabs (a face for each above,
// the chosen one bordered; each tab glowing up when chosen, with its bars, name, level and the experience still
// needed, the three ratios, greyed when defeated); the role chosen alone with its attributes, the equipment's changes
// on the equipment page and its resistances; or the talisman ghost with its element, stats, the changes of the food
// chosen and its chart.
void Menu::DrawRoleTabs(int offset, int mode)
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    const auto color = static_cast<Pixel>(painter.MakeColor(kPanelTextRed, kPanelTextGreen, kPanelTextBlue));
    const auto shadow = static_cast<Pixel>(painter.MakeColor(kPanelShadowRed, kPanelShadowGreen, kPanelShadowBlue));
    const int chosen = this->roleTab == Menu::NoTab ? 0 : this->roleTab;

    auto text = [this, color, shadow](int x, int y, std::string_view line, TextFont& font)
    {
        this->DrawShadowText(x, y, color, shadow, line, font);
    };

    auto faces = [this, &owner, offset]()
    {
        for (int slot = 0; slot < kVisibleRoles; ++slot)
        {
            const int role = this->VisibleRole(slot);

            if (role < 0)
            {
                continue;
            }

            this->gauge.id = Scene::InterfaceAct;
            this->gauge.action = role + kFaceActions;
            const int x = kFaceStep * slot - offset + kFaceX;

            if (slot == this->roleTab)
            {
                owner.actDrawer.DrawBordered(this->gauge, x, kFaceY);
            }
            else
            {
                owner.actDrawer.Draw(this->gauge, x, kFaceY);
            }
        }
    };

    auto tracks = [this, &owner, offset](int top)
    {
        const int x = kBarX - offset;
        owner.actDrawer.Draw(this->barsTrack, x, top + kHitPointBarDown);
        owner.actDrawer.Draw(this->barsTrack, x, top + kSpiritBarDown);
        owner.actDrawer.Draw(this->barsTrack, x, top + kStaminaBarDown);
    };

    auto bars = [this, &owner, &painter, offset](const RoleRecord& role, int top, int glow, double fraction)
    {
        const int x = kBarX - offset;
        painter.SetClipClamped(0, 0, BarLength(role.hitPoints, role.maxHitPoints) + kBarClipX, Display::Height);
        owner.actDrawer.DrawGhost(this->hitPointBar, x, top + kHitPointBarDown, glow, fraction);
        painter.SetClipClamped(0, 0, BarLength(role.spirit, role.maxSpirit) + kBarClipX, Display::Height);
        owner.actDrawer.DrawGhost(this->spiritBar, x, top + kSpiritBarDown, glow, fraction);
        painter.SetClipClamped(0, 0, BarLength(role.stamina, role.maxStamina) + kBarClipX, Display::Height);
        owner.actDrawer.DrawGhost(this->staminaBar, x, top + kStaminaBarDown, glow, fraction);
    };

    // The name (the surname and given name), the level with the experience still needed (the experience itself when
    // the next level lists none) and the ratios; `needed` gets the experience of the next level.
    auto figures = [this, &owner, &text, offset](int slot, const RoleRecord& role, int top, u32& needed)
    {
        const int index = this->VisibleRole(slot);
        const RoleName& name = owner.party.names[static_cast<std::size_t>(index)];
        text(kNameX - offset, top + kNameDown, name.Surname() + name.Given(), owner.mediumFont);
        needed = 0;
        owner.database.LoadLevelEntry(index + 1, role.level + 1, needed);

        if (needed == 0)
        {
            needed = static_cast<u32>(role.totalExperience);
        }

        const std::string level =
            WinFormat(kLevelFormat, {static_cast<i32>(role.level), static_cast<i32>(needed) - role.totalExperience});
        text(kLevelX - offset, top + kLevelDown, level, owner.mediumFont);
        text(
            kRatioX - offset,
            top + kHitPointsDown,
            WinFormat(kRatioFormat, {role.hitPoints, role.maxHitPoints}),
            owner.smallFont
        );
        text(
            kRatioX - offset,
            top + kSpiritDown,
            WinFormat(kRatioFormat, {role.spirit, role.maxSpirit}),
            owner.smallFont
        );
        text(
            kRatioX - offset,
            top + kStaminaDown,
            WinFormat(kRatioFormat, {role.stamina, role.maxStamina}),
            owner.smallFont
        );
    };

    if (mode == TabsPanel)
    {
        faces();

        // Three tabs (the original's loop ends at the fourth item, 0x0050AAEC).
        for (std::size_t index = 0; index < static_cast<std::size_t>(kVisibleRoles); ++index)
        {
            i8& glow = this->tabGlow[index];
            const i8 glowBefore = glow;

            if (this->roleTab == index)
            {
                glow = static_cast<i8>(glow + kTabGlowRise > kTabGlowMaximum ? kTabGlowMaximum : glow + kTabGlowRise);
            }
            else
            {
                glow = static_cast<i8>(glow - 1 < Menu::InitialTabGlow ? Menu::InitialTabGlow : glow - 1);
            }

            if (this->roleTab == Menu::NoTab || this->code == Menu::MainCode)
            {
                glow = kTabGlowMaximum;
            }

            this->Move(TabGlowValue + index, glow, glowBefore, glow);
            const double fineGlow = this->ShownFine(TabGlowValue + index, glow);
            const auto shownGlow = static_cast<i32>(std::floor(fineGlow));
            ActState& item = this->items[index];
            item.id = Scene::InterfaceAct;
            item.action = this->VisibleRole(static_cast<int>(index)) + kTabActions;
            const MenuTab& tab = this->tabs[index];

            if (tab.role == Menu::NoRole)
            {
                continue;
            }

            const RoleRecord& role =
                owner.party.roles[static_cast<std::size_t>(this->VisibleRole(static_cast<int>(index)))];
            owner.actDrawer.DrawGhost(item, tab.x - offset, tab.y, shownGlow, fineGlow - shownGlow);
            tracks(tab.y);
            bars(role, tab.y, shownGlow, fineGlow - shownGlow);
            u32 needed = 0;
            figures(static_cast<int>(index), role, tab.y, needed);
            painter.SetClipClamped(0, 0, Display::Width, Display::Height);

            if ((role.condition & RoleRecord::Defeated) != 0)
            {
                owner.actDrawer.DrawWithMode(item, tab.x - offset, tab.y, kDefeatedMode);
            }
        }

        return;
    }

    if (mode == RolePanel)
    {
        const int role = this->VisibleRole(chosen);
        const RoleRecord& record = owner.party.roles[static_cast<std::size_t>(role)];
        ActState& portrait = this->items[static_cast<std::size_t>(chosen)];
        portrait.id = Scene::InterfaceAct;
        portrait.action = role + kPortraitActions;
        owner.actDrawer.Draw(portrait, -offset, 0);
        faces();
        const int top = this->tabs.front().y - kRoleBarsUp;
        tracks(top);

        if ((record.condition & RoleRecord::Defeated) != 0)
        {
            FillBox(
                painter,
                kDefeatedShadeX,
                kDefeatedShadeY,
                kDefeatedShadeWidth,
                kDefeatedShadeHeight,
                0,
                0,
                0,
                FillMode::Gray
            );
        }

        bars(record, top, kTabGlowMaximum, 0.0);
        u32 needed = 0;
        figures(chosen, record, top, needed);
        // The element's name goes into bytes 8 and 9 of the label (the original reads past the names for an element
        // beyond them; nothing is put in then).
        std::string element = Big5(kRoleElement);
        const auto elementAt = static_cast<std::size_t>(static_cast<i8>(record.resistances[RoleRecord::ElementAt]));

        if (elementAt < kElements.size())
        {
            element.replace(kRoleElementAt, 2, Big5(kElements[elementAt]));
        }

        const int x = kAttributesX - offset;
        int y = kElementY;
        text(x, y, element, owner.mediumFont);
        // The attributes are read as unsigned words: a negative one shows as 65536 more.
        const std::array<i32, 4> attributes = {
            static_cast<u16>(record.strength),
            static_cast<u16>(record.endurance),
            static_cast<u16>(record.wisdom),
            static_cast<u16>(record.agility),
        };

        for (std::size_t at = 0; at < attributes.size(); ++at)
        {
            y += kAttributeStep;
            text(x, y, WinFormat(kRoleAttributes[at], {attributes[at]}), owner.mediumFont);
        }

        y += kAttributeStep;
        text(x, y, WinFormat(kNextLevel, {static_cast<i32>(needed)}), owner.mediumFont);
        y += kAttributeStep;
        text(x, y, WinFormat(kExperience, {record.totalExperience}), owner.mediumFont);

        if (this->page == Menu::EquipmentPage)
        {
            // The changes of the equipment (the strength's and the endurance's at the same place, as in the original).
            const RoleRecord& changes = this->equip.bonuses;
            const std::array<std::pair<i16, int>, 4> shown = {{
                {changes.strength, kElementY + 0x18},
                {changes.endurance, kElementY + 0x18},
                {changes.wisdom, kElementY + 0x40},
                {changes.agility, kElementY + 0x54},
            }};

            for (const auto& [change, changeY] : shown)
            {
                if (change == 0)
                {
                    continue;
                }

                std::string line = WinFormat(kChangeFormat, {std::abs(static_cast<i32>(change))});

                if (change < 0)
                {
                    line[0] = '-';
                }

                text(kChangeX, changeY, line, owner.smallFont);
            }
        }

        const std::array<i8, 5>& resistances = this->roleResistances[static_cast<std::size_t>(role)];
        y = kResistancesY;

        for (std::size_t at = 0; at < kResistances.size(); ++at)
        {
            text(x, y, WinFormat(kResistances[at], {static_cast<i32>(resistances[at])}), owner.mediumFont);
            y += kAttributeStep;
        }

        painter.SetClipClamped(0, 0, Display::Width, Display::Height);
        return;
    }

    if (mode != GhostPanel)
    {
        return;
    }

    painter.SetClipClamped(0, 0, Display::Width, Display::Height);
    ActState& background = this->items[0];
    background.id = Scene::InterfaceAct;
    background.action = kGhostBackground;
    owner.actDrawer.Draw(background, -offset, 0);
    const i32 kind = owner.party.variables[Party::GhostVariable];
    ActState& kindAct = this->items[1];
    kindAct.id = kGhostKindAct;
    kindAct.action = kind - 1;

    if (kind - 1 >= 0)
    {
        owner.actDrawer.Draw(kindAct, kGhostKindX, kGhostKindY);
    }

    if (kind == 0)
    {
        return;
    }

    TalismanGhost& talisman = owner.talismanGhost;
    const GhostPage& page = this->ghost;
    std::string element = Big5(kGhostElement);

    if (static_cast<std::size_t>(kind - 1) < kElements.size())
    {
        element.replace(kGhostElementAt, 2, Big5(kElements[static_cast<std::size_t>(kind - 1)]));
    }

    const int x = kAttributesX - offset;
    const int changeX = kGhostChangeX - offset;

    auto change = [&text, changeX, &owner](i32 value, int y)
    {
        std::string line = WinFormat(kGhostChangeFormat, {std::abs(value)});
        line[0] = value > 0 ? '+' : '-';
        text(changeX, y + kGhostChangeDown, line, owner.smallFont);
    };

    int y = kGhostAttributesY;
    text(x, y, element, owner.mediumFont);
    y += kAttributeStep;
    text(x, y, WinFormat(kGhostStrength, {talisman.Stat(kStatStrength)}), owner.mediumFont);

    if (page.compared != -1)
    {
        change(page.strengthChange, y);
    }

    y += kAttributeStep;
    text(x, y, WinFormat(kGhostSpirit, {talisman.Stat(kStatSpirit)}), owner.mediumFont);
    y += kAttributeStep;
    text(x, y, WinFormat(kGhostEssence, {talisman.Stat(kStatEssence)}), owner.mediumFont);

    if (page.compared != -1)
    {
        change(page.essenceChange, y);
    }

    y += kAttributeStep;
    text(x, y, WinFormat(kGhostWisdom, {talisman.Stat(kStatWisdom)}), owner.mediumFont);

    if (page.compared != -1)
    {
        change(page.wisdomChange, y);
    }

    y += kAttributeStep;
    text(x, y, WinFormat(kGhostAgility, {talisman.Stat(kStatAgility)}), owner.mediumFont);

    if (page.compared != -1)
    {
        change(page.agilityChange, y);
    }

    y += kAttributeStep;
    text(x, y, WinFormat(kGhostInsight, {talisman.Stat(kStatInsight)}), owner.mediumFont);
    this->DrawGhostChart(kGhostChartX, kGhostChartY);
    text(kBalanceX, kBalanceY, Big5(kBalance), owner.smallFont);
    text(kAttackX, kTriangleY, Big5(kAttack), owner.smallFont);
    text(kDefenseX, kTriangleY, Big5(kDefense), owner.smallFont);
}

// swd3e.exe 0x004412E0: a menu box: a filled box 8 pixels larger and the frame of act 0x233B
void Menu::DrawBox(int x, int y, int width, int height, int mode, int red, int green, int blue)
{
    Game& owner = this->game;
    FillBox(
        owner.painter,
        x - kBoxMargin,
        y - kBoxMargin,
        width + 2 * kBoxMargin,
        height + 2 * kBoxMargin,
        red,
        green,
        blue,
        static_cast<FillMode>(mode)
    );
    ActState& panel = owner.battleMessages.panelAct;
    panel.id = BattleMessages::MenuPanelAct;
    panel.action = 0;
    owner.acts.Run(panel);
    DrawWindowFrame(owner.painter, owner.archive, panel.frame, x, y, x + width, y + height, 0, kBoxBorder);
}

// swd3e.exe 0x0043E900: the handler tables of the role pages filled and the animations of the menus at their first
// actions
void Menu::InitActs()
{
    Game& owner = this->game;
    this->flags = 0;
    this->FillPageTables();
    ActState& panel = owner.battleMessages.panelAct;

    for (ActState* state :
         {&this->background,
          &this->titleArrowAct,
          &this->tabsPanelAct,
          &this->hitPointBar,
          &this->spiritBar,
          &this->staminaBar,
          &this->barsTrack,
          &this->barTrackAct,
          &this->barThumbAct,
          &this->upArrowAct,
          &this->downArrowAct,
          &this->gauge,
          &this->items[0],
          &this->items[1],
          &this->items[2],
          &this->items[3],
          &this->titleAct,
          &this->face,
          &panel})
    {
        ResetPlayback(*state);
    }

    this->background.id = Scene::InterfaceAct;
    this->background.action = 0;
    this->tabsPanelAct.id = Scene::InterfaceAct;
    this->tabsPanelAct.action = kTabsPanelAction;
    this->titleArrowAct.id = Scene::InterfaceAct;
    this->titleArrowAct.direction = owner.state.GameFlag(kSwappedPagesFlag) ? ActState::Right : ActState::Left;

    for (ActState* state :
         {&this->hitPointBar,
          &this->spiritBar,
          &this->staminaBar,
          &this->barsTrack,
          &this->barTrackAct,
          &this->barThumbAct,
          &this->upArrowAct,
          &this->downArrowAct,
          &this->face})
    {
        state->id = Scene::InterfaceAct;
    }

    this->hitPointBar.action = kHitPointBarAction;
    this->spiritBar.action = kSpiritBarAction;
    this->staminaBar.action = kStaminaBarAction;
    this->barsTrack.action = kBarsTrackAction;
    this->barTrackAct.action = kBarTrackAction;
    this->barThumbAct.action = kBarThumbAction;
    this->upArrowAct.action = kListUpArrowAction;
    this->downArrowAct.action = kListDownArrowAction;

    for (std::size_t item = 0; item < this->items.size(); ++item)
    {
        this->items[item].id = Scene::EffectAct;
        this->items[item].action = static_cast<i32>(kFirstSystemItemAction + item);
    }

    this->face.action = kFaceAction;
    panel.id = BattleMessages::MenuPanelAct;
    panel.action = 0;
}

}  // namespace swd3e
