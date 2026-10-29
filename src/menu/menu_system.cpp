#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "core/error_box.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "game/game.h"
#include "menu/menu.h"
#include "menu/menu_text.h"
#include "menu/settings_constants.h"
#include "platform/display.h"
#include "text/text_input.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

// The states of the system page.
constexpr i32 kEntries = 1;
constexpr i32 kChosen = 2;
constexpr i32 kSettings = 5;
constexpr i32 kSetupSettings = 6;  // the rebuild's setups, opened from the settings (deviations.md)
constexpr i32 kInGame = 100;
// Steps of Menu::systemRow while an entry is chosen.
constexpr i32 kFadeOut = 0x64;
constexpr i32 kNameEntry = 0x62;  // the name is typed
constexpr i32 kNameMouse = 0x61;  // from this step the mouse works the name page
constexpr i32 kInGameFade = -0x78;
constexpr i32 kTitleFade = -6;
constexpr i32 kHiddenGlow = -0x10;
constexpr i32 kEffectsOffFlag = GameState::PlainScreen;
constexpr i32 kNameEntryFlag = GameState::TextBox;
constexpr int kVolumeSound = 0x2E;
constexpr u32 kScreenPixels = Display::Pixels;
constexpr std::size_t kScreenBytes = sizeof(Pixel) * Display::Pixels;
constexpr int kTextStyle = TextFont::BoldShadow;

// The title entries, side by side: entry n at x 0x70 * (n + 1), only the chosen one drawn.
constexpr i32 kNewGameEntry = 0;
constexpr i32 kLoadEntry = 1;
constexpr i32 kSettingsEntry = 2;
constexpr i32 kQuitEntry = 3;
constexpr int kEntryStep = 0x70;
constexpr int kEntryY = 0x140;
constexpr u32 kEntryWidth = 0x50;  // an entry is clicked within this of its x, between kEntryTop and kEntryBottom
constexpr u32 kEntryTop = 0x140;
constexpr u32 kEntryBottom = 0x180;
constexpr i32 kChosenGlow = 1;
constexpr i32 kFadeInStep = 5;     // the screen fades in from black by this a frame
constexpr i32 kFadeOutEnd = 0x68;  // a chosen entry fades the screen out until this step
constexpr i32 kFadeOutStep = 8;
constexpr int kWithMap = 1;  // Game::StartStory: the first map is entered

// The settings: the rebuild's rows around the five of the original (deviations.md), which keep their places (the
// sound volume at y 0xF0): the launcher switch above them, the display and battle setups below (the box grows down).
constexpr i32 kLauncherRow = 0;
constexpr i32 kSoundRow = 1;
constexpr i32 kMusicRow = 2;
constexpr i32 kCacheRow = 3;
constexpr i32 kEffectsRow = 4;
constexpr i32 kTextSpeedRow = 5;
constexpr i32 kDisplayRow = 6;
constexpr i32 kBattleRow = 7;
constexpr u32 kRowsTop = 0xD0;
constexpr u32 kRowHeight = 0x20;
constexpr u32 kBarsLeft = 0xA8;  // a click between these x sets a bar
constexpr u32 kBarsRight = 0x1C0;
constexpr int kStepsLeft = 0x100;  // the steps of a bar, kStepWidth wide
constexpr int kStepWidth = 0x10;
constexpr int kStepShift = 4;
constexpr int kStepsRight = 0x1B0;
constexpr i32 kStepOff = 0x22;  // the actions of a step of a bar
constexpr i32 kStepOn = 0x23;
constexpr i32 kMinCache = 0x3C;  // the cache size in megabytes: kMinCache and kCacheStep more per step
constexpr i32 kCacheStep = 0x28;
constexpr u32 kCacheSteps = 2;
constexpr i32 kMaxCache = kMinCache + static_cast<i32>(kCacheSteps) * kCacheStep;
constexpr i32 kSlowestText = 4;                        // the text speed counts down to 0 from here
constexpr i32 kMusicFallback = Config::MaxVolume - 1;  // past the maximum the music volume falls back here
constexpr int kVolumeLabelX = 0xC4;
constexpr int kLabelX = 0x98;
constexpr int kLineX = 0x168;  // the cache, effects and speed lines
constexpr int kLineDrop = 6;
constexpr int kSampleX = 0x19E;
constexpr int kLabelRed = 0x15;
constexpr int kLabelGreen = 0xF;
constexpr int kLabelBlue = 8;
constexpr int kSampleRed = 0xF;
constexpr int kSampleGreen = 9;
constexpr int kSampleBlue = 2;
constexpr i32 kSampleDelayFactor = 2;
constexpr int kBoxX = 0x98;
constexpr int kBoxTop = 0xCC;  // a row above the original's first (0xEC)
constexpr int kBoxWidth = 0x190;
constexpr int kSettingsRows = 8;  // the box is as high as its rows (the original's five: 0xA0)
constexpr int kHighlightX = 0x93;
constexpr int kHighlightWidth = 0x19A;
constexpr int kHighlightRed = 0x14;
constexpr int kHighlightGreen = 0xD;
constexpr i32 kSpeeds = 5;

