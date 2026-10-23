#pragma once

#include <array>
#include <memory>
#include <span>

#include "battle/battle_unit.h"
#include "item/item_list.h"
#include "org/fame.h"
#include "party/party.h"

namespace swd3e
{

// The kinds of the accessories (ObjectData::unk48), named after the mon.dat items of each kind.
enum class AccessoryKind : u16
{
    RepeatAttack = 0x15,  // 武聖手鐲, 姜公膽: the attack repeats once
    Unk16 = 0x16,         // no item has it
    Steal = 0x17,         // 神偷面具, 乾坤袋
    Unk18 = 0x18,         // 武神魂玉
    Unk19 = 0x19,         // no item has it
    Unk1A = 0x1A,         // no item has it
    Unk1B = 0x1B,         // 暗殺寶錄, 御夫錄
    Unk1C = 0x1C,         // 硃砂辟魔玉
    Unk1D = 0x1D,         // no item has it
    Unk1E = 0x1E,         // 硃砂神筆
    Unk1F = 0x1F,         // 鼠槌
    Unk20 = 0x20,         // 金鐘罩秘笈
    Unk21 = 0x21,         // 庖丁菜刀
    Unk22 = 0x22,         // no item has it
    Unk23 = 0x23,         // 夜叉肩帶
    Unk24 = 0x24,         // no item has it
    Unk25 = 0x25,         // swd3e: command 0x25 alone (the command name 攻擊)
    Unk27 = 0x27,         // swd3e: unk2EB2 counts up (the command name 物品)
    Unk28 = 0x28,       // swd3e: armed by a first use (equipmentFlags 0x100), taken by the next (the command name 絕招)
    Unk29 = 0x29,       // swd3e: the proficiency steps only (the command name 防禦)
    Unk32 = 0x32,       // no item has it
    ElementOrb = 0x33,  // 地炎玉, 水靈珠, 青龍水珠, ...: an element's resistance lowered by the proficiency
    AttackRing = 0x34,  // 薩登荊環, 星君眼
    BladeReturn = 0x35,  // 返刃玉
    Unk36 = 0x36,        // no item has it
    SpiritDrain = 0x37,  // 吸魔壺
    LifeDrain = 0x38,    // 吸血鬼指環
    SpiritFan = 0x39,    // 靈虛扇
    SpecialAct = 0x3A,   // swd3e: equipment flag 0x80 (BattleRole::StartSpecialAct)
    GoldGuard = 0x3B,    // swd3e: the gold pays for the hits (goldGuard)
};

// A party role in battle (0x2F34 bytes in the original, ten of them at 0x005029D0): a unit with a battle copy of the
// role's record and the fields past the unit.
class BattleRole : public BattleUnit
{
public:
    static constexpr std::u8string_view NotEnoughItems = u8"物品不夠!!";      // 0x004BFA90
    static constexpr std::u8string_view NotEnoughSpirit = u8"靈力值不夠!!";   // 0x004BFA80
    static constexpr std::u8string_view NotEnoughStamina = u8"體力值不夠!!";  // 0x004BFA70
    // The equipment slots of a role (Party::SlotsPerRole).
    static constexpr std::size_t WeaponSlot = 0;
    static constexpr std::size_t FirstAccessorySlot = 7;
    static constexpr std::size_t SecondAccessorySlot = 8;
    static constexpr std::size_t ArmourSlots = 7;  // slots below it give the bonus words
    static constexpr std::size_t NoResistanceSlotA = 9;
    static constexpr std::size_t NoResistanceSlotB = 0xA;

    static constexpr std::size_t ExtraActs = 5;
    // SwarmSpellFrame: where the swarm acts start around the role, by their number (0x004BF9B0, 0x004BF9C0).
    static constexpr std::array<i16, 8> SwarmOffsetX = {0, 0, -120, 120, -70, 70, -70, 70};
    static constexpr std::array<i16, 8> SwarmOffsetY = {-100, 100, 0, 0, -50, -50, 50, 50};

