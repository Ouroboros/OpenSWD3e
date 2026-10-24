#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "core/types.h"
#include "res/act_state.h"

namespace swd3e
{

namespace smooth
{
class MessagesFrame;
class Scene;
}  // namespace smooth

class ActArchive;
class BattleHud;
class BattleState;
class Painter;
class TextFont;
class TswArchive;
struct BattleServices;
struct ObjectData;

// A floating text of the battle (a 0x24-byte list node in the original). The text is not copied: messages formatted
// into the shared battle text show whatever it holds when they are drawn.
struct BattleMessage
{
    static constexpr i32 Left = 1;               // drawn at x 0x12
    static constexpr i32 Centered = 2;           // centred on x 0x13B
    static constexpr i32 FromRight = 0x10;       // slides in from the right, leaves to the left
    static constexpr i32 FromLeft = 0x20;        // slides in, leaves to the right
    static constexpr i32 Rise = 0x40;            // drops in from the top, leaves upwards (y holds the speed)
    static constexpr i32 Stacked = 0x1000000;    // swd3e: placed 0x20 lower per message counted in `stacked`
    static constexpr i32 Exclusive = 0x2000000;  // swd3e: no other message is added while it shows
    static constexpr i32 BlackBar = 0x40000000;  // a black bar behind the text
    static constexpr i32 Frame = static_cast<i32>(0x80000000u);  // a framed box behind the text

    std::unique_ptr<BattleMessage> next;  // +0x00
    i32 x = 0;                            // +0x04
    i32 y = 0;                            // +0x08 the rise speed with Rise
    i32 length = 0;                       // +0x0C bytes of the text
    i32 slideX = 0;                       // +0x10
    i32 slideY = 0;                       // +0x14
    i32 flags = 0;                        // +0x18
    i16 frames = 0;                       // +0x1C frames left before leaving
    u16 colored = 0;                      // +0x20 (swd3e) 1: the text has $n colour codes
    // +0x24 the text. The original keeps the pointer it is given, so a buffer rewritten while the message shows draws
    // its new text; a constant text is kept in `constant`.
    std::span<const char> source;
    std::string constant;

    // The text up to its NUL.
    std::string_view Text() const;
};

// The battle messages and the result panels of a battle (0x00469080..0x00469960).
class BattleMessages
{
    friend class smooth::MessagesFrame;  // the rebuild's smooth battle: Draw split (smooth/smooth_messages.h)

public:
    // The tint of a panel by element (Game::DrawObjectStats, Game::DrawTargetTag).
    struct ElementTint
    {
        i32 red = 0;
        i32 green = 0;
        i32 blue = 0;
    };

    // swd3e.exe 0x004BF674: by element number (0: none, then 金 木 水 火 土)
    static constexpr std::array<ElementTint, 6> ElementTints = {{
        {-8, -8, 1},
        {0x1F, 0x1F, 1},
        {0xE, 0x12, 7},
        {2, 0x18, 0x1F},
        {0x1F, 0xD, 9},
        {0x15, 0xE, 8},
    }};

    // The texts of the battle (Big5 in the executable; UTF-8 here, converted where drawn).
    // 0x004BF884
    static constexpr std::string_view LootLine = "%-12s X %2d";
    static constexpr std::u8string_view LootTitle = u8"戰利品";                  // 0x004BF890
    static constexpr std::u8string_view PartyWiped = u8"隊伍全滅!!";             // 0x004BF898
    static constexpr std::u8string_view DefeatTitle = u8"戰鬥失敗";              // 0x004BF8A4
    static constexpr std::u8string_view NothingGained = u8"沒有得到東西";        // 0x004BF8B0
    static constexpr std::u8string_view CraftFailed = u8"煉符失敗";              // 0x004BF8C0
    static constexpr std::u8string_view CraftGot = u8"得到符咒:%s";              // 0x004BF8CC
    static constexpr std::u8string_view CraftSuccess = u8"煉符成功";             // 0x004BF8D8
    static constexpr std::u8string_view MasteryLine = u8"法寶%s已完全成長!!";    // 0x004BF870
    static constexpr std::u8string_view EvolvedLine = u8"法寶[%s]轉化為[%s]!!";  // 0x004BF858 (swd3e)
    static constexpr std::u8string_view TreasureWord = u8"法寶";                 // 0x004BEB60 (swd3e)
    static constexpr std::u8string_view EvolveWord = u8"轉化為";                 // 0x004BF850 (swd3e)

