#include "game/scene.h"

namespace swd3e
{

namespace
{

constexpr i16 kHeatLength = 4;  // Scene::heatLengths after ResetEffects

}  // namespace

// swd3e.exe 0x0040F840
void Scene::FreeEffects()
{
    this->effects.reset();
}

// swd3e.exe 0x0040F880
void Scene::FreeSpritesA()
{
    this->spritesA.reset();
}

// swd3e.exe 0x0040F8B0
void Scene::FreeSpritesB()
{
    this->spritesB.reset();
}

// swd3e.exe 0x0040F8E0: the talk windows freed; only HoldLocked of playerHold (0x004C2FF8) kept
void Scene::FreeNodes()
{
    this->talkWindows.reset();
    this->playerHold &= Scene::HoldLocked;
}

// swd3e.exe 0x0040F930
void Scene::FreeAnimations()
{
    this->animationsA.reset();
    this->animationsB.reset();
}

// swd3e.exe 0x0040F980
void Scene::FreeMarks()
{
    for (RoleMark& mark : this->marks)
    {
        mark = RoleMark{};
    }
}

// swd3e.exe 0x0040F9C0
void Scene::ResetLeaves()
{
    for (MapParticle& leaf : this->leaves)
    {
        leaf.x = MapParticle::Unplaced;
    }
}

// swd3e.exe 0x0040F9E0: each object is destroyed (0x00435BF0) and freed
void Scene::FreeObjects()
{
    this->objects.reset();
}

// swd3e.exe 0x00411C80: the effects of the scene ended before a game is loaded: the lists freed, the fade stopped (its
// steps 0), the quake cleared and the heat shimmer reset (the original clears 0x100 bytes from 0x004C6154: past
// heatOffsets only bytes no code reads)
void Scene::ResetEffects()
{
    this->FreeEffects();
    this->FreeSpritesA();
    this->FreeSpritesB();
    this->FreeNodes();
    this->FreeAnimations();
    this->FreeMarks();
    this->ResetLeaves();
    this->fade.stepBlue = 0;
    this->shake.fill(Scene::ShakeEnd);
    this->fade.stepGreen = 0;
    this->heatCounts.fill(1);
    this->fade.stepRed = 0;
    this->heatLengths.fill(kHeatLength);
    this->heatFrame = 0;
    this->heatOffsets.fill(0);
}

}  // namespace swd3e
