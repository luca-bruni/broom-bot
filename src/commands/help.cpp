#include "commands/help.hpp"

#include "commands/embeds.hpp"

namespace broom::commands {

dpp::slashcommand Help::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "List every command", app_id);
}

void Help::handle(const dpp::slashcommand_t& event) const {
    std::string body;
    for (const auto& entry : services_->catalog.entries) {
        body += "**/" + entry.name + "** - " + entry.description + "\n";
    }
    if (body.empty()) body = "No commands registered.";

    dpp::embed embed;
    embed.set_title("Commands").set_description(body).set_color(kEmbedColor);
    event.reply(dpp::message().add_embed(embed).set_flags(dpp::m_ephemeral));
}

} // namespace broom::commands
