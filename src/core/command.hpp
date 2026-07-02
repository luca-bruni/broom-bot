#pragma once

#include <dpp/dpp.h>
#include <string>

namespace broom {

// One slash command. Implementations live in src/commands/, one file each,
// and are listed in all_commands().
class Command {
public:
    virtual ~Command() = default;

    virtual std::string name() const = 0;

    // The slash command to register with Discord.
    virtual dpp::slashcommand definition(dpp::snowflake app_id) const = 0;

    // Invoked on the cluster's event threads — must not block.
    virtual void handle(const dpp::slashcommand_t& event) const = 0;
};

} // namespace broom
