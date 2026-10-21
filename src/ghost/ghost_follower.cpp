#include "ghost/ghost_follower.h"

#include <cmath>

#include "crt/msvc.h"
#include "draw/painter.h"
#include "pic/picture.h"
#include "res/act_archive.h"
#include "res/tsw_archive.h"

namespace swd3e
{

namespace
{

constexpr i32 kFadeFrames = 0x20;
constexpr i32 kFirstPhase = -kFadeFrames;
constexpr i32 kLastPhase = 2 * kFadeFrames;
constexpr int kOpaque = 0x10;  // DrawOptions::alpha while shown
constexpr int kFadeDivisor = 2;

}  // namespace

// swd3e.exe 0x0049E5D0: a new ghost (the one before freed) of act `act` and action `action` at (x, y), its phase at
// the start of the fade in
void GhostFollower::Spawn(i32 act, i32 action, float x, float y, float unk14, float unk18)
{
    this->Remove();
    this->body = std::make_unique<Body>();
    this->body->act.id = act;
    this->body->act.action = action;
    this->body->x = x;
    this->body->y = y;
    this->body->unk14 = unk14;
    this->body->unk18 = unk18;
    this->body->phase = kFirstPhase;
}

// swd3e.exe 0x0049E660
void GhostFollower::Remove()
{
    this->body.reset();
}

// swd3e.exe 0x0049E6A0: a step of length one towards (x, y) (none of a definite length on the point itself, where the
// original divides by zero and the position becomes NaN as here); the phase goes on up to 0x40. The original keeps
// the differences in x87 registers; here they are doubles.
void GhostFollower::Follow(float x, float y)
{
    Body* ghost = this->body.get();

    if (ghost == nullptr)
    {
        return;
    }

    const double dx = static_cast<double>(x) - ghost->x;
    const double dyWide = static_cast<double>(y) - ghost->y;
    const auto dy = static_cast<float>(dyWide);
    const double scale = 1.0 / std::sqrt(dyWide * dy + dx * dx);
    const auto scaleNarrow = static_cast<float>(scale);
    ghost->stepX = static_cast<float>(scale * dx + ghost->stepX);
    ghost->stepY = static_cast<float>(static_cast<double>(scaleNarrow) * dy + ghost->stepY);
    ghost->x = ghost->stepX + ghost->x;
    ghost->y = ghost->stepY + ghost->y;
    ghost->stepX = 0;
    ghost->stepY = 0;

    if (++ghost->phase >= kLastPhase)
    {
        ghost->phase = kLastPhase;
    }
}

// swd3e.exe 0x0049E740: the ghost's act steps and its picture is drawn translucent at its place on the screen: fading
// in while the phase is below 0, opaque up to 0x20, fading out to 0x40 (then unseen)
void GhostFollower::Draw(ActArchive& acts, TswArchive& archive, Painter& painter, i32 cameraLeft, i32 cameraTop)
{
    Body* ghost = this->body.get();

    if (ghost == nullptr)
    {
        return;
    }

    acts.Run(ghost->act);
    const Picture* picture = archive.GetPicture(ghost->act.frame, ghost->act.picture);

    // The original takes the picture without a check.
    if (picture == nullptr)
    {
        return;
    }

    DrawOptions& options = painter.Options();
    options.alpha = kOpaque;
    painter.SetSource(picture->data.data());
    const i32 phase = ghost->phase;

    if (phase < 0)
    {
        options.alpha = (phase + kFadeFrames) / kFadeDivisor;
    }
    else if (phase >= kFadeFrames)
    {
        options.alpha = (kLastPhase - phase) / kFadeDivisor;
    }

    const auto top = static_cast<int>(
        crt::Ftol(static_cast<double>(ghost->y) - static_cast<double>(cameraTop) - static_cast<double>(ghost->act.y))
    );
    const auto left = static_cast<int>(
        crt::Ftol(static_cast<double>(ghost->x) - static_cast<double>(cameraLeft) - static_cast<double>(ghost->act.x))
    );
    painter.DrawPicture(left, top, picture->width, picture->height, Painter::Translucent);
}

}  // namespace swd3e
