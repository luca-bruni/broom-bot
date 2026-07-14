#include "commands/eightball.hpp"

#include "commands/eightball_answers.hpp"
#include "core/rng.hpp"

namespace broom::commands {

dpp::slashcommand EightBall::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Ask the magic 8-ball a yes/no question", app_id)
        .add_option(dpp::command_option(dpp::co_string, "question", "Your question", true));
}

void EightBall::handle(const dpp::slashcommand_t& event) const {
    std::string question = std::get<std::string>(event.get_parameter("question"));
    const auto& answers = eightball_answers();
    const auto& pick =
        answers[static_cast<std::size_t>(rng_int(0, static_cast<int>(answers.size()) - 1))];
    event.reply("🎱 **Q:** " + question + "\n**A:** " + pick);
}

} // namespace broom::commands
