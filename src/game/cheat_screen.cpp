#include "game/cheat_screen.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <span>
#include <string>

#include "battle/battle_message.h"
#include "core/log.h"
#include "font/text_font.h"
#include "game/game.h"
#include "talk/talk.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

// The background: the items page's (Scene::InterfaceAct action 0x34), drawn where the menus draw theirs, which puts
// its 640x480 picture over the whole screen (its anchor is that far right).
constexpr i32 kBackgroundAction = 0x34;
constexpr int kBackgroundX = 0xB4;

// The boxes as the menus draw theirs (Menu::DrawBox): the body darkened a margin past the frame, the frame over it.
constexpr int kBoxMargin = 8;
constexpr int kBoxBorder = static_cast<int>(0x80000000u | kBoxMargin);  // bit 31: the frame only

// A box's frame.
struct Frame
{
    int left;
    int top;
    int right;
    int bottom;
};

constexpr Frame kTabsFrame = {20, 14, 620, 60};
constexpr Frame kListFrame = {20, 84, 500, 384};
constexpr Frame kButtonsFrame = {540, 84, 620, 384};
constexpr Frame kFieldsFrame = {20, 408, 620, 452};

// The tabs: two rows of five, the chosen one a pixel up and left in the text colour, the others dimmed (as the items
// page's mode tabs).
constexpr int kTabLeft = 30;
constexpr int kTabTop = 20;
constexpr int kTabStep = 118;
constexpr int kTabRowStep = 22;
constexpr int kTabsPerRow = 5;
constexpr int kTabHeight = 20;
constexpr int kTabPad = 2;
constexpr int kRaised = -1;
constexpr int kDim = -8;

// The list: a header, then the rows in the items page's fonts (the name large, the numbers medium) with its gold bar
// on the chosen one, the menus' scroll bar to the right of the box.
constexpr int kHeaderTop = 88;
constexpr std::array<int, 4> kColumnX = {28, 200, 290, 360};
constexpr int kRowsTop = 112;
constexpr int kRowStep = 24;
constexpr int kVisibleRows = 11;
constexpr int kNumberDown = 2;  // the numbers' medium font below the name's large one
constexpr int kBarLeft = 4;
constexpr int kBarWidth = 472;
constexpr int kHighlightRed = 0x14;  // the items page's (Menu::ItemsDraw)
constexpr int kHighlightGreen = 0xD;
constexpr int kScrollX = 508;
constexpr int kArrowHeight = 0x10;  // Menu::DrawScrollBar's arrows above and below the bar
constexpr int kArrowLeft = 2;
constexpr int kArrowWidth = 16;

// The buttons down the box at the right, the last at its bottom.
// The rebuild's 劇情 runs the event of the id field (RunEvent).
constexpr std::array<std::u8string_view, 6> kButtonLabels =
    {u8"加入", u8"修改", u8"全滿", u8"刪除", u8"劇情", u8"結束"};
constexpr std::array<int, 6> kButtonTop = {96, 136, 176, 216, 256, 352};
constexpr int kButtonLeft = 544;
constexpr int kButtonWidth = 72;
constexpr int kButtonHeight = 24;
constexpr int kButtonTextDown = 4;

// The fields along the box at the bottom: the label, then the value on a darker inset; the outcome below them.
constexpr int kFieldsTop = 414;
constexpr std::array<int, 3> kFieldLabelX = {28, 196, 372};
constexpr std::array<int, 3> kFieldBoxX = {70, 238, 432};
constexpr std::array<int, 3> kFieldBoxWidth = {80, 110, 80};
constexpr int kFieldBoxHeight = 20;
constexpr int kFieldTextPad = 4;
constexpr std::array<std::u8string_view, 3> kFieldLabels = {u8"號碼", u8"數量", u8"附加值"};  // dialog 0x6A
constexpr std::array<int, 3> kFieldLimits = {
    CheatDialog::IdLimit,
    CheatDialog::CountLimit,
    CheatDialog::ValueLimit,
};

constexpr int kStatusX = 28;
constexpr int kStatusTop = 434;

