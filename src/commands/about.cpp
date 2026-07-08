#include "commands/about.hpp"

#include "core/timeparse.hpp"

#include <chrono>
#include <cstdint>

namespace broom::commands {

dpp::slashcommand About::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "About this bot", app_id);
}

void About::handle(const dpp::slashcommand_t& event) const {
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::steady_clock::now() - services_->started_at)
                      .count();

    dpp::embed embed;
    embed.set_title("broom-bot")
        .set_description("A general-purpose Discord bot in C++20.")
        .set_color(0x5865F2)
        .add_field("Version", services_->version, true)
        .add_field("Library", DPP_VERSION_TEXT, true)
        .add_field("Uptime", format_uptime(static_cast<std::int64_t>(uptime)), true)
        .add_field("Source", "https://github.com/luca-bruni/broom-bot", false);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
