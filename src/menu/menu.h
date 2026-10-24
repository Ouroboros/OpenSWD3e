#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "config/config.h"
#include "core/types.h"
#include "item/item_list.h"
#include "menu/menu_list.h"
#include "org/object_record.h"
#include "party/party.h"
#include "res/act_state.h"

namespace swd3e
{

class Game;
class Menu;
class TextFont;
class TextInput;

// A role tab of the menus (0x1C bytes, four of them at 0x004BE5D4): the role shown (0xFFFF: none), the box and the
// three animations of the tab with their actions.
struct MenuTab
{
    u16 role = 0;         // +0x00
    u16 unk02 = 0;        // +0x02
    u16 unk04 = 0;        // +0x04
    u16 x = 0;            // +0x06
    u16 y = 0;            // +0x08
    u16 unk0A = 0;        // +0x0A
    u16 state = 0;        // +0x0C 1, 2 for the selected tab
    u16 stateCopy = 0;    // +0x0E
    u16 frameAct = 0;     // +0x10
    u16 frameAction = 0;  // +0x12
    u16 iconAct = 0;      // +0x14
    u16 iconAction = 0;   // +0x16
    u16 nameAct = 0;      // +0x18
    u16 nameAction = 0;   // +0x1A
};

// The bestiary (role page 0x11; 0x004FC90C..0x004FCA80): the monsters of mon.dat 1..500 by category, with what the
// party knows of each from how many it defeated.
struct Bestiary
{
    static constexpr int Monsters = 0x1F4;
    static constexpr int Categories = 0xB;
    static constexpr std::size_t Listed = 0x40;
    static constexpr int Rows = 0xF;
    static constexpr std::size_t Lines = 0x10;

    ItemNode monster;                              // 0x0050B3C4 the monster chosen
    std::array<i8, Monsters + 1> categories = {};  // 0x0050B524 by monster id (-1: none)
    std::array<u8, Monsters + 1> defeated = {};    // 0x0050B528 by monster id
    std::array<i8, Listed> known = {};             // 0x0050B3DC the defeats of each monster listed
    std::array<std::string, Lines> lines;          // 0x0050B3E0 the lines of the monster chosen
    std::array<std::string, Listed> names;         // the names of the monsters listed (empty: not defeated)
    std::array<u32, Listed> ids = {};              // 0x0050B3C8 the monsters listed, ended by 0
    std::size_t firstShown = 0;                    // 0x0050B3CC the first listed shown (a pointer in the original)
    i32 top = 0;                                   // 0x0050B3B8
    i32 category = 0;                              // 0x0050B3BC the tab chosen
    i32 shown = 0;                                 // 0x0050B3C0
    i32 stamina = 0;                               // 0x0050B3D0
    i32 row = 0;                                   // 0x0050B3D4
    i32 spirit = 0;                                // 0x0050B3D8
    i32 count = 0;                                 // 0x0050B420 the monsters listed
    u16 scrollArrows = 0;                          // 0x0050B52C
};

// The altar page (role page 0xE; 0x0050B534..0x0050B7E4, swd3e): two objects of the party in the two slots give one
// result (a recipe of maps.dat table 0x18, else the object of the category the table's matrix gives nearest the
// level aimed at), made with a morph of the pictures.
struct AltarPage
{
    static constexpr int Objects = 0xD48;  // the object tables (0x3520 bytes each)
    static constexpr int Rows = 0x10;
    static constexpr std::size_t MorphColumns = 0x78;
    static constexpr std::size_t MorphRows = 0xDC;
    static constexpr std::size_t WaveSize = 0x100;   // 0x0050B53C: 0x400 bytes
    static constexpr std::size_t WaveCenter = 0x80;  // the entry of an offset of 0 rows

