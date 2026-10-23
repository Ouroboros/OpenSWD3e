#pragma once

#include <array>
#include <vector>

#include "battle/smooth/smooth_draw.h"
#include "core/types.h"

namespace swd3e
{

struct ActState;
struct AfterimageTrail;
struct Picture;
class TswArchive;
class BattleUnit;
struct BattleServices;

}  // namespace swd3e

// The rebuild's smooth battle (deviations.md): the battle frame functions split into a part deciding the frame and a
// part drawing it, and what they hand over between them. The original functions run both parts in the original's
// order; the smooth battle draws again between frames.
namespace swd3e::smooth
{

// What a walk (WalkToTarget, ReturnHome) draws, as its frame decides it: the picture at (x, y) with its flags, the
// shadow for the place at shadowY, the overlay of a walk home.
struct WalkView
{
    PictureRef picture;
    std::array<int, 2> place = {};  // the walk's point less the act's anchor: the draws move with it
    const void* who = nullptr;      // the unit
    int x = 0;
    int y = 0;
    u32 flags = 0;
    bool shadow = false;
    int shadowY = 0;
    int shadowTint = 0;
    PictureRef overlay;
    int overlayX = 0;
    int overlayY = 0;
    u32 overlayFlags = 0;
};

// What DrawDamageNumbers draws, in the order it does it: the HUD's digit mode cleared, the fade of the rising digits
// set (it holds for the next picture drawn only), a digit drawn.
struct NumberStep
{
    enum class Kind : u8
    {
        ClearMode,
        Fade,
        Digit,
    };

    Kind kind = Kind::Digit;
    i16 font = 0;
    int value = 0;
    int x = 0;
    int y = 0;
    int fade = 0;
};

struct NumbersView
{
    std::vector<NumberStep> steps;
};

// What DrawGuardText draws: the HUD's digit mode cleared, `letters` letters from `left` at their heights (each after
// the width of the ones before), faded by `fade` while they rise.
struct GuardView
{
    bool clearMode = false;
    bool fading = false;
    int fade = 0;
    int left = 0;
    int letters = 0;
    std::array<int, 4> y = {};
};

// What DrawAction draws: the outline (at the unit's place), the shadow (its top at shadowY less a third of the
// picture), the act at (x, y) (smeared when invisible; blended, waved), the overlay.
struct ActionView
{
    PictureRef picture;
    int x = 0;
    int y = 0;
    u32 flags = 0;
    u32 mirrored = 0;
    bool outline = false;
    int shadowY = 0;
    int shadowTint = 0;
    bool smear = false;
    bool signal = false;
    int alpha = 0;
    int wave = 0;
    PictureRef overlay;
    int overlayX = 0;
    int overlayY = 0;
    u32 overlayFlags = 0;
};

// The parts of the frame functions of `unit` (WalkToTarget, ReturnHome, DrawDamageNumbers, DrawGuardText,
// DrawAction):
// what a frame decides into a view, and the view drawn.
class UnitFrame
{
public:
    explicit UnitFrame(BattleUnit& unit);

    // WalkToTarget: false when it draws nothing.
    bool WalkAdvance(BattleServices& services, const BattleUnit& target, WalkView& view);
    // ReturnHome: its result; the view keeps no picture when it draws nothing.
    int HomeAdvance(BattleServices& services, const BattleUnit& target, i32 id, i32 action, WalkView& view);
    static void DrawWalk(BattleServices& services, const WalkView& view);
    // DrawDamageNumbers: its result.
    int NumbersAdvance(BattleServices& services, NumbersView& view);
    static void DrawNumbers(BattleServices& services, const NumbersView& view);
    // DrawGuardText.
    void GuardAdvance(GuardView& view);
    static void DrawGuard(BattleServices& services, const GuardView& view);
    // PlayIdleEffect, PlayIdleFollowUp.
    void IdleEffectAdvance(BattleServices& services, PictureSteps& steps);
    void FollowUpAdvance(BattleServices& services, PictureSteps& steps);
    // DrawRecoil: its result.
    int RecoilAdvance(BattleServices& services, ActState& act, i16 y, PictureSteps& steps);
    // DrawTrail, RunQueuedActions (its result).
    void TrailAdvance(
        BattleServices& services,
        AfterimageTrail& trail,
        u16 frame,
        u16 picture,
        u32 mode,
        i16 x,
        i16 y,
        PictureSteps& steps
    );
    int QueuedAdvance(BattleServices& services, BattleUnit& target, i32 action, PictureSteps& steps);