// The keys at the bottom right in the small font, as the menus' "F1 詳細說明".
constexpr int kHintRight = 620;
constexpr int kHintTop = 464;
constexpr std::u8string_view kHint = u8"左右鍵換頁　上下鍵選擇　Tab 換欄　Enter 修改　Esc 結束";
constexpr std::u8string_view kDone = u8"已完成";
constexpr std::u8string_view kUnchanged = u8"沒有改動（號碼不符或沒有選擇）";
constexpr std::u8string_view kNoEvent = u8"無法執行劇情（號碼不符或劇情進行中）";

// The menus' colours (five bits a channel; Menu::ItemsDraw): the text, the grey of what cannot be used, the red.
constexpr std::array<u32, 3> kTextColor = {0x19, 0x17, 0x11};
constexpr std::array<u32, 3> kGrayColor = {0xD, 0xD, 9};
constexpr std::array<u32, 3> kRedColor = {0x18, 0xA, 0xB};

constexpr int kNoChoice = -1;
constexpr int kLastEvent = 0xFFFE;  // the events a role's u16 event takes (0xFFFF none)
constexpr i32 kNoValue = -1;        // a row without a value (CheatDialog::ObjectCounter)
constexpr int kBlinkMilliseconds = 500;
constexpr std::string_view kCaret = "_";
constexpr char kMinus = '-';

bool Inside(LogicalPoint at, int left, int top, int width, int height)
{
    return at.x >= left && at.x < left + width && at.y >= top && at.y < top + height;
}

// The digit of a key (the row's or the keypad's); 0 when it is none.
char Digit(SDL_Scancode code)
{
    if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9)
    {
        return static_cast<char>('1' + (code - SDL_SCANCODE_1));
    }

    if (code >= SDL_SCANCODE_KP_1 && code <= SDL_SCANCODE_KP_9)
    {
        return static_cast<char>('1' + (code - SDL_SCANCODE_KP_1));
    }

    if (code == SDL_SCANCODE_0 || code == SDL_SCANCODE_KP_0)
    {
        return '0';
    }

    if (code == SDL_SCANCODE_MINUS || code == SDL_SCANCODE_KP_MINUS)
    {
        return kMinus;
    }

    return '\0';
}

}  // namespace

// The rebuild (deviations.md): the cheat screen.
CheatScreen::CheatScreen(Game& game)
    : game(game),
      canvas(Display::Width, Display::Height)
{
}

// The rebuild: nothing to release beyond the members.
CheatScreen::~CheatScreen()
{
}

bool CheatScreen::IsOpen() const
{
    return this->open;
}

void CheatScreen::Open()
{
    Game& owner = this->game;

    if (this->open)
    {
        return;
    }

    if (this->dialog == nullptr)
    {
        this->dialog = std::make_unique<CheatDialog>(owner);
    }

    this->backgroundAct.id = Scene::InterfaceAct;
    this->backgroundAct.action = kBackgroundAction;
    ResetPlayback(this->backgroundAct);
    this->frameAct.id = BattleMessages::MenuPanelAct;
    this->frameAct.action = 0;
    ResetPlayback(this->frameAct);
    this->fields = {};
    this->focus = Field::Id;
    this->status = Status::None;
    this->statusText.clear();
    this->Refresh();
    this->open = true;
    owner.running = 0;
    owner.display.ShowSystemPointer(true);
    LogInfo(u8"cheat screen: opened on tab %d", static_cast<i32>(this->dialog->tab));
}

void CheatScreen::Close()
{
    Game& owner = this->game;

    if (!this->open)
    {
        return;
    }

    this->open = false;
    owner.running = 1;
    owner.display.ShowSystemPointer(false);
    owner.controls.SpendHeld();
    LogInfo(u8"cheat screen: closed");
}

