#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <glib.h>

namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string stripTrailingSlash(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::string withTrailingSlash(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    if (path.back() == '/') {
        return path;
    }
    return path + "/";
}

bool isDirectory(const std::string& path) {
    return !path.empty() && g_file_test(path.c_str(), G_FILE_TEST_IS_DIR);
}

bool isExecutable(const std::string& path) {
    return !path.empty() && g_file_test(path.c_str(), G_FILE_TEST_IS_EXECUTABLE);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

void appendUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    const std::string normalized = stripTrailingSlash(value);
    if (std::find(values.begin(), values.end(), normalized) == values.end()) {
        values.push_back(normalized);
    }
}

std::vector<std::string> parseFbneoRomPaths(const std::string& iniPath) {
    std::vector<std::string> result;
    gchar* contents = nullptr;
    gsize length = 0;
    if (!g_file_get_contents(iniPath.c_str(), &contents, &length, nullptr)) {
        return result;
    }

    const std::string text(contents, length);
    g_free(contents);

    for (const std::string& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (line.rfind("szAppRomPaths[", 0) != 0) {
            continue;
        }
        const auto close = line.find(']');
        if (close == std::string::npos) {
            continue;
        }
        const std::string value = trim(line.substr(close + 1));
        if (value.empty()) {
            continue;
        }
        const std::string expanded = Config::expandPath(value);
        if (isDirectory(expanded)) {
            appendUnique(result, expanded);
        }
    }
    return result;
}

} // namespace

Config::Config() = default;

std::string Config::configDir() {
    return std::string(g_get_user_config_dir()) + "/fbneo-ultra";
}

std::string Config::configFile() {
    return configDir() + "/config.ini";
}

std::string Config::userDataDir() {
    return std::string(g_get_user_data_dir());
}

std::string Config::expandPath(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    if (path == "~") {
        return g_get_home_dir();
    }
    if (path.rfind("~/", 0) == 0) {
        return std::string(g_get_home_dir()) + path.substr(1);
    }
    return path;
}

std::string Config::detectDataDir() {
    const std::string home = g_get_home_dir();
    const std::vector<std::string> candidates = {
        userDataDir() + "/fbneo",
        home + "/.var/app/org.finalburnneo.FBNeo/data/fbneo",
        home + "/.var/app/org.finalburnneo.FBNeo/config/fbneo",
        std::string("/usr/share/fbneo"),
    };
    for (const std::string& candidate : candidates) {
        if (isDirectory(candidate)) {
            return candidate;
        }
    }
    return userDataDir() + "/fbneo";
}

std::string Config::detectDatDir() {
    const std::string home = g_get_home_dir();
    const std::vector<std::string> candidates = {
        detectDataDir() + "/support/lists/dat",
        userDataDir() + "/fbneo/support/lists/dat",
        home + "/.var/app/org.finalburnneo.FBNeo/data/fbneo/support/lists/dat",
        "/usr/share/fbneo/support/lists/dat",
    };
    for (const std::string& candidate : candidates) {
        if (isDirectory(candidate)) {
            return candidate;
        }
    }
    return candidates.front();
}

std::string Config::fbneoIniPath() const {
    const std::string home = g_get_home_dir();
    const std::string defaultPath = userDataDir() + "/fbneo/config/fbneo.ini";
    const std::vector<std::string> candidates = {
        defaultPath,
        home + "/.config/fbneo/fbneo.ini",
        home + "/.fbneo/config/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/config/fbneo/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/config/fbneo/config/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/data/fbneo/config/fbneo.ini",
    };
    for (const std::string& candidate : candidates) {
        if (g_file_test(candidate.c_str(), G_FILE_TEST_IS_REGULAR)) {
            return candidate;
        }
    }
    return defaultPath;
}

std::string Config::findEmulator() {
    if (const char* env = g_getenv("FBNEO_ULTRA_EMULATOR")) {
        const std::string expanded = expandPath(env);
        if (isExecutable(expanded)) {
            return expanded;
        }
    }

    const std::string home = g_get_home_dir();
    const std::vector<std::string> candidates = {
        home + "/.var/Org.Final_Burn_Neo/fbneo",
        home + "/.local/share/fbneo/fbneo",
        home + "/.local/bin/fbneo",
        "/usr/games/fbneo",
        "/usr/local/bin/fbneo",
        "/usr/bin/fbneo",
        "/opt/fbneo/fbneo",
    };
    for (const std::string& candidate : candidates) {
        if (isExecutable(candidate)) {
            return candidate;
        }
    }

    gchar* inPath = g_find_program_in_path("fbneo");
    if (inPath) {
        const std::string result = inPath;
        g_free(inPath);
        return result;
    }
    return {};
}