    // The same decided into a scene, for the parts that run them.
    void Action(BattleServices& services, const BattleUnit& target, ActState& act, Scene& scene);
    int Recoil(BattleServices& services, ActState& act, i16 y, Scene& scene);
    int Fly(
        BattleServices& services,
        const BattleUnit& target,
        ActState& act,
        int slot,
        i32 id,
        int fromX,
        int fromY,
        int steps,
        i16 delay,
        int backwards,
        Scene& scene
    );
    void Trail(
        BattleServices& services,
        AfterimageTrail& trail,
        u16 frame,
        u16 picture,
        u32 mode,
        i16 x,
        i16 y,
        Scene& scene
    );
    int Queued(BattleServices& services, BattleUnit& target, i32 action, Scene& scene);
    int Home(BattleServices& services, const BattleUnit& target, i32 id, i32 action, Scene& scene);
    // WalkToTarget, DrawDamageNumbers and DrawGuardText into a scene.
    void Walk(BattleServices& services, const BattleUnit& target, Scene& scene);
    int Numbers(BattleServices& services, Scene& scene);
    void Guard(Scene& scene);
    // DrawAction: false when it draws nothing.
    bool ActionAdvance(BattleServices& services, const BattleUnit& target, ActState& act, ActionView& view);
    static void DrawActionView(BattleServices& services, const ActionView& view);

    void DrawActionAtAdvance(BattleServices& services, i16 x, i16 y, ActState& act, Scene& scene);

    int DrawAimedAdvance(
        BattleServices& services,
        BattleUnit& target,
        ActState& act,
        i16 shiftX,
        i16 shiftY,
        Scene& scene
    );

    void DrawMainFadedAdvance(BattleServices& services, int tint, Scene& scene);

    int FadeMainInAdvance(BattleServices& services, Scene& scene);

    int FadeMainOutAdvance(BattleServices& services, Scene& scene);

    int FadeInvisibleAdvance(BattleServices& services, int mode, Scene& scene);

    int DrawFadeInAdvance(BattleServices& services, const BattleUnit& anchor, Scene& scene);

    int DrawFadeOutAdvance(BattleServices& services, Scene& scene);

    void DrawEffectIconsAdvance(BattleServices& services, int x, int y, Scene& scene);

    int RunAwayAdvance(BattleServices& services, Scene& scene);

    void DrawDefeatPieceAdvance(
        BattleServices& services,
        const Picture& picture,
        int left,
        int top,
        int right,
        int bottom,
        int x,
        int y,
        int height,
        Scene& scene
    );

    int DefeatFadeOutAdvance(BattleServices& services, const Picture& picture, Scene& scene);

    int DefeatFillAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatHalvesAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatHopAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatQuartersAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatRisingAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatSinkAdvance(BattleServices& services, const Picture& picture, Scene& scene);

    int DefeatSlideAdvance(BattleServices& services, const Picture& picture, Scene& scene);

    int DefeatSplitColumnsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatSplitRowsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatStripsAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatWaveAdvance(BattleServices& services, const Picture& picture, Scene& scene);

    int RunDefeatParticlesAdvance(BattleServices& services, Scene& scene);

    int DefeatBurstAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatParticlesAdvance(BattleServices& services, const Picture& picture, int x, int y, Scene& scene);

    int DefeatProjectedAdvance(BattleServices& services, const Picture& picture, Scene& scene);

    int DrainEffectFrameAdvance(BattleServices& services, int slot, Scene& scene);

private:
    void ChooseOverlay(BattleServices& services, u16 frame, u16 picture, int offsetX, int offsetY, WalkView& view);

    BattleUnit& unit;
};

}  // namespace swd3e::smooth