void CheatScreen::HandleEvent(const SDL_Event& event)
{
    Display& display = this->game.display;

    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
        {
            const SDL_Scancode code = event.key.scancode;
            const char digit = Digit(code);

            if (digit != '\0')
            {
                this->Type(digit);
            }
            else if (code == SDL_SCANCODE_ESCAPE && !event.key.repeat)
            {
                this->Close();
            }
            else if ((code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_KP_ENTER) && !event.key.repeat)
            {
                this->Press(Button::Set);
            }
            else if (code == SDL_SCANCODE_BACKSPACE)
            {
                this->Erase();
            }
            else if (code == SDL_SCANCODE_DELETE)
            {
                this->fields[static_cast<std::size_t>(this->focus)].clear();
            }
            else if (code == SDL_SCANCODE_TAB)
            {
                const int step = (event.key.mod & SDL_KMOD_SHIFT) != 0 ? FieldCount - 1 : 1;
                int next = static_cast<int>(this->focus);

                do
                {
                    next = (next + step) % FieldCount;
                } while (!this->FieldEnabled(static_cast<Field>(next)));

                this->focus = static_cast<Field>(next);
            }
            else if (code == SDL_SCANCODE_LEFT || code == SDL_SCANCODE_RIGHT)
            {
                const int step = code == SDL_SCANCODE_LEFT ? CheatDialog::Tabs - 1 : 1;
                this->ChooseTab((this->dialog->tab + step) % CheatDialog::Tabs);
            }
            else if (code == SDL_SCANCODE_UP)
            {
                this->Select(std::max(this->selected - 1, 0));
            }
            else if (code == SDL_SCANCODE_DOWN)
            {
                this->Select(this->selected + 1);
            }
            else if (code == SDL_SCANCODE_PAGEUP)
            {
                this->Select(std::max(this->selected - kVisibleRows, 0));
            }
            else if (code == SDL_SCANCODE_PAGEDOWN)
            {
                this->Select(this->selected + kVisibleRows);
            }
            else if (code == SDL_SCANCODE_HOME)
            {
                this->Select(0);
            }
            else if (code == SDL_SCANCODE_END)
            {
                this->Select(static_cast<int>(this->rows.size()) - 1);
            }

            break;
        }

        case SDL_EVENT_MOUSE_MOTION:
            this->pointer = display.WindowToLogical(event.motion.x, event.motion.y, false);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                this->pointer = display.WindowToLogical(event.button.x, event.button.y, false);
                this->Click(this->pointer);
            }

            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (event.wheel.y != 0.0F)
            {
                this->Scroll(event.wheel.y > 0.0F ? -1 : 1);
            }

            break;

        default:
            break;
    }
}

void CheatScreen::Present()
{
    Game& owner = this->game;
    Painter& painter = owner.painter;
    const auto [clipX, clipY, clipWidth, clipHeight] = painter.ClipRect();
    Pixel* target = painter.Target();

    std::fill_n(this->canvas.Pixels(), Display::Width * Display::Height, Pixel{0});
    painter.SetTarget(this->canvas.Pixels());
    painter.SetClip(0, 0, Display::Width, Display::Height);
    this->Draw();
    painter.SetTarget(target);
    painter.SetClip(clipX, clipY, clipX + clipWidth, clipY + clipHeight);
    owner.display.Present(this->canvas);
}

void CheatScreen::ChooseTab(int tab)
{
    this->dialog->tab = tab;
    this->selected = kNoChoice;
    this->top = 0;
    this->fields = {};

    if (!this->FieldEnabled(this->focus))
    {
        this->focus = Field::Id;
    }

    this->Refresh();
}

// The rows of the tab again; the row chosen kept within them.
void CheatScreen::Refresh()
{
    this->rows = this->dialog->Rows();
    const auto count = static_cast<int>(this->rows.size());

    if (this->selected >= count)
    {
        this->selected = count - 1;
    }

    this->top = std::clamp(this->top, 0, std::max(count - kVisibleRows, 0));
}

// Row `row` chosen (kept within the rows) and the fields filled from it.
void CheatScreen::Select(int row)
{
    const auto count = static_cast<int>(this->rows.size());

    if (count == 0)
    {
        return;
    }

    this->selected = std::clamp(row, 0, count - 1);
    this->ScrollTo(this->selected);
    const CheatDialog::Row& chosen = this->rows[static_cast<std::size_t>(this->selected)];
    this->fields[static_cast<std::size_t>(Field::Id)] = std::to_string(chosen.id);
    this->fields[static_cast<std::size_t>(Field::Count)] = std::to_string(chosen.count);
    this->fields[static_cast<std::size_t>(Field::Value)] =
        this->FieldEnabled(Field::Value) && chosen.value != kNoValue ? std::to_string(chosen.value) : std::string();
}

