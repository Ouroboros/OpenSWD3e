#pragma once

#include <memory>
#include <vector>

#include "core/types.h"

namespace swd3e
{

class ActArchive;
class AudioManager;
class BattleEnemy;
class BattleRole;
class BattleHud;
class BattleMessages;
class BattleState;
class Config;
class Database;
class ErrorReporter;
struct ItemNode;
class Painter;
class Party;
class Random;
class TswArchive;

namespace crt
{
class Random;
}

namespace smooth
{
class Scene;
class ScenePainter;
}  // namespace smooth

// What the battle code reaches besides the units: shared by the methods of BattleUnit and its kinds.
struct BattleServices
{
    BattleState& state;
    Random& random;
    TswArchive& archive;
    ActArchive& acts;
    Painter& painter;
    AudioManager& audio;
    BattleHud& hud;
    BattleMessages& messages;
    Config& config;
    Party& party;
    crt::Random& crtRandom;  // the C runtime rand (seeded by the explosions)
    u16*& screen;            // Game::drawSurface (0x004CD76C): the locked frame the painter draws to
    ErrorReporter& errors;
    // The rebuild's smooth battle: the scene of the battle frame the parts deciding it add their draws to (drawn once
    // the frame is decided); none: each part's draws are drawn at once, in the original's order.
    smooth::Scene* frame = nullptr;
    // The rebuild's smooth battle: the innermost scene painter alive (the scene a part is adding its draws to now).
    smooth::ScenePainter* recording = nullptr;
    BattleEnemy* enemies = nullptr;  // Game::battleEnemies (BattleState::Enemies of them), set with the services
    BattleRole* roles = nullptr;     // Game::battleRoles (BattleState::RoleUnits of them), set with the services
    Database* database = nullptr;    // Game::database (swd3e: the talisman ghost loads its fed skills in StepGauge)
    // Game::detachedItems: the inventory nodes unlinked by a battle object take (kept as the original leaks them)
    std::vector<std::unique_ptr<ItemNode>>* detachedItems = nullptr;
};

bool AnyRoleBuffed(const BattleServices& services);
bool AllRolesHaveEffect(const BattleServices& services, u32 bits);
void DrawHitPictures(BattleServices& services);
void DrawHitPictures(BattleServices& services, smooth::Scene& scene);

}  // namespace swd3e
