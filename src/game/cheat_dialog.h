#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "item/item_list.h"

struct SDL_Window;

namespace swd3e
{

class ErrorReporter;
class Database;
class ExeResources;
class Game;
class Party;

// The cheat dialog of the debug keys (F4; dialog 0x6A of swd3e.exe): ten tabs (the items, the skills of the four
// roles, the attributes of the four roles, the variables) with a list and three fields to add, top up, delete or set
// entries. On Windows it is the original's dialog from swd3e.exe's resources (asset/launcher/<id>.dlg); elsewhere
// it cannot be shown. Also the name dialog (0x70) the launcher's start command would open. The lists and the
// buttons' work are here, shared with the rebuild's screen of the game (CheatScreen, Ctrl+F4).
class CheatDialog
{
public:
    // A row of the list: the name (Big5), the count, the id and what the value column shows (ValueText).
    struct Row
    {
        std::string name;
        i32 count = 0;
        i32 id = 0;
        i32 value = 0;
        i32 maximum = 0;
    };

    // The texts of the three fields (Big5; empty when the field is).
    struct Fields
    {
        std::string id;
        std::string count;
        std::string value;
    };

    // What a button did: nothing (the dialog leaves the list as it is), the entries changed, or they changed after an
    // insertion or a deletion that failed (the dialog shows its error).
    enum class EditResult
    {
        Ignored,
        Applied,
        InsertFailed,
        DeleteFailed,
    };

    // The tabs (CheatDialog::tab).
    static constexpr int ItemsTab = 0;
    static constexpr int FirstSkillsTab = 1;
    static constexpr int FirstAttributesTab = 5;
    static constexpr int VariablesTab = 9;
    static constexpr int Tabs = 10;
    // The labels of the attributes (0x004B0C60, eight bytes each).
    static constexpr std::array<std::u8string_view, 17> AttributeNames = {
        u8"生命",
        u8"魔法",
        u8"體力",
        u8"大生命",
        u8"大魔法",
        u8"大體力",
        u8"力量",
        u8"耐力",
        u8"智慧",
        u8"速度",
        u8"默契",
        u8"反應力",
        u8"運氣",
        u8"閃躲",
        u8"經驗",
        u8"總經驗",
        u8"等級",
    };

    static constexpr int LastAttribute = 0x10;
    // The rebuild (deviations.md): a row of the role's affection after the attributes.
    static constexpr int AffectionRow = LastAttribute + 1;
    static constexpr std::u8string_view AffectionLabel = u8"好感";
    // The columns of the list: the name, the count, the id, the value (three on the attributes and variables).
    static constexpr std::array<std::u8string_view, 4> ColumnLabels =
        {u8"物品", u8"數量", u8"號碼", u8"附加值"};  // 0x004B1238, 0x004B128C, 0x004B1284, 0x004B127C
    // The characters the fields take (EM_LIMITTEXT): the id, the count, the value.
    static constexpr int IdLimit = 4;
    static constexpr int CountLimit = 8;
    static constexpr int ValueLimit = 4;
    static constexpr std::u8string_view InsertFailedText = u8"物品插入發生錯誤";  // 0x004B1240
    static constexpr std::u8string_view DeleteFailedText = u8"物品刪除發生錯誤";  // 0x004B1268

    explicit CheatDialog(Game& game);

    // DialogBoxParamA(0x6A): the dialog until it is closed; its result (0 when it could not be shown).
    int Run();
    // DialogBoxParamA(0x70): the name of the first role asked for; 1 when one was given.

    // The value column of a row (part of 0x004107E0 and 0x00410950): "" without a value, the value alone without a
    // maximum (its percent sign written and then overwritten), "value/maximum" over a maximum of 1, the text before it
    // (the id) for a maximum of 1.
    static std::string ValueText(i32 value, i32 maximum, const std::string& before);
    // The counter shown for inventory object `node` (part of 0x00410A80): its percent, its proficiency and maximum
    // or its counter by its kind; -1 for none.
    void ObjectCounter(const ItemNode& node, i32& value, i32& maximum) const;
    // The counters of an inventory object set from the value field (part of 0x0040FBE0).
    void SetObjectCounter(const ItemNode& node, u16 id, i32 value);
    // The rebuild (deviations.md): the affection of a party role (the row after the attributes), read and set.
    i32 Affection(int role) const;
    void SetAffection(int role, i32 value);
    // The list of the tab (the inventory or a role's skills after its "無" head); nullptr for the other tabs.
    ItemLink* TabList() const;
    // The label of tab `tab` (Big5).
    std::string TabLabel(int tab) const;
    // The columns of the tab's list (part of 0x00410710).
    int ColumnCount() const;
    // Whether the tab takes the value field, top up, delete and add (the lists of objects).
    bool EntriesEnabled() const;
    // The rows of the tab's list (part of 0x00410A80).
    std::vector<Row> Rows() const;
    // The buttons (part of 0x0040FBE0): row `row` topped up to 90 (`shown` the row as the dialog writes it again),
    // the id of the fields added, row `row` deleted, the fields set.
    EditResult TopUp(int row, Row& shown);
    EditResult Add(const Fields& fields);
    EditResult Delete(int row);
    EditResult Set(const Fields& fields);
    SDL_Window* Window() const;

    Game& game;
    ErrorReporter& errors;  // the game's error boxes
    Party& party;
    Database& database;
    i32& running;
    const ExeResources& resources;
    int tab = ItemsTab;  // 0x004E6D24
};

}  // namespace swd3e