// The rows shown moved to show row `row`.
void CheatScreen::ScrollTo(int row)
{
    if (row < this->top)
    {
        this->top = row;
    }
    else if (row >= this->top + kVisibleRows)
    {
        this->top = row - kVisibleRows + 1;
    }
}

void CheatScreen::Scroll(int rows)
{
    const auto count = static_cast<int>(this->rows.size());
    this->top = std::clamp(this->top + rows, 0, std::max(count - kVisibleRows, 0));
}

// A button's work done by the cheat dialog's, the rows read again after and the fields cleared as the dialog clears
// them (after adding and setting).
void CheatScreen::Press(Button button)
{
    if (!this->ButtonEnabled(button))
    {
        return;
    }

    CheatDialog& cheat = *this->dialog;
    CheatDialog::EditResult result = CheatDialog::EditResult::Ignored;

    switch (button)
    {
        case Button::Add:
            result = cheat.Add(this->CurrentFields());
            break;

        case Button::Set:
            result = cheat.Set(this->CurrentFields());
            break;

        case Button::TopUp:
        {
            CheatDialog::Row shown;
            result = cheat.TopUp(this->selected, shown);
            break;
        }

        case Button::Delete:
            result = cheat.Delete(this->selected);
            break;

        case Button::Event:
            this->RunEvent();
            return;

        case Button::Close:
            this->Close();
            return;
    }

    switch (result)
    {
        case CheatDialog::EditResult::Ignored:
            this->status = Status::Unchanged;
            this->statusText = kUnchanged;
            return;

        case CheatDialog::EditResult::Applied:
            this->status = Status::Done;
            this->statusText = kDone;
            break;

        case CheatDialog::EditResult::InsertFailed:
            this->status = Status::Failed;
            this->statusText = CheatDialog::InsertFailedText;
            break;

        case CheatDialog::EditResult::DeleteFailed:
            this->status = Status::Failed;
            this->statusText = CheatDialog::DeleteFailedText;
            break;
    }

    if (button == Button::Add || button == Button::Set)
    {
        this->fields = {};
    }

    this->Refresh();
}

// The event of the id field started as a map region starts its event (RoleControl::ControlPlayer): the story's leader
// given it beside the controlled role, run from the next game frame on once the screen closes; refused while an event
// runs.
void CheatScreen::RunEvent()
{
    Game& owner = this->game;
    GameState& state = owner.state;
    int event = 0;

    if (!Talk::ParseNumber(this->fields[static_cast<std::size_t>(Field::Id)], event) || event <= 0 ||
        event > kLastEvent || state.leader.id != Role::NoId)
    {
        this->status = Status::Failed;
        this->statusText = kNoEvent;
        return;
    }

    const Role& player = state.roles[static_cast<std::size_t>(state.playerRole)];
    Role& leader = state.leader;
    leader.eventOffset = 0;
    leader.eventCursor = 0;
    leader.event = static_cast<u16>(event);
    leader.id = TalkWindow::NoRole;
    leader.flags = 0;
    leader.x = player.x;
    leader.y = player.y;
    LogInfo(u8"cheat screen: event %d run", event);
    this->Close();
}

void CheatScreen::Type(char character)
{
    std::string& text = this->fields[static_cast<std::size_t>(this->focus)];

    if (static_cast<int>(text.size()) >= kFieldLimits[static_cast<std::size_t>(this->focus)])
    {
        return;
    }

    text.push_back(character);
}

void CheatScreen::Erase()
{
    std::string& text = this->fields[static_cast<std::size_t>(this->focus)];

    if (!text.empty())
    {
        text.pop_back();
    }
}

