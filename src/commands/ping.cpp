#include "commands/ping.hpp"

namespace broom::commands {

dpp::slashcommand Ping::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Ping pong!", app_id);
}

void Ping::handle(const dpp::slashcommand_t& event) const {
    event.reply("Pong!");
}

} // namespace broom::commands
