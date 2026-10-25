#include "battle/smooth/smooth_messages.h"

#include "battle/battle_hud.h"
#include "battle/battle_message.h"
#include "battle/battle_message_constants.h"
#include "battle/battle_services.h"
#include "battle/battle_state.h"
#include "battle/smooth/smooth_hud.h"
#include "core/format.h"
#include "crt/msvc.h"
#include "draw/paint_record.h"
#include "draw/painter.h"
#include "platform/display.h"
#include "res/act_archive.h"
#include "text/big5.h"
#include "ui/ui_draw.h"

namespace swd3e::smooth
{

namespace
{

constexpr u32 kBlack = 0xFF;

// The message bar at the top of the battle screen.
constexpr int kBarLeft = 0xA;
constexpr int kBarTop = 0xA;
constexpr int kBarWidth = 0x26C;
constexpr int kMessageBarHeight = 0x20;
constexpr int kBarTintRed = -8;
constexpr int kBarTintGreen = -8;
constexpr int kBarTintBlue = 0xA;
constexpr int kBarFrameLeft = kBarLeft + kShadeMargin;
constexpr int kBarFrameTop = kBarTop + kShadeMargin;
constexpr int kBarFrameRight = 0x272;
constexpr int kBarFrameBottom = 0x26;
constexpr int kMessageTextRight = 0x10;  // a message is drawn this far right of and kMessageTextDown below its x, y
constexpr int kMessageTextDown = 6;
constexpr int kLeftTextX = 0x12;
constexpr int kCenterX = 0x13B;
constexpr int kCenteredGlyph = 0x14;  // the glyph width used to centre a message

// Sliding and rising messages stop at kSlideStopX (plus half their width) on a black box kSlideBoxWidth wide.
constexpr int kSlideTextY = 0x10;
constexpr int kSlideStopX = 0xE3;
constexpr int kSlideBoxWidth = 0xB0;
constexpr int kSlideOutStep = 0x50;  // pixels an expired message slides out a frame
constexpr int kSlideGlyph = 0x16;
constexpr int kSlideGlyphs = 8;
constexpr int kRiseTarget = 0x20;     // it drops halfway towards kRiseTarget a frame
constexpr i32 kRiseAcceleration = 2;  // an expired rising message doubles its speed every frame

int MessageHalfWidth(const BattleMessage& message)
{
    return (kSlideGlyph * (kSlideGlyphs - (message.length >> 1))) >> 1;
}

constexpr int kBarHeight = 3;
constexpr double kBarScale = 120.0;  // pixels of a full resistance
constexpr int kBarX = 0x11C;
constexpr int kBoxTextX = 2;
constexpr int kBoxTextY = 4;
constexpr int kTrackLevel = 0x1C;      // the grey track under a positive resistance, and the red level of its bar
constexpr int kBarTrackLength = 0x78;  // the track of a full resistance
constexpr int kCraftBodyBottom = 0xFC;
constexpr int kCraftLineX = 0xD0;
constexpr int kCraftLineY = 0xDE;
constexpr int kCraftTitleX = 0x100;
constexpr int kDarkLevel = 2;
constexpr int kDefeatBodyBottom = 0xF4;
constexpr int kDefeatTitleX = 0x104;
constexpr int kElementNameRise = 8;  // the element name sits this far above its bar
constexpr int kElementNameX = 0xFC;
constexpr int kGrayLevel = 0x10;
constexpr int kGrowDivisor = 3;
constexpr int kLootAdvance = 0x12;
constexpr int kLootRowHeight = 0x14;
constexpr int kLootTitleX = 0x108;
constexpr int kLootX = 0xD2;
constexpr int kMasteryCharWidth = 0x11;  // the advance of its text, and the width of two bytes
constexpr int kMasteryShadeExtra = 0xC;
constexpr int kMasteryShadeMargin = 8;
constexpr int kMasteryShadeY = 0xD0;
constexpr int kMasteryTextExtra = 0x20;
constexpr int kMasteryTextX = 0xD8;
constexpr int kMasteryTextY = 0xDA;
constexpr int kMasteryPartStep = 0x12;  // swd3e: the parts of an evolution are placed 18 pixels per two bytes
constexpr Pixel kMasteryNameColor = 0xF000;
constexpr Pixel kMessageTextColor = 0xFFFF;
constexpr int kMessageAdvance = 0x16;  // swd3e: the advance the messages set on their font
constexpr int kCodedByteWidth = 10;    // a coloured piece moves the next one 11 pixels a byte
constexpr int kDecimalBase = 10;
constexpr char kColorCode = '$';
constexpr float kNeutralBarLength = 2.0F;
constexpr int kPanelAdvance = 0x10;  // the usual glyph advance of the panel font
constexpr int kPanelShadeWidth = 0xB8;
constexpr Pixel kPanelTextColor = 0xFFC0;
constexpr int kResistBodyBottom = 0xDE;
constexpr int kResistBodyTop = 0x7A;
constexpr int kResistFirstBar = 0x82;
constexpr int kResistShadeX = 0xEC;
constexpr int kResistLeft = kResistShadeX + kShadeMargin;
constexpr int kResistNameX = 0x130;
constexpr int kResistRight = 0x1A0;
constexpr int kResistRowHeight = 0x14;
constexpr int kResistTitleBottom = 0x6A;
constexpr int kResistTitleTop = kResistShadeY + kShadeMargin;
constexpr int kResultBodyTop = 0xD4;
constexpr int kResultShadeX = 0xC4;
constexpr int kResultLeft = kResultShadeX + kShadeMargin;
constexpr int kResultRight = 0x178;
constexpr int kResultTitleBottom = 0xC4;
constexpr int kResultTitleTop = kResultShadeY + kShadeMargin;
constexpr double kPercent = static_cast<double>(0.01F);  // swd3e: resistances are in percent (a float 0.01)
constexpr int kTextStyle = TextFont::BoldOutline;
constexpr int kWhiteLevel = 0x1F;
constexpr int kWipedAdvance = 0x11;
constexpr int kWipedX = 0xFE;
constexpr int kWipedY = 0xD8;

// The number a colour code starts with, read as the C runtime's atoi reads it (blanks, a sign, then digits).
int ParseCode(std::string_view text)
{
    std::size_t at = 0;

    while (at < text.size() && (text[at] == ' ' || (text[at] >= '\t' && text[at] <= '\r')))
    {
        ++at;
    }

    const bool negative = at < text.size() && text[at] == '-';

    if (at < text.size() && (text[at] == '-' || text[at] == '+'))
    {
        ++at;
    }

    int value = 0;

    while (at < text.size() && text[at] >= '0' && text[at] <= '9')
    {
        value = value * kDecimalBase + (text[at] - '0');
        ++at;
    }

    return negative ? -value : value;
}
}  // namespace

MessagesFrame::MessagesFrame(BattleMessages& messages)
    : messages(messages)
{
}

// What BattleMessages::Draw decides of the frame: the panel act stepped, the slides moved, the frames counted down,
// the messages gone off the screen freed; the draws in their order into the view.
void MessagesFrame::Advance(MessagesView& view)
{
    BattleMessages& owner = this->messages;

    if (owner.messages == nullptr)
    {
        return;
    }

    auto step = [&view](MessageStep::Kind kind, int x, int y)
    {
        view.steps.push_back(MessageStep{.kind = kind, .x = x, .y = y});
        return &view.steps.back();
    };

    owner.messageFont.SetAdvance(kMessageAdvance);
    // Set once per draw (0x0047938B): a coloured message not starting with a code keeps the colour the one before it
    // left.
    Pixel color = kMessageTextColor;

    for (BattleMessage* message = owner.messages.get(); message != nullptr; message = message->next.get())
    {
        int y = message->y + kMessageTextDown;
        int x = message->x + kMessageTextRight;

        if (message->flags < 0)
        {
            owner.panelAct.id = BattleMessages::PanelAct;
            owner.panelAct.action = 0;
            owner.acts.Run(owner.panelAct);
            step(MessageStep::Kind::Bar, 0, 0)->frame = owner.panelAct.frame;
        }

        if ((message->flags & BattleMessage::BlackBar) != 0)
        {
            step(MessageStep::Kind::BlackBar, 0, message->y);
        }

        const int flags = message->flags;

        if ((flags & BattleMessage::Left) != 0)
        {
            x = kLeftTextX;
        }

        if ((flags & BattleMessage::Centered) != 0)
        {
            x = kCenterX - ((kCenteredGlyph * (message->length >> 1)) >> 1);
        }

        if (message->frames <= 0)
        {
            continue;
        }

        if ((flags & BattleMessage::FromRight) != 0)
        {
            y = kSlideTextY;
            const int half = MessageHalfWidth(*message);

            if (message->slideX == 0)
            {
                message->slideX = Display::Width;
            }

            message->slideX = (half + kSlideStopX + message->slideX) >> 1;
            x = message->slideX;
            step(MessageStep::Kind::Box, x - half, kBarTop);
        }

        if ((message->flags & BattleMessage::FromLeft) != 0)
        {
            y = kSlideTextY;
            const int half = MessageHalfWidth(*message);
            message->slideX = (message->slideX + half + kSlideStopX) >> 1;
            x = message->slideX;
            step(MessageStep::Kind::Box, x - half, kBarTop);
        }

        if ((message->flags & BattleMessage::Rise) != 0)
        {
            x = MessageHalfWidth(*message) + kSlideStopX;
            y = (message->slideY + kRiseTarget) / 2;
            message->slideY = y;
            message->slideX = x;
            step(MessageStep::Kind::Box, kSlideStopX, y - kMessageTextDown);
        }

        if (message->colored != 0)
        {
            // The pieces between the codes; a piece is drawn from a 0x80-byte buffer of spaces the original does not
            // end, the rebuild draws the piece only (deviations.md). A text not starting with a code loses its first
            // byte, as in the original.
            const std::string_view text = message->Text();
            std::size_t at = 0;

            do
            {
                if (text[at] == kColorCode)
                {
                    ++at;
                    const int code = ParseCode(text.substr(at));
                    color = code >= 0 && static_cast<std::size_t>(code) < owner.colors.size()
                                ? owner.colors[static_cast<std::size_t>(code)]
                                : kMessageTextColor;
                }

                ++at;
                std::string piece;

                while (at < text.size() && text[at] != kColorCode)
                {
                    piece.push_back(text[at]);
                    ++at;
                }

                const int bytes = static_cast<int>(piece.size());
                MessageStep* const drawn = step(MessageStep::Kind::Text, x, y);
                drawn->text = std::move(piece);
                drawn->color = color;
                x += bytes + kCodedByteWidth * bytes;
            } while (at < text.size());
        }
        else
        {
            step(MessageStep::Kind::Text, x, y)->text = message->Text();
        }

        --message->frames;
    }

    std::unique_ptr<BattleMessage>* link = &owner.messages;

    while (*link != nullptr)
    {
        BattleMessage& message = **link;
        bool keep = message.frames > 0;

        if (!keep && (message.flags & BattleMessage::FromLeft) != 0)
        {
            const int half = MessageHalfWidth(message);
            message.slideX += kSlideOutStep;
            step(MessageStep::Kind::Box, message.slideX - half, kBarTop);
            step(MessageStep::Kind::Text, message.slideX, kSlideTextY)->text = message.Text();
            keep = message.slideX < Display::Width;
        }

        if (!keep && (message.flags & BattleMessage::FromRight) != 0)
        {
            const int half = MessageHalfWidth(message);
            message.slideX -= kSlideOutStep;
            step(MessageStep::Kind::Box, message.slideX - half, kBarTop);
            step(MessageStep::Kind::Text, message.slideX, kSlideTextY)->text = message.Text();
            keep = message.slideX > 0;
        }

        if (!keep && (message.flags & BattleMessage::Rise) != 0)
        {
            const int half = MessageHalfWidth(message);
            message.slideY -= message.y;
            message.y *= kRiseAcceleration;
            step(MessageStep::Kind::Box, message.slideX - half, message.slideY - kMessageTextDown);
            step(MessageStep::Kind::Text, message.slideX, message.slideY)->text = message.Text();
            keep = message.slideY > kRiseStart;
        }

        if (keep)
        {
            link = &message.next;
            continue;
        }

        if ((message.flags & BattleMessage::Exclusive) != 0)
        {
            --owner.exclusive;
        }

        if (owner.stacked > 0)
        {
            --owner.stacked;
        }

        *link = std::move(message.next);
    }
}

// What BattleMessages::Draw draws, as Advance decided it.
void MessagesFrame::Draw(const MessagesView& view)
{
    BattleMessages& owner = this->messages;

    for (const MessageStep& step : view.steps)
    {
        switch (step.kind)
        {
            case MessageStep::Kind::Bar:
                FillBox(
                    owner.painter,
                    kBarLeft,
                    kBarTop,
                    kBarWidth,
                    kMessageBarHeight,
                    kBarTintRed,
                    kBarTintGreen,
                    kBarTintBlue,
                    FillMode::Tint
                );
                DrawWindowFrame(
                    owner.painter,
                    owner.archive,
                    step.frame,
                    kBarFrameLeft,
                    kBarFrameTop,
                    kBarFrameRight,
                    kBarFrameBottom,
                    0,
                    kFrameBorder
                );
                break;

            case MessageStep::Kind::BlackBar:
                owner.hud.FillColor(kBarLeft, step.y, kBarWidth, kMessageBarHeight, kBlack);
                break;

            case MessageStep::Kind::Box:
                owner.hud.FillColor(step.x, step.y, kSlideBoxWidth, kMessageBarHeight, kBlack);
                break;

            case MessageStep::Kind::Text:
                owner.DrawMessageText(step.x, step.y, step.text, step.color);
                break;
        }
    }
}

void MessagesFrame::Frame(Scene& scene)
{
    MessagesView view;
    this->Advance(view);
    scene.Add(
        [view = std::move(view)](BattleServices& drawing)
        {
            MessagesFrame(drawing.messages).Draw(view);
        }
    );
}

// What BattleMessages::DrawPanelText decides of the frame; the draws into `scene`.
void MessagesFrame::DrawPanelTextAdvance(int x, int y, std::string_view text, Scene& scene)
{
    BattleMessages& messages = this->messages;
    SceneFont(messages.panelFont, scene).DrawText(messages.painter.Target(), x, y, text, kPanelTextColor, kTextStyle);
}

// What BattleMessages::DrawMessageText decides of the frame; the draws into `scene`.
void MessagesFrame::DrawMessageTextAdvance(int x, int y, std::string_view text, Pixel color, Scene& scene)
{
    BattleMessages& messages = this->messages;
    SceneFont(messages.messageFont, scene).DrawText(messages.painter.Target(), x, y, text, color, kTextStyle);
}

// What BattleMessages::DrawMessageBox decides of the frame; the draws into `scene`.
void MessagesFrame::DrawMessageBoxAdvance(
    int x,
    int y,
    int width,
    int height,
    int textX,
    int textY,
    std::string_view text,
    Scene& scene
)
{
    BattleMessages& messages = this->messages;
    messages.panelAct.id = BattleMessages::PanelAct;
    messages.panelAct.action = 0;
    messages.acts.Run(messages.panelAct);
    Box(messages.painter, x, y, width, height, kShadeRed, kShadeGreen, kShadeBlue, FillMode::Blend, scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        x + kShadeMargin,
        y + kShadeMargin,
        x + width,
        y + height,
        0,
        kFrameBorder,
        scene
    );

    if (textX != 0 || textY != 0)
    {
        this->DrawPanelTextAdvance(textX, textY, text, scene);
    }
    else
    {
        this->DrawPanelTextAdvance(x + kBoxTextX, y + kBoxTextY, text, scene);
    }
}

// What BattleMessages::DrawLootPanel decides of the frame; the draws into `scene`.
void MessagesFrame::DrawLootPanelAdvance(Scene& scene)
{
    BattleMessages& messages = this->messages;
    const int bottom = kLootRowHeight * messages.lootCount + kResultBodyTop;
    messages.panelFont.SetAdvance(kLootAdvance);
    const int frame = messages.PanelFrame();
    Box(messages.painter,
        kResultShadeX,
        kResultShadeY,
        kPanelShadeWidth,
        messages.panelHeight + kShadeExtra,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Blend,
        scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        frame,
        kResultLeft,
        kResultTitleTop,
        kResultRight,
        kResultTitleBottom,
        0,
        kFrameBorder,
        scene
    );
    this->DrawPanelTextAdvance(kLootTitleX, kResultTitleTop, BattleMessages::LootTitle, scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        kResultLeft,
        kResultBodyTop,
        kResultRight,
        messages.panelHeight + kResultBodyTop,
        0,
        kFrameBorder,
        scene
    );

    if (messages.GrowPanel(kResultBodyTop, bottom, kGrowDivisor) == 1 && messages.lootCount != 0)
    {
        int y = kResultBodyTop;

        for (int item = 0; item < messages.lootCount; ++item)
        {
            const std::string line = WinFormat(
                BattleMessages::LootLine,
                {messages.lootNames[static_cast<std::size_t>(item)],
                 messages.lootCounts[static_cast<std::size_t>(item)]}
            );
            this->DrawPanelTextAdvance(kLootX, y, line, scene);
            y += kLootRowHeight;
        }
    }

    messages.panelFont.SetAdvance(kPanelAdvance);
}

// What BattleMessages::DrawDefeatPanel decides of the frame; the draws into `scene`.
void MessagesFrame::DrawDefeatPanelAdvance(Scene& scene)
{
    BattleMessages& messages = this->messages;
    const int frame = messages.PanelFrame();
    Box(messages.painter,
        kResultShadeX,
        kResultShadeY,
        kPanelShadeWidth,
        messages.panelHeight + kShadeExtra,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Blend,
        scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        frame,
        kResultLeft,
        kResultTitleTop,
        kResultRight,
        kResultTitleBottom,
        0,
        kFrameBorder,
        scene
    );
    this->DrawPanelTextAdvance(kDefeatTitleX, kResultTitleTop, BattleMessages::DefeatTitle, scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        kResultLeft,
        kResultBodyTop,
        kResultRight,
        messages.panelHeight + kResultBodyTop,
        0,
        kFrameBorder,
        scene
    );

    if (messages.GrowPanel(kResultBodyTop, kDefeatBodyBottom, kGrowDivisor) == 1)
    {
        messages.panelFont.SetAdvance(kWipedAdvance);
        this->DrawPanelTextAdvance(kWipedX, kWipedY, BattleMessages::PartyWiped, scene);
        messages.panelFont.SetAdvance(kPanelAdvance);
    }
}

// What BattleMessages::DrawCraftResultPanel decides of the frame; the draws into `scene`.
void MessagesFrame::DrawCraftResultPanelAdvance(Scene& scene)
{
    BattleMessages& messages = this->messages;
    const int frame = messages.PanelFrame();
    Box(messages.painter,
        kResultShadeX,
        kResultShadeY,
        kPanelShadeWidth,
        messages.panelHeight + kShadeExtra,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Blend,
        scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        frame,
        kResultLeft,
        kResultTitleTop,
        kResultRight,
        kResultTitleBottom,
        0,
        kFrameBorder,
        scene
    );
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        kResultLeft,
        kResultBodyTop,
        kResultRight,
        messages.panelHeight + kResultBodyTop,
        0,
        kFrameBorder,
        scene
    );