    BattleRole();
    ~BattleRole();
    void Release();
    void SetSkills(ItemNode* list);
    void SetInventory(ItemLink* list);
    void ClearExtras();
    void Setup(RoleRecord& record, RoleBattleBlock& block, const UnitPlacement& at, ErrorReporter& reporter);
    void SetupEscort(const UnitPlacement& at, BattleServices& services, Database& database);
    void TakeObjectForm(Database& database, u16 id);
    void SetupGhost(const UnitPlacement& at, TalismanGhost& ghost, BattleServices& services);
    void Strike(BattleServices& services, BattleUnit& target, int evadable);
    int AttackFrame(BattleServices& services, BattleUnit& target, int slot);
    void LoadItemAction(Database& database, u16 id, u32& act);
    void StartAbsorb(BattleServices& services, BattleUnit& source);
    void FlyExtraAct(BattleServices& services, i32 id, i32 action, int slot, i16 x, i16 y, int steps);
    int AbsorbFrame(BattleServices& services);
    int ThrowAct(BattleServices& services, const BattleUnit& target);
    int CatchAct(BattleServices& services, const BattleUnit& target);
    int GlowAct(BattleServices& services, int x, int y);
    int PoseAtTarget(BattleServices& services, const BattleUnit& target);
    u16 Steal(BattleServices& services, const BattleUnit& target);
    u16 AttackPose(BattleServices& services);
    static int CanTarget(const BattleUnit& target);
    int CanAbsorb(BattleServices& services, const BattleUnit& target) const;
    int DirectAttack(BattleServices& services, BattleUnit& target);
    int CastAt(BattleServices& services, const BattleUnit& target, i32 spell);
    int MagicFrame(BattleServices& services, const BattleUnit& target);
    int BeamSpellFrame(BattleServices& services, BattleUnit& target);
    int ChargeAttackFrame(BattleServices& services, BattleUnit& target, int slot);
    int ChargeOnTarget(BattleServices& services, BattleUnit& target);
    int SwarmSpellFrame(BattleServices& services, BattleUnit& target);
    int BoomerangFrame(BattleServices& services, BattleUnit& target, i32 act);
    int LeapSpellFrame(BattleServices& services, BattleUnit& target);
    u16 UseSelectedItem(int category);
    void CancelCommand();
    void AddFury(BattleServices& services, int damage);
    int DrainShare(BattleServices& services, i32& amount);
    u16 Unk1EProficiency(BattleServices& services);
    void CountUsableObjects(u16& count);
    int ListedObject(int index, std::string& name, u16& count);
    ItemNode* StepItem();
    static u32 CategoryKinds(int category, int categories);
    ItemNode* StepSkill();
    void CountSkills(int category, int type, u8& count);
    static int ObjectsLeft(const ItemNode& node);
    void CountObjects(int category, u16& count);
    int ListObject(const BattleState& battle, int category, int index, std::string& name, u16& flags, u16& count);
    int ThrowableObject(int throwable, int index, std::string& name, u16& count);
    u16 ListSkill(Database& database, int category, int type, int index, std::string& name, u16& cost);
    int StepWornCounters(CounterList& list, u16 amount);
    u16 SelectSkill(Database& database, int category, int type, int index, u32& act);
    int CanPaySkill(BattleServices& services, int category, int index);
    int SelectObject(Database& database, const BattleState& battle, int category, int index, u32& act, u16& kind);
    void Regenerate(BattleServices& services);
    void UseAccessorySkill(BattleServices& services, u16 kind);
    int RollCritical(BattleServices& services);
    void PrepareCommand(BattleServices& services, Database& database);
    void FinishCommand();
    int BeatsLevel(BattleServices& services, u16 level) const;
    int StepBack(BattleServices& services, int mode);
    void SetupGuest(const UnitPlacement& at, const RoleRecord& base, Database& database, ErrorReporter& reporter);
    void ApplyEquipment(
        std::span<const std::unique_ptr<ItemNode>> slots,
        CounterList& proficiency,
        ErrorReporter& reporter
    );
    void ApplyAccessory(const ObjectRecord& accessory, CounterList& proficiency);
    void ShowAllSkills();
    void ShowAllItems();
    void PayStamina();
    void TakeCommand2F18();

