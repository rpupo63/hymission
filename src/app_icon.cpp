#include "app_icon.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cairo/cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <hyprland/src/render/Renderer.hpp>

namespace hymission {
namespace {

namespace fs = std::filesystem;

std::string toLower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trimCopy(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return std::string{value};
}

std::string prettifyClass(const std::string& windowClass) {
    if (windowClass.empty())
        return "App";

    // org.gnome.Nautilus -> Nautilus; firefox -> Firefox
    const auto lastDot = windowClass.find_last_of('.');
    std::string base = lastDot == std::string::npos ? windowClass : windowClass.substr(lastDot + 1);
    if (base.empty())
        base = windowClass;

    bool capitalize = true;
    for (char& c : base) {
        if (c == '-' || c == '_' || c == ' ') {
            c = ' ';
            capitalize = true;
            continue;
        }
        if (capitalize) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize = false;
        }
    }
    return base;
}

std::vector<fs::path> xdgDataDirs() {
    std::vector<fs::path> dirs;
    if (const char* home = std::getenv("XDG_DATA_HOME"); home && *home)
        dirs.emplace_back(home);
    else if (const char* home = std::getenv("HOME"); home && *home)
        dirs.push_back(fs::path(home) / ".local" / "share");

    const char* dataDirs = std::getenv("XDG_DATA_DIRS");
    std::string dataDirsValue = dataDirs && *dataDirs ? dataDirs : "/usr/local/share:/usr/share";
    std::size_t start = 0;
    while (start <= dataDirsValue.size()) {
        const auto end = dataDirsValue.find(':', start);
        const auto part = dataDirsValue.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty())
            dirs.emplace_back(part);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return dirs;
}

struct DesktopEntry {
    std::string name;
    std::string icon;
    std::string startupWmClass;
    std::string fileStem;
};

std::optional<DesktopEntry> parseDesktopFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in)
        return std::nullopt;

    DesktopEntry entry;
    entry.fileStem = path.stem().string();
    bool inDesktopEntry = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        if (line.front() == '[') {
            inDesktopEntry = line == "[Desktop Entry]";
            continue;
        }
        if (!inDesktopEntry)
            continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const auto key = trimCopy(line.substr(0, eq));
        const auto value = trimCopy(line.substr(eq + 1));
        if (key == "Name" && entry.name.empty())
            entry.name = value;
        else if (key == "Icon" && entry.icon.empty())
            entry.icon = value;
        else if (key == "StartupWMClass" && entry.startupWmClass.empty())
            entry.startupWmClass = value;
    }

    if (entry.name.empty() && entry.icon.empty() && entry.startupWmClass.empty())
        return std::nullopt;
    return entry;
}

bool classMatches(const std::string& needleLower, const std::string& candidate) {
    if (candidate.empty())
        return false;
    const auto lower = toLower(candidate);
    return lower == needleLower || lower.find(needleLower) != std::string::npos || needleLower.find(lower) != std::string::npos;
}

std::optional<DesktopEntry> findDesktopEntry(const std::string& windowClass, const std::string& initialClass) {
    const auto primary = !windowClass.empty() ? windowClass : initialClass;
    const auto secondary = primary == initialClass ? std::string{} : initialClass;
    if (primary.empty() && secondary.empty())
        return std::nullopt;

    const std::string cacheKey = toLower(primary) + '\n' + toLower(secondary);
    static std::unordered_map<std::string, std::optional<DesktopEntry>> cache;
    if (auto it = cache.find(cacheKey); it != cache.end())
        return it->second;

    const auto primaryLower = toLower(primary);
    const auto secondaryLower = toLower(secondary);

    std::optional<DesktopEntry> best;
    int                         bestScore = 0;

    for (const auto& dataDir : xdgDataDirs()) {
        const auto appsDir = dataDir / "applications";
        std::error_code ec;
        if (!fs::exists(appsDir, ec))
            continue;

        for (const auto& item : fs::directory_iterator(appsDir, ec)) {
            if (ec || !item.is_regular_file(ec))
                continue;
            if (item.path().extension() != ".desktop")
                continue;

            auto entry = parseDesktopFile(item.path());
            if (!entry)
                continue;

            int score = 0;
            if (classMatches(primaryLower, entry->startupWmClass))
                score = 100;
            else if (!secondaryLower.empty() && classMatches(secondaryLower, entry->startupWmClass))
                score = 90;
            else if (classMatches(primaryLower, entry->fileStem))
                score = 80;
            else if (!secondaryLower.empty() && classMatches(secondaryLower, entry->fileStem))
                score = 70;
            else if (classMatches(primaryLower, entry->name))
                score = 40;

            if (score > bestScore) {
                bestScore = score;
                best = std::move(entry);
            }
        }
    }

    cache.emplace(cacheKey, best);
    return best;
}

std::vector<fs::path> iconSearchRoots() {
    std::vector<fs::path> roots;
    for (const auto& dataDir : xdgDataDirs()) {
        roots.push_back(dataDir / "icons");
        roots.push_back(dataDir / "pixmaps");
    }
    roots.emplace_back("/usr/share/pixmaps");
    return roots;
}

