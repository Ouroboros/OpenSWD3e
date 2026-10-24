#include <algorithm>
#include <array>

#include "battle/battle_items.h"
#include "core/debug_log.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "game/game.h"
#include "pic/picture_ops.h"
#include "text/big5.h"

namespace swd3e
{

namespace
{

constexpr std::size_t kBattleNameBytes = 10;    // the bytes of a RoleName a battle name is gathered from
constexpr std::size_t kCommandClearBytes = 8;   // the original clears commandEnabled as two dwords (not the ninth)
constexpr std::size_t kClearedItemUses = 0xFC;  // the first 0xFC of the 0xFF item uses are cleared
constexpr std::size_t kClearedStolenLists = 8;  // 0x200 bytes: the stolen items of the first eight enemies
// swd3e.exe 0x004BF17C: the battle act of each party role, dwords read as words.
constexpr std::array<u16, Party::Roles> kRoleActs = {1, 2, 3, 4};
constexpr i16 kPanelMirror = 0x270;
constexpr int kMediumAdvance = 0x10;
constexpr int kLightRed = 0x1F;  // the light and dark colours of the battle panels
constexpr int kLightGreen = 0x1D;
constexpr int kLightBlue = 0x17;
constexpr int kDarkRed = 0xF;
constexpr int kDarkGreen = 0xE;
constexpr int kDarkBlue = 0xB;
constexpr int kGreyRed = 0x19;  // swd3e: unk50D86C
constexpr int kGreyGreen = 0x19;
constexpr int kGreyBlue = 0x1D;
constexpr int kPointerX = Display::Width / 2;  // the pointer starts in the middle of the screen
constexpr int kPointerY = Display::Height / 2;
constexpr int kBarLength = 48;    // swd3e: the pixels of a full gauge of the role panels
constexpr u32 kRowSpeeds = 0x14;  // SlideBattleIn: a row moves kRowStep + 2 * (0..kRowSpeeds-1) pixels a frame
constexpr int kRowStep = 0x1E;
constexpr int kRowSpeedScale = 2;
constexpr u32 kHeadStartRoll = 10;  // the gauges' head start: 0..9, the enemies' steps from 7 on
constexpr u32 kHeadStartFrom = 7;
constexpr int kScenes = 4;
constexpr int kBackgroundPictures = 15000;  // the battle backgrounds are pictures 15000 on
constexpr i32 kNoParty = 0x67;
constexpr std::u8string_view kBadBattle = u8"第%d號戰鬥場景資料錯誤!!";  // 0x004BF2C0
constexpr u32 kThreeRoles = 3;
// The talisman ghost (swd3e): its act (0x5A on by its kind), its place and its status when battle role 0 is down.
constexpr i32 kGhostActBase = 0x5A;
constexpr i32 kGhostUnk10C = 0xA8;
constexpr i16 kGhostX = 0x1C2;
constexpr i16 kGhostY = 0xAA;
constexpr u32 kGhostAloneStatus = 0x5000;

struct Spot
{
    i16 x;
    i16 y;
};

// The places of a party (swd3e) of one role, of two (role 0 and `second`) and of three (role 0, `second` and
// `third`; `cycle` then becomes RoleCycle[1..4]).
constexpr Spot kSolePlace = {0x20F, 0x11F};

struct PartyMakeUp
{
    int second;
    std::array<Spot, 2> spots;
};

constexpr std::array<PartyMakeUp, 3> kPairs = {{
    {1, {{{0x203, 0xE9}, {0x226, 0x13F}}}},
    {2, {{{0x1F2, 0xE3}, {0x203, 0x148}}}},
    {3, {{{0x203, 0x13E}, {0x20D, 0xD4}}}},
}};

struct TrioMakeUp
{
    int second;
    int third;
    std::array<Spot, 3> spots;
    std::array<i32, 4> cycle;
};

constexpr std::array<TrioMakeUp, 3> kTrios = {{
    {1, 2, {{{0x1E2, 0x109}, {0x21D, 0xB8}, {0x207, 0x155}}}, {3, 1, 0, 2}},
    {1, 3, {{{0x20E, 0x10E}, {0x1F7, 0x154}, {0x1FF, 0xB5}}}, {3, 2, 0, 1}},
    {2, 3, {{{0x1D5, 0x109}, {0x20C, 0x153}, {0x215, 0xBA}}}, {3, 2, 0, 1}},
}};

// The offsets of the role panels from the places of the roles (swd3e).
constexpr std::array<Spot, 4> kPanelOffsets = {{{0x14, -0x96}, {5, -0x9E}, {0x14, -0xA0}, {-5, -0xA3}}};

// A gauge of `length` pixels for value out of maximum (the original divides as doubles and truncates with _ftol).
i32 Bar(int value, int maximum)
{
    return static_cast<i32>(crt::Ftol(static_cast<double>(value) / static_cast<double>(maximum) * kBarLength));
}

}  // namespace

// swd3e.exe 0x004599E0: the battle background: picture `background` + kBackgroundPictures loaded directly (the one
// before freed) and shifted right by 640 / `scene`; the formation's animations (animationCount of them) freed, then
// loaded at their places with the same shift. The darkened copies' tint is forgotten.
void Game::LoadBattleBackground(int background, u16 scene)
{
    BattleState& battle = this->battleState;
    std::unique_ptr<Picture> picture =
        this->archive.LoadPictureDirect(static_cast<u32>(background + kBackgroundPictures), 0);
    battle.background = picture != nullptr ? std::move(*picture) : Picture{};
    ShiftPicture(battle.background.data, ShiftMode::Right, Display::Width / scene);
    // The original runs over animationCount slots of the eight; the rebuild stops at the eighth (deviations.md).
    const std::size_t count = std::min<std::size_t>(battle.animationCount, BattleFormation::Animations);

    for (std::size_t index = 0; index < count; ++index)
    {
        this->battleHud.FreeAnimation(battle.backgroundAnimations[index]);
    }

    for (std::size_t index = 0; index < count; ++index)
    {
        this->battleHud.LoadAnimation(
            battle.backgroundAnimations[index],
            0,
            battle.formation.animationX[index],
            battle.formation.animationY[index],
            static_cast<u16>(battle.formation.animations[index]),
            scene
        );
    }

    battle.darkCopyTint.fill(-1);
}

// swd3e.exe 0x004655B0: the y of the enemies and of the roles (from unit 20)
void Game::MeasureUnits()
{
    BattleState& battle = this->battleState;
    battle.unitY.fill(0);
    battle.drawOrder.fill(0);
    u16 x = 0;
    u16 y = 0;

    for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
    {
        this->battleEnemies[enemy].GetDrawPosition(x, y);
        battle.unitY[enemy] = static_cast<i16>(y);
    }

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        this->battleRoles[role].GetDrawPosition(x, y);
        battle.unitY[BattleState::FirstRoleUnit + role] = static_cast<i16>(y);
    }
}

