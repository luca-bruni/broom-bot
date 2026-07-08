#include "commands/roleinfo.hpp"

#include "commands/info_format.hpp"

#include <cstdint>

namespace broom::commands {

namespace {
std::string created_relative(dpp::snowflake id) {
    return "<t:" + std::to_string(static_cast<std::uint64_t>(id.get_creation_time())) + ":R>";
}
} // namespace

dpp::slashcommand RoleInfo::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show info about a role", app_id)
        .add_option(dpp::command_option(dpp::co_role, "role", "The role to inspect", true));
}

void RoleInfo::handle(const dpp::slashcommand_t& event) const {
    if (!event.command.guild_id) {
        event.reply(dpp::message("This command can only be used in a server.")
                        .set_flags(dpp::m_ephemeral));
        return;
    }
    auto role_id = std::get<dpp::snowflake>(event.get_parameter("role"));
    dpp::role* role = dpp::find_role(role_id);
    if (!role) {
        event.reply(dpp::message("Role not found.").set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed;
    embed.set_title(role->name)
        .set_color(role->colour ? role->colour : 0x5865F2)
        .add_field("ID", std::to_string(role->id), true)
        .add_field("Color", role->colour ? format_color(role->colour) : "none", true)
        .add_field("Position", std::to_string(role->position), true)
        .add_field("Mentionable", role->is_mentionable() ? "Yes" : "No", true)
        .add_field("Hoisted", role->is_hoisted() ? "Yes" : "No", true)
        .add_field("Managed", role->is_managed() ? "Yes" : "No", true)
        .add_field("Created", created_relative(role->id), true);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