void CheatScreen::Click(LogicalPoint at)
{
    if (const int tab = this->TabAt(at); tab != kNoChoice)
    {
        this->ChooseTab(tab);
    }
    else if (const int row = this->RowAt(at); row != kNoChoice)
    {
        this->Select(row);
    }
    else if (const int field = this->FieldAt(at); field != kNoChoice)
    {
        this->focus = static_cast<Field>(field);
    }
    else if (const int button = this->ButtonAt(at); button != kNoChoice)
    {
        this->Press(static_cast<Button>(button));
    }
    else if (const int arrow = this->ArrowAt(at); arrow != 0)
    {
        this->Scroll(arrow);
    }
}

bool CheatScreen::FieldEnabled(Field field) const
{
    return field != Field::Value || this->dialog->EntriesEnabled();
}

// As the dialog: the value field, top up, delete and add only on the lists of objects.
bool CheatScreen::ButtonEnabled(Button button) const
{
    return button == Button::Set || button == Button::Event || button == Button::Close ||
           this->dialog->EntriesEnabled();
}

int CheatScreen::TabAt(LogicalPoint at) const
{
    for (int tab = 0; tab < CheatDialog::Tabs; ++tab)
    {
        const int x = kTabLeft + (tab % kTabsPerRow) * kTabStep - kTabPad;
        const int y = kTabTop + (tab / kTabsPerRow) * kTabRowStep - kTabPad;

        if (Inside(at, x, y, kTabStep - kTabPad, kTabHeight))
        {
            return tab;
        }
    }

    return kNoChoice;
}

int CheatScreen::RowAt(LogicalPoint at) const
{
    if (!Inside(at, kListFrame.left, kRowsTop, kListFrame.right - kListFrame.left, kVisibleRows * kRowStep))
    {
        return kNoChoice;
    }

    const int row = this->top + (at.y - kRowsTop) / kRowStep;
    return row < static_cast<int>(this->rows.size()) ? row : kNoChoice;
}

int CheatScreen::FieldAt(LogicalPoint at) const
{
    for (int field = 0; field < FieldCount; ++field)
    {
        const auto index = static_cast<std::size_t>(field);
        const int width = kFieldBoxX[index] + kFieldBoxWidth[index] - kFieldLabelX[index];

        if (this->FieldEnabled(static_cast<Field>(field)) &&
            Inside(at, kFieldLabelX[index], kFieldsTop - kTabPad, width, kFieldBoxHeight))
        {
            return field;
        }
    }

    return kNoChoice;
}

int CheatScreen::ButtonAt(LogicalPoint at) const
{
    for (int button = 0; button < ButtonCount; ++button)
    {
        if (Inside(at, kButtonLeft, kButtonTop[static_cast<std::size_t>(button)], kButtonWidth, kButtonHeight))
        {
            return button;
        }
    }

    return kNoChoice;
}

// The scroll bar's arrows: -1 above, 1 below, 0 elsewhere.
int CheatScreen::ArrowAt(LogicalPoint at) const
{
    const int x = kScrollX - kArrowLeft;

    if (Inside(at, x, kRowsTop - kArrowHeight, kArrowWidth, kArrowHeight))
    {
        return -1;
    }

    if (Inside(at, x, kRowsTop + kVisibleRows * kRowStep, kArrowWidth, kArrowHeight))
    {
        return 1;
    }

    return 0;
}

CheatDialog::Fields CheatScreen::CurrentFields() const
{
    CheatDialog::Fields current{
        this->fields[static_cast<std::size_t>(Field::Id)],
        this->fields[static_cast<std::size_t>(Field::Count)],
        this->fields[static_cast<std::size_t>(Field::Value)],
    };

    if (!this->FieldEnabled(Field::Value))
    {
        current.value.clear();
    }

    return current;
}

void CheatScreen::Draw()
{
    Game& owner = this->game;
    owner.acts.Run(this->frameAct);
    owner.actDrawer.Draw(this->backgroundAct, kBackgroundX, 0);

    for (const Frame& frame : {kTabsFrame, kListFrame, kButtonsFrame, kFieldsFrame})
    {
        this->DrawPanel(frame.left, frame.top, frame.right, frame.bottom);
    }

    this->DrawTabs();
    this->DrawList();
    this->DrawButtons();
    this->DrawFields();
    this->DrawStatus();
}

