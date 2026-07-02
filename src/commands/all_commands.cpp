#include "commands/all_commands.hpp"

#include "commands/ping.hpp"

namespace broom {

std::vector<std::unique_ptr<Command>> all_commands() {
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<commands::Ping>());
    return commands;
}

} // namespace broom
