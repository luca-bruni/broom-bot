#include "commands/coinflip.hpp"

#include <random>

namespace broom::commands {

static bool coinflip() {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    static thread_local std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}

dpp::slashcommand Coinflip::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Flip a coin", app_id);
}

void Coinflip::handle(const dpp::slashcommand_t& event) const {
    event.reply(coinflip() ? "Heads" : "Tails");
}

} // namespace broom::commands
