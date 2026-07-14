#include "commands/coinflip.hpp"

#include "core/rng.hpp"

namespace broom::commands {

static dpp::message flip_message() {
    dpp::message msg(rng_int(0, 1) ? "Heads" : "Tails");
    msg.add_component(dpp::component().add_component(dpp::component()
                                                         .set_type(dpp::cot_button)
                                                         .set_label("Flip again")
                                                         .set_style(dpp::cos_primary)
                                                         .set_id("coinflip:again")));
    return msg;
}

dpp::slashcommand Coinflip::definition(dpp::snowflake app_id) const {
    return dpp::slashcommand(name(), "Flip a coin", app_id);
}

void Coinflip::handle(const dpp::slashcommand_t& event) const { event.reply(flip_message()); }

void Coinflip::handle_button(const dpp::button_click_t& event) const {
    event.reply(dpp::ir_update_message, flip_message());
}

} // namespace broom::commands
