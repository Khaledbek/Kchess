#pragma once
// -----------------------------------------------------------------------------
// Section: Generated drill positions
// -----------------------------------------------------------------------------
#include <string>
namespace kchess {
int practice_move_budget(const std::string& drill, int level);
std::string generate_practice_position(const std::string& drill, int level);
bool practice_dead_position(const std::string& fen);
}
