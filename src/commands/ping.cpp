#include "commands/ping.hpp"

namespace broom::commands {

dpp::slashcommand Ping::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Ping pong!", app_id);
}

void Ping::handle(const dpp::slashcommand_t& event) const {
    int rest_ms = static_cast<int>(event.owner->rest_ping * 1000);
    event.reply("🏓 Pong! REST latency: **" + std::to_string(rest_ms) + "ms**");
}

} // namespace broom::commands
