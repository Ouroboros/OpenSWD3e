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

// The kinds of the accessories (ObjectData::actionKind), named after the mon.dat items of each kind.
enum class AccessoryKind : u16
{
    RepeatAttack = 0x15,    // 電光聖鐲, 姜公膽: the attack repeats once
    Thunder = 0x16,         // 天雷: 陰陽鏡 (天雷: YinYangAttack)
    Steal = 0x17,           // 乾坤袋
    GroupAttack = 0x18,     // 群攻: 武神魂玉
    Control = 0x19,         // 控制: no item has it
    SureHit = 0x1A,         // 命中: no item has it
    Throw = 0x1B,           // 投擲: 暗殺寶錄, 御夫錄
    Invisibility = 0x1C,    // 隱形: no item has it (swd32's 硃砂辟魔玉; swd3e keeps the name with no effect)
    BreakJade = 0x1D,       // 斷玉: no item has it
    RefineTalisman = 0x1E,  // 煉符: no item has it
    MouseHammer = 0x1F,     // 鼠槌: 鼠槌
    Shield = 0x20,          // 護罩: no item has it (swd32's 金鐘罩秘笈; swd3e keeps the name with no effect)
    Dismantle = 0x21,       // 分解: 庖丁菜刀 (分解)
    Focus = 0x22,           // 氣合: no item has it
    RecoverSpirit = 0x23,   // 回氣: 夜叉肩帶
    Meditate = 0x24,        // 運功: no item has it
    Roulette = 0x25,        // 龍颿: swd3e: 藍格怪衣, command 0x25 alone (龍颿: the roulette)
    Gather = 0x27,          // 蓄勁: swd3e: 奇魄古玉, gatheredCharge counts up (蓄勁)
    StrikeBack =
        0x28,     // 逆攻: swd3e: 歸靈指套, armed by a first use (equipmentFlags 0x100), taken by the next (逆攻 / 釋放)
    Fuse = 0x29,  // 融合: swd3e: 三清符筆, the proficiency steps only (融合)
    SureCritical = 0x32,  // every attack critical, RollCritical: no item has it
    ElementOrb = 0x33,    // 朱雀火羽, 青龍靈珠, ...: the resistance to its element raised by 80% of its proficiency
    AttackRing = 0x34,    // 星君眼
    BladeReturn = 0x35,   // no item has it (swd3e's 返刃玉 has no effect)
    HitCancel = 0x36,     // 洞察光環: a hit number taken back (CancelHitNumber)
    SpiritDrain = 0x37,   // 碧綠魔壺
    LifeDrain = 0x38,     // 玄能臂環
    SpiritFan = 0x39,     // 靈虛扇
    SpecialAct = 0x3A,    // swd3e: 七仙神針, equipment flag 0x80 (BattleRole::StartSpecialAct)
    GoldGuard = 0x3B,     // swd3e: 投幣式鎧甲, the gold pays for the hits (goldGuard)
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
    int LevelInReach(Database& database, u16 id, u16 reach);
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
    int MagicFrame(BattleServices& services, BattleUnit& target);
    int BeamSpellFrame(BattleServices& services, BattleUnit& target);
    int ChargeAttackFrame(BattleServices& services, BattleUnit& target, int slot);
    int SwarmSpellFrame(BattleServices& services, BattleUnit& target);
    int BoomerangFrame(BattleServices& services, BattleUnit& target, i32 act);
    int LeapSpellFrame(BattleServices& services, BattleUnit& target);
    u16 UseSelectedItem(int category);
    void CancelCommand();
    void AddFury(BattleServices& services, int damage);
    int DrainShare(BattleServices& services, i32& amount);
    u16 RefineTalismanProficiency(BattleServices& services);
    void CountUsableObjects(u16& count);
    int ListedObject(int index, std::string& name, u16& count);
    ItemNode* StepItem();
    u16 TakeRandomItem(BattleServices& services);
    int Pose36Frame(BattleServices& services);
    static u32 CategoryKinds(int category, int categories);
    ItemNode* StepSkill();
    void CountSkills(int category, int type, u8& count);
    static int ObjectsLeft(const ItemNode& node);
    void CountObjects(int category, u16& count);
    int ListObject(const BattleState& battle, int category, int index, std::string& name, u16& flags, u16& count);
    int ThrowableObject(int throwable, int index, std::string& name, u16& count);
    u16 ListSkill(Database& database, int category, int type, int index, std::string& name, u16& cost);
    int StepWornCounters(BattleServices& services, CounterList& list, u16 amount);
    u16 SelectSkill(Database& database, int category, int type, int index, u32& act);
    int CanPaySkill(BattleServices& services, int category, int index);
    int SelectObject(Database& database, const BattleState& battle, int category, int index, u32& act, u16& kind);
    // The rebuild (deviations.md): the kind bits and the users `node`'s object is listed and used by: with
    // freeCreatures a taken enemy's object (a creature of mon.dat) as a 活物's, called as an escort.
    u32 ListedKind(const ItemNode& node) const;
    u16 ListedUsers(const ItemNode& node) const;
    void Regenerate(BattleServices& services);
    void UseAccessorySkill(BattleServices& services, u16 kind);
    int RollCritical(BattleServices& services);
    int PrepareCommand(BattleServices& services, Database& database);
    void FinishCommand();
    int BeatsLevel(BattleServices& services, u16 level) const;
    int StepBack(BattleServices& services, int mode);
    void ApplyEquipment(
        std::span<const std::unique_ptr<ItemNode>> slots,
        CounterList& proficiency,
        ErrorReporter& reporter
    );
    void ApplyAccessory(const ObjectRecord& accessory, CounterList& proficiency);
    void ShowAllSkills();
    void ShowAllItems();
    void PayStamina();
    void TakeMagicCommand();

