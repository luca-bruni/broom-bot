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

// Guard for guild-only commands. Replies ephemerally and returns false when
// the command was invoked outside a server; call at the top of handle().
inline bool require_guild(const dpp::slashcommand_t& event) {
    if (event.command.guild_id) return true;
    event.reply(dpp::message("This command can only be used in a server.")
                    .set_flags(dpp::m_ephemeral));
    return false;
}

} // namespace broom::commands
