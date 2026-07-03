#pragma once

#include "core/command.hpp"

namespace broom::commands {

class Roll : public Command {
public:
    std::string name() const override { return "roll"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;
};

} // namespace broom::commands