    if (messages.GrowPanel(kResultBodyTop, kCraftBodyBottom, kGrowDivisor) != 1)
    {
        return;
    }

    if (messages.craftSucceeded == 1)
    {
        this->DrawPanelTextAdvance(kCraftTitleX, kResultTitleTop, BattleMessages::CraftSuccess, scene);
        const std::string line = WinFormat(BattleMessages::CraftGot, {messages.lootNames[0]});
        this->DrawPanelTextAdvance(kCraftLineX, kCraftLineY, line, scene);
    }
    else
    {
        this->DrawPanelTextAdvance(kCraftTitleX, kResultTitleTop, BattleMessages::CraftFailed, scene);
        this->DrawPanelTextAdvance(kCraftLineX, kCraftLineY, BattleMessages::NothingGained, scene);
    }
}

// What BattleMessages::DrawResistancePanel decides of the frame; the draws into `scene`.
void MessagesFrame::DrawResistancePanelAdvance(Scene& scene)
{
    BattleMessages& messages = this->messages;
    u32 color = 0;
    float length = 0.0F;

    if ((messages.state.panelFlags & BattleState::ResistancePanel) == 0)
    {
        return;
    }

    const int frame = messages.PanelFrame();
    Box(messages.painter,
        kResistShadeX,
        kResistShadeY,
        kPanelShadeWidth,
        messages.panelHeight + kShadeExtra,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Blend,
        scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        frame,
        kResistLeft,
        kResistTitleTop,
        kResistRight,
        kResistTitleBottom,
        0,
        kFrameBorder,
        scene
    );
    this->DrawPanelTextAdvance(kResistNameX, kResistTitleTop, messages.panelUnit->name.data(), scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        kResistLeft,
        kResistBodyTop,
        kResistRight,
        messages.panelHeight + kResistBodyTop,
        0,
        kFrameBorder,
        scene
    );

    if (messages.GrowPanel(kResistBodyTop, kResistBodyBottom, kGrowDivisor) != 1)
    {
        return;
    }

    int y = kResistFirstBar;

    for (std::size_t element = 0; element < BattleMessages::ElementNames.size(); ++element)
    {
        const auto resistance = static_cast<i8>(messages.panelUnit->resistances[element]);

        // A negative resistance sets neither: the bar keeps the colour and length of the element before it. The
        // original also formats "--" or "%2d%%" for each element, never drawn.
        if (resistance == 0)
        {
            color = messages.painter.MakeColor(kGrayLevel, kGrayLevel, kGrayLevel);
            length = kNeutralBarLength;
        }

        if (resistance > 0)
        {
            const u32 track = messages.painter.MakeColor(kTrackLevel, kTrackLevel, kTrackLevel);
            HudFrame(messages.hud).FillColor(kBarX, y, kBarTrackLength, kBarHeight, track, scene);
            color = messages.painter.MakeColor(kTrackLevel, kDarkLevel, kDarkLevel);
            length = static_cast<float>(static_cast<double>(resistance) * kPercent * kBarScale);
        }

        const u32 white = messages.painter.MakeColor(kWhiteLevel, kWhiteLevel, kWhiteLevel);
        SceneFont(messages.panelFont, scene)
            .DrawText(
                messages.painter.Target(),
                kElementNameX,
                y - kElementNameRise,
                BattleMessages::ElementNames[element],
                static_cast<Pixel>(white),
                kTextStyle
            );
        HudFrame(messages.hud).FillColor(kBarX, y, static_cast<i32>(crt::Ftol(length)), kBarHeight, color, scene);
        y += kResistRowHeight;
    }
}