    ItemNode result;                 // 0x0050B564 (allocated while the page is open)
    ItemNode slotA;                  // 0x0050B660
    ItemNode slotB;                  // 0x0050B714
    std::vector<i32> categories;     // 0x0050B570 by object id (-1: not loaded)
    std::vector<i32> levels;         // 0x0050B558
    std::vector<u32> kinds;          // 0x0050B538
    std::vector<i32> altars;         // 0x0050B578 1: never a result
    ItemLink objects;                // 0x0050B590 the objects of the tab, moved out of the inventory
    ItemNode* firstShown = nullptr;  // 0x0050B7D0
    i32 count = 0;                   // 0x0050B584
    i32 top = 0;                     // 0x0050B57C
    i32 row = 0;                     // 0x0050B658
    i32 shown = 0;                   // 0x0050B64C
    i32 tab = 0;                     // 0x0050B588 0..2
    i32 slot = 0;                    // 0x0050B560 the slot an object goes into
    i32 stage = 0;                   // 0x0050B7E0
    u32 tooHigh = 0;                 // 0x0050B554 3: the result is more than six levels above the first role
    i32 hover = 0;                   // 0x0050B580 the pointer is on the run button
    i32 counter = 0;                 // 0x0050B55C the morph
    i32 phase = 0;                   // 0x0050B574
    i32 bob = 0;                     // 0x0050B7E4 the slot marker bobs with it (0..9)
    i32 unk50B534 = 0;               // 0x0050B534 (only cleared)
    u16 scrollArrows = 0;            // 0x0050B54C
    std::array<i16, 4> vitals = {};  // 0x0050B7D4 the spirit and stamina of the result, then two words never set
    std::vector<i16> columnsA;       // 0x0050B7C8 the morph's offsets
    std::vector<i16> columnsB;       // 0x0050B650
    std::vector<i16> rowsA;          // 0x0050B548
    std::vector<i16> rowsB;          // 0x0050B7CC
    std::vector<i32> wave;           // 0x0050B53C rows to pixel offsets from WaveCenter (the ends left unset)
    std::vector<u16> resultPicture;  // 0x0050B644 the pictures of the morph
    std::vector<u16> pictureA;       // 0x0050B544
    std::vector<u16> pictureB;       // 0x0050B540
};

bool AltarTabTakes(const ItemNode& node, int tab);
std::u8string_view CategoryName(u16 category);

// The figures of a role on the equipment page (0x18 dwords, 0x60 bytes): the vitals and maxima, the attributes, the
// word at +0x20, the two equipment bonuses, then the slot's extra figures (percent, proficiency, fame) and the
// weapon's share (0x0048A7A0).
using EquipStats = std::array<i32, 0x18>;

// The equipment page (role page 0xC; 0x0050B7E8..0x0050BA74): the eleven slots of a role and the inventory's objects
// that fit the slot chosen, with the stats each would give.
struct EquipPage
{
    ItemLink takenOff;                           // 0x0050B8E8 where an object of another kind taken off goes
    ItemNode* inventoryHead = nullptr;           // 0x0050B9B8 the inventory's first node when opened
    i32 openedAt = 0;                            // 0x0050B7E8 Menu::frames when opened
    i32 slide = 0;                               // 0x0050B7EC the page slides in
    i32 unk4FCD30 = 0;                           // 0x0050B7F0
    i32 shown = 0;                               // 0x0050B7F4
    i32 slot = 0;                                // 0x0050B7F8 0..10
    i32 row = 0;                                 // 0x0050B7FC
    i32 listX = 0;                               // 0x0050B800
    i32 listY = 0;                               // 0x0050B804
    void (Menu::*savedBeforeFrame)() = nullptr;  // 0x0050B808
    RoleRecord worn;                             // 0x0050B80C the record the stats are computed in
    RoleRecord bonuses;                          // 0x0050B810 the bonuses of the role's equipment (0x00447DF0)
    // 0x0050B814 0: the slots, 1: the list, 5: a description, 0xF: refused
    i32 stage = 0;
    ItemNode* described = nullptr;   // 0x0050B818
    i32 unk4FCD5C = 0;               // 0x0050B81C
    ItemNode* firstShown = nullptr;  // 0x0050B824
    u16 scrollArrows = 0;            // 0x0050B828
    ItemLink objects;                // 0x0050B830 the objects that fit the slot
    i32 top = 0;                     // 0x0050BA6C
    std::array<EquipStats, Party::Roles + 1> stats =
        {};                 // 0x0050BA70 (allocated) the four roles, then the role with the object tried
    i32 count = 0;          // 0x0050BA74
    i32 previousStage = 0;  // 0x0050B9B0 the stage a description returns to
};

// The skills page (role page 0xD; 0x0050BAE0..0x0050BBD8, swd3e): the role's skills of kinds 0x11 and 0x14 in two
// columns of cells, a heading (an object of kind bit 7) across both with a filler cell before it when it would start
// in the second column and one after it; used on the party or on one role; the town and exit warps.
struct SkillsPage
{
    // The kinds of the cells (SkillsPage::cells, set by 0x0044DA00).
    enum Cell : u8
    {
        SkillCell = 1,
        HeadingCell = 2,
        FillerCell = 3,
        NoCell = 4,
    };

    static constexpr std::size_t CellBytes = 0x400;  // the map's size
    static constexpr std::size_t CellBase = 0x80;    // the map's byte of cell 0
    static constexpr u16 FillerId = 0xFFF9;          // ItemNode::id of a filler node