// The name page: the confirm button, the boxes (Game::nameBoxes, as NameBoxFrame finds them) and the face.
constexpr u32 kConfirmLeft = 0x178;
constexpr u32 kConfirmRight = 0x1B6;
constexpr u32 kConfirmTop = 0x134;
constexpr u32 kConfirmBottom = 0x148;
constexpr u32 kNameRowTop = 0xEC;
constexpr u32 kNameRowBottom = 0x104;
constexpr u32 kSurnameLeft = 0x11C;
constexpr u32 kSurnameRight = 0x154;
constexpr u32 kGivenLeft = 0x174;
constexpr u32 kGivenRight = 0x1AE;
constexpr u32 kNicknameTop = 0x116;
constexpr u32 kNicknameBottom = 0x12E;
constexpr u32 kNicknameLeft = 0x144;
constexpr u32 kNicknameRight = 0x1AE;
constexpr i32 kClickConfirm = 1;  // Menu::systemClick
constexpr i32 kClickCancel = 2;
constexpr i32 kFirstHeldFrame = 1;  // ButtonState::heldFrames in the frame a button goes down
constexpr u32 kLeftClicks = 3;      // Menu::clicks: a single or double left click
constexpr u32 kCoin = 2;
constexpr int kFaceAct = 1;                                      // the face waiting, and after a reply
constexpr int kReplyAct = 0x3E9;                                 // the face replying to a key on the name page
constexpr std::array<i32, 3> kNameReplies = {0, 0xA, 0x4A};      // kReplyAct, by a roll of 3
constexpr std::array<i32, 3> kOtherReplies = {0x4C, 0xD, 0x5F};  // kFaceAct off the name step (not reached)
constexpr std::array<i32, 2> kReplyEnds = {0xD, 0x4C};           // kFaceAct, by a roll of 2
constexpr i32 kOtherReplyEnd = 0x4D;
constexpr i32 kReplyFrames = 0x28;
constexpr i32 kReplyEnd = 2;  // the face changes back when this many frames are left
constexpr int kFaceX = 0xDC;
constexpr int kFaceY = 0xFE;
constexpr int kQuitFadeSteps = 0xF;  // quitting fades the music out over these updates

// In game (modes 1 and 2) the page fades over the saved screen.
constexpr i32 kGrayEnd = -0x5A;  // mode 1: darkened until here, plain, then darkened again from kRedarken
constexpr i32 kRedarken = -0xA;
constexpr i32 kRedarkenStep = 3;
constexpr i32 kInGameFrameAction = 0x44;
constexpr i32 kDarkenEvery = 3;  // mode 2 darkens the saved screen every third frame
constexpr int kDarkenTint = -1;

// Whether `y` is inside settings row `row`.
bool InRow(u32 y, i32 row)
{
    const u32 top = kRowsTop + kRowHeight * static_cast<u32>(row);
    return y < top + kRowHeight - 1 && y > top;
}

// The top of settings row `row`.
int RowY(i32 row)
{
    return static_cast<int>(kRowsTop + kRowHeight * static_cast<u32>(row));
}

}  // namespace

// swd3e.exe 0x00451210: the entries hidden, fading in from -6 (in game, modes 1 and 2, the page fades over a copy of
// the screen; mode 3 chooses entry 1 at once).
void Menu::SystemEnter()
{
    Game& owner = this->game;
    this->glow = {kHiddenGlow, kHiddenGlow, kHiddenGlow, kHiddenGlow};
    this->systemCursor = 0;
    this->systemRow = kTitleFade;
    this->systemState = 0;
    this->savedScreen.clear();

    if (this->mode == Menu::GameModeA || this->mode == Menu::GameModeB)
    {
        this->systemState = kInGame;
        this->systemRow = kInGameFade;
        this->savedScreen.assign(owner.drawSurface, owner.drawSurface + kScreenPixels);
    }

    if (this->mode == Menu::LoadMode)
    {
        this->systemState = kEntries;
        this->systemCursor = kLoadEntry;
        this->SystemSelect();
    }

    owner.unk4C2B58 = 0;

    // The title music (the first list from tune 10) is started only when 0x00411F10 returns true; it always returns 0,
    // so the original never starts it.

    this->sampleDelay = 0;
    this->sampleLength = 0;
    this->textSpeedShown = owner.config.textSpeed;
}

