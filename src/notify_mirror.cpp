#include "notify_mirror.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/config/supplementary/executor/Executor.hpp>
#include <hyprland/src/helpers/Color.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopTimer.hpp>
#include <hyprland/src/notification/Notification.hpp>
#include <hyprland/src/notification/NotificationOverlay.hpp>

namespace hymission {

namespace {

// Hyprland's own banners run 15000ms, and the plugin's run 5000ms, so this
// cannot miss one. A tick with an empty overlay copies an empty vector.
constexpr std::chrono::milliseconds OVERLAY_POLL_INTERVAL{500};

// Notifications already handed to the desktop daemon. Weak, so tracking one
// never extends its life on the overlay.
std::vector<WP<Notification::CNotification>> g_mirrored;
SP<CEventLoopTimer>                          g_pollTimer;

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

void spawnNotifySend(const std::string& appName, const std::string& summary, const std::string& message, bool critical, const std::string& iconName) {
    const auto& exec = Config::Supplementary::executor();
    if (!exec)
        return;

    // Critical never expires on its own, so it survives until dismissed.
    std::string command = "notify-send -a " + shellQuote(appName) + " -u " + (critical ? "critical" : "normal");
    if (!iconName.empty())
        command += " -i " + shellQuote(iconName);
    command += " " + shellQuote(summary) + " " + shellQuote(message);

    exec->spawnRaw(command);
}

// Every failure in this plugin notifies in the same red, and Hyprland's own
// errors use a near-identical one; the orange and cyan summaries fall outside.
bool isFailureColor(const CHyprColor& color) {
    return color.r > 0.9 && color.g < 0.3;
}

std::string freedesktopIconFor(eIcons icon) {
    switch (icon) {
        case ICON_ERROR: return "dialog-error";
        case ICON_WARNING: return "dialog-warning";
        case ICON_INFO:
        case ICON_HINT:
        case ICON_OK: return "dialog-information";
        default: return "";
    }
}

void forgetExpired() {
    std::erase_if(g_mirrored, [](const WP<Notification::CNotification>& tracked) { return !tracked.valid(); });
}

bool alreadyMirrored(const SP<Notification::CNotification>& notification) {
    return std::ranges::any_of(g_mirrored, [&notification](const WP<Notification::CNotification>& tracked) { return tracked == notification; });
}

void pollOverlay() {
    const auto& overlay = Notification::overlay();
    if (!overlay)
        return;

    forgetExpired();

    for (const auto& notification : overlay->getNotifications()) {
        if (!notification || alreadyMirrored(notification))
            continue;

        g_mirrored.emplace_back(notification);

        // Attribute these to Hyprland, not to the plugin: the plugin only
        // noticed them. An ICON_ERROR, or a red banner such as the missing
        // watchdog warning, is worth holding open until it is dismissed.
        const bool critical = notification->icon() == ICON_ERROR || isFailureColor(notification->color());
        spawnNotifySend("Hyprland", "Hyprland", notification->text(), critical, freedesktopIconFor(notification->icon()));
    }
}

}

void mirrorNotification(const std::string& message, bool failure) {
    spawnNotifySend("hymission", "hymission", message, failure, failure ? "dialog-error" : "");

    // The caller has just put this same text on the overlay. Claim that entry so
    // the poller does not send it a second time. Matching on text rather than
    // snapshotting the whole overlay keeps an unmirrored Hyprland banner sitting
    // alongside it from being swallowed.
    const auto& overlay = Notification::overlay();
    if (!overlay)
        return;

    for (const auto& notification : overlay->getNotifications()) {
        if (notification && notification->text() == message && !alreadyMirrored(notification)) {
            g_mirrored.emplace_back(notification);
            return;
        }
    }
}

void startOverlayMirror() {
    if (!g_pEventLoopManager || g_pollTimer)
        return;

    // Hyprland's login banners are added during startup, which can be before the
    // plugin is loaded. Mirror whatever is already on screen before polling, so a
    // banner that predates the plugin is not lost.
    pollOverlay();

    g_pollTimer = makeShared<CEventLoopTimer>(
        OVERLAY_POLL_INTERVAL,
        [](SP<CEventLoopTimer> self, void*) {
            pollOverlay();
            self->updateTimeout(OVERLAY_POLL_INTERVAL);
        },
        nullptr);
    g_pEventLoopManager->addTimer(g_pollTimer);
}

void stopOverlayMirror() {
    if (!g_pollTimer)
        return;

    g_pollTimer->cancel();
    if (g_pEventLoopManager)
        g_pEventLoopManager->removeTimer(g_pollTimer);
    g_pollTimer.reset();
    g_mirrored.clear();
}

}