    // The talisman ghost in battle and the helpers of the roles acting by themselves (battle_ghost.cpp, swd3e).
    u16 SelectedItemId() const;
    void SetGhostOrder(u16 order);
    u16 GhostOrder() const;
    static int RollHalf(BattleServices& services);
    bool GhostOrderIs5() const;
    bool GhostOrderIs6() const;
    bool TakeSkillUsed();
    bool TakeAttackUsed();
    bool TakeHealUsed();
    int TakeOrder7();
    int GhostObeys(BattleServices& services);
    int RestoreGhostSpirit(BattleServices& services);
    int HasCommandEffect() const;
    int SetGhostActs(int value);
    int ChooseGhostSkill(BattleServices& services, Database& database);
    bool LoadGhostSkill(Database& database, i32 skill);
    int ChooseGhostSkillOf(BattleServices& services, Database& database, int mode);
    void TakeGhostOrder(BattleServices& services, Database& database);
    int GhostSkillCommand(Database& database, u32& act, u16& kind);
    void GhostAttackCommand(Database& database);
    int SpendGhostSpirit(int less, int base, int percent);
    int LevelGapRoll(BattleServices& services, u16 level);
    int LowHitPoints(int third) const;
    void SetElement(CounterList& proficiency, i8 element);
    i16 SetUnk34C0(i16 value);
    i16 Unk34C0() const;
    bool LowSpirit() const;
    bool LowStamina() const;
    int StartSpecialAct(BattleState& battle);

    std::unique_ptr<RoleRecord> ownedRecord;     // the 0x38 bytes at +0x00
    ObjectRecord unk2B24;                        // swd32 +0x2B24
    std::array<ActState, ExtraActs> extraActs;   // +0x311C the sparks of an absorb (FlyExtraAct)
    ItemNode* skills = nullptr;                  // +0x3450 the first skill shown in the list
    ItemNode* skillsHead = nullptr;              // swd32 +0x2EC4 the start of the list (its "無" head)
    ItemLink* inventory = nullptr;               // +0x3458 the first item shown (the next of inventoryNode)
    ItemNode* inventoryNode = nullptr;           // the item at +0x2EC8 itself; nullptr: the list head
    ItemLink* inventoryHead = nullptr;           // +0x345C the party's items
    ItemNode* selectedItem = nullptr;            // +0x3460 the item chosen to use
    std::array<i32, ExtraActs> extraSteps = {};  // +0x3488 the line steps of each extra act this frame
    i32 unk2F0C = 0;
    u16 unk2F10 = 0;
    u16 staminaCost = 0;  // +0x34A2 the stamina the command takes when it runs
    u16 usedItem = 0;     // +0x34A4 the id of the item used (UseSelectedItem; GlowAct plays it as its act)
    u16 unk2F16 = 0;
    u16 unk2F18 = 0;
    u16 weaponId = 0;  // +0x34AA
    u16 unk2F1C = 0;
    u16 bonusA = 0;  // swd32 +0x2F1E the sum of the +0x34 words of the armour (slots 1..6 without kind)
    u16 bonusB = 0;  // swd32 +0x2F20 of their +0x36 words
    u16 bonusC = 0;  // swd32 +0x2F22 of their +0x38 words
    u16 unk2F24 = 0;
    u16 ghostOrder = 0;  // +0x34B6 (swd3e) the order the player gave the talisman ghost (SetGhostOrder)
    u16 ghostSkill = 0;  // +0x34B8 (swd3e) the index of the fed skill chosen (bit 15 while choosing)
    // +0x34BA, +0x34BC (swd3e) the words +0x56 and +0x58 of the item used (UseSelectedItem, 0x004818E0): a target of
    // that category is absorbed on a 0..9 roll below the second (CanAbsorb)
    u16 absorbCategory = 0;
    u16 absorbChance = 0;
    i16 absorbFrames = 0;  // +0x34BE the frames of the running absorb (AbsorbFrame)
    i16 unk34C0 = 0;       // +0x34C0 (swd3e) set by MenuConfirm, read by EnemyFrame (0x00487160, 0x00487170)
    u8 critical = 0;       // +0x34CA 1: the next attack is critical (set by 0x0046FF00)
    i32 ghostActs = 0;     // +0x34CC (swd3e) 1: the ghost acts this turn (a half roll of its order)
    i32 unk34D0 = 0;       // +0x34D0 (swd3e) set by StartSpecialAct
};

}  // namespace swd3e