// swd3e.exe 0x004513D0: the mouse on the page: on the name page the confirm button, the three boxes and the right
// button; the four entries (a click chooses); the bars of the settings (a right click or a click elsewhere leaves
// them)
void Menu::SystemFrame()
{
    Game& owner = this->game;
    const ButtonState& leftButton = owner.controls.At(Button::MouseLeft);
    const ButtonState& rightButton = owner.controls.At(Button::MouseRight);
    const auto x = static_cast<u32>(owner.state.pointerX);
    const auto y = static_cast<u32>(owner.state.pointerY);

    if (this->systemState == kSettings || this->systemState == kSetupSettings)
    {
        this->StoreChangedSettings();
    }

    if (this->systemState == kChosen)
    {
        if (this->systemRow < kNameMouse)
        {
            return;
        }

        if (leftButton.heldFrames == kFirstHeldFrame && leftButton.press == 1)
        {
            if (y < kConfirmBottom && y > kConfirmTop && x < kConfirmRight && x > kConfirmLeft)
            {
                this->systemClick = kClickConfirm;
                return;
            }

            if (this->systemRow < kFadeOut)
            {
                if (y < kNameRowBottom && y > kNameRowTop)
                {
                    if (x < kSurnameRight && x > kSurnameLeft)
                    {
                        owner.currentNameBox = owner.nameBoxes[Game::SurnameBox].get();
                        return;
                    }

                    if (x < kGivenRight && x > kGivenLeft)
                    {
                        owner.currentNameBox = owner.nameBoxes[Game::GivenBox].get();
                        return;
                    }
                }

                if (y < kNicknameBottom && y > kNicknameTop && x < kNicknameRight && x > kNicknameLeft)
                {
                    owner.currentNameBox = owner.nameBoxes[Game::NicknameBox].get();
                    return;
                }
            }
        }

        if (rightButton.heldFrames == kFirstHeldFrame && rightButton.press == 1)
        {
            this->systemClick = kClickCancel;
        }

        return;
    }

    if (this->systemState == kEntries)
    {
        for (i32 entry = kNewGameEntry; entry <= kQuitEntry; ++entry)
        {
            const auto left = static_cast<u32>(kEntryStep * (entry + 1));

            if (y < kEntryBottom && y > kEntryTop && x < left + kEntryWidth && x > left)
            {
                this->systemCursor = entry;

                if ((this->clicks & kLeftClicks) != 0)
                {
                    this->SystemSelect();
                }
            }
        }

        return;
    }

    if (this->systemState == kSetupSettings)
    {
        this->SetupSettingsMouse();
        return;
    }

    if (this->systemState != kSettings)
    {
        return;
    }

    // The rebuild's rows (deviations.md): the launcher switch takes the choice clicked, a click on a setup opens it.
    if (leftButton.press != 0 && x > kBarsLeft && x < static_cast<u32>(kBoxX + kBoxWidth))
    {
        if (InRow(y, kLauncherRow))
        {
            this->systemRow = kLauncherRow;
            const std::vector<int> lefts = Menu::ChoiceLefts(Menu::OnOffChoices(), owner.largeFont, kStepsLeft);

            for (std::size_t at = 0; at + 1 < lefts.size(); ++at)
            {
                if (x >= static_cast<u32>(lefts[at]) && x < static_cast<u32>(lefts[at + 1]))
                {
                    this->OptionsSetLauncher(at == 0);
                }
            }

            return;
        }

        for (const i32 row : {kDisplayRow, kBattleRow})
        {
            if (InRow(y, row))
            {
                this->systemRow = row;
                this->SystemSelect();
                return;
            }
        }
    }

    if (leftButton.press == 0 || x >= kBarsRight || x <= kBarsLeft)
    {
        if (rightButton.press != 0)
        {
            this->SystemCancel();
        }

        return;
    }

    u32 step = (x - kStepsLeft) >> kStepShift;
    const bool onBar = HighWord(step) == 0;
    Config& settings = owner.config;

    if (InRow(y, kSoundRow))
    {
        this->systemRow = kSoundRow;

        if (onBar)
        {
            settings.soundVolume = static_cast<i32>(std::min<u32>(step, Config::MaxVolume));
            owner.audio.PlaySound(kVolumeSound, settings.soundVolume);
        }

        return;
    }

    if (InRow(y, kMusicRow))
    {
        this->systemRow = kMusicRow;

        if (onBar)
        {
            settings.musicVolume = static_cast<i32>(std::min<u32>(step, Config::MaxVolume));
            owner.audio.Queue().SetVolume(MusicQueue::MaxVolume * settings.musicVolume / Config::MaxVolume);
        }

        return;
    }

    if (InRow(y, kCacheRow))
    {
        this->systemRow = kCacheRow;

        if (onBar)
        {
            settings.cacheSize =
                static_cast<i32>(kCacheStep * static_cast<i32>(std::min<u32>(step, kCacheSteps)) + kMinCache);
        }

        return;
    }

    if (InRow(y, kEffectsRow))
    {
        this->systemRow = kEffectsRow;

        if (onBar)
        {
            owner.state.SetGameFlag(kEffectsOffFlag);

            if (step != 0)
            {
                owner.state.ClearGameFlag(kEffectsOffFlag);
            }
        }

        return;
    }

    if (InRow(y, kTextSpeedRow))
    {
        this->systemRow = kTextSpeedRow;

        if (onBar)
        {
            step = std::min<u32>(step, kSlowestText);
            settings.textSpeed = kSlowestText - static_cast<i32>(step);
            this->textSpeedShown = kSlowestText - static_cast<i32>(step);
        }

        return;
    }

    if (rightButton.press != 0)
    {
        this->SystemCancel();
    }
}

// swd3e.exe 0x004517D0: on the settings the original stops at the last row; the rebuild goes round to the first
void Menu::SystemDown()
{
    if (this->systemState == kEntries)
    {
        if (++this->systemCursor > kQuitEntry)
        {
            this->systemCursor = kQuitEntry;
        }
    }
    else if (this->systemState == kSettings)
    {
        if (++this->systemRow > kBattleRow)
        {
            this->systemRow = kLauncherRow;
        }
    }
    else if (this->systemState == kSetupSettings && ++this->systemRow > SetupBackRow(this->systemSetup))
    {
        this->systemRow = 0;
    }

    this->SettingsMoveSound();
}

// swd3e.exe 0x00451820 (on the settings the rebuild goes round from the first row to the last)
void Menu::SystemUp()
{
    if (this->systemState == kEntries)
    {
        if (--this->systemCursor < 0)
        {
            this->systemCursor = 0;
        }
    }
    else if (this->systemState == kSettings)
    {
        if (--this->systemRow < 0)
        {
            this->systemRow = kBattleRow;
        }
    }
    else if (this->systemState == kSetupSettings && --this->systemRow < 0)
    {
        this->systemRow = SetupBackRow(this->systemSetup);
    }

    this->SettingsMoveSound();
}

// The rebuild (deviations.md): moving on the settings sounds as on the settings of the system tab (the original's
// title page is silent).
void Menu::SettingsMoveSound()
{
    if (this->systemState == kSettings || this->systemState == kSetupSettings)
    {
        this->game.audio.PlaySound(kVolumeSound, this->game.config.soundVolume);
    }
}

// swd3e.exe 0x00451860
void Menu::SystemLast()
{
    if (this->systemState == kEntries)
    {
        this->systemCursor = kQuitEntry;
    }
    else if (this->systemState == kSettings)
    {
        this->systemRow = kBattleRow;
    }
    else if (this->systemState == kSetupSettings)
    {
        this->systemRow = SetupBackRow(this->systemSetup);
    }
}

