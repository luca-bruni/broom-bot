#include "commands/stats.hpp"

#include "commands/embeds.hpp"
#include "core/timeparse.hpp"

namespace broom::commands {

dpp::slashcommand Stats::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Bot statistics", app_id);
}

void Stats::handle(const dpp::slashcommand_t& event) const {
    std::size_t guilds = dpp::get_guild_cache() ? dpp::get_guild_cache()->count() : 0;

    dpp::embed embed;
    embed.set_title("Stats")
        .set_color(kEmbedColor)
        .add_field("Servers", std::to_string(guilds), true)
        .add_field("Commands", std::to_string(services_->catalog.entries.size()), true)
        .add_field("REST latency",
                   std::to_string(static_cast<int>(event.owner->rest_ping * 1000)) + "ms",
                   true)
        .add_field("Uptime", format_uptime(services_->uptime_seconds()), false);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
