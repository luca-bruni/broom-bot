#include "commands/all_commands.hpp"

#include "commands/coinflip.hpp"
#include "commands/ping.hpp"
#include "commands/roll.hpp"

namespace broom {

std::vector<std::unique_ptr<Command>> all_commands() {
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<commands::Coinflip>());
    commands.push_back(std::make_unique<commands::Ping>());
    commands.push_back(std::make_unique<commands::Roll>());
    return commands;
}

} // namespace broom