std::optional<fs::path> findIconPath(const std::string& iconName, int preferredSize) {
    if (iconName.empty())
        return std::nullopt;

    if (iconName.front() == '/' || iconName.find('/') != std::string::npos) {
        const fs::path direct{iconName};
        std::error_code ec;
        if (fs::exists(direct, ec))
            return direct;
        return std::nullopt;
    }

    static const char* kThemes[] = {"hicolor", "Adwaita", "Yaru", "Papirus", "breeze", "oxygen"};
    static const char* kExts[] = {".png", ".svg", ".xpm"};
    const std::vector<std::string> sizes = {
        std::to_string(preferredSize) + "x" + std::to_string(preferredSize),
        "48x48",
        "64x64",
        "32x32",
        "128x128",
        "24x24",
        "scalable",
    };

    for (const auto& root : iconSearchRoots()) {
        std::error_code ec;
        if (!fs::exists(root, ec))
            continue;

        // pixmaps-style flat lookup
        for (const char* ext : kExts) {
            const auto candidate = root / (iconName + ext);
            if (fs::exists(candidate, ec))
                return candidate;
        }

        for (const char* theme : kThemes) {
            for (const auto& size : sizes) {
                for (const char* category : {"apps", "applications", "mimetypes", "places"}) {
                    for (const char* ext : kExts) {
                        const auto candidate = root / theme / size / category / (iconName + ext);
                        if (fs::exists(candidate, ec))
                            return candidate;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

SP<Render::ITexture> loadIconTexture(const fs::path& path, int pixelSize) {
    if (!g_pHyprRenderer || pixelSize <= 0)
        return {};

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_size(path.c_str(), pixelSize, pixelSize, &error);
    if (error) {
        g_error_free(error);
        error = nullptr;
    }
    if (!pixbuf)
        return {};

    const int width = gdk_pixbuf_get_width(pixbuf);
    const int height = gdk_pixbuf_get_height(pixbuf);
    const int channels = gdk_pixbuf_get_n_channels(pixbuf);
    const bool hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf) != FALSE;
    const int srcStride = gdk_pixbuf_get_rowstride(pixbuf);
    const guchar* src = gdk_pixbuf_get_pixels(pixbuf);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        g_object_unref(pixbuf);
        cairo_surface_destroy(surface);
        return {};
    }

    const int dstStride = cairo_image_surface_get_stride(surface);
    auto* dst = cairo_image_surface_get_data(surface);
    for (int y = 0; y < height; ++y) {
        const guchar* srcRow = src + y * srcStride;
        auto* dstRow = reinterpret_cast<uint32_t*>(dst + y * dstStride);
        for (int x = 0; x < width; ++x) {
            const guchar* px = srcRow + x * channels;
            const uint8_t r = px[0];
            const uint8_t g = channels > 1 ? px[1] : r;
            const uint8_t b = channels > 2 ? px[2] : r;
            const uint8_t a = hasAlpha && channels > 3 ? px[3] : 255;
            // Cairo ARGB32 little-endian memory order is BGRA.
            dstRow[x] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
        }
    }
    cairo_surface_mark_dirty(surface);
    g_object_unref(pixbuf);

    auto texture = g_pHyprRenderer->createTexture(surface);
    cairo_surface_destroy(surface);
    return texture;
}

} // namespace

AppIdentity resolveAppIdentity(const std::string& windowClass, const std::string& initialClass, const std::string& windowTitle) {
    AppIdentity identity;
    identity.appClass = !windowClass.empty() ? windowClass : initialClass;

    if (const auto desktop = findDesktopEntry(windowClass, initialClass)) {
        identity.appName = desktop->name.empty() ? prettifyClass(identity.appClass) : desktop->name;
        identity.iconName = desktop->icon.empty() ? identity.appClass : desktop->icon;
    } else {
        identity.appName = !identity.appClass.empty() ? prettifyClass(identity.appClass) : (!windowTitle.empty() ? windowTitle : "App");
        identity.iconName = identity.appClass;
    }

    return identity;
}

SP<Render::ITexture> AppIconCache::textureFor(const AppIdentity& identity, int pixelSize) {
    const std::string key = identity.iconName.empty() ? identity.appClass : identity.iconName;
    if (key.empty() || pixelSize <= 0)
        return {};

    if (auto it = m_entries.find(key); it != m_entries.end() && it->second.texture && it->second.pixelSize == pixelSize)
        return it->second.texture;

    const auto path = findIconPath(key, pixelSize);
    if (!path) {
        m_entries[key] = {.texture = {}, .pixelSize = pixelSize};
        return {};
    }

    auto texture = loadIconTexture(*path, pixelSize);
    m_entries[key] = {.texture = texture, .pixelSize = pixelSize};
    return texture;
}

void AppIconCache::clear() {
    m_entries.clear();
}

} // namespace hymission
