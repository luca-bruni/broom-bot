#include "core/registry.hpp"

namespace broom {

namespace {

// Dispatch guard: a throwing handler must not unwind DPP's event thread.
// Log it and answer with an ephemeral error so the user isn't left with a
// silently "thinking…" interaction. The reply may itself fail if the handler
// already replied before throwing - DPP logs that; nothing more to do.
template <typename Event, typename Fn>
void guarded(const Event& event, const std::string& what, Fn&& fn) {
    try {
        std::forward<Fn>(fn)();
    } catch (const std::exception& e) {
        event.owner->log(dpp::ll_error, what + " threw: " + e.what());
        event.reply(dpp::message("⚠️ Something went wrong - the error has been logged.")
                        .set_flags(dpp::m_ephemeral));
    } catch (...) {
        event.owner->log(dpp::ll_error, what + " threw a non-standard exception");
        event.reply(dpp::message("⚠️ Something went wrong - the error has been logged.")
                        .set_flags(dpp::m_ephemeral));
    }
}

} // namespace

CommandRegistry::CommandRegistry(std::vector<std::unique_ptr<Command>> commands) {
    for (auto& command : commands) {
        commands_.emplace(command->name(), std::move(command));
    }
}

void CommandRegistry::set_usage_hook(std::function<void(const std::string&)> hook) {
    usage_hook_ = std::move(hook);
}

void CommandRegistry::attach(dpp::cluster& bot, dpp::snowflake dev_guild_id) {
    bot.on_slashcommand([this](const dpp::slashcommand_t& event) {
        auto name = event.command.get_command_name();
        auto it = commands_.find(name);
        if (it == commands_.end()) {
            event.owner->log(dpp::ll_warning, "Unknown command: " + name);
            return;
        }
        guarded(event, "/" + name, [&] {
            if (usage_hook_) usage_hook_(name);
            it->second->handle(event);
        });
    });

    // custom_id convention: "<command>:<action>" - route to the owning command.
    bot.on_button_click([this](const dpp::button_click_t& event) {
        auto owner = event.custom_id.substr(0, event.custom_id.find(':'));
        auto it = commands_.find(owner);
        if (it == commands_.end()) {
            // Not ours - other subsystems (e.g. JobRunner) attach their own
            // button handlers on this event.
            event.owner->log(dpp::ll_debug, "Button not command-routed: " + event.custom_id);
            return;
        }
        guarded(event, "button " + event.custom_id, [&] { it->second->handle_button(event); });
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
