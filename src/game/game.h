#pragma once

#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "audio/audio_manager.h"
#include "battle/battle_enemy.h"
#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_role.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/fight_talk.h"
#include "battle/smooth/smooth_stage.h"
#include "config/config.h"
#include "core/types.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "draw/weave.h"
#include "font/text_font.h"
#include "game/databases.h"
#include "game/game_state.h"
#include "game/notices.h"
#include "game/scene.h"
#include "gfx/graphics.h"
#include "ghost/ghost_follower.h"
#include "ghost/talisman_ghost.h"
#include "input/controls.h"
#include "input/input.h"
#include "launcher/launcher.h"
#include "map/field.h"
#include "map/field_view.h"
#include "map/map_cache.h"
#include "menu/menu.h"
#include "minigame/ascent_game.h"
#include "role/role_control.h"
#include "role/role_path.h"
#include "talk/talk.h"

#include "movie/movie.h"
#include "org/database.h"
#include "party/party.h"
#include "pic/rle.h"
#include "platform/audio_device.h"
#include "platform/display.h"
#include "platform/exe_resources.h"
#include "platform/glyph_rasterizer.h"
#include "platform/host_settings.h"
#include "platform/input_device.h"
#include "platform/message_box.h"
#include "platform/surface.h"
#include "platform/system_info.h"
#include "platform/system_sound.h"
#include "random/random.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"
#include "role/path_finder.h"
#include "save/save_slots.h"
#include "talk/text_colors.h"
#include "text/big5.h"
#include "text/text_input.h"
#include "ui/act_draw.h"

namespace swd3e::smooth
{

class HitFrame;
class TurnFrame;
class DamageFrame;
class BattleFrame;
class TalkFrame;

}  // namespace swd3e::smooth

namespace swd3e
{

// The root object: the window procedure and message loop of WinMain and the subsystems the original keeps in global
// objects. Built step by step along the start of the game.
class Game
{
    friend class CheatDialog;
    friend class Menu;
    friend class FieldView;
    friend class Talk;
    friend class RoleControl;
    friend class AscentGame;
    friend class RolePath;
    friend class smooth::HitFrame;
    friend class smooth::TurnFrame;
    friend class smooth::DamageFrame;
    friend class smooth::BattleFrame;
    friend class smooth::TalkFrame;

public:
    static constexpr std::u8string_view WindowTitle = u8"軒轅劍參 外傳";                   // swd3e 0x004B0BAC
    static constexpr std::u8string_view Title = u8"軒轅劍三";                              // swd3e 0x004B1F98
    static constexpr std::u8string_view DirectInputFailed = u8"DirectInput 初始化錯誤%n";  // swd3e 0x004B1F7C
    static constexpr u32 StartSound = 0x78;
    static constexpr u32 WindowIcon = 0x7D;       // the icon group of the window class (0x0040A140)
    static constexpr i32 FirstFrameTime = 0x400;  // the initial value of frameTime (0x004AF7A4)
    static constexpr u32 WindowCursor = 0x67;     // the cursor group (0x0040A140)
    static constexpr int FrameInterval = 0x23;
    static constexpr int AutoRenderScale = 0;  // renderScaleSetting: the largest whole scale the window shows
    static constexpr int MaxRenderScale = MaxWeaveScale;  // DisplaySettings::MaximumRenderScale
    // 0x004B0C20: the signed speeds of the nibbles of a map setting (8 and above count down from -7).
    static constexpr std::array<i32, 16> SpeedNibbles = {0, 1, 2, 3, 4, 5, 6, 7, 8, -7, -6, -5, -4, -3, -2, -1};

    // `menuState`: the menu shown (0: none), bit 31 while it is being entered.
    enum MenuNumber : u32
    {
        NoMenu = 0,
        MainMenu = 1,
        ItemChoiceMenu = 2,            // the items listed by talk command ItemMenu
        TitleScreenMenu = 3,           // the system page in mode 0
        SystemMenuA = 4,               // the system page in mode 1 (talk command EndGame)
        SystemMenuB = 5,               // the system page in mode 2
        NameEntryMenu = 6,             // the system page in mode 3 (swd3e: load at once; no setter known)
        MenuOnEquipment = 0x40000000,  // the main menu opens on its equipment page (the shop)
        MenuActsReset = 0x20000000,    // the shop resets its acts (ShopFrame)
        MenuEntering = 0x80000000,
        MenuNumberMask = 0x0FFFFFFF,
        MenuEnteredMask = 0x3FFFFFFF,
    };

    static constexpr u32 TitleMenu = MenuEntering | TitleScreenMenu;