    ItemLink list;                   // 0x0050BB10 the skills taken from the role's list, with the fillers
    ItemNode* firstShown = nullptr;  // 0x0050BB0C
    i32 top = 0;                     // 0x0050BAE0 the first cell shown (even)
    i32 column = 0;                  // 0x0050BAE4 the column left or right chose last
    i32 shown = 0;                   // 0x0050BAEC the cells shown, at most 24
    i32 buttonOffset = 0;            // 0x0050BAF0 -1 while the description button is pressed
    i32 unk50BAF4 = 0;               // 0x0050BAF4 only cleared; the costs and elements show while it is at most 1
    i32 target = 0;                  // 0x0050BAF8 the slot of the visible role a skill is used on
    i32 tabs = 0;                    // 0x0050BAFC three and the target's two extra flags (never read)
    i32 hasSkills = 0;               // 0x0050BB00 a skill cell was made (the empty entry counts)
    i32 descriptionSlide = 0;        // 0x0050BB04
    i32 count = 0;                   // 0x0050BB08 the cells of the list
    i32 row = 0;                     // 0x0050BBC4 the cell chosen among those shown
    i32 roleCycle = 0;               // 0x0050BBC8 the tab Shift steps from (never reset)
    i32 stage = 0;                   // 0x0050BBCC
    i32 descriptionTop = 0;          // 0x0050BBD0
    u16 scrollArrows = 0;            // 0x0050BBD4 the list's (bits 0..7) and the towns' (bits 8..15)
    std::vector<u8> cells;           // 0x0050BBD8 the cell kinds from CellBase, allocated while the page is open
    SkillScript skill;               // 0x004D3F98 the skill loaded last
};

// A town of the town warp (0x004FB8D0 holds them, six bytes and the name).
struct MenuTown
{
    u16 map = 0;
    u16 x = 0;
    u16 y = 0;
    std::string name;
};

// The system tab (role page 0xF; 0x004FD2E0..0x004FD3A8): save, load, the notes, the settings with the key setup, quit.
struct OptionsPage
{
    i32 entry = 0;     // 0x0050BE7C the tab: save, load, notes, settings, quit
    i32 stage = 0;     // 0x0050BE64 0: the tabs, 1: a tab opened, 2: confirming, 5..7: the key setup, 8: display
    i32 row = 0;       // 0x0050BE78 the row of the settings or of the quit box
    i32 locked = 0;    // 0x0050BF14 the quit fade runs
    i32 choice = 0;    // 0x0050BF10 the confirmation's choice, then the fade's frames from 0x1E
    i32 quitKind = 0;  // 0x0050BE58 1: end the game, 2: restart
    i32 keyRow = 0;    // 0x0050BE5C the key row (0x10..0x12: the buttons)
    std::array<i32, Config::Keys> keys = {};  // 0x0050BE90 the keys being set
    i32 heldKey = 0;                          // 0x0050BE80 a key down since the last one taken
    // 0x0050BE54 the key a swap took from another row (kept, never read)
    i32 swappedKey = 0;
    std::vector<u16> notes;  // 0x0050BE84 the numbers of the notes
    i32 noteCount = 0;       // 0x0050BE8C
    i32 noteTop = 0;         // 0x0050BE60
    i32 notesShown = 0;      // 0x0050BE88
    i32 noteRow = 0;         // 0x0050BE50 (kept, never read)
    std::size_t noteAt = 0;  // 0x0050BE68 (kept, never read)
    u32 arrows = 0;          // 0x0050BE4C frames the scroll arrows stay pressed (bits 0..3 up, 4..7 down)
    i32 sampleLength = 0;    // 0x0050BE6C characters of the speed sample shown
    i32 textSpeedShown = 0;  // 0x0050BE70
    i32 sampleDelay = 0;     // 0x0050BE74
    i32 displayRow = 0;      // the rebuild's display setup (stage 8): the row chosen
};

// The items page (role page 0xB; 0x004FD088..0x004FD164): the inventory of one category with its counts, used, sorted
// or discarded.
struct InventoryPage
{
    static constexpr i32 Unk4FC3D0 = 0x60;
    static constexpr i32 Unk4FC3DC = 2;
    static constexpr u16 FirstRowNumber = 0x64;

    i32 openedAt = 0;         // 0x0050BBE0 the frame before the page opened
    i32 category = 0;         // 0x0050BBEC 0..6
    i32 unk4FD160 = 0;        // 0x0050BCC4 (only cleared)
    i32 target = 0;           // 0x0050BBF8 the slot of the visible role an object is used on
    i32 followers = 0;        // 0x0050BBE8 the followers that can be called
    i32 followerRow = 0;      // 0x0050BBFC
    i32 followerPresent = 0;  // 0x0050BCB8 the follower chosen is called
    i32 discardChoice = 0;    // 0x0050BCA8 0: yes, 1: no
    i32 unk4FD148 = 0;        // 0x0050BCAC the top of the role box below its first row, the follower box's choice
    i32 glow = 0;             // 0x0050BCB0 the mode tab drawn raised (0x23..0x26 after sorting)
    u16 arrows = 0;           // 0x0050BCBC frames the arrows stay pressed: the list's (bits 0..7), the towns'
    u16 unk4FD1F0 = 0;        // 0x0050BD58 (only cleared)
    i32 unk4FC3D0 = 0;        // 0x0050AD78 (only set)
    i32 unk4FC3D4 = 0;        // 0x0050AD7C
    i32 unk4FC3DC = 0;        // 0x0050AD84
    std::array<u16, 14> rowNumbers = {};  // 0x004BE59C the rows shown numbered from 100, ended by 0 (never read)
};

// An object of the ghost page's list (0x80 bytes, 64 of them at 0x0050BAA4, swd3e): an inventory object of the tab's
// category with up to five skills it teaches the ghost (its itemA..C and healItemA..B objects), ended by SkillsEnd.
struct GhostFood
{
    static constexpr std::size_t Skills = 5;
    static constexpr i32 SkillsEnd = -1;
    static constexpr i32 NoIcon = -1;

    i32 id = 0;        // [0] (0: an unused entry)
    i32 category = 0;  // [1] ObjectData::category
    i32 level = 0;     // [2] ObjectData::level, what feeding it is worth
    i32 count = 0;     // [3] the count and pending count
    std::string name;  // [4]
    // [5] the description buffer as the last object left it (never read)
    std::string note;                                             // [6] the record's description
    std::array<i32, Skills> skills = {};                          // [7..0xB]; the end mark of five lands in icons[0]
    std::array<i32, Skills> icons = {};                           // [0xC..0x10] by the skill script's unk10
    std::array<std::optional<std::string>, Skills> names;         // [0x11..0x15]
    std::array<std::optional<std::string>, Skills> descriptions;  // [0x16..0x1A]
    std::array<std::optional<std::string>, Skills> notes;         // [0x1B..0x1F]
};

// The ghost's skill slots as the ghost page shows them (0x80 bytes at 0x0050BAB8, swd3e): TalismanGhost::Skill 0..4
// with their names ("無" when unknown), descriptions and notes.
struct GhostSlots
{
    static constexpr std::size_t Slots = 5;

    std::array<i32, Slots> skills = {};           // +0x1C
    std::array<i32, Slots> icons = {};            // +0x30
    std::array<std::string, Slots> names;         // +0x44
    std::array<std::string, Slots> descriptions;  // +0x58
    std::array<std::string, Slots> notes;         // +0x6C
};

// The talisman ghost's feeding page (role page 0xF; 0x0050BA78..0x0050BADC, swd3e): ten tabs of object categories, the
// objects of the tab in the inventory, what each teaches, the ghost's three skill slots and its charts.
struct GhostPage
{
    static constexpr i32 Tabs = 10;
    static constexpr i32 Rows = 8;
    static constexpr std::size_t Foods = 0x40;