// swd3e.exe 0x00451890
void Menu::SystemFirst()
{
    if (this->systemState == kEntries)
    {
        this->systemCursor = 0;
    }
    else if (this->systemState == kSettings || this->systemState == kSetupSettings)
    {
        this->systemRow = 0;
    }
}

// swd3e.exe 0x004518C0: on the settings: sound volume down (with sound 0x2E), music volume down, cache size down to
// 60 MB, effects off, text slower (up to 4).
void Menu::SystemLeft()
{
    Game& owner = this->game;
    Config& settings = owner.config;

    if (this->systemState == kEntries)
    {
        if (--this->systemCursor < 0)
        {
            this->systemCursor = 0;
        }

        return;
    }

    if (this->systemState == kSetupSettings)
    {
        this->SetupStep(this->systemSetup, this->systemRow, false, false);
        return;
    }

    if (this->systemState != kSettings)
    {
        return;
    }

    switch (this->systemRow)
    {
        case kLauncherRow:
            this->OptionsSetLauncher(true);
            break;

        case kSoundRow:
        {
            int volume = --settings.soundVolume;

            if (settings.soundVolume <= 0)
            {
                volume = 0;
                settings.soundVolume = 0;
            }

            owner.audio.PlaySound(kVolumeSound, volume);
            break;
        }

        case kMusicRow:
        {
            int volume = --settings.musicVolume;

            if (settings.musicVolume <= 0)
            {
                volume = 0;
                settings.musicVolume = 0;
            }

            owner.audio.Queue().SetVolume(MusicQueue::MaxVolume * volume / Config::MaxVolume);
            break;
        }

        case kCacheRow:
            settings.cacheSize -= kCacheStep;

            if (settings.cacheSize <= kMinCache)
            {
                settings.cacheSize = kMinCache;
            }

            break;

        case kEffectsRow:
            owner.state.SetGameFlag(kEffectsOffFlag);
            break;

        case kTextSpeedRow:
        {
            int level = ++settings.textSpeed;

            if (settings.textSpeed > kSlowestText)
            {
                level = kSlowestText;
                settings.textSpeed = kSlowestText;
            }

            this->textSpeedShown = level;
            break;
        }

        default:
            break;
    }
}

// swd3e.exe 0x004519C0: on the settings: sound volume up to 11, music volume (past 11 it falls back to 10), cache size
// up to 140 MB, effects on, text faster (down to 0)
void Menu::SystemRight()
{
    Game& owner = this->game;
    Config& settings = owner.config;

    if (this->systemState == kEntries)
    {
        if (++this->systemCursor > kQuitEntry)
        {
            this->systemCursor = kQuitEntry;
        }

        return;
    }

    if (this->systemState == kSetupSettings)
    {
        this->SetupStep(this->systemSetup, this->systemRow, true, false);
        return;
    }

    if (this->systemState != kSettings)
    {
        return;
    }

    switch (this->systemRow)
    {
        case kLauncherRow:
            this->OptionsSetLauncher(false);
            break;

        case kSoundRow:
        {
            int volume = ++settings.soundVolume;

            if (settings.soundVolume > Config::MaxVolume)
            {
                volume = Config::MaxVolume;
                settings.soundVolume = Config::MaxVolume;
            }

            owner.audio.PlaySound(kVolumeSound, volume);
            break;
        }

        case kMusicRow:
        {
            int volume = ++settings.musicVolume;

            if (settings.musicVolume > Config::MaxVolume)
            {
                volume = kMusicFallback;
                settings.musicVolume = kMusicFallback;
            }

            owner.audio.Queue().SetVolume(MusicQueue::MaxVolume * volume / Config::MaxVolume);
            break;
        }

        case kCacheRow:
            settings.cacheSize += kCacheStep;

            if (settings.cacheSize >= kMaxCache)
            {
                settings.cacheSize = kMaxCache;
            }

            break;

        case kEffectsRow:
            owner.state.ClearGameFlag(kEffectsOffFlag);
            break;

        case kTextSpeedRow:
        {
            int level = --settings.textSpeed;

            if (settings.textSpeed <= 0)
            {
                level = 0;
                settings.textSpeed = 0;
            }

            this->textSpeedShown = level;
            break;
        }

        default:
            break;
    }
}

// swd3e.exe 0x00451AD0: on the name page the key of button slot 2 goes on to the next box
void Menu::SystemShift()
{
    Game& owner = this->game;

    if (this->systemState <= 0 || this->systemState > kChosen || this->systemRow >= kFadeOut)
    {
        return;
    }

    TextInput* surname = owner.nameBoxes[Game::SurnameBox].get();
    TextInput* given = owner.nameBoxes[Game::GivenBox].get();
    TextInput* nickname = owner.nameBoxes[Game::NicknameBox].get();

    if (owner.currentNameBox == surname)
    {
        owner.currentNameBox = given;
    }
    else if (owner.currentNameBox == given)
    {
        owner.currentNameBox = nickname;
    }
    else if (owner.currentNameBox == nickname)
    {
        owner.currentNameBox = surname;
    }
}

// swd3e.exe 0x00451B30
void Menu::SystemEnd()
{
    if (this->systemState == kEntries)
    {
        if (++this->systemCursor > kQuitEntry)
        {
            this->systemCursor = kQuitEntry;
        }
    }
}

