#include "commands/avatar.hpp"

#include "commands/embeds.hpp"

namespace broom::commands {

dpp::slashcommand Avatar::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show a user's avatar", app_id)
        .add_option(dpp::command_option(dpp::co_user, "user",
                                        "User whose avatar to show (default: you)", false));
}

void Avatar::handle(const dpp::slashcommand_t& event) const {
    dpp::user user = event.command.usr;
    if (auto param = event.get_parameter("user");
        std::holds_alternative<dpp::snowflake>(param)) {
        user = event.command.get_resolved_user(std::get<dpp::snowflake>(param));
    }

    dpp::embed embed;
    embed.set_title(user.format_username())
        .set_color(kEmbedColor)
        .set_image(user.get_avatar_url(1024));

    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
