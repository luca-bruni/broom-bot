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

    // custom_id convention: "<command>:<action>" — route to the owning command.
    bot.on_button_click([this](const dpp::button_click_t& event) {
        auto owner = event.custom_id.substr(0, event.custom_id.find(':'));
        auto it = commands_.find(owner);
        if (it == commands_.end()) {
            // Not ours — other subsystems (e.g. JobRunner) attach their own
            // button handlers on this event.
            event.owner->log(dpp::ll_debug, "Button not command-routed: " + event.custom_id);
            return;
        }
        it->second->handle_button(event);
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
