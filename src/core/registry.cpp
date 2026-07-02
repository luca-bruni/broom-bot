#include "core/registry.hpp"

namespace broom {

CommandRegistry::CommandRegistry(std::vector<std::unique_ptr<Command>> commands) {
    for (auto& command : commands) {
        commands_.emplace(command->name(), std::move(command));
    }
}

void CommandRegistry::attach(dpp::cluster& bot, dpp::snowflake dev_guild_id) {
    bot.on_slashcommand([this](const dpp::slashcommand_t& event) {
        auto it = commands_.find(event.command.get_command_name());
        if (it == commands_.end()) {
            event.owner->log(dpp::ll_warning,
                             "Unknown command: " + event.command.get_command_name());
            return;
        }
        it->second->handle(event);
    });

    bot.on_ready([this, &bot, dev_guild_id](const dpp::ready_t&) {
        if (!dpp::run_once<struct register_commands>()) return;

        std::vector<dpp::slashcommand> definitions;
        definitions.reserve(commands_.size());
        for (const auto& [name, command] : commands_) {
            definitions.push_back(command->definition(bot.me.id));
        }

        if (dev_guild_id) {
            bot.guild_bulk_command_create(definitions, dev_guild_id);
        } else {
            bot.global_bulk_command_create(definitions);
        }
    });
}

} // namespace broom
