#include "commands/all_commands.hpp"

#include "commands/avatar.hpp"
#include "commands/choose.hpp"
#include "commands/coinflip.hpp"
#include "commands/ping.hpp"
#include "commands/purge.hpp"
#include "commands/roll.hpp"
#include "commands/serverinfo.hpp"
#include "commands/userinfo.hpp"

namespace broom {

std::vector<std::unique_ptr<Command>> all_commands(Services& services) {
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<commands::Avatar>());
    commands.push_back(std::make_unique<commands::Choose>());
    commands.push_back(std::make_unique<commands::Coinflip>());
    commands.push_back(std::make_unique<commands::Ping>());
    commands.push_back(std::make_unique<commands::Purge>(services));
    commands.push_back(std::make_unique<commands::Roll>());
    commands.push_back(std::make_unique<commands::Serverinfo>());
    commands.push_back(std::make_unique<commands::Userinfo>());
    return commands;
}

} // namespace broom