// A box as the menus draw theirs (Menu::DrawBox): the body darkened a margin past the frame, the frame over it.
void CheatScreen::DrawPanel(int left, int top, int right, int bottom)
{
    Game& owner = this->game;
    FillBox(
        owner.painter,
        left - kBoxMargin,
        top - kBoxMargin,
        right - left + 2 * kBoxMargin,
        bottom - top + 2 * kBoxMargin,
        0,
        0,
        0,
        FillMode::Quarter
    );
    DrawWindowFrame(owner.painter, owner.archive, this->frameAct.frame, left, top, right, bottom, 0, kBoxBorder);
}

// The gold bar of the menus' chosen row (Menu::ItemsDraw).
void CheatScreen::DrawHighlight(int x, int y, int width, int height)
{
    FillBox(this->game.painter, x, y, width, height, kHighlightRed, kHighlightGreen, 0, FillMode::Gradient);
}

void CheatScreen::DrawTabs()
{
    Game& owner = this->game;
    const CheatDialog& cheat = *this->dialog;
    const Pixel text = this->Color(kTextColor);
    Pixel dim = text;
    owner.painter.TintPixels(std::span<u16>(&dim, 1), kDim, kDim, kDim);
    const int hovered = this->TabAt(this->pointer);

    for (int tab = 0; tab < CheatDialog::Tabs; ++tab)
    {
        const bool chosen = tab == cheat.tab;
        const int raise = chosen ? kRaised : 0;
        const int x = kTabLeft + (tab % kTabsPerRow) * kTabStep + raise;
        const int y = kTabTop + (tab / kTabsPerRow) * kTabRowStep + raise;
        owner.mediumFont.DrawText(
            this->canvas.Pixels(),
            x,
            y,
            cheat.TabLabel(tab),
            chosen || tab == hovered ? text : dim,
            TextFont::BoldShadow
        );
    }
}

void CheatScreen::DrawList()
{
    Game& owner = this->game;
    const CheatDialog& cheat = *this->dialog;
    const Pixel text = this->Color(kTextColor);
    const Pixel gray = this->Color(kGrayColor);
    const int columns = cheat.ColumnCount();

    for (int column = 0; column < columns; ++column)
    {
        const auto index = static_cast<std::size_t>(column);
        owner.mediumFont.DrawText(
            this->canvas.Pixels(),
            kColumnX[index],
            kHeaderTop,
            CheatDialog::ColumnLabels[index],
            gray,
            TextFont::BoldShadow
        );
    }

    const auto count = static_cast<int>(this->rows.size());

    for (int line = 0; line < kVisibleRows && this->top + line < count; ++line)
    {
        const int row = this->top + line;
        const CheatDialog::Row& shown = this->rows[static_cast<std::size_t>(row)];
        const int y = kRowsTop + line * kRowStep;
        const std::string idText = std::to_string(shown.id);
        const std::array<std::string, 4> numbers = {
            std::string(),
            std::to_string(shown.count),
            idText,
            CheatDialog::ValueText(shown.value, shown.maximum, idText),
        };

        owner.largeFont.DrawText(this->canvas.Pixels(), kColumnX[0], y, shown.name, text, TextFont::BoldShadow);

        for (int column = 1; column < columns; ++column)
        {
            const auto index = static_cast<std::size_t>(column);
            owner.mediumFont.DrawText(
                this->canvas.Pixels(),
                kColumnX[index],
                y + kNumberDown,
                numbers[index],
                text,
                TextFont::BoldShadow
            );
        }

        if (row == this->selected)
        {
            this->DrawHighlight(kListFrame.left + kBarLeft, y, kBarWidth, kRowStep);
        }
    }

    // The menus' scroll bar (Menu::DrawScrollBar), which draws its box with the battle messages' panel act: that act
    // is put back as it was.
    if (count > kVisibleRows)
    {
        const ActState panel = owner.battleMessages.panelAct;
        MenuScrollBar bar;
        owner.menu.DrawScrollBar(
            kScrollX,
            kRowsTop,
            kVisibleRows * kRowStep,
            0,
            static_cast<float>(this->top) / static_cast<float>(count),
            static_cast<float>(this->top + kVisibleRows) / static_cast<float>(count),
            bar
        );
        owner.battleMessages.panelAct = panel;
    }
}

