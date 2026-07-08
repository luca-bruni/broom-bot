#include "commands/stats.hpp"

#include "core/timeparse.hpp"

#include <chrono>
#include <cstdint>

namespace broom::commands {

dpp::slashcommand Stats::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Bot statistics", app_id);
}

void Stats::handle(const dpp::slashcommand_t& event) const {
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::steady_clock::now() - services_->started_at)
                      .count();
    std::size_t guilds = dpp::get_guild_cache() ? dpp::get_guild_cache()->count() : 0;

    dpp::embed embed;
    embed.set_title("Stats")
        .set_color(0x5865F2)
        .add_field("Servers", std::to_string(guilds), true)
        .add_field("Commands", std::to_string(services_->catalog.entries.size()), true)
        .add_field("REST latency",
                   std::to_string(static_cast<int>(event.owner->rest_ping * 1000)) + "ms", true)
        .add_field("Uptime", format_uptime(static_cast<std::int64_t>(uptime)), false);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
