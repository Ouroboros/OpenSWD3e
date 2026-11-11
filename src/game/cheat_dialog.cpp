#include "game/cheat_dialog.h"

#include <algorithm>
#include <cstring>

#include "core/format.h"
#include "game/game.h"
#include "menu/menu_list.h"
#include "org/fame.h"
#include "party/party.h"
#include "talk/talk.h"
#include "text/big5.h"

namespace swd3e
{

namespace
{

constexpr u32 kPendingKindBit = 0x8000;    // cleared from the kind bits compared with a slot's kind
constexpr std::size_t kWeaponSlot = 0;     // Party::SlotKinds: the kinds with a percent counter
constexpr std::size_t kMagicSlot = 7;      // with a proficiency
constexpr std::size_t kCountedSlot = 9;    // with a capped counter
constexpr u16 kLastCountedObject = 0x1F4;  // objects 1..0x1F4 also have a capped counter
constexpr i32 kNoValue = -1;
// The affection (as the menu's role page reads it): variable 1 (the low byte) of role id place + 1.
constexpr int kFirstRoleId = 1;
constexpr int kAffectionVariable = 1;

constexpr i16 kFullStock = 0x5A;  // the top up and the counts set fill to 90
constexpr i16 kDeleteAll = -0x400;
constexpr u16 kFirstSkill = 0x1771;  // the skill tabs take ids 0x1771..0x1963 (swd3e)
constexpr u16 kSkillsEnd = 0x1964;
constexpr int kLastVariable = 0x3F;
// An id field that is not a number leaves the id in the slot of the message argument (0x00435AE0 writes nothing):
// WM_COMMAND (0x111). The count keeps stack garbage and the value the button's window (lParam); 0 here
// (deviations.md).
constexpr int kUnparsedId = 0x111;
constexpr std::u8string_view kItemsTab = u8"物品";          // 0x004B1238
constexpr std::u8string_view kAttributesSuffix = u8"屬性";  // the end of "%s屬性" (0x004B1228)
constexpr std::u8string_view kSkillsSuffix = u8"法術";      // the end of "%s法術" (0x004B1230)
constexpr std::u8string_view kVariablesTab = u8"變數";      // 0x004B1220
constexpr std::u8string_view kMoneyLabel = u8"錢";          // the first variable (0x004B12BC)
constexpr std::u8string_view kGhostLabel = u8"符鬼";        // swd3e: variable 2 (0x004B12B4)
constexpr std::string_view kCompanyA = "Cmpy1";             // variables 11 and 12 (0x004B12AC, 0x004B12A4)
constexpr std::string_view kCompanyB = "Cmpy2";
constexpr std::size_t kCompanyAVariable = 11;
constexpr std::size_t kCompanyBVariable = 12;
constexpr int kObjectColumns = 4;
constexpr int kValueColumns = 3;

// Whether `kind` holds all of `slot`'s kind bits (bit 15 aside).
bool HasKind(u32 kind, u32 slot)
{
    return ((kind & slot) & ~kPendingKindBit) == slot;
}

int ParsedNumber(const std::string& text, int fallback)
{
    int value = fallback;
    Talk::ParseNumber(text, value);
    return value;
}

std::string NodeName(const ItemNode& node)
{
    const std::array<char, 0x20>& name = node.record.data.name;
    return std::string(name.data(), strnlen(name.data(), name.size()));
}

}  // namespace

CheatDialog::CheatDialog(Game& game)
    : game(game),
      errors(game.errors),
      party(game.party),
      database(game.database),
      running(game.running),
      resources(game.resources)
{
}

// The rebuild (deviations.md): the affection of the role at place `role`.
i32 CheatDialog::Affection(int role) const
{
    i32 affection = 0;
    this->game.talk
        .RoleVariable(static_cast<u32>(role + kFirstRoleId), kAffectionVariable, affection, Talk::ReadVariable);
    return affection;
}

// The rebuild (deviations.md): the affection of the role at place `role` set (its low byte kept).
void CheatDialog::SetAffection(int role, i32 value)
{
    this->game.talk.RoleVariable(static_cast<u32>(role + kFirstRoleId), kAffectionVariable, value, Talk::WriteVariable);
}

std::string CheatDialog::TabLabel(int tab) const
{
    if (tab == CheatDialog::ItemsTab)
    {
        return Big5(kItemsTab);
    }

    if (tab == CheatDialog::VariablesTab)
    {
        return Big5(kVariablesTab);
    }

    const bool skills = tab < CheatDialog::FirstAttributesTab;
    const int role = tab - (skills ? CheatDialog::FirstSkillsTab : CheatDialog::FirstAttributesTab);
    return this->party.names[static_cast<std::size_t>(role)].FullName() +
           Big5(skills ? kSkillsSuffix : kAttributesSuffix);
}

int CheatDialog::ColumnCount() const
{
    return this->tab >= CheatDialog::FirstAttributesTab ? kValueColumns : kObjectColumns;
}

bool CheatDialog::EntriesEnabled() const
{
    return this->tab < CheatDialog::FirstAttributesTab;
}

std::vector<CheatDialog::Row> CheatDialog::Rows() const
{
    std::vector<Row> rows;

    if (this->tab == CheatDialog::VariablesTab)
    {
        for (std::size_t variable = 0; variable < this->party.variables.size(); ++variable)
        {
            std::string label;

            if (variable == 0)
            {
                label = Big5(kMoneyLabel);
            }
            else if (variable == Party::GhostVariable)
            {
                label = Big5(kGhostLabel);
            }
            else if (variable == kCompanyAVariable)
            {
                label = std::string(kCompanyA);
            }
            else if (variable == kCompanyBVariable)
            {
                label = std::string(kCompanyB);
            }

            const auto index = static_cast<i32>(variable);
            rows.push_back(Row{label, this->party.variables[variable], index, kNoValue, 0});
        }

        return rows;
    }

    if (this->tab >= CheatDialog::FirstAttributesTab)
    {
        const int role = this->tab - CheatDialog::FirstAttributesTab;

        for (std::size_t field = 0; field < CheatDialog::AttributeNames.size(); ++field)
        {
            const auto index = static_cast<i32>(field);
            rows.push_back(
                Row{Big5(CheatDialog::AttributeNames[field]), this->party.RoleStat(index, role), index, 0, 0}
            );
        }

        // The rebuild (deviations.md): the affection row.
        rows.push_back(Row{Big5(CheatDialog::AffectionLabel), this->Affection(role), CheatDialog::AffectionRow, 0, 0});
        return rows;
    }

    ItemLink* link = this->TabList();

    for (const ItemNode* node = link != nullptr ? link->get() : nullptr; node != nullptr; node = node->next.get())
    {
        i32 value = kNoValue;
        i32 maximum = kNoValue;

        if (this->tab == CheatDialog::ItemsTab)
        {
            this->ObjectCounter(*node, value, maximum);
        }

        rows.push_back(Row{NodeName(*node), node->count + node->pending, node->id, value, maximum});
    }

    return rows;
}

CheatDialog::EditResult CheatDialog::TopUp(int row, Row& shown)
{
    if (this->tab >= CheatDialog::FirstAttributesTab || row < 0 || this->tab != CheatDialog::ItemsTab)
    {
        return EditResult::Ignored;
    }

    ItemNode* node = nullptr;
    SkipItems(row, this->party.inventory, node);

    if (node == nullptr)
    {
        return EditResult::Ignored;
    }

    if (node->pending + node->count < kFullStock)
    {
        node->pending = kFullStock;

        if (node->pending + node->count > kFullStock)
        {
            node->count = static_cast<i16>(kFullStock - node->pending);
        }
    }

    shown = Row{NodeName(*node), node->count + node->pending, node->id, 0, 0};
    return EditResult::Applied;
}

CheatDialog::EditResult CheatDialog::Add(const Fields& fields)
{
    if (this->tab >= CheatDialog::FirstAttributesTab || fields.id.empty())
    {
        return EditResult::Ignored;
    }

    const int id = ParsedNumber(fields.id, kUnparsedId);
    ItemLink* link = this->TabList();
    const bool skill = this->tab != CheatDialog::ItemsTab;

    if (link == nullptr || (skill && (id < kFirstSkill || id >= kSkillsEnd)))
    {
        return EditResult::Ignored;
    }

    const int count = ParsedNumber(fields.count.empty() || skill ? std::string("1") : fields.count, 0);
    const int value = ParsedNumber(fields.value.empty() || skill ? std::string("0") : fields.value, 0);
    const ItemNode* node =
        AddItem(*link, static_cast<u16>(id), static_cast<i16>(count), ItemCountMode::Pending, this->database);

    if (node == nullptr)
    {
        return EditResult::InsertFailed;
    }

    if (!skill)
    {
        this->SetObjectCounter(*node, static_cast<u16>(id), value);
    }

    return EditResult::Applied;
}

CheatDialog::EditResult CheatDialog::Delete(int row)
{
    if (this->tab >= CheatDialog::FirstAttributesTab)
    {
        return EditResult::Ignored;
    }

    ItemLink* link = this->TabList();

    if (row < 0 || link == nullptr)
    {
        return EditResult::Ignored;
    }

    ItemNode* node = nullptr;
    SkipItems(row, *link, node);

    if (node != nullptr && AddItem(*link, node->id, kDeleteAll, ItemCountMode::Pending, this->database) != nullptr)
    {
        return EditResult::DeleteFailed;
    }

    return EditResult::Applied;
}

CheatDialog::EditResult CheatDialog::Set(const Fields& fields)
{
    if (fields.id.empty())
    {
        return EditResult::Ignored;
    }

    const int id = ParsedNumber(fields.id, kUnparsedId);
    // The original keeps stack garbage for a count or value field left empty; 0 here.
    int count = 0;
    int value = 0;

    if (this->tab == CheatDialog::VariablesTab)
    {
        if (id < 0 || id > kLastVariable)
        {
            return EditResult::Ignored;
        }

        if (!fields.count.empty())
        {
            count = ParsedNumber(fields.count, 0);
        }

        this->party.variables[static_cast<std::size_t>(id)] = count;
    }
    else if (this->tab < CheatDialog::FirstAttributesTab)
    {
        ItemNode* node = FindItemOfKind(this->party.inventory, static_cast<i16>(id));

        if (node == nullptr)
        {
            return EditResult::Ignored;
        }

        if (!fields.count.empty())
        {
            count = ParsedNumber(fields.count, 0);
            node->count = 0;
            node->pending = static_cast<i16>(std::min<int>(count, kFullStock));
        }

        if (!fields.value.empty())
        {
            value = ParsedNumber(fields.value, 0);
        }

        if (this->tab == CheatDialog::ItemsTab)
        {
            this->SetObjectCounter(*node, static_cast<u16>(id), value);
        }
    }
    else
    {
        if (id > CheatDialog::AffectionRow)
        {
            return EditResult::Ignored;
        }

        if (!fields.count.empty())
        {
            count = ParsedNumber(fields.count, 0);
        }

        const int role = this->tab - CheatDialog::FirstAttributesTab;

        // The rebuild (deviations.md): the affection row sets the role variable (its low byte).
        if (id == CheatDialog::AffectionRow)
        {
            this->SetAffection(role, count);
        }
        else
        {
            this->party.SetRoleStat(id, count, role, this->database);
        }
    }

    return EditResult::Applied;
}

ItemLink* CheatDialog::TabList() const
{
    if (this->tab == CheatDialog::ItemsTab)
    {
        return &this->party.inventory;
    }

    if (this->tab >= CheatDialog::FirstSkillsTab && this->tab < CheatDialog::FirstAttributesTab)
    {
        ItemLink& head = this->party.skills[static_cast<std::size_t>(this->tab - CheatDialog::FirstSkillsTab)];
        return head != nullptr ? &head->next : nullptr;
    }

    return nullptr;
}

SDL_Window* CheatDialog::Window() const
{
    return this->game.display.Window();
}

std::string CheatDialog::ValueText(i32 value, i32 maximum, const std::string& before)
{
    if (value == kNoValue)
    {
        return {};
    }

    if (maximum > 1)
    {
        return WinFormat("%d/%d", {value, maximum});
    }

    if (maximum == 1)
    {
        return before;
    }

    return WinFormat("%d", {value});
}

void CheatDialog::ObjectCounter(const ItemNode& node, i32& value, i32& maximum) const
{
    std::array<CounterList, Fame::Lists>& lists = this->party.fame.lists;
    const u32 kind = node.record.data.kind;
    value = kNoValue;
    maximum = kNoValue;

    if (HasKind(kind, Party::SlotKinds[kWeaponSlot]))
    {
        value = static_cast<u16>(FindCounterPercent(lists[Fame::MasteryList], static_cast<i16>(node.id)));
        maximum = 0;
    }

    if (HasKind(kind, Party::SlotKinds[kMagicSlot]))
    {
        u16 most = 0;
        u16 got = 0;
        GetProficiency(lists[Fame::ProficiencyList], node.id, most, got, this->database);
        value = got;
        maximum = most;
    }

    if (HasKind(kind, Party::SlotKinds[kCountedSlot]))
    {
        value = static_cast<u16>(FindCounterValue(lists[Fame::FameList], static_cast<i16>(node.id)));
        maximum = kNoValue;
    }

    if (node.id != 0 && node.id <= kLastCountedObject)
    {
        value = static_cast<u16>(FindCounterValue(lists[Fame::FameList], static_cast<i16>(node.id)));
        maximum = kNoValue;
    }
}

void CheatDialog::SetObjectCounter(const ItemNode& node, u16 id, i32 value)
{
    std::array<CounterList, Fame::Lists>& lists = this->party.fame.lists;
    const u32 kind = node.record.data.kind;

    if (HasKind(kind, Party::SlotKinds[kWeaponSlot]))
    {
        SetCounterProgress(
            lists[Fame::MasteryList],
            static_cast<i16>(id),
            node.record.data.counterMax,
            static_cast<u16>(value)
        );
    }

    if (HasKind(kind, Party::SlotKinds[kMagicSlot]))
    {
        SetProficiency(lists[Fame::ProficiencyList], id, static_cast<u16>(value), this->database);
    }

    if (HasKind(kind, Party::SlotKinds[kCountedSlot]))
    {
        SetCounterCapped(lists[Fame::FameList], static_cast<i16>(id), static_cast<u16>(value));
    }

    if (node.id != 0 && node.id <= kLastCountedObject)
    {
        SetCounterCapped(lists[Fame::FameList], static_cast<i16>(id), static_cast<u16>(value));
    }
}

}  // namespace swd3e