    // `stage`
    enum Stage : i32
    {
        Fed = -1,        // a food was eaten: the next select steps on to Choosing
        Choosing = 0,    // the list of the tab
        Options = 1,     // the skills of the object chosen and 直接餵食
        Slots = 2,       // the slot a skill goes into
        TooFull = 0xA,   // 太飽了 無法餵食
        Unsuited = 0xB,  // 屬性不合 無法餵食
    };

    std::vector<GhostFood> foods;     // 0x0050BAA4 (empty: not made)
    std::optional<GhostSlots> slots;  // 0x0050BAB8
    std::size_t current = 0;          // 0x0050BA9C the entry of the row chosen
    i32 count = 0;                    // 0x0050BAB0
    i32 top = 0;                      // 0x0050BABC
    i32 lastRow = 0;                  // 0x0050BA78 the last row shown (0..7)
    i32 row = 0;                      // 0x0050BAC0
    i32 tab = 0;                      // 0x0050BAC4
    i32 option = 0;                   // 0x0050BA94
    i32 slot = 0;                     // 0x0050BAA0
    i32 stage = Choosing;             // 0x0050BAD8
    // 0x0050BAAC the category whose changes the ghost panel shows (-1: none) and the changes (0x0050BA8C,
    // 0x0050BA88, 0x0050BA80, 0x0050BAA8: 力量, 精魄, 智慧, 敏捷)
    i32 compared = -1;
    i32 strengthChange = 0;
    i32 essenceChange = 0;
    i32 wisdomChange = 0;
    i32 agilityChange = 0;
    std::optional<std::string> description;  // 0x0050BA7C
    std::optional<std::string> note;         // 0x0050BA90
    u32 arrows = 0;                          // 0x0050BAC8 frames the scroll arrows stay pressed
    i32 tick = 0;                            // 0x0050BA98
    i32 dayMarker = 0;                       // 0x0050BADC
    i32 satietyMarker = 0;                   // 0x0050BA84
    std::vector<u16> chartPixels;            // 0x0050BACC 120 x 120
    std::vector<u8> chart;                   // 0x0050BAD4 the chart as an RLE picture (empty: none)
};

// The edges of a list's scroll bar as last drawn (DrawScrollBar), read by the clicks.
struct MenuScrollBar
{
    i32 top = 0;          // +0x00
    i32 thumbBottom = 0;  // +0x04
    i32 thumbTop = 0;     // +0x08
    i32 bottom = 0;       // +0x0C
};

// The menus (the state at 0x004FB000..0x004FDFFF): a page installs the handlers of the frame, the keys and the drawing,
// which the frame dispatches. Handler slots are kept separately: some pages leave a slot of the page before them.
class Menu
{
public:
    using Handler = void (Menu::*)();

    // The code of the system page (the title and the settings).
    static constexpr u16 SystemPage = 0xEA60;
    // The modes of the system page (Open on SystemPage).
    static constexpr int TitleMode = 0;
    static constexpr int GameModeA = 1;
    static constexpr int GameModeB = 2;
    // swd3e: the page chooses entry 1 (load) at once; no setter of the menu that opens it is known
    static constexpr int LoadMode = 3;

    // The role pages: `page` counts from FirstRolePage as the mode counts from FirstPageMode in PageModeStep steps.
    static constexpr u16 FirstRolePage = 0xB;
    static constexpr u16 EquipmentPage = 0xC;
    static constexpr u16 AltarRolePage = 0xE;
    // swd3e: the talisman ghost's feeding page (its background is drawn without sliding while flag 0x49 is set)
    static constexpr u16 GhostFeedPage = 0xF;
    static constexpr u16 BestiaryPage = 0x11;  // drawn without the title and the tabs
    static constexpr std::size_t Pages = 0x12;
    static constexpr int FirstPageMode = 0x1E;
    static constexpr int PageModeStep = 6;
    static constexpr int EquipmentMode = FirstPageMode + PageModeStep;
    // The codes the role pages are opened with.
    static constexpr u16 MainCode = 1;          // the gold is shown and the title glows in
    static constexpr u16 PageCode = 2;          // a role page open
    static constexpr u16 SubpageCodes = 0x1F4;  // codes from here slide the tabs out on the first page
    static constexpr u16 NoTab = 5;             // `roleTab`: no tab raised
    static constexpr u16 NoRole = 0xFFFF;       // MenuTab::role
    static constexpr u16 PlainTab = 1;          // MenuTab::state
    static constexpr u16 RaisedTab = 2;
    static constexpr i8 InitialTabGlow = 12;
    // `Game::menuMode`: where the save slots were opened from.
    static constexpr int NoSlotsMode = 0;
    static constexpr int KeySaveMode = 1;     // F12 or Ctrl+S on the map
    static constexpr int KeyLoadMode = 2;     // Ctrl+L on the map
    static constexpr int TitleLoadMode = 3;   // loading from the title or the launcher
    static constexpr int ScriptSaveMode = 4;  // talk command SaveMenu
    static constexpr int ItemSaveMode = 5;    // the items page (0x004485F0)
    static constexpr int Tabs = 4;
    // 0x004BE5D4: the tabs as the executable holds them (down the left side in swd3e).
    static constexpr std::array<MenuTab, Tabs> InitialTabs = {{
        {0, 0x17, 0x16, 0xE, 0x34, 0, 0, 0, 0x232B, 0, 0x232C, 0, 0x232A, 0},
        {0, 0x17, 0x82, 0xE, 0xB6, 0, 0, 0, 0x232B, 0, 0x232C, 0, 0x232A, 0},
        {0, 0x17, 0xE7, 0xE, 0x138, 0, 0, 0, 0x232B, 0, 0x232C, 0, 0x232A, 0},
        {0, 0x17, 0x150, 0xE, 0x1BA, 0, 0, 0, 0x232B, 0, 0x232C, 0, 0x232A, 0},
    }};

