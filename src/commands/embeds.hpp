#pragma once

#include <dpp/dpp.h>

#include <cstdint>
#include <string>

namespace broom::commands {

// Accent color for every bot embed (Discord blurple). Change it here, not in
// individual commands.
inline constexpr std::uint32_t kEmbedColor = 0x5865F2;

// Discord relative timestamp ("<t:...:R>") for a snowflake's creation time.
inline std::string created_relative(dpp::snowflake id) {
    return "<t:" + std::to_string(static_cast<std::uint64_t>(id.get_creation_time())) + ":R>";
}

// Prev/Next action row for paginated lists, using the "<command>:page:<n>"
// custom_id convention (routed back to the command's handle_button). Callers
// should only attach it when there is more than one page. `page` is 0-based.
inline dpp::component page_buttons(const std::string& command, int page, int pages) {
    dpp::component row;
    row.add_component(dpp::component()
                          .set_type(dpp::cot_button)
                          .set_label("◀ Prev")
                          .set_style(dpp::cos_secondary)
                          .set_id(command + ":page:" + std::to_string(page - 1))
                          .set_disabled(page <= 0));
    row.add_component(dpp::component()
                          .set_type(dpp::cot_button)
                          .set_label("Next ▶")
                          .set_style(dpp::cos_secondary)
                          .set_id(command + ":page:" + std::to_string(page + 1))
                          .set_disabled(page >= pages - 1));
    return row;
}

// Guard for guild-only commands. Replies ephemerally and returns false when
// the command was invoked outside a server; call at the top of handle().
inline bool require_guild(const dpp::slashcommand_t& event) {
    if (event.command.guild_id) return true;
    event.reply(dpp::message("This command can only be used in a server.")
                    .set_flags(dpp::m_ephemeral));
    return false;
}

} // namespace broom::commands