// swd3e.exe 0x00451B60: the chosen entry fades out; entry 0 first asks for the first role's name in three boxes
// (flag 0x50 routes the keyboard to the current one; they start with the default name), entry 2 opens the settings,
// entry 3 also stops the music after a fade of 15 updates.
void Menu::SystemSelect()
{
    Game& owner = this->game;

    // The rebuild's rows (deviations.md): Enter switches the launcher, opens a setup; there it steps a row's choice
    // round, or goes back.
    if (this->systemState == kSettings && this->systemRow == kLauncherRow)
    {
        this->OptionsSetLauncher(!owner.config.showLauncher);
        return;
    }

    if (this->systemState == kSettings && (this->systemRow == kDisplayRow || this->systemRow == kBattleRow))
    {
        this->systemSetup = this->systemRow == kDisplayRow ? Setup::Display : Setup::Battle;
        this->systemState = kSetupSettings;
        this->systemRow = 0;
        owner.audio.PlaySound(kVolumeSound, owner.config.soundVolume);
        return;
    }

    if (this->systemState == kSetupSettings)
    {
        if (this->systemRow == SetupBackRow(this->systemSetup))
        {
            this->SystemCancel();
            return;
        }

        this->SetupStep(this->systemSetup, this->systemRow, true, true);
        return;
    }

    if (this->systemState != kEntries)
    {
        return;
    }

    this->systemRow = kFadeOut;
    this->systemState = kChosen;

    if (this->systemCursor == kNewGameEntry)
    {
        owner.state.SetGameFlag(kNameEntryFlag);
        this->face.id = kFaceAct;
        this->face.action = 0;
        this->face.direction = 0;
        this->nameReply = 0;
        this->enteredName.assign(sizeof(RoleName), 0);
        this->systemRow = kNameEntry;
        const RoleName::Parts& name = Party::DefaultNames[0];
        const std::array<std::u8string_view, Game::NameBoxCount> parts = {name.surname, name.given, name.nickname};

        for (std::size_t box = 0; box < Game::NameBoxCount; ++box)
        {
            const Game::NameBoxPlace& place = Game::NameBoxPlaces[box];
            owner.nameBoxes[box] = std::make_unique<TextInput>(parts[box], place.capacity, place.x, place.y);
        }

        owner.currentNameBox = owner.nameBoxes[Game::SurnameBox].get();
    }
    else if (this->systemCursor == kSettingsEntry)
    {
        owner.acts.Run(owner.battleMessages.panelAct);
        this->systemState = kSettings;
        this->systemRow = kLauncherRow;
        this->gauge.id = Scene::InterfaceAct;
        this->gauge.action = kStepOff;
    }
    else if (this->systemCursor == kQuitEntry)
    {
        owner.audio.Queue().Switch(MusicQueue::Silence, MusicQueue::RepeatList, kQuitFadeSteps);
    }
}

// swd3e.exe 0x00451D30: Escape on the entries points at the last entry; on the settings it goes back and writes them
// (to [game], Env.dat in the original)
void Menu::SystemCancel()
{
    Game& owner = this->game;

    if (this->systemState == kEntries)
    {
        this->systemCursor = kQuitEntry;
        return;
    }

    if (this->systemState == kSetupSettings)
    {
        this->systemState = kSettings;
        this->systemRow = this->systemSetup == Setup::Display ? kDisplayRow : kBattleRow;
        owner.audio.PlaySound(kVolumeSound, owner.config.soundVolume);
        return;
    }

    if (this->systemState == kSettings)
    {
        this->systemState = kEntries;
        owner.config.Update(this->CurrentSettings());
        this->storedSettings.reset();
    }
}

// swd3e.exe 0x00451DA0: the page. The chosen entry alone is drawn over the background while the screen fades in from
// black; a chosen entry fades the screen out and acts (0: a new game after the name, 1: load, 2: the settings, 3:
// quit). The name page shows the three boxes and the face replying to a key; the settings show the five rows with
// their bars; in game the page fades over the saved screen and back.
// swd3e.exe EH funclet (tail) of 0x00451DA0, swd32 0x00498330 (EH unwind: the new object freed (operator delete))
// swd3e.exe EH funclet (tail) of 0x00451DA0, swd32 0x0049833B (EH handler of 0x004490C0)
void Menu::SystemDraw()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;

    if (this->systemState <= kChosen)
    {
        owner.actDrawer.Draw(this->background, 0, 0);
        const auto chosen = static_cast<std::size_t>(this->systemCursor);
        this->glow[0] = 0;
        this->glow[chosen] = kChosenGlow;
        this->DrawActAlpha(this->items[chosen], kEntryStep * (this->systemCursor + 1), kEntryY, true);

        if (this->systemRow < 0)
        {
            if (++this->systemRow >= 0)
            {
                this->systemRow = 0;
                this->systemState = kEntries;
                this->systemClick = 0;
                return;
            }

            const int level = kFadeInStep * this->systemRow;
            painter.TintPixels(std::span<u16>(owner.drawSurface, kScreenPixels), level, level, level);
        }
    }

    if (this->systemState == kChosen)
    {
        if (this->systemRow < kFadeOut)
        {
            this->DrawNameEntry();
        }
        else if (++this->systemRow <= kFadeOutEnd)
        {
            const int level = kFadeOutStep * (kFadeOut - this->systemRow);
            painter.TintPixels(std::span<u16>(owner.drawSurface, kScreenPixels), level, level, level);
        }
        else
        {
            this->ActOnEntry();
            std::memset(owner.drawSurface, 0, kScreenBytes);
        }
    }

    if (this->systemState == kSettings)
    {
        this->DrawSettings();
    }
    else if (this->systemState == kSetupSettings)
    {
        this->DrawSetupSettings();
    }

    if (this->systemState != kInGame)
    {
        this->systemClick = 0;
        return;
    }

    i32 row = this->systemRow;

    if (this->mode == Menu::GameModeA && row == kInGameFade)
    {
        painter.GrayPixels(this->savedScreen);
    }

    if (this->mode == Menu::GameModeA)
    {
        if (row >= kInGameFade)
        {
            if (row < kGrayEnd)
            {
                this->tint = row - kGrayEnd;
            }
            else if (row < kRedarken)
            {
                this->tint = 0;
            }
            else if (row <= 0)
            {
                this->tint = kRedarkenStep * (kRedarken - row);
            }
        }

        this->gauge.id = Scene::InterfaceAct;
        this->gauge.action = kInGameFrameAction;
        owner.actDrawer.Draw(this->gauge, 0, 0);
    }
    else if (this->mode == Menu::GameModeB)
    {
        std::copy(this->savedScreen.begin(), this->savedScreen.end(), owner.drawSurface);

        if (row % kDarkenEvery == 0)
        {
            painter.TintPixels(this->savedScreen, kDarkenTint, kDarkenTint, kDarkenTint);
        }
    }

    this->systemRow = row + 1;

    if (row + 1 < 0)
    {
        this->systemClick = 0;
        return;
    }

    owner.menuState = Game::TitleMenu;
    owner.storyState = Game::StoryRunning;
    owner.saveScreenPage = 0;
    owner.slotsLoad = 0;
    owner.menuMode = Menu::NoSlotsMode;
    owner.scene.playerHold = 0;
    this->savedScreen.clear();
    std::memset(owner.drawSurface, 0, kScreenBytes);
}

