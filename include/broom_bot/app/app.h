#pragma once

namespace dpp {
class cluster;
}

namespace broom_bot::config {
struct BotConfig;
}

namespace broom_bot::app {

/*
 * Returns process exit code.
 */
int run(const broom_bot::config::BotConfig& cfg);

} // namespace broom_bot::app
