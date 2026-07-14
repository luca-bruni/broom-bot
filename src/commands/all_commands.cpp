#include "commands/all_commands.hpp"

#include "commands/about.hpp"
#include "commands/avatar.hpp"
#include "commands/channelinfo.hpp"
#include "commands/choose.hpp"
#include "commands/coinflip.hpp"
#include "commands/eightball.hpp"
#include "commands/emojiinfo.hpp"
#include "commands/help.hpp"
#include "commands/jobs.hpp"
#include "commands/ping.hpp"
#include "commands/purge.hpp"
#include "commands/remind.hpp"
#include "commands/roleinfo.hpp"
#include "commands/roll.hpp"
#include "commands/serverinfo.hpp"
#include "commands/stats.hpp"
#include "commands/userinfo.hpp"

namespace broom {

std::vector<std::unique_ptr<Command>> all_commands(Services& services) {
    std::vector<std::unique_ptr<Command>> commands;
    commands.push_back(std::make_unique<commands::About>(services));
    commands.push_back(std::make_unique<commands::Avatar>());
    commands.push_back(std::make_unique<commands::ChannelInfo>());
    commands.push_back(std::make_unique<commands::Choose>());
    commands.push_back(std::make_unique<commands::Coinflip>());
    commands.push_back(std::make_unique<commands::EightBall>());
    commands.push_back(std::make_unique<commands::EmojiInfo>());
    commands.push_back(std::make_unique<commands::Help>(services));
    commands.push_back(std::make_unique<commands::Jobs>(services));
    commands.push_back(std::make_unique<commands::Ping>());
    commands.push_back(std::make_unique<commands::Purge>(services));
    commands.push_back(std::make_unique<commands::Remind>(services));
    commands.push_back(std::make_unique<commands::RoleInfo>());
    commands.push_back(std::make_unique<commands::Roll>());
    commands.push_back(std::make_unique<commands::Serverinfo>());
    commands.push_back(std::make_unique<commands::Stats>(services));
    commands.push_back(std::make_unique<commands::Userinfo>());
    return commands;
}

} // namespace broom