// The name page of the system page (part of 0x00451DA0): a key makes the face reply (at random) for 40 frames, and it
// changes back two frames before the end; the boxes and the face are drawn. Enter or the confirm button takes the
// name typed as the first role's (the boxes freed); Escape or the right button gives up, the boxes left as they are.
void Menu::DrawNameEntry()
{
    Game& owner = this->game;

    if (owner.input.FirstKeyDown() != 0)
    {
        if (this->nameReply == 0)
        {
            const auto roll = static_cast<std::size_t>(owner.random.Below(kNameReplies.size()));

            if (this->systemRow == kNameEntry)
            {
                this->face.id = kReplyAct;
                this->face.action = kNameReplies[roll];
            }
            else
            {
                this->face.id = kFaceAct;
                this->face.action = kOtherReplies[roll];
            }
        }

        this->nameReply = kReplyFrames;
    }
    else
    {
        if (this->nameReply > 0)
        {
            --this->nameReply;
        }

        if (this->nameReply == kReplyEnd)
        {
            this->face.id = kFaceAct;

            if (this->systemRow == kNameEntry)
            {
                this->face.action = kReplyEnds[static_cast<std::size_t>(owner.random.Below(kCoin))];
            }
            else
            {
                this->face.action = kOtherReplyEnd;
            }
        }
    }

    for (std::size_t box = 0; box < Game::NameBoxCount; ++box)
    {
        const TextInput* drawn = owner.nameBoxes[box].get();

        if (drawn != nullptr)
        {
            owner.DrawTextInput(*drawn, owner.currentNameBox == drawn, box == Game::SurnameBox);
        }
    }

    owner.actDrawer.Draw(this->face, kFaceX, kFaceY);
    int result = owner.currentNameBox != nullptr ? owner.currentNameBox->GetResult() : 0;

    if (result == 0 && this->systemClick != 0)
    {
        result = this->systemClick;
    }

    this->systemClick = 0;

    if (result == TextInput::Escape)
    {
        this->enteredName.clear();
        this->systemState = kEntries;
        owner.state.ClearGameFlag(kNameEntryFlag);
        return;
    }

    if (result != TextInput::Enter)
    {
        return;
    }

    // The role's name becomes the one the texts write for it, then the whole of the name typed (each part cleared
    // for its box's bytes and filled with its text) is put over it.
    owner.party.names[0] = owner.party.nameTags[0];
    const std::array<std::size_t, Game::NameBoxCount> partAt = {
        offsetof(RoleName, surname),
        offsetof(RoleName, given),
        offsetof(RoleName, nickname),
    };

    for (std::size_t box = 0; box < Game::NameBoxCount; ++box)
    {
        const auto size = static_cast<std::size_t>(Game::NameBoxPlaces[box].capacity);
        std::fill_n(this->enteredName.begin() + static_cast<std::ptrdiff_t>(partAt[box]), size, 0);

        if (owner.nameBoxes[box] != nullptr)
        {
            const std::string text = owner.nameBoxes[box]->CopyText(static_cast<int>(size));
            std::copy(text.begin(), text.end(), this->enteredName.begin() + static_cast<std::ptrdiff_t>(partAt[box]));
        }
    }

    for (std::unique_ptr<TextInput>& box : owner.nameBoxes)
    {
        box.reset();
    }

    owner.currentNameBox = nullptr;
    std::memcpy(&owner.party.names[0], this->enteredName.data(), sizeof(RoleName));
    this->enteredName.clear();
    this->systemRow = kFadeOut;
    owner.state.ClearGameFlag(kNameEntryFlag);
}

// What a faded out entry does (part of 0x00451DA0).
void Menu::ActOnEntry()
{
    Game& owner = this->game;

    switch (this->systemCursor)
    {
        case kNewGameEntry:
            owner.playSeconds = 0;
            owner.playClockStart = crt::Time();
            this->systemState = kSettings;
            std::memset(owner.drawSurface, 0, kScreenBytes);
            owner.StartStory(kWithMap);
            owner.menuState = Game::NoMenu;
            owner.storyState = Game::StoryRunning;
            owner.saveScreenPage = 1;
            owner.slotsLoad = 1;
            owner.ResetSaveSlots();
            owner.LoadFameFile();
            break;

        case kLoadEntry:
            owner.menuState = Game::NoMenu;
            owner.storyState = Game::SaveSlotsState;
            owner.saveScreenPage = 1;
            owner.slotsLoad = 1;
            owner.menuMode = Menu::TitleLoadMode;
            owner.ResetSaveSlots();
            // The rebuild (deviations.md): the load starts on the newest save, as the launcher's continue does.
            owner.saveSlots.FindNewest(owner.config.SaveDirectory(), owner.painter);
            break;

        case kSettingsEntry:
            this->systemState = kEntries;
            break;

        case kQuitEntry:
            owner.systemFlags |= Game::QuitRequested;
            break;

        default:
            break;
    }
}

