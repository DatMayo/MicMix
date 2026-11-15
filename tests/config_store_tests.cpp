#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "micmix_core.h"

namespace {

namespace fs = std::filesystem;
std::vector<fs::path> g_tempDirs;

bool NearlyEqual(float a, float b, float eps = 0.0001f) {
    return std::fabs(a - b) <= eps;
}

fs::path MakeUniqueBaseDir(const char* testName) {
    const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const fs::path base = fs::temp_directory_path() /
        ("micmix_test_" + std::string(testName) + "_" + std::to_string(tick));
    std::error_code ec;
    fs::create_directories(base, ec);
    g_tempDirs.push_back(base);
    return base;
}

void CleanupTempDirs() {
    for (const auto& dir : g_tempDirs) {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    g_tempDirs.clear();
}

bool WriteFile(const std::string& path, const std::string& payload) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    out.flush();
    return out.good();
}

bool Expect(bool cond, const std::string& msg) {
    if (!cond) {
        std::cerr << "FAIL: " << msg << std::endl;
        return false;
    }
    return true;
}

bool TestJsonWithEqualsInString() {
    const fs::path base = MakeUniqueBaseDir("json_equals");
    ConfigStore store(base.string());
    const std::string payload =
        "{\n"
        "  \"source.mode\": \"app_session\",\n"
        "  \"source.app.process_name\": \"foo=bar.exe\",\n"
        "  \"mix.music_gain_db\": -12.5,\n"
        "  \"source.autostart_enabled\": true\n"
        "}\n";
    if (!WriteFile(store.ConfigPath(), payload)) {
        std::cerr << "FAIL: could not write config file for json_equals" << std::endl;
        return false;
    }

    MicMixSettings settings{};
    settings.sourceMode = SourceMode::Loopback;
    settings.appProcessName = "default.exe";
    settings.musicGainDb = -20.0f;
    settings.autostartEnabled = false;

    std::string warning;
    const bool ok = store.Load(settings, warning);
    bool pass = true;
    pass &= Expect(ok, "Load(json_equals) should return true");
    pass &= Expect(settings.sourceMode == SourceMode::AppSession, "JSON source.mode should be parsed as app_session");
    pass &= Expect(settings.appProcessName == "foo=bar.exe", "JSON string with '=' should not be treated as INI pair");
    pass &= Expect(NearlyEqual(settings.musicGainDb, -12.5f), "JSON float value should be parsed");
    pass &= Expect(settings.autostartEnabled, "JSON bool value should be parsed");
    pass &= Expect(warning.empty(), "Valid JSON config should not produce parse warnings");
    return pass;
}

bool TestIniStillParses() {
    const fs::path base = MakeUniqueBaseDir("ini_ok");
    ConfigStore store(base.string());
    const std::string payload =
        "source.mode=app_session\n"
        "source.app.process_name=spotify.exe\n"
        "mix.music_gain_db=-18.0\n"
        "source.autostart_enabled=true\n";
    if (!WriteFile(store.ConfigPath(), payload)) {
        std::cerr << "FAIL: could not write config file for ini_ok" << std::endl;
        return false;
    }

    MicMixSettings settings{};
    std::string warning;
    const bool ok = store.Load(settings, warning);
    bool pass = true;
    pass &= Expect(ok, "Load(ini_ok) should return true");
    pass &= Expect(settings.sourceMode == SourceMode::AppSession, "INI source.mode should be parsed");
    pass &= Expect(settings.appProcessName == "spotify.exe", "INI process name should be parsed");
    pass &= Expect(NearlyEqual(settings.musicGainDb, -18.0f), "INI music gain should be parsed");
    pass &= Expect(settings.autostartEnabled, "INI autostart should be parsed");
    pass &= Expect(warning.empty(), "Valid INI config should not produce parse warnings");
    return pass;
}

bool TestInvalidJsonReportsParseWarning() {
    const fs::path base = MakeUniqueBaseDir("invalid_json");
    ConfigStore store(base.string());
    const std::string payload =
        "{\n"
        "  \"source.mode\": \"app_session\",\n"
        "  \"source.app.process_name\": \"foo=bar.exe\"\n";
    if (!WriteFile(store.ConfigPath(), payload)) {
        std::cerr << "FAIL: could not write config file for invalid_json" << std::endl;
        return false;
    }

    MicMixSettings settings{};
    settings.sourceMode = SourceMode::Loopback;
    settings.appProcessName = "fallback.exe";

    std::string warning;
    const bool ok = store.Load(settings, warning);
    bool pass = true;
    pass &= Expect(ok, "Load(invalid_json) should return true with warning");
    pass &= Expect(settings.sourceMode == SourceMode::Loopback, "Invalid JSON must not be silently interpreted as INI");
    pass &= Expect(settings.appProcessName == "fallback.exe", "Invalid JSON should keep fallback process name");
    pass &= Expect(warning.find("Config parse issue detected") != std::string::npos,
                   "Invalid JSON should emit parse warning");
    return pass;
}

bool TestOversizedConfigIgnored() {
    const fs::path base = MakeUniqueBaseDir("oversized_config");
    ConfigStore store(base.string());
    std::string payload(1024 * 1024 + 64, 'a');
    payload += "\nsource.mode=app_session\n";
    if (!WriteFile(store.ConfigPath(), payload)) {
        std::cerr << "FAIL: could not write config file for oversized_config" << std::endl;
        return false;
    }

    MicMixSettings settings{};
    settings.sourceMode = SourceMode::Loopback;

    std::string warning;
    const bool ok = store.Load(settings, warning);
    bool pass = true;
    pass &= Expect(ok, "Load(oversized_config) should return true with warning");
    pass &= Expect(settings.sourceMode == SourceMode::Loopback, "Oversized config should be ignored");
    pass &= Expect(warning.find("Config too large; file ignored.") != std::string::npos,
                   "Oversized config should emit size warning");
    return pass;
}

bool TestUiThemeRoundTrip() {
    const fs::path base = MakeUniqueBaseDir("ui_theme");
    ConfigStore store(base.string());
    bool pass = true;

    MicMixSettings defaults{};
    pass &= Expect(defaults.uiThemeMode == UiThemeMode::System, "Theme should default to system");

    MicMixSettings settings{};
    settings.uiThemeMode = UiThemeMode::Dark;
    std::string error;
    pass &= Expect(store.Save(settings, error), "Save(ui_theme) should succeed: " + error);

    MicMixSettings loaded{};
    std::string warning;
    pass &= Expect(store.Load(loaded, warning), "Load(ui_theme) should succeed");
    pass &= Expect(loaded.uiThemeMode == UiThemeMode::Dark, "Dark theme should survive save/load");

    if (!WriteFile(store.ConfigPath(), "ui.theme=light\n")) {
        std::cerr << "FAIL: could not write config file for ui_theme" << std::endl;
        return false;
    }
    MicMixSettings light{};
    pass &= Expect(store.Load(light, warning), "Load(ui_theme light) should succeed");
    pass &= Expect(light.uiThemeMode == UiThemeMode::Light, "INI ui.theme=light should be parsed");

    if (!WriteFile(store.ConfigPath(), "ui.theme=neon\n")) {
        std::cerr << "FAIL: could not write config file for ui_theme" << std::endl;
        return false;
    }
    MicMixSettings unknown{};
    pass &= Expect(store.Load(unknown, warning), "Load(ui_theme unknown) should succeed");
    pass &= Expect(unknown.uiThemeMode == UiThemeMode::System, "Unknown ui.theme should fall back to system");
    return pass;
}

bool TestMiniWindowStateRoundTrip() {
    const fs::path base = MakeUniqueBaseDir("ui_mini");
    ConfigStore store(base.string());
    bool pass = true;

    MicMixSettings defaults{};
    pass &= Expect(!defaults.uiMiniMode && !defaults.uiMiniPinned, "Mini mode should default to off and unpinned");
    pass &= Expect(defaults.uiMiniX == kUiPosUnset && defaults.uiMiniY == kUiPosUnset, "Mini position should default to unset");

    MicMixSettings settings{};
    settings.uiMiniMode = true;
    settings.uiMiniPinned = true;
    settings.uiMiniX = -1700;
    settings.uiMiniY = 240;
    std::string error;
    pass &= Expect(store.Save(settings, error), "Save(ui_mini) should succeed: " + error);

    MicMixSettings loaded{};
    std::string warning;
    pass &= Expect(store.Load(loaded, warning), "Load(ui_mini) should succeed");
    pass &= Expect(loaded.uiMiniMode, "Mini mode should survive save/load");
    pass &= Expect(loaded.uiMiniPinned, "Mini pin state should survive save/load");
    pass &= Expect(loaded.uiMiniX == -1700 && loaded.uiMiniY == 240, "Mini position (incl. negative x) should survive save/load");

    if (!WriteFile(store.ConfigPath(), "ui.mini.x=50\nui.mini.y=999999\n")) {
        std::cerr << "FAIL: could not write config file for ui_mini" << std::endl;
        return false;
    }
    MicMixSettings bogus{};
    pass &= Expect(store.Load(bogus, warning), "Load(ui_mini bogus) should succeed");
    pass &= Expect(bogus.uiMiniX == kUiPosUnset && bogus.uiMiniY == kUiPosUnset, "Out-of-range position should reset both coordinates");
    return pass;
}

} // namespace

int main() {
    int failed = 0;
    if (!TestJsonWithEqualsInString()) {
        ++failed;
    }
    if (!TestIniStillParses()) {
        ++failed;
    }
    if (!TestInvalidJsonReportsParseWarning()) {
        ++failed;
    }
    if (!TestOversizedConfigIgnored()) {
        ++failed;
    }
    if (!TestUiThemeRoundTrip()) {
        ++failed;
    }
    if (!TestMiniWindowStateRoundTrip()) {
        ++failed;
    }
    CleanupTempDirs();

    if (failed != 0) {
        std::cerr << "config_store_tests failed: " << failed << std::endl;
        return 1;
    }
    std::cout << "config_store_tests passed" << std::endl;
    return 0;
}
