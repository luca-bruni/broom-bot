#include <dpp/dpp.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <map>

// Simple .env loader
std::map<std::string, std::string> load_env(const std::string& path) {
    std::map<std::string, std::string> env;
    std::ifstream file(path);
    if (!file.is_open()) return env;

    std::string line;
    while (std::getline(file, line)) {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        env[key] = value;
        setenv(key.c_str(), value.c_str(), 1); // also set in process env
    }
    return env;
}

int main() {
    // Load .env file (must exist in the project root)
    auto env = load_env(".env");

	dpp::cluster bot(std::getenv("BOT_TOKEN"));

	bot.on_slashcommand([](auto event) {
		if (event.command.get_command_name() == "ping") {
			event.reply("Pong!");
		}
	});

	bot.on_ready([&bot](auto event) {
		if (dpp::run_once<struct register_bot_commands>()) {
			bot.global_command_create(
				dpp::slashcommand("ping", "Ping pong!", bot.me.id)
			);
		}
	});

	bot.start(dpp::st_wait);
	return 0;
}