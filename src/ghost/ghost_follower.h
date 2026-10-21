#pragma once

#include <memory>

#include "core/types.h"
#include "res/act_state.h"

namespace swd3e
{

class ActArchive;
class Painter;
class TswArchive;

// The talisman ghost (符鬼) that follows the controlled role on the map (swd3e; 0x20 bytes at 0x00579A10, made when
// the role enters a city with story flag 0x19 and a ghost chosen). It drifts a step a frame towards a point beside the
// role and fades in over 32 frames, stays 32 and fades out over 32.
class GhostFollower
{
public:
    void Spawn(i32 act, i32 action, float x, float y, float unk14, float unk18);
    void Remove();
    void Follow(float x, float y);
    void Draw(ActArchive& acts, TswArchive& archive, Painter& painter, i32 cameraLeft, i32 cameraTop);

    bool Active() const
    {
        return this->body != nullptr;
    }

private:
    struct Body
    {
        ActState act;     // +0x00 (a pointer to 0xA4 cleared bytes in the original)
        float x = 0;      // +0x04
        float y = 0;      // +0x08
        float stepX = 0;  // +0x0C cleared after every step
        float stepY = 0;  // +0x10
        float unk14 = 0;  // +0x14 set by Spawn (2.0 by LoadCity), no reader known
        float unk18 = 0;  // +0x18 (4.0), no reader known
        i32 phase = 0;    // +0x1C from -0x20 up to 0x40 a step: the fade in, the shown frames and the fade out
    };

    std::unique_ptr<Body> body;  // 0x00579A10
};

}  // namespace swd3e
