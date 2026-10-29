#include "party/party.h"
#include "core/byte_reader.h"
#include "org/database.h"
#include "org/object_record.h"
#include "text/big5.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace swd3e
{

namespace
{

constexpr std::size_t kStartingResistances = 9;  // the resistances of a starting role in maps.dat

}  // namespace

// swd3e.exe 0x0040E0D0: a role of the starting state in maps.dat (0x34 bytes): the maxima come before the current
// values, the experience is a word; the last of its nine resistances fills the two more of swd3e
void RoleRecord::Load(std::span<const u8> record)
{
    ByteReader reader(record);
    this->totalExperience = static_cast<i32>(reader.Word32());
    this->maxHitPoints = static_cast<i16>(reader.Word16());
    this->maxSpirit = static_cast<i16>(reader.Word16());
    this->maxStamina = static_cast<i16>(reader.Word16());
    this->hitPoints = static_cast<i16>(reader.Word16());
    this->spirit = static_cast<i16>(reader.Word16());
    this->stamina = static_cast<i16>(reader.Word16());
    this->strength = static_cast<i16>(reader.Word16());
    this->endurance = static_cast<i16>(reader.Word16());
    this->wisdom = static_cast<i16>(reader.Word16());
    this->agility = static_cast<i16>(reader.Word16());
    this->rapport = static_cast<i16>(reader.Word16());
    this->reaction = static_cast<i16>(reader.Word16());
    this->ailmentChance = static_cast<i16>(reader.Word16());
    this->evasion = static_cast<i16>(reader.Word16());
    this->unk2A = reader.Word16();
    this->experience = static_cast<i32>(reader.Word16());
    this->condition = reader.Word16();
    this->attackBonus = reader.Word16();
    this->defenseBonus = reader.Word16();
    this->level = reader.Byte();

    for (std::size_t element = 0; element < kStartingResistances; ++element)
    {
        this->resistances[element] = reader.Byte();
    }

    for (std::size_t element = kStartingResistances; element < this->resistances.size(); ++element)
    {
        this->resistances[element] = this->resistances[kStartingResistances - 1];
    }
}

namespace
{

constexpr u16 kPlainObjectLimit = 0x1B;  // ApplyObject: ObjectData::category up to it applies by useKind
constexpr u16 kVitalsObject = 0;         // ObjectData::useKind
constexpr u16 kMaximaObject = 0x100;
constexpr u16 kPercentObject = 0x200;
constexpr int kPercent = 100;

}  // namespace

// The text from the part at `offset` up to its NUL, within the record
std::string RoleName::TextAt(std::size_t offset) const
{
    const auto* bytes = reinterpret_cast<const char*>(this);
    const std::string_view rest(bytes + offset, sizeof(RoleName) - offset);
    return std::string(rest.substr(0, rest.find('\0')));
}

std::string RoleName::Surname() const
{
    return this->TextAt(offsetof(RoleName, surname));
}

std::string RoleName::Given() const
{
    return this->TextAt(offsetof(RoleName, given));
}

std::string RoleName::Nickname() const
{
    return this->TextAt(offsetof(RoleName, nickname));
}

std::string RoleName::FullName() const
{
    return this->Surname() + this->Given();
}

RoleName RoleName::From(const Parts& parts)
{
    RoleName name;
    const std::string surnameText = Big5(parts.surname);
    const std::string givenText = Big5(parts.given);
    const std::string nicknameText = Big5(parts.nickname);
    std::copy_n(surnameText.begin(), std::min(surnameText.size(), name.surname.size() - 1), name.surname.begin());
    std::copy_n(givenText.begin(), std::min(givenText.size(), name.given.size() - 1), name.given.begin());
    std::copy_n(nicknameText.begin(), std::min(nicknameText.size(), name.nickname.size() - 1), name.nickname.begin());
    return name;
}

// swd3e.exe 0x00429790: the default names of the roles and the names the talk texts write for them
void Party::ResetNames()
{
    for (std::size_t role = 0; role < Party::Roles; ++role)
    {
        this->names[role] = RoleName::From(Party::DefaultNames[role]);
        this->nameTags[role] = RoleName::From(Party::NameTags[role]);
    }
}

// swd3e.exe 0x00411390: a field of role `role` set by number (0..0xD the words from +4, 0xE the experience, 0xF the
// first dword, 0x10 the level with the experience of that level)
void Party::SetRoleStat(int field, int value, int role, Database& database)
{
    RoleRecord& record = this->roles[static_cast<std::size_t>(role)];
    const std::array<i16*, Party::WordStats> words = {
        &record.hitPoints,
        &record.spirit,
        &record.stamina,
        &record.maxHitPoints,
        &record.maxSpirit,
        &record.maxStamina,
        &record.strength,
        &record.endurance,
        &record.wisdom,
        &record.agility,
        &record.rapport,
        &record.reaction,
        &record.ailmentChance,
        &record.evasion,
    };

    if (field >= 0 && field < Party::WordStats)
    {
        *words[static_cast<std::size_t>(field)] = static_cast<i16>(value);
    }
    else if (field == Party::ExperienceStat)
    {
        record.experience = value;
    }
    else if (field == Party::TotalExperienceStat)
    {
        record.totalExperience = value;
    }
    else if (field == Party::LevelStat)
    {
        record.level = static_cast<u8>(value);
        u32 experience = 0;

        if (database.LoadLevelEntry(role + 1, value + 1, experience))
        {
            record.experience = static_cast<i32>(experience);
        }
    }
}

// swd3e.exe 0x004115D0: a field of role `role` by number (the first six words signed, the others unsigned)
int Party::RoleStat(int field, int role) const
{
    const RoleRecord& record = this->roles[static_cast<std::size_t>(role)];
    const std::array<i16, Party::WordStats> words = {
        record.hitPoints,
        record.spirit,
        record.stamina,
        record.maxHitPoints,
        record.maxSpirit,
        record.maxStamina,
        record.strength,
        record.endurance,
        record.wisdom,
        record.agility,
        record.rapport,
        record.reaction,
        record.ailmentChance,
        record.evasion,
    };

    if (field >= 0 && field < Party::SignedStats)
    {
        return words[static_cast<std::size_t>(field)];
    }

    if (field >= Party::SignedStats && field < Party::WordStats)
    {
        return static_cast<u16>(words[static_cast<std::size_t>(field)]);
    }

    if (field == Party::ExperienceStat)
    {
        return record.experience;
    }

    if (field == Party::TotalExperienceStat)
    {
        return record.totalExperience;
    }

    if (field == Party::LevelStat)
    {
        return record.level;
    }

    return 0;
}

// swd3e.exe 0x00454D80: object `object` used on (or worn by) a party role: its skill's ailment with bit 15 revives a
// defeated role. A standing role then gains, when the object's category is above 0x1B, only the vitals (up to the
// maxima); otherwise by useKind: 0 the vitals (up to the maxima) and the attributes, 0x100 the maxima and the
// attributes, 0x200 the vitals in percent of the maxima. A role still defeated gains nothing.
void ApplyObject(RoleRecord& record, const ObjectData& object, Database& database)
{
    SkillScript skill;
    database.LoadSkill(skill, object.skillScript);

    if (static_cast<i16>(skill.ailment) < 0)
    {
        record.condition &= static_cast<u16>(~RoleRecord::Defeated);
    }

    auto add = [](i16& field, u16 amount)
    {
        field = static_cast<i16>(field + amount);
    };

    auto addBonus = [](u16& field, u16 amount)
    {
        field = static_cast<u16>(field + amount);
    };

    auto clampVitals = [&record]
    {
        record.hitPoints = std::min(record.hitPoints, record.maxHitPoints);
        record.spirit = std::min(record.spirit, record.maxSpirit);
        record.stamina = std::min(record.stamina, record.maxStamina);
    };

    if (object.category > kPlainObjectLimit && (record.condition & RoleRecord::Defeated) == 0)
    {
        add(record.hitPoints, object.hitPointsUp);
        add(record.spirit, object.spiritUp);
        add(record.stamina, object.staminaUp);
        clampVitals();
        return;
    }

    if ((record.condition & RoleRecord::Defeated) != 0)
    {
        return;
    }

    if (object.useKind == kVitalsObject)
    {
        add(record.hitPoints, object.hitPointsUp);
        add(record.spirit, object.spiritUp);
        add(record.stamina, object.staminaUp);
        clampVitals();
        add(record.strength, object.strengthUp);
        add(record.endurance, object.enduranceUp);
        add(record.wisdom, object.wisdomUp);
        add(record.agility, object.agilityUp);
        add(record.rapport, object.rapportUp);
        add(record.evasion, object.evasionUp);
        addBonus(record.attackBonus, object.attackBonusUp);
        addBonus(record.defenseBonus, object.defenseBonusUp);
    }

    if (object.useKind == kMaximaObject)
    {
        add(record.maxHitPoints, object.hitPointsUp);
        add(record.maxSpirit, object.spiritUp);
        add(record.maxStamina, object.staminaUp);
        add(record.strength, object.strengthUp);
        add(record.endurance, object.enduranceUp);
        add(record.wisdom, object.wisdomUp);
        add(record.rapport, object.rapportUp);
        add(record.agility, object.agilityUp);
        add(record.evasion, object.evasionUp);
        addBonus(record.attackBonus, object.attackBonusUp);
        addBonus(record.defenseBonus, object.defenseBonusUp);
    }

    if (object.useKind == kPercentObject)
    {
        const i16 maxHitPoints = record.maxHitPoints;
        const i16 maxSpirit = record.maxSpirit;
        const i16 maxStamina = record.maxStamina;
        // 0x0044D887..0x0044D8E4: each a 32-bit product of the two signed words, then / 100
        add(record.hitPoints, static_cast<u16>(maxHitPoints * static_cast<i16>(object.hitPointsUp) / kPercent));
        add(record.spirit, static_cast<u16>(maxSpirit * static_cast<i16>(object.spiritUp) / kPercent));
        add(record.stamina, static_cast<u16>(maxStamina * static_cast<i16>(object.staminaUp) / kPercent));
        clampVitals();
    }
}
}  // namespace swd3e
