#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

#include <string>
#include <vector>

namespace broom::commands {

// /remind set|list|cancel — personal reminders delivered as a mention in the
// channel where they were set. Rows live in the reminders table
// (kind='reminder'); delivery is handled by ScheduleService (schedule.hpp),
// which /schedule shares.
class Remind : public Command {
public:
    explicit Remind(Services& services) : services_(&services) {}

    std::string name() const override { return "remind"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;
    // Pagination buttons for /remind list ("remind:page:<n>").
    void handle_button(const dpp::button_click_t& event) const override;

private:
    Services* services_;
};

// Schema steps for the reminders table; appended to the global migration list.
const std::vector<std::string>& remind_schema();

} // namespace broom::commands