    // The talisman ghost in battle and the helpers of the roles acting by themselves (battle_ghost.cpp, swd3e).
    u16 SelectedItemId() const;
    void SetGhostOrder(i32 order);
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
    int HasCommandEffect(const BattleUnit& target) const;
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
    i16 SetTransferTarget(i16 value);
    i16 TransferTarget() const;
    bool LowSpirit() const;
    bool LowStamina() const;
    int StartSpecialAct(BattleState& battle);
    void SpecialActFrame(BattleServices& services);

    std::unique_ptr<RoleRecord> ownedRecord;  // the 0x3C bytes at +0x00 (allocated cleared by the constructor)
    ObjectRecord loadedObject;  // +0x3074 an object read for the ghost's skill, 融合 or a level check (ClearExtras)
    std::array<ActState, ExtraActs> extraActs;   // +0x311C the sparks of an absorb (FlyExtraAct)
    ItemNode* skills = nullptr;                  // +0x3450 the first skill shown in the list
    ItemNode* skillsHead = nullptr;              // +0x3454 the start of the list (its "無" head)
    ItemLink* inventory = nullptr;               // +0x3458 the first item shown (the next of inventoryNode)
    ItemNode* inventoryNode = nullptr;           // the node +0x3458 is the next of; nullptr: the list head
    ItemLink* inventoryHead = nullptr;           // +0x345C the party's items
    ItemNode* selectedItem = nullptr;            // +0x3460 the item chosen to use
    std::array<i32, ExtraActs> extraSteps = {};  // +0x3488 the line steps of each extra act this frame
    i32 queueIndex = 0;                          // the boomerang leg; the swarm action queued (RoleFrame)
    u16 commandCost = 0;                         // the spirit owed, or the item used up (CanPaySkill, FinishCommand)
    u16 staminaCost = 0;                         // +0x34A2 the stamina the command takes when it runs
    u16 usedItem = 0;  // +0x34A4 the id of the item used (UseSelectedItem; GlowAct plays it as its act)
    u16 unk34A6 = 0;
    u16 magicCommand = 0;  // command A of a magic object (SelectSkill)
    u16 weaponId = 0;      // +0x34AA
    u16 unk34AC = 0;
    u16 bonusA = 0;      // +0x34AE the sum of the +0x34 words of the armour (slots 1..6 without kind)
    u16 bonusB = 0;      // +0x34B0 of their +0x36 words
    u16 bonusC = 0;      // +0x34B2 of their +0x38 words
    u16 swarmCount = 0;  // the swarm actions queued so far
    u16 ghostOrder = 0;  // +0x34B6 (swd3e) the order the player gave the talisman ghost (SetGhostOrder)
    u16 ghostSkill = 0;  // +0x34B8 (swd3e) the index of the fed skill chosen (bit 15 while choosing)
    // +0x34BA, +0x34BC (swd3e) the words +0x56 and +0x58 of the item used (UseSelectedItem, 0x004818E0): a target of
    // that category is absorbed on a 0..9 roll below the second (CanAbsorb)
    u16 absorbCategory = 0;
    u16 absorbChance = 0;
    i16 absorbFrames = 0;    // +0x34BE the frames of the running absorb (AbsorbFrame)
    i16 transferTarget = 0;  // +0x34C0 (swd3e) the enemy a 傷害轉移 skill names (ConfirmTarget), taking the damage the
                             // role takes (EnemyAfterAction)
    u8 critical = 0;         // +0x34CA 1: the next attack is critical (set by 0x0046FF00)
    i32 ghostActs = 0;       // +0x34CC (swd3e) 1: the ghost acts this turn (a half roll of its order)
    i32 specialActRunning = 0;  // +0x34D0 (swd3e) 1 while the special act runs
    // The rebuild (deviations.md): [game] free_creatures as the battle started (Game::StartBattle).
    bool freeCreatures = false;
};

}  // namespace swd3e