// swd3e.exe 0x00452800: runs the act of `state` (a failure reported) and takes its picture as the source; with `draw`
// draws it at (x, y) minus the act's anchor
void Menu::DrawActAlpha(ActState& state, int x, int y, bool draw)
{
    Game& owner = this->game;

    if (owner.acts.Run(state) == 0)
    {
        ErrorBox(
            owner.errors,
            u8"Act Err(Menu::DrawActAlpha)",
            u8"設定錯誤 \nact[%d] qq[%d] dir[%d]",
            state.id,
            state.action,
            state.direction
        );  // 0x004B0D58
        return;
    }

    const Picture* picture = owner.archive.GetPicture(state.frame, state.picture);

    if (picture == nullptr)
    {
        return;
    }

    owner.painter.SetSource(picture->data.data());

    if (draw)
    {
        owner.painter.DrawPicture(x - state.x, y - state.y, picture->width, picture->height, 0);
    }
}

// The settings of the system page (part of 0x00451DA0): the box, the five labels (with the rebuild's two around
// them), a bar of steps for each (the current value lit), the cache line, the effects line, the speed with its sample
// text typing out, and the row highlight.
void Menu::DrawSettings()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    Config& settings = owner.config;
    auto color = static_cast<Pixel>(painter.MakeColor(kLabelRed, kLabelGreen, kLabelBlue));
    owner.actDrawer.Draw(this->background, 0, 0);
    this->DrawBox(
        kBoxX,
        kBoxTop,
        kBoxWidth,
        kSettingsRows * static_cast<int>(kRowHeight),
        static_cast<int>(FillMode::Eighth),
        0,
        0,
        0
    );
    this->gauge.id = Scene::InterfaceAct;
    this->gauge.action = kStepOff;
    TextFont& large = owner.largeFont;
    TextFont& medium = owner.mediumFont;
    u16* pixels = painter.Target();
    // The rebuild's rows (deviations.md): the launcher switch with its choices (the one in use bright), the display
    // and battle setups.
    large.DrawText(pixels, kLabelX, RowY(kLauncherRow), kLauncherSetup, color, kTextStyle);
    this->DrawSystemChoices(kLauncherRow, Menu::OnOffChoices(), owner.config.showLauncher ? 0 : 1, true);
    large.DrawText(pixels, kLabelX, RowY(kDisplayRow), kDisplaySetup, color, kTextStyle);
    large.DrawText(pixels, kLabelX, RowY(kBattleRow), kBattleSetup, color, kTextStyle);
    large.DrawText(pixels, kVolumeLabelX, RowY(kSoundRow), menu_text::kSoundVolume, color, kTextStyle);
    large.DrawText(pixels, kVolumeLabelX, RowY(kMusicRow), menu_text::kMusicVolume, color, kTextStyle);
    large.DrawText(pixels, kLabelX, RowY(kCacheRow), menu_text::kCacheSize, color, kTextStyle);
    large.DrawText(pixels, kLabelX, RowY(kEffectsRow), menu_text::kMapEffects, color, kTextStyle);
    large.DrawText(pixels, kLabelX, RowY(kTextSpeedRow), menu_text::kTextSpeed, color, kTextStyle);

    auto bar = [this, &owner](int y, auto lit)
    {
        int step = 0;

        for (int x = kStepsLeft; x <= kStepsRight; x += kStepWidth)
        {
            this->gauge.action = lit(step) ? kStepOn : kStepOff;
            owner.actDrawer.Draw(this->gauge, x, y);
            ++step;
        }
    };

    bar(RowY(kSoundRow),
        [&settings](int step)
        {
            return step <= settings.soundVolume;
        });

    bar(RowY(kMusicRow),
        [&settings](int step)
        {
            return step <= settings.musicVolume;
        });

    int size = kMinCache;

    for (int x = kStepsLeft; x < kStepsLeft + static_cast<int>(kCacheSteps + 1) * kStepWidth; x += kStepWidth)
    {
        this->gauge.action = kStepOff;

        if (size == settings.cacheSize)
        {
            this->gauge.action = kStepOn;
            const std::string line = WinFormat(
                menu_text::kCacheLine,
                {static_cast<i32>(
                     static_cast<u8>(settings.installDirectory.empty() ? 0 : settings.installDirectory[0])
                 ),
                 settings.cacheSize}
            );
            color = static_cast<Pixel>(painter.MakeColor(kLabelRed, kLabelGreen, kLabelBlue));
            medium.DrawText(pixels, kLineX, RowY(kCacheRow) + kLineDrop, line, color, kTextStyle);
        }

        owner.actDrawer.Draw(this->gauge, x, RowY(kCacheRow));
        size += kCacheStep;
    }

    const int effectsOff = owner.state.GameFlag(kEffectsOffFlag) ? 1 : 0;

    for (int step = 0, x = kStepsLeft; x < kStepsLeft + 2 * kStepWidth; x += kStepWidth, ++step)
    {
        this->gauge.action = step == effectsOff ? kStepOff : kStepOn;
        owner.actDrawer.Draw(this->gauge, x, RowY(kEffectsRow));
    }

    const std::string effects =
        WinFormat(menu_text::kEffectsLine, {effectsOff != 0 ? menu_text::kOff : menu_text::kOn});
    medium.DrawText(pixels, kLineX, RowY(kEffectsRow) + kLineDrop, effects, color, kTextStyle);

    for (int step = 0, x = kStepsLeft + kSlowestText * kStepWidth; step < kSpeeds; ++step, x -= kStepWidth)
    {
        this->gauge.action = step == settings.textSpeed ? kStepOn : kStepOff;
        owner.actDrawer.Draw(this->gauge, x, RowY(kTextSpeedRow));
    }

    const std::string speed =
        menu_text::SpeedSample(settings.textSpeed, static_cast<std::size_t>(std::max(this->sampleLength, 0)));
    const std::string sample = Big5(menu_text::kSample);
    medium.DrawText(
        pixels,
        kSampleX,
        RowY(kTextSpeedRow) + kLineDrop,
        sample,
        static_cast<Pixel>(painter.MakeColor(kSampleRed, kSampleGreen, kSampleBlue)),
        kTextStyle
    );

    if (--this->sampleDelay <= 0)
    {
        ++this->sampleLength;
        this->sampleDelay = kSampleDelayFactor * this->textSpeedShown + 1;
    }

    medium.DrawText(pixels, kLineX, RowY(kTextSpeedRow) + kLineDrop, speed, color, kTextStyle);

    if (menu_text::SampleEnds(static_cast<std::size_t>(std::max(this->sampleLength, 0))))
    {
        this->sampleLength = 0;
    }

    FillBox(
        painter,
        kHighlightX,
        static_cast<int>(kRowHeight) * this->systemRow + kBoxTop,
        kHighlightWidth,
        static_cast<int>(kRowHeight),
        kHighlightRed,
        kHighlightGreen,
        0,
        FillMode::Gradient
    );
}

