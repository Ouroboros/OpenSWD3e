#include "battle/battle_message.h"
#include "text/big5.h"

#include <array>
#include <cstring>
#include <string>

#include "battle/battle_hud.h"
#include "battle/battle_message_constants.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/smooth/smooth_draw.h"
#include "battle/smooth/smooth_messages.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "draw/paint_record.h"
#include "draw/painter.h"
#include "font/text_font.h"
#include "org/object_record.h"
#include "platform/display.h"
#include "res/act_archive.h"
#include "ui/ui_draw.h"

namespace swd3e
{

namespace
{

// The shade behind every panel: a blend towards (kShadeRed, kShadeGreen, kShadeBlue), kShadeExtra taller than the
// grown body; the frames sit kShadeMargin inside it.

// The result panels (loot, defeat, craft): a title frame and a body frame growing below it.

// DrawMastery.

// DrawMessageBox: the text without a position starts this far inside the box.

// The resistance panel: the unit name over one bar per element.

constexpr i32 kRiseSpeed = 1;              // the speed a rising message starts with
constexpr i32 kStackStep = 0x20;           // a Stacked message sits this much lower per Centered one shown
constexpr std::size_t kCodeSearch = 0xFF;  // the bytes Add looks through for colour codes
constexpr char kColorCode = '$';           // a colour code: $ and its digit
constexpr i32 kCodeBytes = 2;
constexpr int kWhite = 0x1F;  // the colours of the codes: white, light yellow, orange and red
constexpr int kLightGreen = 0x1D;
constexpr int kLightBlue = 0x17;
constexpr int kOrangeGreen = 0x11;

}  // namespace

BattleMessages::BattleMessages(
    Painter& painter,
    TswArchive& archive,
    ActArchive& acts,
    BattleHud& hud,
    BattleState& state,
    TextFont& panelFont,
    TextFont& messageFont
)
    : painter(painter),
      archive(archive),
      acts(acts),
      hud(hud),
      state(state),
      panelFont(panelFont),
      messageFont(messageFont)
{
}

void BattleMessages::Attach(BattleServices& battle)
{
    this->services = &battle;
}

// The frame picture of the battle panels: act 0x2396, action 0.
int BattleMessages::PanelFrame()
{
    this->panelAct.id = BattleMessages::PanelAct;
    this->panelAct.action = 0;
    this->acts.Run(this->panelAct);
    return this->panelAct.frame;
}

void BattleMessages::DrawPanelText(int x, int y, std::string_view text)
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawPanelTextAdvance(x, y, text, scene);
    scene.Show(*this->services);
}

void BattleMessages::DrawPanelText(int x, int y, std::u8string_view text)
{
    this->DrawPanelText(x, y, Big5(text));
}

void BattleMessages::DrawMessageText(int x, int y, std::string_view text, Pixel color)
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawMessageTextAdvance(x, y, text, color, scene);
    scene.Show(*this->services);
}

std::string_view BattleMessage::Text() const
{
    const std::string_view all(this->source.data(), this->source.size());
    return all.substr(0, all.find('\0'));
}

