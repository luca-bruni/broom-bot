#include "commands/emojiinfo.hpp"

#include "commands/info_format.hpp"

#include <cstdint>

namespace broom::commands {

dpp::slashcommand EmojiInfo::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show info about a custom emoji", app_id)
        .add_option(dpp::command_option(dpp::co_string, "emoji",
                                        "A custom emoji, e.g. :your_emoji:", true));
}

void EmojiInfo::handle(const dpp::slashcommand_t& event) const {
    std::string raw = std::get<std::string>(event.get_parameter("emoji"));
    auto emoji = parse_custom_emoji(raw);
    if (!emoji) {
        event.reply(dpp::message("Give me a **custom** emoji (built-in unicode emoji "
                                 "aren't supported).")
                        .set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed;
    embed.set_title(":" + emoji->name + ":")
        .set_color(0x5865F2)
        .set_thumbnail(emoji_url(emoji->id, emoji->animated))
        .add_field("ID", std::to_string(emoji->id), true)
        .add_field("Animated", emoji->animated ? "Yes" : "No", true)
        .add_field("Created",
                   "<t:" +
                       std::to_string(static_cast<std::uint64_t>(
                           dpp::snowflake(emoji->id).get_creation_time())) +
                       ":R>",
                   true)
        .add_field("URL", emoji_url(emoji->id, emoji->animated), false);
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
