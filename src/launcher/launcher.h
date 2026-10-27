#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/types.h"
#include "platform/surface.h"

namespace swd3e
{

class ExeResources;
class GameState;
class SystemSound;

// The launcher shown before the game (dialog 0x6B: the 640 x 480 picture 0x71 with five highlight pictures that light
// up under the mouse). A click on a hotspot ends it with the code of the choice; the highlights and the hover sound
// follow the mouse messages as in the dialog procedure.
class Launcher
{
public:
    static constexpr int Items = 5;
    static constexpr u32 Dialog = 0x6B;
    static constexpr u32 Background = 0x71;
    static constexpr u32 HoverSound = 0x77;
    static constexpr int SavedGameFlag = 0x12;
    static constexpr std::string_view Website = "www.softstar.com.tw";
    static constexpr std::u8string_view Readme = u8"Readme.txt";

    // How the launcher ended (the EndDialog codes).
    enum class Choice
    {
        None = 0,
        Continue = 1,  // the newest save (only with a saved game)
        Start = 2,
        Command3 = 3,
        Exit = 6,
    };

    // Where the highlight pictures are placed (WM_INITDIALOG, 0x70 x 0x28 controls); the mouse lights an item inside
    // its picture's rectangle (client pixels).
    struct Item
    {
        u32 picture = 0;
        int x = 0;
        int y = 0;
    };

    static constexpr i8 Shown = 5;  // `lit` of an item under the mouse (SW_SHOW)
    static constexpr int ItemWidth = 0x70;
    static constexpr int ItemHeight = 0x28;
    static constexpr std::array<Item, Items> Layout = {{
        {0x72, 0xB8, 0x140},   // continue (control 0x40E)
        {0x73, 0xB8, 0x168},   // start (0x40F)
        {0x74, 0x160, 0x140},  // website (0x410)
        {0x75, 0x160, 0x168},  // Readme (0x411)
        {0x76, 0x110, 0x1A0},  // exit (0x412)
    }};

    Launcher(const ExeResources& resources, const GameState& state, SystemSound& sound, std::u8string installDirectory);

    // The background, the highlights and the hover sound; false when a picture is missing.
    bool LoadPictures();
    void Init();
    Choice OnMouse(u32 buttons, int x, int y);
    Choice OnCommand(int command);
    Choice OnClose();
    void Draw(Surface& screen) const;

    static void RunBrowser(const std::string& address);
    static void RunTextViewer(const std::u8string& path);

    std::array<i8, Items> lit = {};        // 0x004EF5A4 (4 bytes), 0x004EF5A8: SW_SHOW (5) under the mouse
    std::array<i8, Items> litBefore = {};  // 0x004EF5A9 (4 bytes), 0x004EF5AD
    std::array<bool, Items> shown = {};    // the highlight windows (hidden by Init)

private:
    const ExeResources& resources;
    const GameState& state;
    SystemSound& sound;
    std::u8string installDirectory;
    Surface background;
    std::array<Surface, Items> pictures;
    std::vector<u8> hoverSound;  // read with the pictures
};

}  // namespace swd3e
