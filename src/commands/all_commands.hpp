#pragma once

#include "core/command.hpp"

#include <memory>
#include <vector>

namespace broom {

// Every command the bot ships. Add new commands to the list in all_commands.cpp.
std::vector<std::unique_ptr<Command>> all_commands();

} // namespace broom
