#pragma once

#include <string>

#include "core/types.h"

namespace swd3e
{

class Database;
class ErrorReporter;
class Party;
struct RoleRecord;

// The skill of the last level-up or learning (0x0057747C, 0x18 bytes before noticeText in the original).
struct LearnedSkill
{
    std::string name;
};

bool LevelUp(
    RoleRecord& role,
    int role1,
    int level,
    int& learned,
    Party& party,
    Database& database,
    LearnedSkill& last,
    ErrorReporter& reporter
);
int LearnSkill(int role, i16 id, Party& party, Database& database, LearnedSkill& last, ErrorReporter& reporter);

}  // namespace swd3e