    // The bits of `systemFlags`.
    enum SystemFlag : u32
    {
        UnkSystem1 = 1,       // the map frame is skipped (no setter is known)
        MovieOpened = 2,      // a map movie plays (talk command MapMovie)
        QuitRequested = 4,    // the game ends after the frame
        DiscWait = 0x10,      // swd3e: the frame is skipped while the disc check waits (0x004121C0, removed: never set)
        VideoPlaying = 0x20,  // a full-screen video plays
        // swd3e: the name boxes of talk command 209 are open (set by it in 0x0042BAF0, cleared by 0x004172A0 when the
        // name is taken or given up); the controlled role is not driven (0x00402AA0)
        NameBoxesOpen = 0x40,
        AscentRunning = 0x80,  // swd3e: the ascent game runs instead of the frame (0x0040A7B0)
    };

    // The name boxes (swd3e): the surname, the given name and the nickname of a RoleName.
    enum NameBox : std::size_t
    {
        SurnameBox = 0,
        GivenBox = 1,
        NicknameBox = 2,
        NameBoxCount = 3,
    };

    // Where a name box is typed and the bytes it takes (talk command 209 and 0x00451B60 make them alike).
    struct NameBoxPlace
    {
        int x;
        int y;
        int capacity;
    };

    static constexpr std::array<NameBoxPlace, NameBoxCount> NameBoxPlaces = {{
        {0x122, 0xEE, 4},
        {0x17A, 0xEE, 4},
        {0x14A, 0x118, 8},
    }};

    // `storyState`: 0 while the story runs, SaveSlotsState while the save slots are shown.
    static constexpr i32 StoryRunning = 0;
    static constexpr i32 SaveSlotsState = 3;
    static constexpr std::u8string_view UnknownPlace = u8"不知道";  // 0x004B0EF0

    explicit Game(std::u8string installDirectory);

    int Run(std::u8string_view commandLine);

    HostSettings host;               // the rebuild's swd3er.toml (set before Run)
    HangWatch* hangWatch = nullptr;  // told of every round of the main loop (set before Run)

    // The window as left at the end of Run (written back to swd3er.toml).
    Display::WindowState WindowState() const
    {
        return this->display.State();
    }

private:
    void Start();
    Launcher::Choice RunLauncher();
    void InitInstall();
    void ApplyHostPaths();
    void SizeCaches();
    void MakeDirectory(std::u8string_view name);
    void StartGraphics();
    void InitDrawKernel();
    bool InitDevices();
    void SetupFont(TextFont& font, int size);
    static u32 DiskFreeMegabytes();
    void ResetBattle();
    void NewGame();
    void RefreshCursor();
    void StepPointer();
    void DrawPointer(int x, int y);
    void ShowFrame();
    // The rebuild's smooth motion (deviations.md): a map frame kept for the frames drawn until the next game frame
    // (shown at once without smooth motion), and one such frame drawn and shown when it is due.
    void ShowMapFrame();
    // The same for a frame of the main menu or the shop (Menu::PaintBetween).
    void ShowMenuFrame();
    bool PresentBetween();
    // The rebuild's continuous motion (deviations.md) up to now, at most to the due time of the next game frame.
    void AdvanceMotion();
    bool CopyToPrimary();
    // The rebuild's map filling the window (deviations.md): the map frame is drawn to mapCanvas, the size the
    // window asks for (the 640x480 screen with wide_map off): its world over the whole of it (BeginMapCanvas), its
    // interface on the 640x480 screen in its middle (DrawOnMapScreen); EndMapCanvas copies that screen to the
    // offscreen surface (what reads the frame after it sees the screen as before) and draws there again.
    void BeginMapCanvas();
    void DrawOnMapCanvas();
    void DrawOnMapScreen();
    void DrawOnOffscreen();
    void EndMapCanvas();
    // The canvas and font targets of `width` x `height` (changed only when they differ).
    void SetDrawSize(int width, int height);
    // The camera's view the size of `layout`, keeping where the 640x480 screen lies on the map.
    void SetMapView(const ScreenLayout& layout);
    // The window shows the frames presented next: the map canvas, a 640x480 frame over the last map canvas (the
    // menus and the shop over the map), or the 640x480 screen alone.
    void ShowLayout(bool map);
    // The display setup of the system tab (deviations.md): smoothMotion, vsync, frameRate and renderScaleSetting put
    // to use, and written to [display] of the program's settings file.
    void ApplyDisplay();
    void StoreDisplay() const;
    std::string MusicName(int index) const;
    std::u8string MusicPath(int index) const;
    void ResetSaveSlots();
    void StartStory(int withMap);
    void LoadFameFile();
    void SaveFameFile();
    void SaveScreenFrame();
    void SaveScreenKeys();
    void FlagViewKeys();
    void LeaveSaveScreen();
    void FreeSavePreviews();
    void ReloadSavePage();
    void DrawSaveScreen();
    void DrawFlagView();

