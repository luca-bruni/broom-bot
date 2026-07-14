#include "commands/channelinfo.hpp"

#include "commands/embeds.hpp"
#include "commands/info_format.hpp"

namespace broom::commands {

dpp::slashcommand ChannelInfo::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Show info about a channel", app_id)
        .add_option(
            dpp::command_option(dpp::co_channel, "channel", "The channel to inspect", true));
}

void ChannelInfo::handle(const dpp::slashcommand_t& event) const {
    auto channel_id = std::get<dpp::snowflake>(event.get_parameter("channel"));
    dpp::channel* channel = dpp::find_channel(channel_id);
    if (!channel) {
        event.reply(dpp::message("Channel not found.").set_flags(dpp::m_ephemeral));
        return;
    }

    dpp::embed embed;
    embed.set_title("#" + channel->name)
        .set_color(kEmbedColor)
        .add_field("ID", std::to_string(channel->id), true)
        .add_field("Type", channel_type_name(static_cast<int>(channel->get_type())), true)
        .add_field("NSFW", channel->is_nsfw() ? "Yes" : "No", true);

    if (channel->rate_limit_per_user > 0) {
        embed.add_field("Slowmode", std::to_string(channel->rate_limit_per_user) + "s", true);
    }
    if (channel->parent_id) {
        embed.add_field("Category", "<#" + std::to_string(channel->parent_id) + ">", true);
    }
    embed.add_field("Created", created_relative(channel->id), true);
    if (!channel->topic.empty()) {
        embed.set_description(channel->topic);
    }
    event.reply(dpp::message().add_embed(embed));
}

} // namespace broom::commands
