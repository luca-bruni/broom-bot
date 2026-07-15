#include "commands/userinfo.hpp"

#include "commands/embeds.hpp"

namespace broom::commands {

dpp::slashcommand Userinfo::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show info about a user", app_id)
        .add_option(dpp::command_option(dpp::co_user, "user", "User to inspect (default: you)",
                                        false));
}

void Userinfo::handle(const dpp::slashcommand_t& event) const {
    dpp::user user = event.command.usr;
    if (auto param = event.get_parameter("user");
        std::holds_alternative<dpp::snowflake>(param)) {
        user = event.command.get_resolved_user(std::get<dpp::snowflake>(param));
    }

    dpp::embed embed;
    embed.set_title(user.format_username())
        .set_thumbnail(user.get_avatar_url(256))
        .set_color(kEmbedColor)
        .add_field("ID", std::to_string(user.id), true)
        .add_field("Bot", user.is_bot() ? "Yes" : "No", true)
        .add_field("Created", created_relative(user.id), true);

    // Guild-specific info when the target is a member of this server.
    try {
        const dpp::guild_member& member = (user.id == event.command.usr.id)
                                              ? event.command.member
                                              : event.command.get_resolved_member(user.id);
        if (member.joined_at) {
            embed.add_field("Joined", "<t:" + std::to_string(member.joined_at) + ":R>", true);
        }
        embed.add_field("Roles", std::to_string(member.get_roles().size()), true);
    } catch (const dpp::logic_exception&) {
        // Not a member (e.g. DM context) - user-level fields only.
    }

    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