    // 0x004BF61C: the names of the five elements (pointers to the strings in the original)
    static constexpr std::array<std::u8string_view, 5> ElementNames = {
        u8"金",  // 0x004BF73C
        u8"木",  // 0x004BF738
        u8"水",  // 0x004BF734
        u8"火",  // 0x004BED3C
        u8"土",  // 0x004BED38
    };

    static constexpr int LootSlots = 10;
    static constexpr int PanelAct = 0x2396;      // swd3e: the frame of the battle panels and lists (0x00473880...)
    static constexpr int MenuPanelAct = 0x233B;  // the frame of the menu panels (0x0043E900, 0x004412E0, 0x00442E10)

    BattleMessages(
        Painter& painter,
        TswArchive& archive,
        ActArchive& acts,
        BattleHud& hud,
        BattleState& state,
        TextFont& panelFont,
        TextFont& messageFont
    );

    // The rebuild's smooth battle: the services its draws are handed to (BattleServices::frame), set once the battle's
    // services are built.
    void Attach(BattleServices& battle);

    void DrawLootPanel();
    void DrawDefeatPanel();
    void DrawCraftResultPanel();
    void DrawMessageBox(int x, int y, int width, int height, int textX, int textY, std::string_view text);
    void DrawMessageBox(int x, int y, int width, int height, int textX, int textY, std::u8string_view text);
    int DrawSmallPanel(int x, int y, smooth::Scene& scene);
    int DrawTallPanel(int x, int y, smooth::Scene& scene);
    int DrawSlidPanel(int x, int y, int width, int height, int frameRight, int frameBottom, smooth::Scene& scene);
    bool DrawWidePanel(int x, int y, smooth::Scene& scene);
    int GrowPanel(int top, int bottom, int divisor);
    void DrawResistancePanel();
    void DrawMastery();
    void Add(int x, int y, i16 frames, std::span<const char> text, int flags);
    void Add(int x, int y, i16 frames, std::u8string_view text, int flags);
    void Draw();
    void Clear();

    std::unique_ptr<BattleMessage> messages;  // 0x00537388
    i32 stacked = 0;    // 0x00577A7C (swd3e) the Centered messages shown, which Stacked ones are placed below
    i32 exclusive = 0;  // 0x00577A80 (swd3e) the Exclusive messages shown
    std::array<u16, 4> colors = {};     // 0x00577A74 (swd3e) the colours of the $0..$3 codes, made by Add
    std::array<i32, 8> unk5214FC = {};  // cleared with messages by Game::EndBattle
    ActState panelAct;                  // 0x0050AF70 act 0x233B: the frame picture of the panels
    i32 panelHeight = 0;                // 0x00577A48 grown by GrowPanel
    u16 lootCount = 0;                  // 0x005777EC
    std::array<std::string_view, LootSlots> lootNames = {};  // 0x00537970
    std::array<u16, LootSlots> lootCounts = {};              // 0x0053949C
    u8 craftSucceeded = 0;                                   // 0x00577998
    const ObjectData* panelUnit = nullptr;                   // 0x00577A44 the object shown by DrawResistancePanel

private:
    int PanelFrame();
    void Append(std::unique_ptr<BattleMessage> message, int x, int y, i16 frames, int flags);
    void DrawPanelText(int x, int y, std::string_view text);
    void DrawPanelText(int x, int y, std::u8string_view text);
    void DrawMessageText(int x, int y, std::string_view text, Pixel color);

    Painter& painter;
    TswArchive& archive;
    ActArchive& acts;
    BattleHud& hud;
    BattleState& state;
    TextFont& panelFont;    // 0x004E5830 (16 pixels)
    TextFont& messageFont;  // 0x004C5130 (20 pixels)
    BattleServices* services = nullptr;
};

}  // namespace swd3e
