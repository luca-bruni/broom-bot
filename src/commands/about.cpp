#include "commands/about.hpp"

#include "commands/embeds.hpp"
#include "core/timeparse.hpp"

namespace broom::commands {

dpp::slashcommand About::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "About this bot", app_id);
}

void About::handle(const dpp::slashcommand_t& event) const {
    dpp::embed embed;
    embed.set_title("broom-bot")
        .set_description("A general-purpose Discord bot in C++20.")
        .set_color(kEmbedColor)
        .add_field("Version", services_->version, true)
        .add_field("Library", DPP_VERSION_TEXT, true)
        .add_field("Uptime", format_uptime(services_->uptime_seconds()), true)
        .add_field("Source", "https://github.com/luca-bruni/broom-bot", false);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