    // What the slots page does after the mouse and the keys (SaveSlotsFrame).
    enum class SlotsStep
    {
        Keys,    // the keys are read, then Escape
        Chosen,  // a slot is taken
        Leave,   // the screen is left
        Done,    // nothing more this frame
    };

    void SaveSlotsFrame();
    SlotsStep SlotsMouse();
    SlotsStep TakeSlot();
    SlotsStep SlotsKeys();
    void SaveGame();
    void WriteSave(File& file);
    std::vector<u8> PackParty() const;
    void LoadGame();
    void ReadSave(File& file);
    void UnpackParty(std::span<const u8> data);
    void DrawSlotsPage();
    void ShopFrame();
    // The page of the shop drawn over the grey map (part of 0x0044EA60), also painted again between frames.
    void DrawShop();
    void OpenShop();
    void ResetShopActs();
    void CloseShop();
    void ShopBack();
    void ShopRowChanged(bool sound, u16 arrows);
    void ShopDown();
    void ShopUp();
    void ShopPageDown();
    void ShopPageUp();
    ItemNode* ShopChosenItem();
    void ShopLess();
    void ShopMore();
    void ShopConfirm();
    void ShopOpenList();
    void ShopSettle();
    void ShopAnswered();
    void ShopKeys();
    void ShopMouse();
    void ShopListMouse();
    void CompareEquipment(const ItemNode* item);
    void DrawEquipmentCompare();
    void DrawShopList(Pixel color);
    void DrawShopChosen(const ItemNode& item, Pixel color);
    void DrawWrappedText(std::string_view text, int x, int y, int lines, int width);
    void DrawTextInput(const TextInput& box, bool current, bool withPicture);
    bool NameBoxFrame();
    void MainFrame();
    void EndMainFrame();
    void RunTimers();
    void EnterBattle();
    void BattleMainFrame();
    bool MusicCue();
    void AudioPump();
    void HandleEvent(const SDL_Event& event);
    void LoopFrame();
    static bool SDLCALL LiveResize(void* data, SDL_Event* event);
    void TakeScreenshot();
    std::u8string ScreenshotPath();
    void SendText(TextMessage message, u32 parameter);
    void ShowLoading(int percent);
    void MusicZone(int map);
    bool LoadRoleSound(Role& role);
    void BuildTriggers(MapTrigger& head);
    void InitRole(Role& role);
    bool MapName(int map, std::string& name) const;
    static constexpr i16 KeepValue = -1;  // an UpdateRoleRecord argument that leaves its field
    // swd3e: UpdateRoleRecord's `variables` (the role variables word) left as it is
    static constexpr i16 KeepVariables = static_cast<i16>(0x8080);
    static constexpr u32 ClockFrames = 0x14;  // frames between the ticks of Party::clock, less one
    bool UpdateRoleRecord(
        u16 id,
        i16 act,
        i16 action,
        i16 direction,
        i16 x,
        i16 y,
        i16 event,
        i16 talk,
        i16 setBits,
        i16 clearBits,
        i16 map,
        i16 variables = Game::KeepVariables
    );
    void SaveRoleStates(i16 x, i16 y);
    void ReleaseRole(int index);
    void StoreRole(const Role& role);
    void FollowLeader(int index);
    static constexpr int SaveRoles = 1;  // LoadCity option: the roles of the map left are written back
    void LoadCity(int map, int x, int y, i16 act, i16 action, i16 direction, int options);
    void Destroy();
    void DrawPauseBanner();
    static int CheckStart();
    void Activate(int state);
    void FreeItemLists();
    void ReleaseBattle();
    void ReleaseGame();
    void PlaySoundAt(u16 sound, int x, int y);
    u32 RollEncounter(int steps, int force);
    u32 LoadRoleRecord(Role& role, u16 id);
    void LeaveParty(u16 id, u16 path, u16 event, i32 act, i32 action, i32 direction, u16 flags);
    bool ChangeMap(int map, int x, int y, int act, int action, int direction, int options);
    void StartTimer(int minutes, int seconds, int event, int which);
    bool PlayVideo(std::size_t& cursor, u16& pc);
    int VideoPosition() const;
    bool SetVideoMarks(i32 first, i32 second);
    void StopVideo();
    void VideoFrame();
    void StartBattle(u16 battle);
    void PlaceParty();
    void LoadBattleBackground(int background, u16 scene);
    void SlideBattleIn(const Surface& incoming, const Surface& outgoing);
    void ShakeBattleBackground(int shake);
    void DrawBattleBackground(int shake);
    void DarkenByTint();
    void SpreadPoison(int enemies);
    int YinYangAttack(int role);
    u32 PickRoleTarget(i16 criterion);
    void RaiseLevel(u16 role, u16 level);
    void ApplyDrain(i32& amount, BattleUnit& attacker, BattleUnit& target);
    void ApplyDrainSums(BattleUnit& unit);
    int SlideUnits(int unit, i16 x, i16 target);
    void CureHeldBy(int enemy);
    void TryRetreat(int role);
    int DrainSpellFrame(BattleRole& role, BattleUnit& target);
    int MagicAttackFrame(BattleRole& role, BattleUnit& target);
    int EnemyAttackFrame(BattleUnit& enemy, BattleUnit& target);
    int EnemySkillFrame(BattleUnit& enemy, BattleUnit& target);
    void RandomRoleAct(int unit);
    void RetargetRoles(int role, int enemy);
    int RemoveDefeated(int index, int role);
    int PlayHitAct(BattleUnit& unit, u32 act, int index);
    int FadeHitAct(BattleUnit& unit, u32 act, i32 action, int index);
    int WalkLine(i32& x, i32& y, int fromX, int fromY, int toX, int toY, u32 speed, int delay, int index);
    void SortActingDrawOrder();
    int PushUnits(u16 picture, int restart);
    void ShiftUnits(int shift);
    int FadeFieldOut();
    void ReturnBattleItems();
    void CheckBattleEnd();
    void DrawBattlePrompt();
    void QuakeFrame();
    void FieldFadeFrame(int counting);
    int CheckStoneSides();
    void TakeSlotRecord(BattleSlotRecord& taken);
    int DamageNumbersFrame(int index, int role);
    void ApplyHits();
    void NextEnemyTurn();
    void NextRoleTurn();
    void ScrollListBack();
    void ScrollListForward();
    void AutoBattleTurn();
    bool CanSelectRole(int unit);
    static std::size_t PanelPlace(int unit);
    int NextSelectableRole(int unit);
    int PreviousSelectableRole(int unit);
    void SelectRoleUnit(int unit);
    void SelectPreviousRole();
    void SelectNextRole();
    void MenuRight();
    void MenuLeft();
    void BuildCommandMenu();
    void DrawCommandMenu(int x, int y, int cursor);
    void AnnounceMastery();
    void MenuCancel();
    void DrawListTabs(int x, int y);
    void DrawThrowList(int x, int y, int cursor, int top);
    void DrawRefineList(int x, int y, int cursor);
    void DrawSpecialList(int x, int y, int cursor);
    void DrawEscortList(int x, int y);
    void DrawTargetTag(int x, int y);
    void DrawControlMenu(int x, int y, int cursor);
    void DrawSpellList(int x, int y, int cursor, int top);
    void DrawItemList(int x, int y, int cursor, int top);
    int DrawObjectStats(int x, int y);
    int DrawRefineStats(int x, int y);
    int DrawChargePanel(int x, int y, int cursor);
    void DrawEscortFusionList(int x, int y);
    void DrawTitledItemList(int x, int y, int cursor, int top, int category, std::u8string_view title);
    void DrawVictoryPanel();
    void DrawLevelUpPanel();
    void CheckLevelUps();
    void DrawNewSpell();
    void DrawNewSpecial();
    void DrawLearnedBox(std::u8string_view line, bool sound);
    void LearnSpecials();
    void DrawLevelUpStats();
    void MenuUp();
    static void StepWrapped(i32& place, int step, i32 count);
    void AimStep(int step);
    void AimEnemyStep(int step, UnitRect& rect);
    void AimGhostStep(int step);
    void MenuDown();
    int BattleDebugKeys();
    void DrawBattleDebug();
    int MouseHover();
    int HoverRefineCell();
    int HoverTarget();
    int HoverGhostTarget();
    void MenuConfirm();
    bool PrepareOrder();
    void AimAtParty();
    void OpenTargets();
    void EndChoice();
    void ConfirmCommand();
    void ConfirmSpell();
    void ConfirmTarget();
    void ConfirmItem();
    void CallEscort(BattleRole& role);
    void ConfirmEscort();
    void ConfirmTalk();
    void ConfirmSpecial();
    void ConfirmThrow();
    void ConfirmRefine();
    void ConfirmGhostTarget();
    void ConfirmFusePlace();
    void ConfirmFuse();
    void ConfirmBait();
    void ConfirmEnd();
    void ConfirmKey();
    static bool KeyRepeats(const ButtonState& button);
    void BattleKeys();
    void DrawBattleMenu();
    void DrawCommandPanel(i32 nameIndex);
    void DrawAimPointers();
    void DrawGhostAimPointer();
    void BattleEndFrame();
    void EndCheckFrame();
    int HitTargetFrame(BattleUnit& target, BattleUnit& attacker, u32 act, int index);
    void RippleHitFrame(BattleUnit& target, BattleUnit& attacker, int index);
    void DrawHitPicture(BattleUnit& target, BattleUnit& attacker, u32 act, int index);
    void TakeHitBits(BattleUnit& target, BattleUnit& attacker, int index);
    int HitAllFrame(BattleUnit& target, BattleUnit& attacker, u32 act, int index, int all);
    void PlaceAllHit(BattleUnit& attacker, int width, int index, u16& delay);
    void NudgeAllHit(BattleUnit& target, BattleUnit& attacker, int all);
    void RaiseAllHit(BattleUnit& target, int all);
    void ShowAllHitDigits(BattleUnit& unit, i16 damage, u16& delay, i32& spirit, i32& stamina);
    void StrikeAllHit(BattleUnit& target, BattleUnit& attacker, int index, int all, u16& delay);
    int HitFrame();
    void FlashFrameWhite();
    void ClearHitDamage();
    void ClearHitTotals();
    void StepSweep(u32 units);
    void DrainToSpirit(BattleRole& role, i32& damage);
    int RoleHitFrame(int role);
    bool SweepFromRole(BattleRole& attacker, i16 target, u32 act);
    int EnemyHitFrame(int enemy);
    bool SweepFromEnemy(BattleEnemy& attacker, i16 target, u32 act);
    void EndEnemyDrains(BattleEnemy& attacker);
    int EnemyActionFrame(int enemy, int target);
    int EnemyAttackAction(int enemy, BattleEnemy& unit, int target);
    int EnemySpellAction(int enemy, BattleEnemy& unit, int target);
    int CallEnemies(BattleEnemy& unit);
    int EnemyFrame(int enemy);
    void EnemyAimSide(BattleEnemy& unit);
    bool EnemyMayAim(const BattleRole& role) const;
    bool EnemyPickTarget(BattleEnemy& unit, int enemy);
    bool AimedDown(i32 index) const;
    void EnemyChooseCommand(BattleEnemy& unit, int enemy);
    void EnemyAimHeal(BattleEnemy& unit, int enemy);
    void EnemyActFrame(BattleEnemy& unit, int enemy);
    void ReturnHeldEscort(BattleRole& role, std::size_t index, bool dropCommand);
    void EnemyAfterAction(BattleEnemy& unit, int enemy);
    int RoleActionFrame(int role, int target);
    BattleUnit& AimedUnit(int target);
    void AddSpecialPoints(int role, i32 points);
    void AddSpellPoints(int role, i32 points);
    void RoleHitLanded(int target);
    void RoleDefeated(int target);
    void LowerBattleMusic();
    void RestoreBattleMusic();
    void DarkenForCommand();
    void EndCommandDarkening();
    int RoleSpecialFrame(BattleRole& unit, int role, int target, u16 command);
    int ComboFrame(BattleRole& unit, int role, int target);
    int RoleCommandFrame(BattleRole& unit, int role, int target, u16 command);
    CounterList& ProficiencyList();
    int RoleAttackFrame(BattleRole& unit, int target);
    void StartRoleCast(BattleRole& unit);
    void DefeatAllByCast(BattleRole& unit);
    int RoleSpellFrame(BattleRole& unit, int role, int target);
    int RoleItemFrame(BattleRole& unit, int role, int target);
    int AbsorbFrame(BattleRole& unit, int target, bool sealed);
    int ThrowTalismanFrame(BattleRole& unit, int role, int target);
    int SummonEscortFrame(BattleRole& unit, int role);
    int RetreatCommandFrame(BattleRole& unit, int role);
    int CoinTossFrame(int role);
    int StealFrame(BattleRole& unit, int target);
    int PoseSweepFrame(BattleRole& unit);
    int ControlFrame(BattleRole& unit, int role, int target);
    int TimingAttackFrame(BattleRole& unit, int target);
    int AccessoryBuffFrame(BattleRole& unit, int target, int command);
    int TimerAttackFrame(BattleRole& unit, int target);
    int SelfHitFrame(BattleRole& unit, int command);
    int RouletteFrame(BattleRole& unit, int target);
    int MeterAttackFrame(BattleRole& unit, int target);
    int RoleFrame(int role);
    void RoleGaugeFull(BattleRole& unit, int role);
    void RoleTakeTurn(BattleRole& unit, int role);
    void RoleAutoCommand(BattleRole& unit, int role);
    void RoleOrderFrame(BattleRole& unit, int role);
    void RoleGatherSweep(BattleRole& unit, PendingOrder& order);
    void RoleTakeOrder(BattleRole& unit, int role);
    void RoleActFrame(BattleRole& unit, int role);
    void RoleAfterAction(BattleRole& unit, int role);
    void RoleRetreatFrame(BattleRole& unit, int role);
    void UpdateUnits();
    int BattleFrame();
    void EndBattle();
    void DrawIntroField();
    std::vector<u8> PackSurface(Surface& surface);
    void TargetFrame();
    void ClearFrame();
    void BattleIntro(u16 style);
    void StartBattleMusic();
    int FightTalkFrame();
    void FightTalkEnd();
    int FightTalkWait();
    int FightTalkWindow();
    int FightTalkMove();
    int FightTalkPause();
    int FightTalkHitPoints();
    int FightTalkOrder();
    BattleUnit& TalkUnit(i32 unit);
    u16 TalkCommandUnit() const;
    void SetTalkCommandUnit(u16 unit);
    int TalkCommandStarted();
    int FightTalkDefeat();
    int FightTalkCommand(int command);
    int FightTalkWin();
    int FightTalkLift();
    int FightTalkAllDone();
    int FightTalkMarks(bool set);
    int FightTalkRepeat();
    int FightTalkRandomJump();
    int FightTalkShift();
    int FightTalkSkill();
    void TalkStandingRole(i32& unit);
    int FightTalkNextBattle();
    int FightTalkLoot();
    int FightTalkUnits(u16 command);
    int FightTalkChoice();
    int FightTalkLines();
    int FightTalkLinesMode();
    void ShiftTalkUnits();
    int FightTalkCurve();
    int FightTalkSlide();
    int FightTalkSpeaker();
    int FightTalkTimedWindow();
    void ClearTalkMarks();
    void SkipTalkText();
    int FightTalkMirror();
    int FightTalkEnemyCommand();
    int FightTalkLineWindow();
    int FightTalkFieldFade();
    int FightTalkObjects(bool take);
    int FightTalkSlideTo();
    int FightTalkCatch();
    int FightTalkDamage();
    int FightTalkMisc(u16 command);
    int FightTalkIntro();
    int FightTalkChanceJump();
    void FightTalkEnemyFollows(i16 enemy);
    int FightTalkAddLoot();
    int FightTalkMusic();
    int FightTalkCounters();
    void DrawRolePanels();
    void MeasureUnits();

