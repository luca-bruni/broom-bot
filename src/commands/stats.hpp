#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

namespace broom::commands {

class Stats : public Command {
public:
    explicit Stats(Services& services) : services_(&services) {}

    std::string name() const override { return "stats"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;

private:
    Services* services_;
};

} // namespace broom::commands
