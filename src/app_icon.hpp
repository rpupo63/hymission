#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/render/Texture.hpp>

namespace hymission {

struct AppIdentity {
    std::string appClass;
    std::string appName;
    std::string iconName;
};

// Resolve a human-facing app name / icon hint from a Hyprland window class.
[[nodiscard]] AppIdentity resolveAppIdentity(const std::string& windowClass, const std::string& initialClass,
                                             const std::string& windowTitle);

// Cache of loaded app icon textures keyed by icon lookup token.
class AppIconCache {
  public:
    [[nodiscard]] SP<Render::ITexture> textureFor(const AppIdentity& identity, int pixelSize);
    void                               clear();

  private:
    struct Entry {
        SP<Render::ITexture> texture;
        int                  pixelSize = 0;
    };

    std::unordered_map<std::string, Entry> m_entries;
};

} // namespace hymission
