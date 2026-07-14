#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

#include <string>

namespace broom::commands {

// Lists recent background jobs for the server and cancels one by ID.
// Complements the per-message Cancel button (which is unreachable once the
// progress message scrolls away). Gated to Manage Messages.
class Jobs : public Command {
public:
    explicit Jobs(Services& services) : services_(&services) {}

    std::string name() const override { return "jobs"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;
    // Pagination buttons for /jobs list ("jobs:page:<n>").
    void handle_button(const dpp::button_click_t& event) const override;

private:
    Services* services_;
};

} // namespace broom::commands
