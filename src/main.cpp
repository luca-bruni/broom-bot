#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

#include <fstream>
#include <string>

#include <windows.h>

static std::string exe_dir() {
    char buf[MAX_PATH]{};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf, n ? n : 0);
    auto pos = p.find_last_of("\\/");
    return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
}

static nlohmann::json read_json_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        return nlohmann::json::object();

    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return nlohmann::json::object();
    }

    return j;
}

int main() {
    // Prefer config.json next to the executable, then fall back to cwd.
    const std::string cfg_path = exe_dir() + "\\config.json";
    nlohmann::json cfg = read_json_file(cfg_path);
    if (cfg.empty())
        cfg = read_json_file("config.json");

    const std::string token = cfg.value("BOT_TOKEN", "");
    if (token.empty()) {
        fprintf(stderr, "Missing BOT_TOKEN in config.json.\n");
        return 1;
    }

    dpp::cluster bot(token);

    bot.on_slashcommand([](const dpp::slashcommand_t& e) {
        if (e.command.get_command_name() == "ping") e.reply("Pong!");
    });

    bot.on_ready([&bot](const dpp::ready_t&) {
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }
    });

    bot.start(dpp::st_wait);
    return 0;
}
