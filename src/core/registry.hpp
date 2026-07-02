#pragma once

#include "core/command.hpp"

#include <dpp/dpp.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace broom {

// Owns all commands, registers them with Discord on ready, and routes slash
// command events to the right handler. Must outlive the cluster's run.
class CommandRegistry {
public:
    explicit CommandRegistry(std::vector<std::unique_ptr<Command>> commands);

    // Wires on_ready (bulk registration) and on_slashcommand (dispatch).
    // dev_guild_id != 0 registers guild-scoped (instant); 0 registers globally.
    void attach(dpp::cluster& bot, dpp::snowflake dev_guild_id = 0);

private:
    std::map<std::string, std::unique_ptr<Command>> commands_;
};

} // namespace broom
