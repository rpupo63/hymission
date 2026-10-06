#pragma once

#include <string>

namespace hymission {

// Hyprland's notification overlay keeps no history and handles no input: a
// notification is unrecoverable once its timer runs out. Mirror it to the
// desktop daemon so the text stays readable and copyable afterwards.
void mirrorNotification(const std::string& message, bool failure);

// Hyprland's *own* notifications never pass through this plugin, and never reach
// D-Bus either — performUserChecks() and the monitor code call
// Notification::overlay()->addNotification directly. Those are the ones that
// matter most, because they fire at login and describe things the user has to
// act on (the .conf deprecation deadline, a rejected monitor scale, a missing
// watchdog). Poll the overlay so they are mirrored too.
void startOverlayMirror();
void stopOverlayMirror();

}