// swd3e.exe 0x00459BD0: a battle starts: the debug mode follows game flag 0x52, the battle state is cleared, the party
// (story flags 0x1E..) takes its places by its make-up (a party of three also orders RoleCycle), the formation of
// battle.ffd brings the enemies and the background, the role panels get their gauges, the role of the start pose is
// picked, the talisman ghost joins with game flag 0x5A, the units are put in drawing order and the gauges get a head
// start (one in ten times three in ten: the enemies' run 0..9 steps; otherwise the roles of the enemies' places are
// randomized, as in the original). A formation without enemies is reported and the battle does not start.
void Game::StartBattle(u16 number)
{
    LogInfo(u8"battle %d on map %d", static_cast<i32>(number), this->currentMap);
    BattleState& battle = this->battleState;
    BattleServices& services = this->battleServices;
    GameState& state = this->state;
    battle.battle = number;
    this->config.debugMode = state.GameFlag(GameState::DebugKeys) ? 1 : 0;
    battle.musicBeforeBattle = this->audio.Queue().Mode();
    this->acts.SetEffectEnabled(1);
    this->battleMessages.Clear();
    SetGaugeFull(battle, this->config.battleSpeed);
    this->battleStage.Clear();
    this->mapBehind = false;
    battle.unk525470 = ActState{};
    battle.unk4FF168.fill(0);
    battle.ClearActing();
    battle.unk524324.fill(0);
    battle.unk4FE5D4.fill(0);
    battle.actionQueue.fill(0);
    battle.commandNames[0] = {};
    battle.commandKinds = {};
    battle.commandNames[1] = {};
    battle.commandNames[2] = {};
    this->battleMessages.Clear();
    std::fill_n(battle.commandEnabled.begin(), kCommandClearBytes, u8{0});
    battle.pendingOrders.fill(PendingOrder{});
    battle.enemyPendingOrders.fill(PendingOrder{});
    battle.commandNames[3] = {};
    battle.unk4FF0BC.fill(0);
    battle.slotDamage.fill(0);
    battle.roleListScrolls.fill(0);
    battle.unk502984.fill(-1);
    battle.slotTargets.fill(-1);
    battle.unk53AE90.fill(-1);
    battle.unk5244E8.fill(-1);
    battle.unk5244D8 = 0;
    battle.unk5244DC = 0;
    battle.inParty.fill(0);
    battle.guestUsed.fill(0);
    battle.unk524268.fill(0);
    battle.enemyHits.fill(0);
    battle.unk577224 = 0;
    battle.unk577228 = 0;
    std::fill_n(battle.itemUses.begin(), kClearedItemUses, u8{0});
    battle.unk51359C = 0;
    battle.unk51359E = 0;
    battle.ailment400From = {};
    battle.skillNamePending = {};
    battle.roleNames = {};
    battle.ghostLockTarget = 0;
    battle.unk50CE6C.fill(0);
    std::fill_n(battle.stolenItems.begin(), kClearedStolenLists, std::array<u16, 0x20>{});
    battle.unk5774E8 = 0;
    battle.unk576FC4 = 0;
    battle.coinTossing = 0;
    battle.unk576FD0 = 0;
    battle.unk576FC8 = 0;
    battle.unk576FCC = 0;
    this->battleHud.ResetMeter();
    battle.unk53C048 = 0;
    battle.actingUnit = -1;
    battle.listedCommands = 0;
    battle.unk53C4B0 = 0;

    for (std::size_t role = 0; role < Party::Roles; ++role)
    {
        const auto* bytes = reinterpret_cast<const char*>(&this->party.names[role]);
        std::size_t length = 0;

        for (std::size_t at = 0; at < kBattleNameBytes; ++at)
        {
            if (bytes[at] != '\0')
            {
                battle.roleNames[role][length++] = bytes[at];
            }
        }
    }

    for (BattleSlotRecord& record : battle.slotRecords)
    {
        record.unit = -1;
        record.unk18 = 0;
        record.unk0C = 0;
        record.unk10 = 0;
        record.unk14 = 0;
        record.unk0A = 0;
    }

    battle.roleListRows.fill(1);
    ResetPlayback(this->talk.EndAct());
    this->talk.EndAct().id = Scene::PointerAct;
    this->talk.EndAct().action = Scene::ButtonPointer;
    this->graphics.SetClearColor(this->painter.FillColorPair());

    for (int role = 0; role < Party::Roles; ++role)
    {
        if (state.GameFlag(GameState::InParty + role))
        {
            battle.inParty[static_cast<std::size_t>(role)] = 1;
            ++battle.roleCount;
            battle.partyMask = static_cast<u16>(battle.partyMask | (1u << role));
        }
    }

    u16 next = 0;

    for (u16 slot = 0; slot < battle.roleCount; ++slot)
    {
        while (next < Party::Roles && battle.inParty[next] == 0)
        {
            ++next;
        }

        battle.partyRoles[slot] = next;
        ++next;

        if (next >= Party::Roles)
        {
            break;
        }
    }

    battle.timingWindow = BattleState::DefaultTimingWindow;
    this->mediumFont.SetAdvance(kMediumAdvance);
    battle.lightColor = static_cast<u16>(this->painter.MakeColor(kLightRed, kLightGreen, kLightBlue));
    battle.darkColor = static_cast<u16>(this->painter.MakeColor(kDarkRed, kDarkGreen, kDarkBlue));
    battle.unk50D86C = static_cast<u16>(this->painter.MakeColor(kGreyRed, kGreyGreen, kGreyBlue));
    battle.projector.Init(this->graphics);
    state.pointerX = kPointerX;
    battle.pointerX = kPointerX;
    state.pointerY = kPointerY;
    battle.pointerY = kPointerY;
    this->input.SetMousePosition(kPointerX, kPointerY);
    ReleaseBattleSurfaces(battle);
    CreateBattleSurfaces(battle, this->graphics);
    this->fightTalk.Load(this->config.installDirectory, number);
    const std::u8string path = Concat({this->config.installDirectory, u8"battle.ffd"});
    battle.file.ReadHeader(path);
    battle.file.ReadFormation(path, battle.formation, number, 0);
    battle.enemyCount = battle.formation.enemyCount;
    battle.animationCount = battle.formation.animationCount;

    if (battle.formation.enemyCount == 0)
    {
        ErrorLog(kBadBattle, number);
        return;
    }

    battle.scene = static_cast<u16>(this->random.Below(kScenes) + 1);
    this->LoadBattleBackground(battle.formation.background, battle.scene);
    battle.unk525470 = ActState{};

    for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
    {
        BattleEnemy& unit = this->battleEnemies[enemy];
        unit.Reset(services);
        battle.unk5246E0 = ObjectRecord{};
        UnitPlacement& place = battle.enemyPlaces[enemy];
        place.actId = static_cast<u16>(battle.formation.enemies[enemy]);
        place.x = static_cast<i16>(battle.formation.enemyX[enemy]);
        place.y = static_cast<i16>(battle.formation.enemyY[enemy]);
        place.kind = 0;

        if (battle.mirrored == 1)
        {
            unit.SetFacing(1);
            place.x = static_cast<i16>(Display::Width - place.x);
        }

        unit.Setup(place, static_cast<u16>(battle.formation.enemies[enemy]), this->database, this->acts);

        if (battle.formation.guarding[enemy] == 1)
        {
            unit.SetGuarding(1);
        }
    }

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        UnitPlacement& place = battle.rolePlaces[role];
        place.actId = kRoleActs[static_cast<std::size_t>(battle.partyRoles[role])];
        place.kind = BattleUnit::PartyRole;
    }

    this->PlaceParty();
    const std::array<Spot, Party::Roles> spots = {
        Spot{battle.rolePlaces[0].x, battle.rolePlaces[0].y},
        Spot{battle.rolePlaces[1].x, battle.rolePlaces[1].y},
        Spot{battle.rolePlaces[2].x, battle.rolePlaces[2].y},
        Spot{battle.rolePlaces[3].x, battle.rolePlaces[3].y},
    };

    for (std::size_t panel = 0; panel < Party::Roles; ++panel)
    {
        battle.panelPlaces[2 * panel] = spots[panel].x + kPanelOffsets[panel].x;
        battle.panelPlaces[2 * panel + 1] = spots[panel].y + kPanelOffsets[panel].y;
    }

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        BattleRole& unit = this->battleRoles[role];
        const auto member = static_cast<std::size_t>(battle.partyRoles[role]);
        static_cast<BattleUnit&>(unit).Reset(services);

        if (battle.mirrored == 1)
        {
            unit.SetFacing(1);
            battle.rolePlaces[role].x = static_cast<i16>(Display::Width - battle.rolePlaces[role].x);
            battle.panelPlaces[2 * role] = kPanelMirror - battle.panelPlaces[2 * role];
        }

        unit.Setup(this->party.roles[member], this->party.battleBlocks[member], battle.rolePlaces[role], this->errors);

        if (unit.IsDefeated())
        {
            ++battle.defeatedRoles;
        }
    }

    SortItems(this->party.inventory);
    SortSkills(this->party.skills);

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        BattleRole& unit = this->battleRoles[role];
        const auto member = static_cast<std::size_t>(battle.partyRoles[role]);
        const auto slots = std::span<const std::unique_ptr<ItemNode>>(this->party.equipment)
                               .subspan(member * Party::SlotsPerRole, Party::SlotsPerRole);
        unit.ApplyEquipment(slots, this->party.fame.lists[Fame::ProficiencyList], this->errors);
        unit.SetSkills(this->party.skills[member].get());
        unit.SetInventory(&this->party.inventory);
        unit.SetDisplayName(battle.roleNames[member]);
    }

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        BattleRole& unit = this->battleRoles[role];
        i32 hitPoints = 0;
        i32 maximum = 0;
        unit.GetHitPoints(hitPoints, maximum);
        battle.hitPointBars[role] = Bar(hitPoints, maximum);
        battle.hitPointBarsShown[role] = battle.hitPointBars[role];
        battle.hitPoints[role] = hitPoints;
        u16 spiritMaximum = 0;
        u16 spirit = 0;
        unit.GetSpirit(spiritMaximum, spirit);
        battle.spiritBars[role] = Bar(static_cast<i16>(spirit), static_cast<i16>(spiritMaximum));
        battle.spiritBarsShown[role] = battle.spiritBars[role];
        battle.spirit[role] = static_cast<i16>(spirit);
        u16 staminaMaximum = 0;
        u16 stamina = 0;
        unit.GetStamina(staminaMaximum, stamina);
        battle.staminaBars[role] = Bar(static_cast<i16>(stamina), static_cast<i16>(staminaMaximum));
        battle.staminaBarsShown[role] = battle.staminaBars[role];
        battle.stamina[role] = static_cast<i16>(stamina);
        const i32 block = unit.roleBlock != nullptr ? unit.roleBlock[0] : 0;
        battle.blockValues[role] = block;
        battle.blockValuesShown[role] = block;
    }

    if (battle.defeatedRoles >= static_cast<i32>(battle.roleCount))
    {
        battle.startPoseRole |= BattleState::StartPoseDone;
    }
    else if (battle.roleCount == 1)
    {
        battle.startPoseRole = 0;
    }
    else
    {
        do
        {
            battle.startPoseRole = static_cast<u8>(this->random.Below(battle.roleCount));
        } while (this->battleRoles[battle.startPoseRole].IsDefeated());
    }

    battle.unk5770D4 = 0;

    if (state.GameFlag(GameState::Unk5A))
    {
        this->battleRoles[0].SelectEntry(1);
        TalismanGhost& ghost = this->talismanGhost;
        ghost.unk10C = kGhostUnk10C;
        ghost.unk108 = this->party.variables[Party::GhostVariable] + kGhostActBase;
        UnitPlacement& place = battle.rolePlaces[battle.roleCount];
        place.x = kGhostX;
        place.y = kGhostY;
        BattleRole& unit = this->battleRoles[battle.roleCount];
        static_cast<BattleUnit&>(unit).Reset(services);
        unit.SetupGhost(place, ghost, services);
        battle.ghostOrderRow = unit.GhostOrder();
        battle.ghostSlot = static_cast<i16>(battle.roleCount);
        battle.unk5770D4 = 1;
        ++battle.roleCount;

        if (this->battleRoles[0].IsDefeated())
        {
            battle.unk577370 = 1;
            unit.SetStatus(kGhostAloneStatus);
        }
    }

    this->MeasureUnits();
    battle.SortDrawOrder();
    battle.ListEnemyOrder();
    const u32 headStart = this->random.Below(kHeadStartRoll);

    for (u32 enemy = 0; enemy < battle.enemyCount; ++enemy)
    {
        if (headStart >= kHeadStartFrom)
        {
            const u32 steps = this->random.Below(kHeadStartRoll);

            for (u32 step = 0; step < steps; ++step)
            {
                this->battleEnemies[enemy].AdvanceGauge(services, 0);
            }
        }
        else if (enemy < BattleState::RoleUnits)
        {
            // The original randomizes the role of the enemy's place (past the roles: the memory after them; not here).
            this->battleRoles[enemy].RandomizeGauge(services, 0);
        }
    }

    for (u32 role = 0; role < battle.roleCount; ++role)
    {
        this->battleRoles[role].RandomizeGauge(services, role == battle.startPoseRole ? 1 : 0);
    }

    const i32 fighting = static_cast<i32>(battle.roleCount) - battle.escortCount - battle.guestCount - battle.unk5770D4;

    if (battle.defeatedRoles >= fighting)
    {
        battle.result = kNoParty;
    }
}

