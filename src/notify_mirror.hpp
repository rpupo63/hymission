#pragma once

#include <string>

namespace hymission {

// Hyprland's notification overlay keeps no history and handles no input: a
// notification is unrecoverable once its timer runs out. Mirror it to the
// desktop daemon so the text stays readable and copyable afterwards.
void mirrorNotification(const std::string& message, bool failure);

}