    static constexpr int NoVisibleRole = -1;  // VisibleRole past the roles listed

    explicit Menu(Game& game);
    ~Menu();
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;

    void InitActs();
    void Run();
    void Open(int menuMode, u16 menuCode);
    void BuildTabs(int selected);
    void SetPage(int menuCode);
    void Dispatch();
    void ReadMouse();
    void Present();
    void DrawBox(int x, int y, int width, int height, int mode, int red, int green, int blue);
    void DrawPageTitle();
    int ListPartyRoles();
    int VisibleRole(int slot) const;
    int VisibleCount() const;
    void DrawShadowText(int x, int y, Pixel color, Pixel shadow, std::string_view text, TextFont& font);
    void DrawRoleTabs(int offset, int mode);
    void ItemsAfterSave(int saved);
    void DrawScrollBar(int x, int y, int height, int arrows, float from, float to, MenuScrollBar& bar);

    // The rebuild's continuous motion of the menus and the shop (deviations.md): the values a frame steps that move a
    // picture (the slides, the growing boxes) or change a glow or a tint. A frame decides each step; the frames painted
    // between it and the next run its drawing again from the state it started from, each value shown that far along
    // its step.
    enum MovingValue : std::size_t
    {
        SlideValue = 0,                        // Menu::slide
        EquipSlideValue,                       // EquipPage::slide
        DescriptionTopValue,                   // Menu::descriptionTop
        SkillsDescriptionValue,                // SkillsPage::descriptionTop
        ItemsGrowValue,                        // the frames the items page's boxes have grown, in GrowSteps
        TitleGlowValue,                        // Menu::titleGlow
        TabGlowValue,                          // Menu::tabGlow, a value for each tab
        FaceSlideValue = TabGlowValue + Tabs,  // BattleState::faceSlides, a value for each role
        MovingValues = FaceSlideValue + Party::Roles,
    };

    static constexpr i32 GrowSteps = 0x100;  // ItemsGrowValue: the parts of a frame of growth

    // A frame steps value `which` from `from` to `to`, having drawn `drawn`: the frames painted until the next frame
    // show it moving there over the frame interval from where it is shown now.
    void Move(std::size_t which, i32 drawn, i32 from, i32 to);
    // Where value `which` is shown when the frame drew `value`: that value in a frame; between frames how far its step
    // has gone (the value drawn when it took no step).
    i32 Shown(std::size_t which, i32 value) const;
    // The same unrounded (a glow drawn between two translucency levels).
    double ShownFine(std::size_t which, i32 value) const;
    // The drawing of the menu frame (the main menu or the shop) starts: the state it reads and the screen under it are
    // kept; its end: whether the frames between it and the next can paint it again.
    void KeepFrame();
    void FrameDrawn();
    // The drawing of the frame changes what cannot be put back (`replayable` false): it is not painted again.
    void KeepFrameFrom(bool replayable);
    // The menu frame painted again on the 640 x 480 `target` with its values where they are now; false when the frame
    // drawn stays shown (no step under way).
    bool PaintBetween(Pixel* target);
    bool Replaying() const;
    void LogTotals() const;

    // The bestiary (role page 0x11).
    void BestiaryEnter();
    void BestiaryLeave();
    void BestiaryFrame();
    void BestiaryMoved(u16 arrows);
    void BestiaryShowFrom(int top);
    void BestiaryDown();
    void BestiaryUp();
    void BestiaryPageDown();
    void BestiaryPageUp();
    void BestiaryLeft();
    void BestiaryRight();
    void BestiaryShift();
    void BestiaryCancel();
    void BestiaryDraw();
    void BestiaryList(int category);
    void BestiaryCountShown();
    void DrawBestiaryRow(int index, int row, Pixel color, bool chosen);
    void BestiaryChoose(u32 id);
    std::string KnownValue(std::u8string_view label, int known, int needed, int value, int most);
    void BestiaryLines(int index);
    void DrawBestiaryTabs();

    // The altar page (role page 0x10).
    void AltarEnter();
    void AltarLeave();
    void AltarFillTab();
    void AltarReturnObjects();
    void AltarTakeObjects(ItemLink& source, int tab);
    void AltarSortObjects();
    void AltarPick();
    void AltarPut(ItemNode& slot, int index);
    bool AltarCombine(const ItemNode& first, const ItemNode& second);
    void AltarMoved(u16 arrows);
    void AltarSkipMorph();
    void AltarDown();
    void AltarUp();
    void AltarPageDown();
    void AltarPageUp();
    void AltarTabChanged();
    void AltarLeft();
    void AltarRight();
    void AltarShift();
    void AltarSelect();
    void AltarFinish();
    void AltarCancel();
    void AltarFrame();
    void AltarDraw();
    void AltarDrawList(Pixel color);
    void AltarDrawOffer(Pixel color);
    void AltarDrawResult(const ItemNode& result, int x, int y, u32 flags);
    void AltarPrepareMorph();
    void AltarMorph();
    void AltarFinishMorph();
    void AltarResultVitals();