// What BattleMessages::DrawMastery decides of the frame; the draws into `scene`.
void MessagesFrame::DrawMasteryAdvance(Scene& scene)
{
    BattleMessages& messages = this->messages;
    BattleState& state = messages.state;

    if (state.learnedPending != 1)
    {
        return;
    }

    const bool evolved = !state.evolvedName.empty();
    const std::string text = evolved
                                 ? WinFormat(BattleMessages::EvolvedLine, {state.lastLearned.name, state.evolvedName})
                                 : WinFormat(BattleMessages::MasteryLine, {state.lastLearned.name});
    const int halves = static_cast<int>(text.size()) / 2;
    const int width = kMasteryCharWidth * halves + kMasteryTextExtra;
    Box(messages.painter,
        kResultShadeX,
        kMasteryShadeY,
        width + kMasteryShadeExtra,
        messages.panelHeight + kMasteryShadeMargin,
        kShadeRed,
        kShadeGreen,
        kShadeBlue,
        FillMode::Blend,
        scene);
    WindowFrame(
        messages.painter,
        messages.archive,
        messages.panelAct.frame,
        kResultLeft,
        kResultBodyTop,
        width + kResultLeft,
        messages.panelHeight + kResultBodyTop,
        0,
        kFrameBorder,
        scene
    );

    // swd3e: an evolution is drawn in parts every frame, the names coloured, before the panel has grown.
    if (evolved)
    {
        messages.panelFont.SetAdvance(kMasteryCharWidth);
        SceneFont font(messages.panelFont, scene);
        const std::string treasure = Big5(BattleMessages::TreasureWord);
        const std::string evolve = Big5(BattleMessages::EvolveWord);
        int x = kMasteryTextX;
        font.DrawText(messages.painter.Target(), x, kMasteryTextY, treasure, kPanelTextColor, kTextStyle);
        x += kMasteryPartStep * (static_cast<int>(treasure.size()) / 2);
        const std::string& name = state.lastLearned.name;
        font.DrawText(messages.painter.Target(), x, kMasteryTextY, name, kMasteryNameColor, kTextStyle);
        x += kMasteryPartStep * (static_cast<int>(name.size()) / 2);
        font.DrawText(messages.painter.Target(), x, kMasteryTextY, evolve, kPanelTextColor, kTextStyle);
        x += kMasteryPartStep * (static_cast<int>(evolve.size()) / 2);
        font.DrawText(messages.painter.Target(), x, kMasteryTextY, state.evolvedName, kMasteryNameColor, kTextStyle);
    }

    if (messages.GrowPanel(kResultBodyTop, kDefeatBodyBottom, kGrowDivisor) != 1)
    {
        return;
    }

    // The original draws its shared buffer here: after the parts of an evolution it holds only the evolved name
    // (0x00477E41), which then covers the start of the line.
    messages.panelFont.SetAdvance(kMasteryCharWidth);
    this->DrawPanelTextAdvance(kMasteryTextX, kMasteryTextY, evolved ? state.evolvedName : text, scene);
    messages.panelFont.SetAdvance(kPanelAdvance);
}

void MessagesFrame::DrawPanelTextAdvance(int x, int y, std::u8string_view text, Scene& scene)
{
    this->DrawPanelTextAdvance(x, y, Big5(text), scene);
}

void MessagesFrame::DrawMessageBoxAdvance(
    int x,
    int y,
    int width,
    int height,
    int textX,
    int textY,
    std::u8string_view text,
    Scene& scene
)
{
    this->DrawMessageBoxAdvance(x, y, width, height, textX, textY, Big5(text), scene);
}

}  // namespace swd3e::smooth