// swd3e.exe 0x00478070: the "戰利品" panel with one "%-12s X %2d" line per item
void BattleMessages::DrawLootPanel()
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawLootPanelAdvance(scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x00478210: the "戰鬥失敗" panel with "隊伍全滅!!"
void BattleMessages::DrawDefeatPanel()
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawDefeatPanelAdvance(scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x00478330: "煉符成功" with "得到符咒:%s" of the first loot name, or "煉符失敗" with "沒有得到東西"
void BattleMessages::DrawCraftResultPanel()
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawCraftResultPanelAdvance(scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x004784D0: a shaded framed box (the frame to its right and bottom edges); the text goes to (textX, textY)
// unless both are 0, then to (x + 2, y + 4)
void BattleMessages::DrawMessageBox(int x, int y, int width, int height, int textX, int textY, std::string_view text)
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawMessageBoxAdvance(x, y, width, height, textX, textY, text, scene);
    scene.Show(*this->services);
}

// DrawMessageBox with a UTF-8 text.
void BattleMessages::DrawMessageBox(int x, int y, int width, int height, int textX, int textY, std::u8string_view text)
{
    this->DrawMessageBox(x, y, width, height, textX, textY, Big5(text));
}

// swd3e.exe 0x00478880: a small framed box (0x34 square) shaded a quarter dark at x less the menu slide (halved first
// while it lasts); its x
int BattleMessages::DrawSmallPanel(int x, int y, smooth::Scene& scene)
{
    return this->DrawSlidPanel(x, y, kSmallPanelSide, kSmallPanelSide, kSmallFrameRight, kSmallFrameBottom, scene);
}

// swd3e.exe 0x004787F0: the same 0x64 wide and 0xC8 high
int BattleMessages::DrawTallPanel(int x, int y, smooth::Scene& scene)
{
    return this->DrawSlidPanel(x, y, kTallPanelWidth, kTallPanelHeight, kTallFrameRight, kTallFrameBottom, scene);
}

// The box of DrawSmallPanel and DrawTallPanel (inline in each): the panel act stepped, the box at x less the slide.
int BattleMessages::DrawSlidPanel(
    int x,
    int y,
    int width,
    int height,
    int frameRight,
    int frameBottom,
    smooth::Scene& scene
)
{
    BattleState& battle = this->services->state;
    this->panelAct.id = BattleMessages::PanelAct;
    this->panelAct.action = 0;
    this->acts.Run(this->panelAct);

    if (battle.menuSlide > 0)
    {
        battle.menuSlide /= 2;
    }

    const int left = x - battle.menuSlide;
    smooth::Box(this->painter, left, y, width, height, kShadeRed, kShadeGreen, kShadeBlue, FillMode::Quarter, scene);
    smooth::WindowFrame(
        this->painter,
        this->archive,
        this->panelAct.frame,
        left + kShadeMargin,
        y + kShadeMargin,
        left + frameRight,
        y + frameBottom,
        0,
        kFrameBorder,
        scene
    );
    return left;
}

// swd3e.exe 0x00478900: a framed box 0x82 wide and 0x1E high shaded a quarter dark at y less the menu slide (halved
// first while it lasts); true once the slide is over
bool BattleMessages::DrawWidePanel(int x, int y, smooth::Scene& scene)
{
    BattleState& battle = this->services->state;
    this->panelAct.id = BattleMessages::PanelAct;
    this->panelAct.action = 0;
    this->acts.Run(this->panelAct);

    if (battle.menuSlide > 0)
    {
        battle.menuSlide /= 2;
    }

    const int top = y - battle.menuSlide;
    smooth::Box(
        this->painter,
        x,
        top,
        kWidePanelWidth,
        kWidePanelHeight,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Quarter,
        scene
    );
    smooth::WindowFrame(
        this->painter,
        this->archive,
        this->panelAct.frame,
        x + kShadeMargin,
        top + kShadeMargin,
        x + kWideFrameRight,
        top + kWideFrameBottom,
        0,
        kFrameBorder,
        scene
    );
    return battle.menuSlide <= 0;
}

// swd3e.exe 0x00478590: grows the panel by 1 / divisor of the rest; 1 once it stopped
int BattleMessages::GrowPanel(int top, int bottom, int divisor)
{
    const int step = (bottom - this->panelHeight - top) / divisor;
    this->panelHeight += step;
    return step == 0 ? 1 : 0;
}

// swd3e.exe 0x004785C0: the name and the five element resistances (percent) of the panel object as bars over a grey
// track, while bit 5 of the panel flags is set (a negative one repeats the bar before it).
void BattleMessages::DrawResistancePanel()
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawResistancePanelAdvance(scene);
    scene.Show(*this->services);
}

// swd3e.exe 0x00479220: appends a message unless an Exclusive one shows (the text is kept by address). Rise starts
// with speed 1 at y -32; a Stacked one is placed 0x20 lower per Centered one shown; Centered and Exclusive ones are
// counted. Each '$' in the first 0xFF bytes marks the text coloured and takes two bytes off its length; the colours
// of the codes are made anew.
void BattleMessages::Add(int x, int y, i16 frames, std::span<const char> text, int flags)
{
    if (this->exclusive > 0)
    {
        return;
    }

    auto message = std::make_unique<BattleMessage>();
    message->source = text;
    this->Append(std::move(message), x, y, frames, flags);
}

// A constant text (the original passes the address of the string in the executable).
void BattleMessages::Add(int x, int y, i16 frames, std::u8string_view text, int flags)
{
    if (this->exclusive > 0)
    {
        return;
    }

    auto message = std::make_unique<BattleMessage>();
    message->constant = Big5(text);
    message->source = message->constant;
    this->Append(std::move(message), x, y, frames, flags);
}

void BattleMessages::Append(std::unique_ptr<BattleMessage> message, int x, int y, i16 frames, int flags)
{
    message->x = x;
    message->y = y;
    message->frames = frames;
    message->length = static_cast<i32>(message->Text().size());
    message->flags |= flags;

    if ((flags & BattleMessage::Rise) != 0)
    {
        message->y = kRiseSpeed;
        message->slideY = kRiseStart;
    }

    if ((message->flags & BattleMessage::Stacked) != 0)
    {
        message->y += kStackStep * this->stacked;
    }

    if ((message->flags & BattleMessage::Centered) != 0)
    {
        ++this->stacked;
    }

    if ((flags & BattleMessage::Exclusive) != 0)
    {
        ++this->exclusive;
    }

    const std::string_view text = message->Text();

    for (std::size_t at = 0; at < kCodeSearch && at < text.size(); ++at)
    {
        if (text[at] == kColorCode)
        {
            message->colored = 1;
            message->length -= kCodeBytes;
        }
    }

    this->colors = {
        static_cast<u16>(this->painter.MakeColor(kWhite, kWhite, kWhite)),
        static_cast<u16>(this->painter.MakeColor(kWhite, kLightGreen, kLightBlue)),
        static_cast<u16>(this->painter.MakeColor(kWhite, kOrangeGreen, 0)),
        static_cast<u16>(this->painter.MakeColor(kWhite, 0, 0)),
    };

    std::unique_ptr<BattleMessage>* link = &this->messages;

    while (*link != nullptr)
    {
        link = &(*link)->next;
    }

    *link = std::move(message);
}

// The rebuild: the list dropped as Game::EndBattle zeroes its head in the original (0x00465CD9), the messages freed
// instead of leaked; the Centered and Exclusive counts are kept for the next battle.
void BattleMessages::Drop()
{
    while (this->messages != nullptr)
    {
        this->messages = std::move(this->messages->next);
    }
}

// swd3e.exe 0x00479820: every message freed; with any freed, the Centered and Exclusive counts cleared
void BattleMessages::Clear()
{
    if (this->messages == nullptr)
    {
        return;
    }

    while (this->messages != nullptr)
    {
        this->messages = std::move(this->messages->next);
    }

    this->exclusive = 0;
    this->stacked = 0;
}

// swd3e.exe 0x00479360: draws the messages (the message font at advance 0x16) and counts their frames down; a coloured
// one in pieces between its $n codes, 11 pixels a byte; expired ones slide out and are freed once off screen (the
// Exclusive and Centered counts going down), drawn whole.
void BattleMessages::Draw()
{
    smooth::MessagesView view;
    smooth::MessagesFrame(*this).Advance(view);
    smooth::MessagesFrame(*this).Draw(view);
}

// swd3e.exe 0x00477C40: while a proficiency has just filled (learnedPending), the box "法寶%s已完全成長!!" with the
// accessory's name, or "法寶[%s]轉化為[%s]!!" with the object it turned into (evolvedName): a shade and a frame as wide
// as the text (17 pixels per two bytes); an evolution is drawn every frame in parts (法寶, the name, 轉化為, the new
// name, the names in 0xF000, 18 pixels per two bytes); grown open (GrowPanel), the whole text at a 17-pixel advance.
void BattleMessages::DrawMastery()
{
    smooth::Scene scene;
    smooth::MessagesFrame(*this).DrawMasteryAdvance(scene);
    scene.Show(*this->services);
}
}  // namespace swd3e