std::vector<std::string> Config::detectRomDirs() {
    const std::string home = g_get_home_dir();
    std::vector<std::string> result;

    const std::vector<std::string> iniCandidates = {
        userDataDir() + "/fbneo/config/fbneo.ini",
        home + "/.config/fbneo/fbneo.ini",
        home + "/.fbneo/config/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/config/fbneo/config/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/config/fbneo/fbneo.ini",
        home + "/.var/app/org.finalburnneo.FBNeo/data/fbneo/config/fbneo.ini",
    };

    for (const std::string& ini : iniCandidates) {
        for (const std::string& path : parseFbneoRomPaths(ini)) {
            appendUnique(result, path);
        }
    }

    if (result.empty()) {
        const std::vector<std::string> fallbacks = {
            userDataDir() + "/fbneo/roms",
            home + "/.var/app/org.finalburnneo.FBNeo/data/fbneo/roms",
            "/usr/share/fbneo/roms",
            home + "/.local/share/fbneo/roms",
        };
        for (const std::string& candidate : fallbacks) {
            if (isDirectory(candidate)) {
                appendUnique(result, candidate);
            }
        }
    }

    return result;
}

void Config::ensureDefaults() {
    if (emulatorPath.empty()) {
        emulatorPath = findEmulator();
    } else {
        emulatorPath = expandPath(emulatorPath);
    }

    if (datDir.empty() || !isDirectory(datDir)) {
        datDir = detectDatDir();
    } else {
        datDir = expandPath(datDir);
    }

    if (artworkDir.empty()) {
        artworkDir.clear();
    } else {
        artworkDir = expandPath(artworkDir);
    }

    if (romDirs.empty()) {
        romDirs = detectRomDirs();
    }

    std::vector<std::string> normalized;
    for (const std::string& dir : romDirs) {
        const std::string expanded = expandPath(dir);
        if (isDirectory(expanded)) {
            appendUnique(normalized, expanded);
        }
    }
    romDirs = std::move(normalized);

    std::unordered_set<std::string> lowered;
    for (const std::string& rom : favorites) {
        gchar* lower = g_ascii_strdown(rom.c_str(), -1);
        lowered.insert(lower);
        g_free(lower);
    }
    favorites = std::move(lowered);
}

void Config::load() {
    GKeyFile* keyFile = g_key_file_new();
    GError* error = nullptr;

    if (g_key_file_load_from_file(keyFile, configFile().c_str(), G_KEY_FILE_NONE, &error)) {
        gchar* value = g_key_file_get_string(keyFile, "general", "emulator", nullptr);
        if (value) {
            emulatorPath = value;
            g_free(value);
        }
        value = g_key_file_get_string(keyFile, "paths", "dat_dir", nullptr);
        if (value) {
            datDir = value;
            g_free(value);
        }
        value = g_key_file_get_string(keyFile, "paths", "artwork_dir", nullptr);
        if (value) {
            artworkDir = value;
            g_free(value);
        }
        value = g_key_file_get_string(keyFile, "general", "extra_args", nullptr);
        if (value) {
            extraArgs = value;
            g_free(value);
        }
        fullscreen = g_key_file_get_boolean(keyFile, "general", "fullscreen", nullptr);
        syncRomPaths = g_key_file_get_boolean(keyFile, "general", "sync_rom_paths", nullptr);
        autoDat = g_key_file_get_boolean(keyFile, "general", "auto_dat", nullptr);
        if (!g_key_file_has_key(keyFile, "general", "sync_rom_paths", nullptr)) {
            syncRomPaths = true;
        }
        if (!g_key_file_has_key(keyFile, "general", "auto_dat", nullptr)) {
            autoDat = true;
        }

        gsize count = 0;
        gchar** dirs = g_key_file_get_string_list(keyFile, "paths", "rom_dirs", &count, nullptr);
        if (dirs) {
            romDirs.clear();
            for (gsize i = 0; i < count; ++i) {
                appendUnique(romDirs, expandPath(dirs[i]));
            }
            g_strfreev(dirs);
        }

        gchar** favoriteKeys = g_key_file_get_keys(keyFile, "favorites", nullptr, nullptr);
        if (favoriteKeys) {
            for (gchar** key = favoriteKeys; *key; ++key) {
                favorites.insert(*key);
            }
            g_strfreev(favoriteKeys);
        }
    }

    if (error) {
        g_error_free(error);
    }
    g_key_file_free(keyFile);

    ensureDefaults();
}

