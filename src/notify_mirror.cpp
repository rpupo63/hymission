#include "notify_mirror.hpp"

#include <hyprland/src/config/supplementary/executor/Executor.hpp>

namespace hymission {

namespace {
// The executor runs the command through /bin/sh -c.
std::string shellQuote(const std::string& text) {
    std::string quoted = "'";
    for (const char c : text) {
        if (c == '\'')
            quoted += "'\\''";
        else
            quoted += c;
    }
    quoted += "'";
    return quoted;
}
}

void mirrorNotification(const std::string& message, bool failure) {
    const auto& exec = Config::Supplementary::executor();
    if (!exec)
        return;

    // Critical never expires on its own, so a failure survives until dismissed.
    exec->spawnRaw("notify-send -a hymission -u " + std::string(failure ? "critical" : "normal") + " hymission " + shellQuote(message));
}

}
