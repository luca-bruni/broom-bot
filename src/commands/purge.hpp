#pragma once

#include "core/command.hpp"
#include "core/services.hpp"

#include <string>
#include <vector>

namespace broom {
class JobRunner;
class Db;
} // namespace broom

namespace broom::commands {

// Bulk-deletes messages matching keywords, per-channel or guild-wide, over an
// optional date range. Two-phase: a purge_scan job records matches and posts a
// Confirm/Discard prompt; confirming enqueues a purge_delete job. See
// docs/bulk-message-processing.md.
class Purge : public Command {
public:
    explicit Purge(Services& services) : services_(&services) {}

    std::string name() const override { return "purge"; }
    dpp::slashcommand definition(dpp::snowflake app_id) const override;
    void handle(const dpp::slashcommand_t& event) const override;
    void handle_button(const dpp::button_click_t& event) const override;

private:
    Services* services_;
};

// Registers the purge_scan and purge_delete job kinds. Called from main.
void register_purge_jobs(JobRunner& jobs, Db& db);

// Schema steps for purge's tables; appended to the global migration list.
const std::vector<std::string>& purge_schema();

} // namespace broom::commands