    // The equipment page (role page 0xC).
    void EquipEnter();
    void EquipLeave();
    void EquipDescribeSlot();
    void EquipDescribeRow();
    ItemNode* EquipSlotNode();
    void EquipTake(ItemLink& source, int slot);
    void EquipCountShown();
    void EquipFillList();
    void EquipReturn();
    void EquipComputeStats();
    const ItemNode* EquipTried();
    void EquipRoleStats(int role, EquipStats& stats);
    void EquipTryStats(int role, int slot, EquipStats& stats, const ItemNode* tried);
    void EquipStatsOf(EquipStats& stats);
    void EquipExtraStats(const ItemNode* tried, EquipStats& stats);
    void EquipSwapVitals(const ItemNode* added, const ItemNode& removed);
    void EquipSumBonuses();
    void EquipResistances();
    int TabRole() const;
    void EquipListMoved(u16 arrows);
    void EquipSlotMoved();
    void EquipDown();
    void EquipUp();
    void EquipPageDown();
    void EquipPageUp();
    void EquipLeft();
    void EquipRight();
    void EquipShift();
    void EquipSelect();
    void EquipPutOn();
    void EquipCancel();
    void EquipFrame();
    void EquipDraw();
    void EquipDrawSlots(int x, int y, int slide, int width, int chosen);
    void EquipDrawPicture(u16 file, u16 picture, int x, int y);
    void EquipDrawStats(int slot, int role);
    void DrawDescriptionBox(std::string_view text);

    // The skills page (role page 0xD).
    void SkillsEnter();
    void SkillsLeave();
    void SkillsDescribe();
    void SkillsTake(ItemLink& source);
    void SkillsCountShown();
    void SkillsFillList();
    void SkillsReturn();
    void SkillsCountTabs();
    int SkillsCell(int cell) const;
    ItemLink& SkillsRoleList();
    RoleRecord& SkillsRole();
    void SkillsDown();
    void SkillsUp();
    void SkillsPageDown();
    void SkillsPageUp();
    void SkillsOtherColumn(u16 arrows);
    void SkillsLeft();
    void SkillsRight();
    void SkillsShift();
    void SkillsEnd();
    void SkillsSelect();
    void SkillsCancel();
    void SkillsFrame();
    void SkillsDraw();
    void SkillsDrawTowns(Pixel color);
    std::size_t FindExitWarp(int map) const;
    void ListTowns();
    void Warp(u16 map, u16 x, u16 y, i16 event);

    // The talisman ghost's feeding page (role page 0xF, swd3e).
    void GhostEnter();
    void GhostLeave();
    void GhostFrame();
    void GhostDown();
    void GhostUp();
    void GhostPageDown();
    void GhostPageUp();
    void GhostLeft();
    void GhostRight();
    void GhostShift();
    void GhostSelect();
    void GhostCancel();
    void GhostDraw();
    void GhostDrawGauges();
    void GhostDrawList();
    void GhostDrawOptions();
    void GhostDrawSlots();
    void GhostDrawDescription();
    void DrawGhostChart(int x, int y);
    void GhostMakeChartBuffer();
    void GhostFreeChart();
    void GhostMakeChart(int balance, int attack, int defense, u32 top, u32 left, u32 right);
    void GhostBuildList(int tab);
    void GhostFreeList();
    int GhostFeed(i32 id, i32 skill);
    void GhostRow();
    void GhostDescribe();
    void GhostMakeSlots();
    void GhostFreeSlots();
    void GhostMoveRow(int delta);
    void GhostMovePage(int delta);
    void GhostCountShown();
    void GhostSlotFor(i32 icon);
    void GhostMove(int kind, int delta);
    void GhostCompare();
    bool GhostEat(i32 level, i32 category);
    void GhostShiftTriangle(i32 balance, i32 attack, i32 defense);

    // The system tab (role page 0xF).
    // The key slot of each row of the key setup (0x004BED40).
    static constexpr std::array<u16, 16> KeyRowSlots = {4, 6, 3, 5, 0xC, 1, 9, 0, 0xA, 2, 0x13, 7, 8, 0x10, 0x11, 0x12};
    void OptionsEnter();
    void CountNotes();
    void FindNote(int index);
    void DrawNote(int index, int x, int y);
    void OptionsFrame();
    void OptionsDown();
    void OptionsUp();
    void OptionsPageDown();
    void OptionsPageUp();
    void OptionsLeft();
    void OptionsRight();
    void OptionsSelect();
    void OptionsCancel();
    void OptionsDrawSettings(Pixel color);
    void OptionsDrawKeys(Pixel color);
    void OptionsDrawDisplay(Pixel color);
    // The rebuild's settings rows of choices (deviations.md): an on/off row's choices (Big5), and the left of
    // each of `choices` drawn with `font` from `left`, kChoiceGap apart, with the right of the last.
    static std::vector<std::string> OnOffChoices();
    static std::vector<int> ChoiceLefts(const std::vector<std::string>& choices, const TextFont& font, int left);
    Config::Settings CurrentSettings() const;
    void StoreChangedSettings();
    void OptionsSetLauncher(bool shown);
    std::vector<std::string> OptionsDisplayChoices(i32 row, i32& chosen) const;
    void OptionsChooseDisplay(i32 row, i32 choice);
    void OptionsStepDisplay(i32 row, bool up, bool wrap);
    void OptionsDraw();