void Config::save() const {
    g_mkdir_with_parents(configDir().c_str(), 0755);

    GKeyFile* keyFile = g_key_file_new();
    g_key_file_set_string(keyFile, "general", "emulator", emulatorPath.c_str());
    g_key_file_set_string(keyFile, "general", "extra_args", extraArgs.c_str());
    g_key_file_set_boolean(keyFile, "general", "fullscreen", fullscreen);
    g_key_file_set_boolean(keyFile, "general", "sync_rom_paths", syncRomPaths);
    g_key_file_set_boolean(keyFile, "general", "auto_dat", autoDat);
    g_key_file_set_string(keyFile, "paths", "dat_dir", datDir.c_str());
    g_key_file_set_string(keyFile, "paths", "artwork_dir", artworkDir.c_str());

    std::vector<const gchar*> rawDirs;
    rawDirs.reserve(romDirs.size());
    for (const std::string& dir : romDirs) {
        rawDirs.push_back(dir.c_str());
    }
    if (!rawDirs.empty()) {
        g_key_file_set_string_list(keyFile, "paths", "rom_dirs", rawDirs.data(), rawDirs.size());
    }

    for (const std::string& rom : favorites) {
        g_key_file_set_boolean(keyFile, "favorites", rom.c_str(), TRUE);
    }

    GError* error = nullptr;
    gchar* data = g_key_file_to_data(keyFile, nullptr, &error);
    if (data && !error) {
        g_file_set_contents(configFile().c_str(), data, -1, nullptr);
    }
    if (data) {
        g_free(data);
    }
    if (error) {
        g_error_free(error);
    }
    g_key_file_free(keyFile);
}

bool Config::isFavorite(const std::string& rom) const {
    return favorites.find(rom) != favorites.end();
}

void Config::toggleFavorite(const std::string& rom) {
    const auto it = favorites.find(rom);
    if (it == favorites.end()) {
        favorites.insert(rom);
    } else {
        favorites.erase(it);
    }
}

std::vector<std::string> Config::artworkDirs() const {
    std::vector<std::string> result;
    appendUnique(result, artworkDir);

    std::vector<std::string> supportRoots;
    if (!datDir.empty()) {
        std::string root = datDir;
        for (int i = 0; i < 2; ++i) {
            const auto slash = root.find_last_of('/');
            if (slash == std::string::npos) {
                break;
            }
            root.erase(slash);
        }
        appendUnique(supportRoots, root);
    }
    appendUnique(supportRoots, userDataDir() + "/fbneo/support");
    appendUnique(supportRoots, std::string(g_get_home_dir()) + "/.local/share/fbneo/support");

    for (const std::string& root : supportRoots) {
        for (const char* folder : {"previews", "titles", "marquees", "flyers", "snap"}) {
            appendUnique(result, root + "/" + folder);
        }
    }

    return result;
}

void Config::syncRomPathsToFbneoIni() const {
    if (!syncRomPaths || romDirs.empty()) {
        return;
    }

    const std::string iniPath = fbneoIniPath();
    gchar* rawContents = nullptr;
    gsize rawLength = 0;
    const bool exists = g_file_get_contents(iniPath.c_str(), &rawContents, &rawLength, nullptr);
    const std::string original = exists ? std::string(rawContents, rawLength) : std::string();
    if (rawContents) {
        g_free(rawContents);
    }

    std::vector<std::string> lines = splitLines(original);
    std::vector<bool> seen(20, false);

    for (std::string& line : lines) {
        const std::string trimmed = trim(line);
        if (trimmed.rfind("szAppRomPaths[", 0) != 0) {
            continue;
        }
        const auto open = trimmed.find('[');
        const auto close = trimmed.find(']', open);
        if (open == std::string::npos || close == std::string::npos) {
            continue;
        }
        int index = -1;
        try {
            index = std::stoi(trimmed.substr(open + 1, close - open - 1));
        } catch (...) {
            continue;
        }
        if (index < 0 || index >= 20) {
            continue;
        }
        seen[index] = true;
        if (static_cast<size_t>(index) < romDirs.size()) {
            line = "szAppRomPaths[" + std::to_string(index) + "] " +
                   withTrailingSlash(stripTrailingSlash(romDirs[index]));
        } else {
            line = "szAppRomPaths[" + std::to_string(index) + "] ";
        }
    }

    for (int i = 0; i < 20; ++i) {
        if (seen[i]) {
            continue;
        }
        std::string line = "szAppRomPaths[" + std::to_string(i) + "] ";
        if (static_cast<size_t>(i) < romDirs.size()) {
            line += withTrailingSlash(stripTrailingSlash(romDirs[i]));
        }
        lines.push_back(line);
    }

    std::string output;
    if (!exists) {
        output += "// Configuracion de FinalBurn Neo generada por FB Neo Ultra\n";
        output += "// Las rutas de ROMs se sincronizan automaticamente desde el frontend.\n";
    }
    for (size_t i = 0; i < lines.size(); ++i) {
        output += lines[i];
        output += "\n";
    }

    if (exists && output == original) {
        return;
    }

    const std::string directory = iniPath.substr(0, iniPath.find_last_of('/'));
    g_mkdir_with_parents(directory.c_str(), 0755);

    if (exists && !original.empty()) {
        g_file_set_contents((iniPath + ".fbneo-ultra.bak").c_str(), original.c_str(),
                            static_cast<gssize>(original.size()), nullptr);
    }
    g_file_set_contents(iniPath.c_str(), output.c_str(), static_cast<gssize>(output.size()), nullptr);
}