// The rebuild's setups on the title page (deviations.md): the rows of the setup opened in the settings box, each with
// its choices (the one in use bright; a row without effect all dim), and the row highlight.
void Menu::DrawSetupSettings()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    const auto color = static_cast<Pixel>(painter.MakeColor(kLabelRed, kLabelGreen, kLabelBlue));
    owner.actDrawer.Draw(this->background, 0, 0);
    const Setup setup = this->systemSetup;
    const std::span<const std::u8string_view> labels = SetupLabels(setup);
    const auto rows = static_cast<int>(labels.size());
    this->DrawBox(
        kBoxX,
        kBoxTop,
        kBoxWidth,
        rows * static_cast<int>(kRowHeight),
        static_cast<int>(FillMode::Eighth),
        0,
        0,
        0
    );

    for (i32 row = 0; row < rows; ++row)
    {
        owner.largeFont
            .DrawText(painter.Target(), kLabelX, RowY(row), labels[static_cast<std::size_t>(row)], color, kTextStyle);

        if (row != SetupBackRow(setup))
        {
            i32 chosen = -1;
            const std::vector<std::string> choices = this->SetupChoices(setup, row, chosen);
            this->DrawSystemChoices(row, choices, chosen, SetupRowCounts(setup, row, owner.smoothMotion));
        }
    }

    FillBox(
        painter,
        kHighlightX,
        static_cast<int>(kRowHeight) * this->systemRow + kBoxTop,
        kHighlightWidth,
        static_cast<int>(kRowHeight),
        kHighlightRed,
        kHighlightGreen,
        0,
        FillMode::Gradient
    );
}

// The choices of a rebuild's row of the title page from the steps' left: `chosen` bright (unless the row has no
// effect, `counts` false), the others dim.
void Menu::DrawSystemChoices(i32 row, const std::vector<std::string>& choices, i32 chosen, bool counts)
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    const auto color = static_cast<Pixel>(painter.MakeColor(kLabelRed, kLabelGreen, kLabelBlue));
    Pixel unchosen = color;
    painter.TintPixels(std::span<u16>(&unchosen, 1), kUnchosenTint, kUnchosenTint, kUnchosenTint);
    const std::vector<int> lefts = Menu::ChoiceLefts(choices, owner.largeFont, kStepsLeft);

    for (std::size_t at = 0; at < choices.size(); ++at)
    {
        const bool shown = counts && static_cast<i32>(at) == chosen;
        owner.largeFont
            .DrawText(painter.Target(), lefts[at], RowY(row), choices[at], shown ? color : unchosen, kTextStyle);
    }
}

// The mouse on the title page's setups (deviations.md): a click on a row's choice takes it, on its last row goes back;
// a right click goes back.
void Menu::SetupSettingsMouse()
{
    Game& owner = this->game;
    const Setup setup = this->systemSetup;
    const i32 backRow = SetupBackRow(setup);
    const ButtonState& leftButton = owner.controls.At(Button::MouseLeft);
    const auto x = static_cast<u32>(owner.state.pointerX);
    const auto y = static_cast<u32>(owner.state.pointerY);

    if (owner.controls.At(Button::MouseRight).press != 0)
    {
        this->SystemCancel();
        return;
    }

    if (leftButton.press == 0 || x <= static_cast<u32>(kBoxX) || x >= static_cast<u32>(kBoxX + kBoxWidth))
    {
        return;
    }

    for (i32 row = 0; row <= backRow; ++row)
    {
        if (!InRow(y, row))
        {
            continue;
        }

        this->systemRow = row;

        if (row == backRow)
        {
            this->SystemCancel();
            return;
        }

        i32 chosen = -1;
        const std::vector<std::string> choices = this->SetupChoices(setup, row, chosen);
        const std::vector<int> lefts = Menu::ChoiceLefts(choices, owner.largeFont, kStepsLeft);

        for (std::size_t at = 0; at + 1 < lefts.size(); ++at)
        {
            if (x >= static_cast<u32>(lefts[at]) && x < static_cast<u32>(lefts[at + 1]) &&
                static_cast<i32>(at) != chosen)
            {
                this->SetupChoose(setup, row, static_cast<i32>(at));
            }
        }

        return;
    }
}
}  // namespace swd3e