    DialogErrorReporter errors;
    Display display;
    Surface screen;
    WindowInputDevice inputDevice;
    Input input;
    Graphics graphics;
    Painter painter;
    RleCodec codec;
    TswArchive archive;
    ActArchive acts;
    GlyphLibrary glyphs;
    LibraryGlyphRasterizer rasterizer;
    TextFont largeFont;   // 0x004C5130 (20 pixels)
    TextFont mediumFont;  // 0x004E5830 (16 pixels)
    TextFont smallFont;   // 0x004C3658 (12 pixels)
    AudioDevice audioDevice;
    AudioManager audio;
    Movie movie;
    bool videoLoaded = false;  // 0x00578624 is set: a movie object exists (from PlayVideo to StopVideo)
    // 0x0057861C the primary surface's pitch PlayVideo takes for Bink's copy; the platform copy uses the target
    // surface's own pitch (the same value)
    i32 videoPitch = 0;
    i32 ghostFrames = 0;  // 0x004E4C1C frames since the talisman ghost last ticked (MainFrame)
    i32 videoMarkA = 0;   // 0x004C30E0 SetVideoMarks
    i32 videoMarkB = 0;   // 0x004C30E8
    Random random;
    crt::Random crtRandom;
    GameState state;
    Config config;
    Controls controls;
    MapCache mapCache;
    Field field;
    FieldView fieldView;
    Talk talk;
    RoleControl roleControl;
    RolePath rolePath;
    AscentGame ascentGame;
    GhostFollower ghostFollower;
    TalismanGhost talismanGhost;  // 0x004D0C10 the talisman ghost carried