// The places of the party by its make-up (inline in 0x00459BD0): one role at kSolePlace; two or three roles by which
// of the story flags 0x1F..0x21 joins 0x1E, a party of three also ordering RoleCycle[1..4]. Other make-ups keep the
// places they had.
void Game::PlaceParty()
{
    BattleState& battle = this->battleState;
    GameState& state = this->state;
    auto& places = battle.rolePlaces;

    auto in = [&state](int role)
    {
        return state.GameFlag(GameState::InParty + role);
    };

    auto place = [&places](std::size_t role, Spot spot)
    {
        places[role].x = spot.x;
        places[role].y = spot.y;
    };

    if (battle.roleCount == 1)
    {
        place(0, kSolePlace);
        return;
    }

    if (battle.roleCount == 2)
    {
        for (const PartyMakeUp& makeUp : kPairs)
        {
            if (in(0) && in(makeUp.second))
            {
                place(0, makeUp.spots[0]);
                place(1, makeUp.spots[1]);
            }
        }
    }

    if (battle.roleCount == kThreeRoles)
    {
        for (const TrioMakeUp& makeUp : kTrios)
        {
            if (in(0) && in(makeUp.second) && in(makeUp.third))
            {
                place(0, makeUp.spots[0]);
                place(1, makeUp.spots[1]);
                place(2, makeUp.spots[2]);
                std::copy(makeUp.cycle.begin(), makeUp.cycle.end(), battle.roleCycle.begin() + 1);
            }
        }
    }
}