    // The items page (role page 0xB).
    void ItemsDescribe();
    void ItemsFit();
    void ItemsMoved(u16 arrows);
    void ItemsEnter();
    void ItemsLeave();
    void ItemsFrame();
    void ItemsDown();
    void ItemsUp();
    void ItemsPageDown();
    void ItemsPageUp();
    void ItemsCategory(int step);
    void ItemsLeft();
    void ItemsRight();
    void ItemsShift();
    void ItemsEnd();
    void ItemsFreeTowns();
    void ItemsUse(ItemNode& node);
    void ItemsSelect();
    void ItemsCancel();
    void ItemsDrawNotice(Pixel color);
    void ItemsDrawFollowers(Pixel color, std::string& line);
    void ItemsDrawTowns(Pixel color);
    void ItemsDraw();
    void ItemsCountShown();
    void ItemsTake(int category);
    void ItemsFill();
    void ItemsReturn();
    bool ItemsCallFollower();
    void ItemsSave();

    // The main page (the titles of the role pages) and the role page tables.
    void ShowPageBackground(int page);
    void FillPageTables();
    void MainFrame();
    void MainLeft();
    void MainRight();
    void MainSelect();
    void MainCancel();
    void MainDraw();
    void OptionsLeave();
    void DrawNameEntry();
    void ActOnEntry();
    void DrawSettings();
    // The rebuild's display setup on the title page (deviations.md).
    void DrawDisplaySettings();
    void DrawSystemChoices(i32 row, const std::vector<std::string>& choices, i32 chosen, bool counts);
    void DisplaySettingsMouse();
    void SettingsMoveSound();

    // The system page (the title in mode 0, the settings).
    void SystemEnter();
    void SystemFrame();
    void SystemDown();
    void SystemUp();
    void SystemLast();
    void SystemFirst();
    void SystemLeft();
    void SystemRight();
    void SystemShift();
    void SystemEnd();
    void SystemSelect();
    void SystemCancel();
    void SystemDraw();
    void DrawActAlpha(ActState& state, int x, int y, bool draw);

    void Nothing();

    // The handler slots.
    Handler beforeFrame = nullptr;               // 0x0050A118 (none: not called)
    Handler frame = &Menu::Nothing;              // 0x0050B018
    Handler down = &Menu::Nothing;               // 0x0050B2C4
    Handler up = &Menu::Nothing;                 // 0x0050AE9C
    Handler pageDown = &Menu::Nothing;           // 0x0050A8D4
    Handler pageUp = &Menu::Nothing;             // 0x0050A35C
    Handler left = &Menu::Nothing;               // 0x0050A414
    Handler right = &Menu::Nothing;              // 0x0050B21C
    Handler cancel = &Menu::Nothing;             // 0x0050B2D4
    Handler shift = &Menu::Nothing;              // 0x0050B2D8
    Handler end = &Menu::Nothing;                // 0x0050A8D8
    Handler select = &Menu::Nothing;             // 0x0050AD6C
    Handler draw = &Menu::Nothing;               // swd32 0x004FBEC8
    std::array<Handler, Pages> enterTable = {};  // 0x0050B35C the role pages opened (entries 0xB..0x11)
    std::array<Handler, Pages> leaveTable = {};  // swd32 0x004FB884 left
    std::array<Handler, Pages> drawTable = {};   // swd32 0x004FBF54 drawn

    u16 code = 0;        // 0x0050B3AC the code the menu was opened with
    u16 page = 0;        // 0x0050AD70 (low word) the page index
    u16 mode = 0;        // 0x0050A1C4
    u16 lastMode = 0;    // 0x0050AB94 the menu mode (the items page: its row) last set; no code reads it
    u16 openedMode = 0;  // 0x0050A40C the mode Open was given; no code reads it
    u16 roleTab = 0;     // 0x0050A820 (low word) the role tab raised (the equipment page's role)
    // 0x0050B2C8 bit 0: the menu opened on the equipment, left back to the item choice
    u16 openedOnEquipment = 0;
    u16 unk4A6AA8 = 0;  // 0x004BE644 the word after the tabs (BuildTabs sets it to NoRole)
    i32 tint = 0;       // 0x0050ACBC a screen tint while non-zero
    std::array<int, Party::Roles> visibleRoles = {};  // 0x0050A344 the roles in the party in order (ListPartyRoles)
    int visibleCount = 0;                             // 0x0050AB90
    // 0x0050B99C the five element resistances of each role with its equipment (0x00447E40)
    std::array<std::array<i8, 5>, Party::Roles> roleResistances = {};  // 0x0050B99C (5 bytes a role)
    i32 frames = 0;                                                    // 0x0050B218
    u32 flags = 0;                                                     // 0x0050A82C
    i32 pointerX = 0;                                                  // 0x0050A418 the pointer of the last frame
    i32 pointerY = 0;                                                  // 0x0050A824
    i32 escapeDelay = 0;           // 0x0050B3B4 frames before RightControl acts as Escape again
    u32 clicks = 0;                // 0x0050A1C8 bits 0..1: a left click (single, double), bits 2..3: a right click
    i32 slide = 0;                 // swd32 0x004FC3C8
    i32 lastPage = FirstRolePage;  // 0x004BE64C (initial 0xB)
    i32 titleGlow = 0;             // 0x0050B214 counted by DrawPageTitle, no code reads it for drawing
    // swd32 0x004A6AAC
    std::array<i8, Tabs> tabGlow = {InitialTabGlow, InitialTabGlow, InitialTabGlow, InitialTabGlow};
    // 0x004BE5D4
    std::array<MenuTab, Tabs> tabs = InitialTabs;

