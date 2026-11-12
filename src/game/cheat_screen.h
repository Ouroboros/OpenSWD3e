#pragma once

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"
#include "game/cheat_dialog.h"
#include "platform/display.h"
#include "platform/surface.h"
#include "res/act_state.h"

union SDL_Event;

namespace swd3e
{

class Game;

// The rebuild's cheat screen (deviations.md): Ctrl+F4 with the debug keys shows the cheat dialog's tabs, list,
// fields and buttons on the game's screen, in the menus' frames and fonts, the game paused as the dialog pauses it;
// CheatDialog does the work of the buttons. The pointer chooses the tabs, rows, fields and buttons and the wheel
// scrolls; the keys: Left and Right the tabs, Up, Down, Page Up, Page Down, Home and End the rows, Tab the fields,
// the digits and the minus type, Backspace erases, Enter sets, Escape closes. 劇情 (the rebuild's) runs the event
// numbered in the id field once it closes. A row chosen fills the fields with its
// id, count and value. It is drawn on a 640x480 screen of its own in the items page's look: its background, the
// menus' boxes, fonts, colours, gold bar and scroll bar.
class CheatScreen
{
public:
    explicit CheatScreen(Game& game);
    ~CheatScreen();
    CheatScreen(const CheatScreen&) = delete;
    CheatScreen& operator=(const CheatScreen&) = delete;

    bool IsOpen() const;
    // Opens it (the game's frames stop: Game::running 0, as the dialog stops them).
    void Open();
    void Close();
    // An event while it is open (it takes them all).
    void HandleEvent(const SDL_Event& event);
    void Present();

private:
    // The fields (CheatDialog::Fields).
    enum class Field
    {
        Id,
        Count,
        Value,
    };

    // The buttons, left to right.
    enum class Button
    {
        Add,
        Set,
        TopUp,
        Delete,
        Event,
        Close,
    };

    static constexpr int FieldCount = 3;
    static constexpr int ButtonCount = 6;

    // What the status line shows: a button's outcome, its colour.
    enum class Status
    {
        None,
        Done,
        Unchanged,
        Failed,
    };

    void ChooseTab(int tab);
    void Refresh();
    void Select(int row);
    void ScrollTo(int row);
    void Scroll(int rows);
    void Press(Button button);
    void RunEvent();
    void Type(char character);
    void Erase();
    void Click(LogicalPoint at);
    bool FieldEnabled(Field field) const;
    bool ButtonEnabled(Button button) const;
    // The tab, row, field and button under `at` (-1 for none).
    int TabAt(LogicalPoint at) const;
    int RowAt(LogicalPoint at) const;
    int FieldAt(LogicalPoint at) const;
    int ButtonAt(LogicalPoint at) const;
    int ArrowAt(LogicalPoint at) const;
    CheatDialog::Fields CurrentFields() const;

    void Draw();
    void DrawPanel(int left, int top, int right, int bottom);
    void DrawHighlight(int x, int y, int width, int height);
    void DrawTabs();
    void DrawList();
    void DrawFields();
    void DrawButtons();
    void DrawStatus();
    // The width of `text` (Big5) in the medium font.
    int TextWidth(std::string_view text) const;
    Pixel Color(const std::array<u32, 3>& channels) const;

    Game& game;
    std::unique_ptr<CheatDialog> dialog;  // made at the first opening, its tab kept from one opening to the next
    bool open = false;
    Surface canvas;          // what is shown
    ActState backgroundAct;  // the items page's background
    ActState frameAct;       // the menus' panel frame
    std::vector<CheatDialog::Row> rows;
    int selected = -1;  // the row chosen (-1 for none)
    int top = 0;        // the first row shown
    std::array<std::string, FieldCount> fields;
    Field focus = Field::Id;
    Status status = Status::None;
    std::u8string statusText;
    LogicalPoint pointer{-1, -1};  // where the pointer is on the screen
};

}  // namespace swd3e