    GameDatabases databases;
    PathNodePool pathNodes;
    TextColors textColors;
    Database database;
    Party party;
    Scene scene;
    BattleState battleState;
    BattleHud battleHud;
    BattleMessages battleMessages;
    FightTalk fightTalk;
    BattleServices battleServices;
    smooth::Stage battleStage;                                    // the rebuild's smooth battle
    std::array<BattleRole, BattleState::RoleUnits> battleRoles;   // 0x005136E0
    std::array<BattleEnemy, BattleState::Enemies> battleEnemies;  // 0x00539590
    BattleEnemy talkStrayUnit;  // the rebuild: the stand-in of a talk unit number past the tables (deviations.md)
    // the rebuild: the inventory nodes battle talk command 0x42 unlinked, kept as the original leaks them (pointers
    // such as BattleRole::selectedItem may still read them; deviations.md)
    std::vector<std::unique_ptr<ItemNode>> detachedItems;
    ActDrawer actDrawer;
    Notices notices;
    i32 scrollStepX = 0;        // 0x004D0E80 the step of the camera and the controlled role across (-1, 0, 1)
    i32 scrollStepY = 0;        // 0x004D1288 and down
    i32 walkStepX = 0;          // 0x004C2B60 the step of the controlled role alone across
    i32 walkStepY = 0;          // 0x004C2B64 and down
    i32 lastPointerAction = 0;  // 0x004E6D48
    Menu menu;
    SaveSlots saveSlots;
    // The boxes a name is typed in (swd3e; talk command 209 and the new game's name page), by NameBox: 0x004D0E04,
    // 0x004D3C74 and 0x004E6B54
    std::array<std::unique_ptr<TextInput>, NameBoxCount> nameBoxes;
    TextInput* currentNameBox = nullptr;  // 0x004C6620 the box the keyboard goes to (with flag 0x50)
    u32 nameRole = 0;                     // 0x004E6CD0 the role talk command 209 names
    ActState saveScreenAct;               // 0x004C2700 reset by ResetSaveSlots (0x00406B50); no code reads it
    i32 flagCursor = 0;                   // 0x004C27A8 the flag chosen in the flag view
    u32 saveFrames = 0;                   // 0x004C27B0 frames since the save screen opened
    u16 slotsPageFrames = 0;              // 0x004C27B6 counted 0..13 by DrawSlotsPage (0x00408CC0); no code reads it
    u8 downArrowPress = 0;                // 0x004C2650 frames the lit down arrow of the slots stays pressed down
    u8 upArrowPress = 0;                  // 0x004C2651 and the up arrow up
    i32 slotPrompt = 0;                   // 0x004C27A4 browsing, the overwrite answer or the slot chosen
    i32 slotMouseBlock = 0;               // 0x004C1610 frames a key or click keeps the pointer from choosing a slot
    u32 systemFlags = 0;                  // 0x004D1404 SystemFlag bits
    i32 counterAlpha = 0;                 // 0x004E582C the alpha the counters (gold, digits) draw with
    i32 unk4C2B58 = 0;                    // 0x004C2B58 only cleared by the system page (no reader)
    static constexpr u32 ScriptedBattle = 0x80000000;  // in `battleRequest`: started by a talk command
    i32 battleRequest = 0;                             // 0x004C2B84 a battle to start (its number in the low word)
    i32 musicBeforeBattle = 0;   // 0x004C15D0 the music queue's mode when a map battle starts (0x0040A7B0 restores it)
    i32 inBattle = 0;            // 0x004E6CF8 a battle from the map runs
    std::vector<u16> shopGoods;  // 0x004D1290 the items of talk command 0x85 for the shop, ended by 0
    i32 currentMap = 0;          // 0x004C3190 the number of the map shown
    i32 playClockStart = 0;      // swd32 0x004AB990 the time() the play time was last counted from
    i32 playSeconds = 0;         // swd32 0x004ACEF4 the play time saved, in seconds
    i32 partyLeaderId =
        0;  // swd32 0x004B7BC4 the role id leading the party on the map (its cells marked as the leader's)
    TextEntryState textEntry;
    i32 running = 0;    // 0x004E6D08 frames run (F8 toggles)
    u32 lastFrame = 0;  // 0x004E6D0C
    // The rebuild's smooth motion: whether the last game frame showed the map, the game frames run so far, when the
    // last game frame was due (SDL nanoseconds), the frame drawn between game frames and when the next one is due.
    bool smoothMotion = false;
    bool mapFrameShown = false;
    bool battleFrameShown = false;  // the last game frame showed a battle frame that can be drawn again
    // battleFrameShown of the game frame before: a frame whose battle talk runs no battle frame (BattleMainFrame)
    // shows that one on, as the original's screen stays.
    bool battleFrameKept = false;
    u32 battleFramesRun = 0;  // BattleFrame: the battle frames run so far
    // The rebuild's smooth battle checked: with smooth motion off, the battle frame's draws gathered into one scene
    // and drawn once the frame is decided, where they are (off: each drawn at once, in the original's order).
    bool battleDeferred = false;
    bool menuFrameShown = false;  // the last game frame showed a frame of the main menu or the shop
    u32 gameFrames = 0;
    u32 framesBetween = 0;
    u64 paintBetweenNs = 0;  // the time the frames between took to paint, in all and the longest
    u64 paintBetweenMostNs = 0;
    i32 backgroundShifted = 0;  // the rebuild's smooth battle: the background's shifts so far (where it stands)
    u64 frameDueNs = 0;
    u64 frameRatePeriodNs = 0;
    u64 nextDrawNs = 0;
    std::unique_ptr<Surface> smoothScreen;  // the frames between, at the render scale
    std::unique_ptr<Surface> menuScreen;    // the menu frames between
    std::unique_ptr<Surface> mapCanvas;     // the map filling the window (BeginMapCanvas)
    ScreenLayout mapLayout;                 // its size and where the 640x480 screen lies on it
    bool mapBehind = false;                 // the last map canvas lies behind the menus
    bool backdropTaken = false;             // the display shows the last map canvas behind the frames (ShowLayout)
    int drawWidth = Display::Width;         // SetDrawSize
    int drawHeight = Display::Height;
    int renderScaleSetting = 0;            // [display] render_scale (AutoRenderScale: by the window)
    bool wideMap = true;                   // [display] wide_map: the map fills the window
    bool vsync = true;                     // [display] vsync
    i64 frameRate = 0;                     // [display] frame_rate (0: no limit)
    u32 frameStart = 0;                    // 0x004C4654
    u32 mainFrameStart = 0;                // 0x004E4C2C frameStart when the main frame began
    u32 lastFrameStart = 0;                // 0x004E4C24 frameStart of the main frame before
    u32 mainFrameTime = 0;                 // 0x004E4C28 the time the last main frame took (debug line)
    i32 debugText = 0;                     // 0x004E6CE0 the debug lines (debug key F1)
    i32 debugCells = 0;                    // 0x004E6CE4 the debug cell marks (debug key F2)
    i32 frameTime = Game::FirstFrameTime;  // 0x004AF7A4 the milliseconds between the last two main frames
    i32 frameWallTime = 0;                 // 0x004C30D8 time(0) at each MainFrame (0x0040A7B0); no code reads it
    i32 frameUnkCleared = 0;               // 0x004E581C cleared by MainFrame (0x0040A7B0); no code reads it
    i32 counterWait = 0;     // 0x004E6B58 frames the controlled role stood still (the counter fades in after 0x64)
    i32 loadingPercent = 0;  // 0x004E6D14
    i32 loadingUnusedA = 0;  // 0x004E6D1C cleared when the loading screen starts; no code reads it
    i32 loadingRunner = 0;   // 0x004E6D2C the act running along the bar (1 or 2)
    i32 loadingRed = 0;      // 0x004E6D18
    i32 loadingGreen = 0;    // 0x004E6D10
    i32 loadingBlue = 0;     // 0x004E6D28
    i32 loadingUnusedB = 0;  // 0x004E6D20 cleared when the loading screen starts; no code reads it
    ActState loadingAct;     // 0x004D0D60
    ExeResources resources;
    SystemSound systemSound;
    std::unique_ptr<Surface> offscreen;   // 0x004C6344 the frame drawn by the game
    u16* drawSurface = nullptr;           // 0x004EDA44 the locked frame (the target of the painter and the fonts)
    i32 unknown14 = 0;                    // the dword of Env.dat at +0x14
    u8 effectsOff = 0;                    // Env.dat +0x1B
    u32 deviceFlags = 0;                  // 0x004C6334 bit 1: DirectInput failed
    i32 active = 0;                       // 0x004D1630 1 once the display runs
    i32 inactive = 0;                     // 0x004D3FC4
    i32 frameCounter = 0;                 // 0x004C2B78
    u32 menuState = 0;                    // 0x004D1C88 bit 31: entered, low bits the menu
    i32 menuMode = 0;                     // 0x0050A404
    i32 storyState = Game::StoryRunning;  // 0x004E6CF4
    i32 saveScreenPage = 0;  // 0x004E6D00 the page of the save screen: 0 the flag view (F1), 1 the slots (F2)
    i32 slotsLoad = 0;       // 0x004E6D04 the slots load (1) or save (0)
    bool quit = false;
    bool inLoopFrame = false;  // LoopFrame runs (the live-resize exposes come from inside it too)
};

}  // namespace swd3e
