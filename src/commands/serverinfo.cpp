#include "commands/serverinfo.hpp"

#include <cstdint>

namespace broom::commands {

dpp::slashcommand Serverinfo::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show info about this server", app_id);
}

void Serverinfo::handle(const dpp::slashcommand_t& event) const {
    dpp::guild* guild = dpp::find_guild(event.command.guild_id);
    if (!guild) {
        event.reply(dpp::message("This command only works in a server.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed;
    embed.set_title(guild->name)
        .set_color(0x5865F2)
        .add_field("ID", std::to_string(guild->id), true)
        .add_field("Owner", "<@" + std::to_string(guild->owner_id) + ">", true)
        .add_field("Members", std::to_string(guild->member_count), true)
        .add_field("Channels", std::to_string(guild->channels.size()), true)
        .add_field("Roles", std::to_string(guild->roles.size()), true)
        .add_field("Created",
                   "<t:" + std::to_string(
                               static_cast<std::uint64_t>(guild->id.get_creation_time())) +
                       ":R>",
                   true);

    if (!guild->get_icon_url().empty()) {
        embed.set_thumbnail(guild->get_icon_url(256));
    }

    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
