#pragma once

#include <string>
#include <vector>

namespace broom::commands {

// Classic magic 8-ball responses. Pure data (DPP-free) so it can be tested.
inline const std::vector<std::string>& eightball_answers() {
    static const std::vector<std::string> answers = {
        "It is certain.", "It is decidedly so.", "Without a doubt.",
        "Yes — definitely.", "You may rely on it.", "As I see it, yes.",
        "Most likely.", "Outlook good.", "Yes.", "Signs point to yes.",
        "Reply hazy, try again.", "Ask again later.", "Better not tell you now.",
        "Cannot predict now.", "Concentrate and ask again.",
        "Don't count on it.", "My reply is no.", "My sources say no.",
        "Outlook not so good.", "Very doubtful.",
    };
    return answers;
}

} // namespace broom::commands
