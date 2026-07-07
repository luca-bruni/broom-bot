#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

#include <memory>
#include <vector>

namespace broom {

// Every command the bot ships. Add new commands to the list in all_commands.cpp.
// Services is passed to commands that need shared infrastructure (jobs, db).
std::vector<std::unique_ptr<Command>> all_commands(Services& services);

} // namespace broom
