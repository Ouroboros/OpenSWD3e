#include "battle/battle_state.h"

#include "core/words.h"

#include <algorithm>

#include "crt/msvc.h"
#include "game/scene.h"

namespace swd3e
{

namespace
{

constexpr int kColourCommand = 4;       // MenuKind: the menu command whose word is unk50D86C
constexpr int kFirstListedCommand = 6;  // the menu command of the first listed one
constexpr int kSecondKind = 2;
constexpr u16 kAllOnes16 = 0xFFFF;  // a free record has every byte 0xFF

}  // namespace

// The battle state: every field at its opening value (the original's globals).
BattleState::BattleState() = default;

BattleState::~BattleState() = default;

// swd3e.exe 0x00465660: the units from the top down: each pass takes the first unit with a y not yet taken, then the
// lowest y among the enemies after it and the roles (a role at the same y as an enemy comes after it); a unit with y 0
// is never the first pick. With none left the first pick is unit 30, whose y is the dword after unitY (darkFrames).
void BattleState::SortDrawOrder()
{
    const u32 units = this->enemyCount + this->roleCount;

    for (u32 pass = 0; pass < units; ++pass)
    {
        u32 first = 0;

        while (first < static_cast<u32>(BattleState::Units) && (this->unitY[first] == 0 || this->drawTaken[first] == 1))
        {
            ++first;
        }

        i32 lowest = first < static_cast<u32>(BattleState::Units) ? this->unitY[first] : this->darkFrames;
        u32 chosen = first;

        for (u32 enemy = first + 1; enemy < this->enemyCount; ++enemy)
        {
            if (this->drawTaken[enemy] != 1 && this->unitY[enemy] < lowest)
            {
                lowest = this->unitY[enemy];
                chosen = enemy;
            }
        }

        for (u32 unit = BattleState::FirstRoleUnit; unit < this->roleCount + BattleState::FirstRoleUnit; ++unit)
        {
            if (this->drawTaken[unit] != 1 && this->unitY[unit] < lowest)
            {
                lowest = this->unitY[unit];
                chosen = unit;
            }
        }

        if (chosen < static_cast<u32>(BattleState::Units))
        {
            this->drawTaken[chosen] = 1;
        }

        this->drawOrder[pass] = static_cast<i32>(chosen);
    }

    this->drawTaken.fill(0);
}

// swd3e.exe 0x00465A70: the enemies in drawing order
void BattleState::ListEnemyOrder()
{
    u32 listed = 0;

    for (const i32 unit : this->drawOrder)
    {
        if (unit < BattleState::FirstRoleUnit)
        {
            // Past the 20 entries the original writes the table after it (deviations.md): not kept.
            if (listed < this->enemyOrder.size())
            {
                this->enemyOrder[listed] = unit;
            }

            ++listed;

            if (listed == this->enemyCount)
            {
                break;
            }
        }
    }
}

// swd3e.exe 0x0046AA00: `unit` with kind 1 or 2 put in the first free record (none when all are used); kind 2 also
// takes the enemy's order (enemyPendingOrders, then cleared) into it; other kinds add nothing
void BattleState::AddSlotRecord(int kind, i32 unit)
{
    if (kind != 1 && kind != kSecondKind)
    {
        return;
    }

    for (BattleSlotRecord& record : this->slotRecords)
    {
        if (record.unit != -1)
        {
            continue;
        }

        record.unit = unit;
        record.kind = static_cast<u16>(kind);

        if (kind == kSecondKind)
        {
            PendingOrder& order = this->enemyPendingOrders[static_cast<std::size_t>(unit)];
            record.unk14 = order.unk04;
            record.unk10 = order.unk00;
            record.unk18 = order.unk08;
            record.unk0A = order.unk0C;
            record.targetCount = order.targetCount;
            order = PendingOrder{};
        }

        return;
    }
}

// swd3e.exe 0x0046AC40: the record of `unit` removed, the later ones moving up and the last made free with every byte
// 0xFF (the original first copies the bytes after the records into the last one)
void BattleState::RemoveSlotRecord(i32 unit)
{
    auto& records = this->slotRecords;
    auto found = std::find_if(
        records.begin(),
        records.end(),
        [unit](const BattleSlotRecord& record)
        {
            return record.unit == unit;
        }
    );

    if (found == records.end())
    {
        return;
    }

    std::move(found + 1, records.end(), found);
    records.back() = BattleSlotRecord{-1, -1, kAllOnes16, kAllOnes16, -1, -1, -1, -1};
}

// The uses of object `id` in this battle (itemUses, read inline by ListObject and SelectObject); an id past the table
// reads the bytes after it in the original, 0 here (deviations.md).
u8 BattleState::ItemUses(u16 id) const
{
    return id < this->itemUses.size() ? this->itemUses[id] : u8{0};
}

// One more use of object `id` (MenuConfirm, inline in states 4 and 5); an id past the table is not counted
// (deviations.md).
void BattleState::CountItemUse(u16 id)
{
    if (id < this->itemUses.size())
    {
        ++this->itemUses[id];
    }
}

// swd3e.exe 0x00469F80: 1 + the enemy hit most (the first of them)
u8 BattleState::MostHitEnemy() const
{
    u8 best = this->enemyHits[0];
    u8 index = 0;

    for (std::size_t at = 0; at < this->enemyHits.size(); ++at)
    {
        if (this->enemyHits[at] > best)
        {
            index = static_cast<u8>(at);
            best = this->enemyHits[at];
        }
    }

    return static_cast<u8>(index + 1);
}

// swd3e.exe 0x00467C40: the field fades from (red, green, blue) to (toRed, toGreen, toBlue) over `frames` frames; the
// steps are taken from the levels truncated to integers
void BattleState::StartFieldFade(int red, int green, int blue, int toRed, int toGreen, int toBlue, int frames)
{
    ColorFade& fade = this->fieldFade;
    fade.frames = frames;
    fade.red = static_cast<float>(red);
    fade.green = static_cast<float>(green);
    fade.blue = static_cast<float>(blue);
    fade.startRed = static_cast<float>(toRed);
    fade.startGreen = static_cast<float>(toGreen);
    fade.startBlue = static_cast<float>(toBlue);
    const auto count = static_cast<double>(frames);
    fade.stepRed = static_cast<float>(static_cast<double>(toRed - static_cast<i32>(crt::Ftol(fade.red))) / count);
    fade.stepGreen = static_cast<float>(static_cast<double>(toGreen - static_cast<i32>(crt::Ftol(fade.green))) / count);
    fade.stepBlue = static_cast<float>(static_cast<double>(toBlue - static_cast<i32>(crt::Ftol(fade.blue))) / count);
}

// swd3e.exe 0x0046AAE0: unless already in a record, `unit` put at place `at` among the records (the records from there
// to the first free one move down a place). Kind 1 (a party unit) at -1 goes to the first free record, and takes its
// pending order (cleared, as are unk53BF80 and unk53BFD0). The original counts from 0 when no record is free, puts a
// kind 1 unit placed past the first free record at place 0, and moves the last record past the array when only it was
// free.
void BattleState::InsertSlotRecord(int kind, i32 unit, int at)
{
    auto& records = this->slotRecords;

    // swd3e: a unit already in a record is not put in again.
    for (const BattleSlotRecord& record : records)
    {
        if (record.unit == unit)
        {
            return;
        }
    }

    int count = 0;

    for (std::size_t place = 0; place < records.size(); ++place)
    {
        if (records[place].unit == -1)
        {
            count = static_cast<int>(place);
            break;
        }
    }

    auto shiftFrom = [&records, count](int first)
    {
        for (int place = count; place >= first; --place)
        {
            if (place + 1 < static_cast<int>(records.size()))
            {
                records[static_cast<std::size_t>(place + 1)] = records[static_cast<std::size_t>(place)];
            }
        }
    };

    if (kind != 1)
    {
        if (count >= at)
        {
            shiftFrom(at);
        }

        BattleSlotRecord& record = records[static_cast<std::size_t>(at)];
        record.unit = unit;
        record.kind = static_cast<u16>(kind);
        return;
    }

    int place = 0;

    if (at == -1)
    {
        place = count;
    }
    else if (count >= at)
    {
        place = at;
        shiftFrom(at);
    }

    BattleSlotRecord& record = records[static_cast<std::size_t>(place)];
    record.unit = unit;
    record.kind = 1;

    // A unit before the party units reads before the orders in the original.
    if (unit >= BattleState::FirstRoleUnit)
    {
        PendingOrder& order = this->pendingOrders[static_cast<std::size_t>(unit - BattleState::FirstRoleUnit)];
        record.unk14 = order.unk04;
        record.unk0C = order.unk00;
        record.unk18 = order.unk08;
        record.unk0A = order.unk0C;
        record.targetCount = order.targetCount;
        order = PendingOrder{};
    }

    this->unk53BF80 = 0;
    this->unk53BFD0 = 0;
}

// The slot records filled with 0xFF bytes (inline in the original, by 0x0045ADF0, 0x0045AA00 and others).
void BattleState::WipeSlotRecords()
{
    for (BattleSlotRecord& record : this->slotRecords)
    {
        record = BattleSlotRecord{-1, -1, 0xFFFF, 0xFFFF, -1, -1, -1, -1};
    }
}

// The order of 0x0045B280 (inline in the original): first the units of the acting unit's side above it (lower y, not
// it or `target`), sorted by an insertion that also copies the last placed unit one further; then each pass takes the
// first unit not taken that is off that side or above the acting unit, and the lowest enemy (unless a role acts) or
// role (unless an enemy acts) not taken below it; `target` brings the acting unit right after it. With no unit left
// the first pick reads the dword after unitY (darkFrames) as its y and places unit 18, and the order is then made
// again by SortDrawOrder.
void BattleState::SortDrawOrderAround(i32 acting, i32 target)
{
    const bool roleActs = acting >= FirstRoleUnit;
    const int sideFirst = roleActs ? FirstRoleUnit : 0;
    const int sideEnd = roleActs ? Units : FirstRoleUnit;
    i32 bound = this->unitY[static_cast<std::size_t>(acting)];
    int placed = 0;
    int at = 0;

    for (int unit = sideFirst; unit < sideEnd; ++unit)
    {
        const i32 y = this->unitY[static_cast<std::size_t>(unit)];

        if (y == 0 || unit == acting || unit == target || y >= bound)
        {
            continue;
        }

        if (placed > 0)
        {
            int position = 0;
            bool found = true;

            while (y >= this->unitY[static_cast<std::size_t>(this->drawOrder[static_cast<std::size_t>(position)])])
            {
                if (++position >= placed)
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                at = position;

                for (int move = placed + 1; move > position; --move)
                {
                    this->drawOrder[static_cast<std::size_t>(move)] =
                        this->drawOrder[static_cast<std::size_t>(move - 1)];
                }
            }
        }

        this->drawOrder[static_cast<std::size_t>(at)] = unit;
        this->drawTaken[static_cast<std::size_t>(unit)] = 1;
        ++placed;
        at = placed;
    }

    const auto total = static_cast<int>(this->roleCount + this->enemyCount);
    i32 actingLeft = acting;
    i32 targetLeft = target;

    // The original runs on past the order (writing the globals after it) when the pair is placed at its end.
    while (placed != total && at < Units)
    {
        if (actingLeft != -1)
        {
            bound = this->unitY[static_cast<std::size_t>(actingLeft)];
        }

        int first = 0;

        for (; first < Units; ++first)
        {
            const i32 y = this->unitY[static_cast<std::size_t>(first)];
            const bool allowed = actingLeft == -1 || first < sideFirst || first >= sideEnd || y < bound;

            if (y != 0 && allowed && this->drawTaken[static_cast<std::size_t>(first)] != 1)
            {
                break;
            }
        }

        i32 lowest = first < Units ? this->unitY[static_cast<std::size_t>(first)] : this->darkFrames;
        int chosen = first;

        for (u32 enemy = 0; enemy < this->enemyCount; ++enemy)
        {
            const auto unit = static_cast<std::size_t>(enemy);
            const bool allowed = actingLeft >= FirstRoleUnit || actingLeft == -1;

            if (this->drawTaken[unit] != 1 && this->unitY[unit] < lowest && static_cast<i32>(enemy) != actingLeft &&
                allowed)
            {
                lowest = this->unitY[unit];
                chosen = static_cast<int>(enemy);
            }
        }

        for (u32 role = FirstRoleUnit; role < this->roleCount + FirstRoleUnit; ++role)
        {
            const auto unit = static_cast<std::size_t>(role);
            const bool allowed = actingLeft < FirstRoleUnit || actingLeft == -1;

            if (this->drawTaken[unit] != 1 && this->unitY[unit] < lowest && static_cast<i32>(role) != actingLeft &&
                allowed)
            {
                lowest = this->unitY[unit];
                chosen = static_cast<int>(role);
            }
        }

        if (chosen == targetLeft && at + 1 < Units)
        {
            this->drawOrder[static_cast<std::size_t>(at)] = targetLeft;
            this->drawTaken[static_cast<std::size_t>(targetLeft)] = 1;
            this->drawTaken[static_cast<std::size_t>(actingLeft)] = 1;
            this->drawOrder[static_cast<std::size_t>(at + 1)] = actingLeft;
            at += 2;
            placed += 2;
            actingLeft = -1;
            targetLeft = -1;
            continue;
        }

        this->drawOrder[static_cast<std::size_t>(at)] = chosen;

        // The original marks unit 30 past the table (over the ripple pointer at 0x00536880).
        if (chosen < Units)
        {
            this->drawTaken[static_cast<std::size_t>(chosen)] = 1;
        }

        ++placed;
        ++at;
    }

    this->drawTaken.fill(0);
    this->orderSorted = 1;

    if (std::find(this->drawOrder.begin(), this->drawOrder.end(), Units) != this->drawOrder.end())
    {
        this->SortDrawOrder();
    }
}

// swd3e.exe 0x00469BA0: object `id` held back as returned object `index` (1 or 2; given back by ReturnBattleItems)
void BattleState::SetReturnedItem(int index, u16 id)
{
    this->givenBack[static_cast<std::size_t>(index)] = id;
}

// The removal of 0x0045F020 (inline in the original): record `at` copied into `taken`; a used one is removed, the
// later records moving up, and from the first free record on (from `at` when none is free) the records are cleared.
// An `at` past the records (all of them held) reads after them in the original; a free record is taken then.
void BattleState::TakeSlotRecordAt(std::size_t at, BattleSlotRecord& taken)
{
    auto& records = this->slotRecords;

    if (at >= records.size())
    {
        taken = BattleSlotRecord{};
        return;
    }

    taken = records[at];

    if (taken.unit == -1)
    {
        return;
    }

    for (std::size_t move = at; move + 1 < records.size(); ++move)
    {
        records[move] = records[move + 1];
    }

    std::size_t clearFrom = at;

    for (std::size_t place = 0; place < records.size(); ++place)
    {
        if (records[place].unit == -1)
        {
            clearFrom = place;
            break;
        }
    }

    for (std::size_t place = clearFrom; place < records.size(); ++place)
    {
        records[place] = BattleSlotRecord{};
    }
}

// The seven dwords from 0x0053AE70 cleared (a memset in the original: the acting unit and its hit)
void BattleState::ClearActing()
{
    this->actingUnit = 0;
    this->sweepTotal = 0;
    this->unk53AE78 = 0;
    this->slideBackPicture = 0;
    this->unk575F24 = 0;
    this->unk53AE80 = 0;
    this->hitsAtOnce = 0;
    this->wholeSide = 0;
}

// swd3e: the word at 0x0050D864 + 2 * `command` that MenuRight, MenuLeft and MenuConfirm read as the kind of menu
// command `command`: the kinds of the listed commands for 6..9, before them unk50D86C for 4 and its high word, never
// written, for 5. The words past the kinds (later commands) are other battle data in the original (deviations.md).
u16 BattleState::MenuKind(int command) const
{
    if (command == kColourCommand)
    {
        return this->unk50D86C;
    }

    const auto listed = static_cast<std::size_t>(command - kFirstListedCommand);
    return command >= kFirstListedCommand && listed < this->commandKinds.size() ? this->commandKinds[listed] : u16{0};
}

// The dword at 0x004BF16C + 4 * slot as the original reads it by battle role: partyRoles, then RoleActs (0x004BF17C)
// for the roles after the party (the ghost and the escorts) and frameMilliseconds (0x004BF18C). None past it (the
// QuakeSteps bytes and on; deviations.md).
std::optional<i32> BattleState::PartyRoleRead(u32 slot) const
{
    const std::size_t roles = this->partyRoles.size();

    if (slot < roles)
    {
        return this->partyRoles[slot];
    }

    if (slot < roles + BattleState::RoleActs.size())
    {
        return BattleState::RoleActs[slot - roles];
    }

    if (slot == roles + BattleState::RoleActs.size())
    {
        return this->frameMilliseconds;
    }

    return std::nullopt;
}

// RoleCycle read at 0x004BF578 + 4 * at as the original does: chargingRole (0x004BF58C), meterRole (0x004BF590) and
// Unk4BF594 follow it. None past them (string bytes that never name a role; deviations.md).
std::optional<i32> BattleState::RoleCycleRead(i32 at) const
{
    if (at < 0)
    {
        return std::nullopt;
    }

    const auto place = static_cast<std::size_t>(at);
    const std::size_t cycle = this->roleCycle.size();

    if (place < cycle)
    {
        return this->roleCycle[place];
    }

    const std::array<i32, 3> after = {this->chargingRole, this->meterRole, BattleState::Unk4BF594};

    if (place - cycle < after.size())
    {
        return after[place - cycle];
    }

    return std::nullopt;
}

// PortraitX read at 0x004BF13C + 4 * place as the original does: NameX (0x004BF14C) follows it. None for other
// places (deviations.md).
std::optional<int> BattleState::PortraitXRead(i32 place)
{
    if (place < 0)
    {
        return std::nullopt;
    }

    const auto at = static_cast<std::size_t>(place);

    if (at < BattleState::PortraitX.size())
    {
        return BattleState::PortraitX[at];
    }

    if (at < BattleState::PortraitX.size() + BattleState::NameX.size())
    {
        return BattleState::NameX[at - BattleState::PortraitX.size()];
    }

    return std::nullopt;
}

}  // namespace swd3e