// swd3e.exe 0x0045B490: the rows of `incoming` spread over `outgoing` from the left, each by a random step a frame,
// shown on the primary surface until the rows have reached the right edge 0x1E0 times in all
void Game::SlideBattleIn(const Surface& incoming, const Surface& outgoing)
{
    std::unique_ptr<Surface> frame = this->graphics.CreateOffscreen(Display::Width, Display::Height);
    std::array<int, Display::Height> rowWidth = {};
    int arrived = 0;

    for (;;)
    {
        for (int row = 0; row < Display::Height; ++row)
        {
            std::copy_n(outgoing.Row(row), Display::Width, frame->Row(row));
        }

        for (int row = 0; row < Display::Height; ++row)
        {
            int& width = rowWidth[static_cast<std::size_t>(row)];
            width = kRowSpeedScale * static_cast<int>(this->random.Below(kRowSpeeds)) + kRowStep + width;

            if (width >= Display::Width)
            {
                width = Display::Width;
                ++arrived;
            }

            std::copy_n(incoming.Row(row), width, frame->Row(row));
        }

        Surface* primary = this->graphics.GetSurface(Graphics::SurfaceId::Primary);

        if (primary != nullptr)
        {
            for (int row = 0; row < Display::Height; ++row)
            {
                std::copy_n(frame->Row(row), Display::Width, primary->Row(row));
            }

            this->graphics.Present();
        }

        if (arrived > Display::Height)
        {
            break;
        }
    }
}
}  // namespace swd3e
