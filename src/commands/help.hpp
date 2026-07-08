#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

namespace broom::commands {

class Help : public Command {
public:
    explicit Help(Services& services) : services_(&services) {}

    std::string name() const override { return "help"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;

private:
    Services* services_;
};

} // namespace broom::commands