    ActState background;               // 0x0050A2A0
    ActState hitPointBar;              // 0x0050A120 the bars of the role tabs
    ActState barsTrack;                // 0x0050B020 the track under each of them (action 0x51)
    ActState downArrowAct;             // 0x0050A1F8 the down arrow of the lists (the shop's minus button)
    ActState upArrowAct;               // 0x0050A360 the up arrow of the lists (the shop's plus button)
    std::array<ActState, Tabs> items;  // 0x0050A900 the four entries of a page
    ActState titleAct;                 // 0x0050A830 the title of the page, lit and glowing
    ActState face;                     // 0x0050AB98 the face of the name entry
    ActState barTrackAct;              // 0x0050ACC8 the track of the scroll bars, repeated down
    ActState spiritBar;                // 0x0050ADF8
    ActState staminaBar;               // 0x0050AEC8
    ActState gauge;                    // 0x0050B0C8 a step of the settings bars
    ActState barThumbAct;              // 0x0050B170 their thumb
    ActState titleArrowAct;            // 0x0050B220 the arrow beside the title
    ActState tabsPanelAct;             // 0x0050B2E0 the panel of the tabs (the shop's frame)

    // The system page (0x0050BE08..0x0050BE40).
    i32 systemClick = 0;           // 0x0050BE08 1: the confirm button clicked, 2: a right click
    i32 nameReply = 0;             // 0x0050BE0C frames of the face's reply to a name
    i32 textSpeedShown = 0;        // 0x0050BE10
    i32 systemState = 0;           // 0x0050BE18 1: the title entries, 2: chosen (5: the settings, 100: in game)
    std::vector<u16> savedScreen;  // 0x0050BE1C the frame under the page in game
    i32 systemCursor = 0;          // 0x0050BE20 the title entry
    // 0x0050BE24 by entry: -0x10 on entering; swd3e's page clears the first and sets the chosen one to 1 each frame
    // (no reader is known)
    std::array<i32, 4> glow = {};
    i32 systemRow = 0;  // 0x0050BE34 the settings row, the fade and name entry steps otherwise
    std::optional<Config::Settings> storedSettings;  // StoreChangedSettings: what the settings page last wrote
    i32 sampleLength = 0;                            // 0x0050BE38 characters of the text speed sample shown
    std::vector<u8> enteredName;                     // 0x0050BE3C the name typed (a RoleName's 0x20 bytes)
    i32 sampleDelay = 0;                             // 0x0050BE40

    // The items page.
    MenuList itemList;        // 0x0050BBE4 (see MenuList)
    MenuScrollBar scrollBar;  // 0x0050AEA0
    GhostPage ghost;          // swd3e: the talisman ghost's feeding page
    Bestiary bestiary;
    AltarPage altar;
    EquipPage equip;
    SkillsPage skills;
    std::vector<MenuTown> towns;  // 0x0050A1EC (its count at 0x004FC834)
    i32 townTop = 0;              // swd32 0x004FBED4
    i32 townRow = 0;              // swd32 0x004FC510
    i32 townShown = 0;            // 0x0050A1F0
    MenuScrollBar townBar;        // 0x0050AEB0
    MenuScrollBar unk4FBAB4;      // 0x0050A408 a second bar of the equipment list (only drawn)
    i32 descriptionSlide = 0;     // 0x0050B014 the description box slides (up while positive)
    i32 descriptionTop = 0x1E0;   // 0x0050B2CC its top
    i32 buttonPress = 0;          // 0x0050B3B0 -1 while the description button is pressed
    ActState buttonAct;           // 0x0050BC00 the description button (the skills page's button)
    ItemLink itemObjects;         // 0x0050BD50 the objects of the items page
    ItemLink savedInventory;      // 0x0050BCC0 the inventory kept while the items page saves
    ItemLink savedItems;          // 0x0050BCA4 `itemObjects` kept with it
    std::string description;      // 0x0050AC3C the description of the chosen object
    OptionsPage options;
    InventoryPage itemsPage;

private:
    // A step of a moving value: from `from` to `to` over `length` milliseconds from `start` (the time of game frame
    // `frame`), the frame having drawn `drawn`.
    struct ValueMotion
    {
        double from = 0.0;
        i32 to = 0;
        i32 drawn = 0;
        u32 start = 0;
        i32 length = 1;
        u32 frame = 0;
    };

    struct FrameState;

    void Call(Handler handler);
    // The menu frame drawn from Present: the page over its background, the tabs, the gold, the pointer, the tint and
    // the notices; false when the page closed the menu.
    bool PaintPage(bool replay);
    void Replay(Pixel* target, u32 now, bool exact, i32 pointerX, i32 pointerY);
    double ValueAt(const ValueMotion& motion, u32 now) const;
    bool Moving(u32 now) const;
    void SaveFrame(FrameState& kept) const;
    void LoadFrame(const FrameState& kept);

    std::array<ValueMotion, MovingValues> motions = {};
    std::unique_ptr<FrameState> frameBefore;  // the state the frame's drawing started from
    std::unique_ptr<FrameState> frameAfter;   // the state it left (put back after a painting between)
    std::vector<Pixel> frameBase;             // the screen under the frame's drawing
    std::vector<Pixel> frameShown;            // the frame drawn
    bool frameKept = false;
    bool frameReplayable = false;
    u32 frameKeptAt = 0;  // Game::gameFrames of the frame kept
    bool frameChecked = false;
    bool replaying = false;
    bool replayExact = false;  // the values shown as the frame drew them
    // The frames kept to be painted again, those whose drawing run again did not give them, the paintings between.
    u32 framesKept = 0;
    u32 framesMismatched = 0;
    u32 framesPainted = 0;
    u32 replayNow = 0;

    Game& game;
};

}  // namespace swd3e