void CheatScreen::DrawButtons()
{
    Game& owner = this->game;
    const Pixel text = this->Color(kTextColor);
    const Pixel gray = this->Color(kGrayColor);
    const int hovered = this->ButtonAt(this->pointer);

    for (int button = 0; button < ButtonCount; ++button)
    {
        const auto index = static_cast<std::size_t>(button);
        const bool enabled = this->ButtonEnabled(static_cast<Button>(button));
        const std::string label = Big5(kButtonLabels[index]);
        owner.mediumFont.DrawText(
            this->canvas.Pixels(),
            kButtonLeft + (kButtonWidth - this->TextWidth(label)) / 2,
            kButtonTop[index] + kButtonTextDown,
            label,
            enabled ? text : gray,
            TextFont::BoldShadow
        );

        if (enabled && button == hovered)
        {
            this->DrawHighlight(kButtonLeft, kButtonTop[index], kButtonWidth, kButtonHeight);
        }
    }
}

void CheatScreen::DrawFields()
{
    Game& owner = this->game;
    const Pixel text = this->Color(kTextColor);
    const Pixel gray = this->Color(kGrayColor);
    const bool caretShown = (SDL_GetTicks() / kBlinkMilliseconds) % 2 == 0;

    for (int field = 0; field < FieldCount; ++field)
    {
        const auto index = static_cast<std::size_t>(field);
        const bool enabled = this->FieldEnabled(static_cast<Field>(field));
        const bool focused = enabled && static_cast<Field>(field) == this->focus;
        owner.mediumFont.DrawText(
            this->canvas.Pixels(),
            kFieldLabelX[index],
            kFieldsTop,
            kFieldLabels[index],
            enabled ? text : gray,
            TextFont::BoldShadow
        );
        FillBox(
            owner.painter,
            kFieldBoxX[index],
            kFieldsTop - kTabPad,
            kFieldBoxWidth[index],
            kFieldBoxHeight,
            0,
            0,
            0,
            FillMode::Quarter
        );

        if (!enabled)
        {
            continue;
        }

        const std::string& shown = this->fields[index];
        const int x = kFieldBoxX[index] + kFieldTextPad;
        owner.mediumFont.DrawText(this->canvas.Pixels(), x, kFieldsTop, shown, text, TextFont::BoldShadow);

        if (focused)
        {
            this->DrawHighlight(kFieldBoxX[index], kFieldsTop - kTabPad, kFieldBoxWidth[index], kFieldBoxHeight);

            if (caretShown)
            {
                owner.mediumFont.DrawText(
                    this->canvas.Pixels(),
                    x + this->TextWidth(shown),
                    kFieldsTop,
                    kCaret,
                    text,
                    TextFont::BoldShadow
                );
            }
        }
    }
}

void CheatScreen::DrawStatus()
{
    Game& owner = this->game;

    if (this->status != Status::None)
    {
        const std::array<u32, 3>& shade =
            this->status == Status::Done ? kTextColor : (this->status == Status::Failed ? kRedColor : kGrayColor);
        owner.mediumFont.DrawText(
            this->canvas.Pixels(),
            kStatusX,
            kStatusTop,
            std::u8string_view(this->statusText),
            this->Color(shade),
            TextFont::BoldShadow
        );
    }

    const std::string hint = Big5(kHint);
    const int width = static_cast<int>(hint.size()) * (owner.smallFont.Advance() / 2);
    owner.smallFont.DrawText(
        this->canvas.Pixels(),
        kHintRight - width,
        kHintTop,
        hint,
        this->Color(kTextColor),
        TextFont::BoldShadow
    );
}

int CheatScreen::TextWidth(std::string_view text) const
{
    return static_cast<int>(text.size()) * (this->game.mediumFont.Advance() / 2);
}

Pixel CheatScreen::Color(const std::array<u32, 3>& channels) const
{
    return static_cast<Pixel>(this->game.painter.MakeColor(channels[0], channels[1], channels[2]));
}

}  // namespace swd3e
