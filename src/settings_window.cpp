#include "settings_window.h"

#include "settings_ui.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Gdiplus.lib")

namespace micmix_ui {









std::mutex g_mutex;
std::mutex g_enumMutex;
std::mutex g_sourceRefreshThreadMutex;
std::thread g_thread;
std::thread g_sourceRefreshThread;
std::atomic<bool> g_running{false};
std::atomic<HWND> g_hwnd{nullptr};
std::vector<LoopbackDeviceInfo> g_loopbacks;
std::vector<CaptureDeviceInfo> g_captureDevices;
std::vector<AppProcessInfo> g_apps;
std::vector<SourceChoice> g_sourceChoices;
int g_lastValidSourceSel = -1;
UiTheme g_theme;
UiThemeMode g_themeMode = UiThemeMode::System;
HWND g_hoverControl = nullptr;
ULONG_PTR g_gdiplusToken = 0;
bool g_gdiplusReady = false;
int g_dpi = 96;
bool g_loadingUi = false;
int g_hotkeyRefreshTick = 0;
HotkeyCaptureTarget g_hotkeyCaptureTarget = HotkeyCaptureTarget::None;
UINT g_muteHotkeyModifiers = 0;
UINT g_muteHotkeyVk = 0;
UINT g_micInputMuteHotkeyModifiers = 0;
UINT g_micInputMuteHotkeyVk = 0;
std::vector<LoopbackDeviceInfo> g_pendingLoopbacks;
std::vector<CaptureDeviceInfo> g_pendingCaptureDevices;
std::vector<AppProcessInfo> g_pendingApps;
std::atomic_uint64_t g_sourceRefreshSeq{0};
std::atomic_uint64_t g_sourceRefreshActiveSeq{0};
std::atomic<bool> g_sourceRefreshInFlight{false};
std::atomic<bool> g_sourceRefreshPending{false};
std::atomic<bool> g_sourceRefreshPendingReload{false};
std::atomic<bool> g_saveDebouncePending{false};
std::unordered_map<uint32_t, HICON> g_appIconsByPid;
HICON g_loopbackFallbackIcon = nullptr;
HICON g_appFallbackIcon = nullptr;
bool g_ownerAutostartChecked = false;
bool g_ownerUpdateCheckChecked = true;
bool g_ownerForceTxChecked = false;
bool g_ownerMicInputMuteChecked = false;
bool g_ownerMuteChecked = false;
HeaderStatusBadgeState g_headerStatusBadgeState = HeaderStatusBadgeState::Off;
bool g_windowTitleActive = false;
bool g_windowTitleStateValid = false;



HFONT g_fontBody = nullptr;
HFONT g_fontSmall = nullptr;
HFONT g_fontHint = nullptr;
HFONT g_fontTiny = nullptr;
HFONT g_fontClipInfo = nullptr;
HFONT g_fontControlLarge = nullptr;
HFONT g_fontTitle = nullptr;
HFONT g_fontMono = nullptr;
HFONT g_fontButton = nullptr;
HFONT g_fontSection = nullptr;
HBRUSH g_brushBg = nullptr;
HBRUSH g_brushCard = nullptr;
HBRUSH g_brushInput = nullptr;
HBRUSH g_brushContent = nullptr;
HFONT g_fontPageTitle = nullptr;
HFONT g_fontRowTitle = nullptr;
HFONT g_fontBrand = nullptr;


std::vector<RowDef> g_rows;
std::vector<CardDef> g_cards;
std::vector<PageControl> g_pageControls;
int g_page = kPageMix;
HWND g_miniHwnd = nullptr;
std::atomic<bool> g_miniMode{false};
bool g_miniPinned = false;
int g_miniMusicLit = -1;
int g_miniMicLit = -1;
RECT g_rcMiniPanel{};
RECT g_rcMiniGainText{};
RECT g_rcMiniMusicMeter{};
RECT g_rcMiniMicMeter{};
RECT g_rcMiniHint{};
std::wstring g_statusTitle;
std::wstring g_statusSubtitle;
RECT g_rcHeaderBadge{};
RECT g_rcMeter{};
RECT g_rcMicMeter{};
RECT g_rcMusicClip{};
RECT g_rcMicClip{};
RECT g_rcStatus{};
RECT g_rcThemeSegment{};
float g_meterVisualDb = -60.0f;
float g_meterHoldDb = -60.0f;
float g_micMeterVisualDb = -60.0f;
float g_micMeterHoldDb = -60.0f;
int g_lastMusicMeterLevelPx = -1;
int g_lastMusicMeterHoldPx = -1;
bool g_lastMusicMeterActive = false;
int g_lastMicMeterLevelPx = -1;
int g_lastMicMeterHoldPx = -1;
bool g_lastMicMeterActive = false;
int g_lastMusicClipLitSegments = -1;
bool g_lastMusicClipRecent = false;
int g_lastMicClipLitSegments = -1;
bool g_lastMicClipRecent = false;
std::wstring g_lastMusicMeterText;
std::wstring g_lastMicMeterText;
TelemetrySnapshot g_lastTelemetry{};
uint64_t g_lastMusicClipEvents = std::numeric_limits<uint64_t>::max();
uint64_t g_lastMicClipEvents = std::numeric_limits<uint64_t>::max();
std::wstring g_lastStatusText;

int g_miniPosX = kUiPosUnset;
std::string g_uiLanguage = "auto";
BackBuffer g_mainBuffer;
bool g_statusTimerIdle = false;
constexpr UINT kStatusTimerIdleMs = 500;
int g_miniPosY = kUiPosUnset;
bool IsOwnerCheckboxControlId(int id) {
    return id == IDC_AUTOSTART || id == IDC_FORCE_TX || id == IDC_MIC_INPUT_MUTE || id == IDC_MUTE || id == IDC_UPDATE_CHECK;
}

bool IsThemeSegmentControlId(int id) {
    return id == IDC_THEME_SYSTEM || id == IDC_THEME_LIGHT || id == IDC_THEME_DARK;
}

bool IsOwnerPushButtonControlId(int id) {
    switch (id) {
    case IDC_SAVE:
    case IDC_MONITOR:
    case IDC_RESCAN:
    case IDC_MUTE_HOTKEY_SET:
    case IDC_MIC_INPUT_HOTKEY_SET:
    case IDC_OPEN_MINI:
        return true;
    default:
        return false;
    }
}

bool IsNavControlId(int id) {
    return id >= IDC_NAV_FIRST && id <= IDC_NAV_LAST;
}

UiThemeMode ThemeModeForControlId(int id) {
    if (id == IDC_THEME_LIGHT) return UiThemeMode::Light;
    if (id == IDC_THEME_DARK) return UiThemeMode::Dark;
    return UiThemeMode::System;
}

bool IsHandCursorControlId(int id) {
    switch (id) {
    case IDC_REPO_LINK:
    case IDC_SOURCE:
    case IDC_MIC_DEVICE:
    case IDC_RESCAN:
    case IDC_SAVE:
    case IDC_MONITOR:
    case IDC_AUTOSTART:
    case IDC_FORCE_TX:
    case IDC_MIC_INPUT_MUTE:
    case IDC_MUTE:
    case IDC_MUTE_HOTKEY_SET:
    case IDC_MIC_INPUT_HOTKEY_SET:
    case IDC_THEME_SYSTEM:
    case IDC_THEME_LIGHT:
    case IDC_THEME_DARK:
    case IDC_POWER:
    case IDC_OPEN_MINI:
    case IDC_LANGUAGE:
    case IDC_UPDATE:
    case IDC_UPDATE_CHECK:
    case IDC_MINI_POWER:
    case IDC_MINI_SOURCE:
    case IDC_MINI_MUTE_MUSIC:
    case IDC_MINI_MUTE_MIC:
    case IDC_MINI_PIN:
    case IDC_MINI_EXPAND:
    case IDC_MINI_CLOSE:
        return true;
    default:
        return IsNavControlId(id);
    }
}

bool IsPointOverGainThumb(HWND gainHwnd, const POINT& screenPt) {
    if (!gainHwnd) {
        return false;
    }
    RECT thumbRc{};
    SendMessageW(gainHwnd, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumbRc));
    POINT clientPt = screenPt;
    if (!ScreenToClient(gainHwnd, &clientPt)) {
        return false;
    }
    return PtInRect(&thumbRc, clientPt) != FALSE;
}

bool GetOwnerCheckboxValue(int id) {
    switch (id) {
    case IDC_AUTOSTART: return g_ownerAutostartChecked;
    case IDC_UPDATE_CHECK: return g_ownerUpdateCheckChecked;
    case IDC_FORCE_TX: return g_ownerForceTxChecked;
    case IDC_MIC_INPUT_MUTE: return g_ownerMicInputMuteChecked;
    case IDC_MUTE: return g_ownerMuteChecked;
    default: return false;
    }
}

void SetOwnerCheckboxValue(int id, bool value) {
    switch (id) {
    case IDC_AUTOSTART: g_ownerAutostartChecked = value; break;
    case IDC_UPDATE_CHECK: g_ownerUpdateCheckChecked = value; break;
    case IDC_FORCE_TX: g_ownerForceTxChecked = value; break;
    case IDC_MIC_INPUT_MUTE: g_ownerMicInputMuteChecked = value; break;
    case IDC_MUTE: g_ownerMuteChecked = value; break;
    default: break;
    }
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

void DestroyIconHandle(HICON& icon) {
    if (icon) {
        DestroyIcon(icon);
        icon = nullptr;
    }
}

void ClearAppIconCache() {
    for (auto& entry : g_appIconsByPid) {
        DestroyIconHandle(entry.second);
    }
    g_appIconsByPid.clear();
}

HICON CopySharedIcon(HICON sharedIcon) {
    if (!sharedIcon) {
        return nullptr;
    }
    return CopyIcon(sharedIcon);
}

HICON LoadStockSmallIcon(SHSTOCKICONID stockId) {
    SHSTOCKICONINFO sii{};
    sii.cbSize = sizeof(sii);
    if (SUCCEEDED(SHGetStockIconInfo(stockId, SHGSI_ICON | SHGSI_SMALLICON, &sii)) && sii.hIcon) {
        return sii.hIcon;
    }
    return nullptr;
}

bool TryGetProcessImagePath(uint32_t pid, std::wstring& outPath) {
    outPath.clear();
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = static_cast<DWORD>(buffer.size());
    while (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) == FALSE) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer.size() >= 32768) {
            CloseHandle(process);
            return false;
        }
        buffer.resize(buffer.size() * 2);
        size = static_cast<DWORD>(buffer.size());
    }
    buffer.resize(size);
    outPath = std::move(buffer);
    CloseHandle(process);
    return true;
}

HICON TryLoadExeSmallIcon(const std::wstring& exePath) {
    if (exePath.empty()) {
        return nullptr;
    }
    SHFILEINFOW sfi{};
    if (SHGetFileInfoW(exePath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) == 0) {
        return nullptr;
    }
    return sfi.hIcon;
}

void EnsureFallbackIcons() {
    if (!g_loopbackFallbackIcon) {
        g_loopbackFallbackIcon = LoadStockSmallIcon(SIID_AUDIOFILES);
        if (!g_loopbackFallbackIcon) {
            g_loopbackFallbackIcon = CopySharedIcon(LoadIconW(nullptr, IDI_INFORMATION));
        }
    }
    if (!g_appFallbackIcon) {
        g_appFallbackIcon = LoadStockSmallIcon(SIID_APPLICATION);
        if (!g_appFallbackIcon) {
            g_appFallbackIcon = CopySharedIcon(LoadIconW(nullptr, IDI_APPLICATION));
        }
    }
}

void EnsureAppIconForPid(uint32_t pid) {
    if (pid == 0 || g_appIconsByPid.find(pid) != g_appIconsByPid.end()) {
        return;
    }
    std::wstring exePath;
    if (TryGetProcessImagePath(pid, exePath)) {
        if (HICON icon = TryLoadExeSmallIcon(exePath)) {
            g_appIconsByPid.emplace(pid, icon);
            return;
        }
    }
    if (g_appFallbackIcon) {
        if (HICON iconCopy = CopyIcon(g_appFallbackIcon)) {
            g_appIconsByPid.emplace(pid, iconCopy);
        }
    }
}

void PruneAppIconCache(const std::unordered_set<uint32_t>& activePids) {
    for (auto it = g_appIconsByPid.begin(); it != g_appIconsByPid.end();) {
        if (activePids.find(it->first) == activePids.end()) {
            DestroyIconHandle(it->second);
            it = g_appIconsByPid.erase(it);
        } else {
            ++it;
        }
    }
}

std::wstring ToLowerWide(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return text;
}

std::wstring NormalizeDeviceName(std::wstring text) {
    text = ToLowerWide(std::move(text));
    const size_t defPos = text.find(L"(default)");
    if (defPos != std::wstring::npos) {
        text = text.substr(0, defPos);
    }
    while (!text.empty() && iswspace(text.back()) != 0) {
        text.pop_back();
    }
    while (!text.empty() && iswspace(text.front()) != 0) {
        text.erase(text.begin());
    }
    return text;
}

HINSTANCE GetPluginModuleHandle() {
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&GetPluginModuleHandle),
                           &mod) == 0 || mod == nullptr) {
        return GetModuleHandleW(nullptr);
    }
    return mod;
}

void SetControlFont(HWND hwnd, int id, HFONT font) {
    HWND ctl = GetDlgItem(hwnd, id);
    if (ctl && font) SendMessageW(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void SetStatusText(HWND hwnd, const std::wstring& text) {
    if (text == g_lastStatusText) {
        return;
    }
    g_lastStatusText = text;
    if (hwnd && g_rcStatus.right > g_rcStatus.left && g_rcStatus.bottom > g_rcStatus.top) {
        // Only the Diagnostics page shows this text. Its rectangle overlaps controls of other
        // pages (e.g. the microphone dropdown), which would otherwise be repainted constantly.
        if (g_page == kPageDiagnostics) {
            InvalidateRect(hwnd, &g_rcStatus, FALSE);
        }
    }
}

void SetControlEnabled(HWND hwnd, int controlId, bool enabled) {
    HWND ctl = GetDlgItem(hwnd, controlId);
    if (!ctl) {
        return;
    }
    const BOOL currentlyEnabled = IsWindowEnabled(ctl);
    if ((currentlyEnabled != FALSE) == enabled) {
        return;
    }
    EnableWindow(ctl, enabled ? TRUE : FALSE);
    InvalidateRect(ctl, nullptr, TRUE);
}

HeaderStatusBadgeState ResolveHeaderStatusBadgeState(const MicMixSettings& settings, const SourceStatus& status) {
    if (status.state == SourceState::Error) {
        return HeaderStatusBadgeState::Error;
    }
    if (!IsSourceStateActive(status.state)) {
        return HeaderStatusBadgeState::Off;
    }
    if (settings.musicMuted) {
        return HeaderStatusBadgeState::Muted;
    }
    return HeaderStatusBadgeState::Active;
}

void UpdateWindowTitleState(HWND hwnd, bool active) {
    if (!hwnd) {
        return;
    }
    if (g_windowTitleStateValid && g_windowTitleActive == active) {
        return;
    }
    g_windowTitleStateValid = true;
    g_windowTitleActive = active;
    const std::wstring title = std::wstring(Tr(Str::WindowTitle)) + L" \x2013 " + Tr(active ? Str::PowerOn : Str::PowerOff);
    SetWindowTextW(hwnd, title.c_str());
}

void UpdateHeaderStatusBadgeState(HWND hwnd, const MicMixSettings& settings, const SourceStatus& status) {
    const HeaderStatusBadgeState nextBadge = ResolveHeaderStatusBadgeState(settings, status);
    if (nextBadge != g_headerStatusBadgeState) {
        g_headerStatusBadgeState = nextBadge;
        InvalidateRect(GetDlgItem(hwnd, IDC_POWER), nullptr, FALSE);
        if (g_rcHeaderBadge.right > g_rcHeaderBadge.left) {
            InvalidateRect(hwnd, &g_rcHeaderBadge, FALSE);
        } else {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
    UpdateWindowTitleState(hwnd, IsSourceStateActive(status.state) && status.state != SourceState::Error);
}

int GainDbToSlider(float db) {
    const float clamped = std::clamp(db, kMusicGainMinDb, kMusicGainMaxDb);
    const int value = static_cast<int>((clamped - kMusicGainMinDb) * static_cast<float>(kMusicGainStepPerDb));
    return std::max(0, std::min(kMusicGainSliderMax, value));
}

float SliderToGainDb(int slider) {
    const int clamped = std::max(0, std::min(kMusicGainSliderMax, slider));
    return kMusicGainMinDb + (static_cast<float>(clamped) / static_cast<float>(kMusicGainStepPerDb));
}

int MeterDbToPixels(float dbfs, int widthPx) {
    const float clamped = std::clamp(dbfs, -60.0f, 0.0f);
    return static_cast<int>(((clamped + 60.0f) / 60.0f) * static_cast<float>(widthPx));
}

float MeterDbToUnit(float dbfs) {
    const float clamped = std::clamp(dbfs, -60.0f, 0.0f);
    return (clamped + 60.0f) / 60.0f;
}

float ClipDangerUnitFromDb(float dbfs) {
    // Clip risk visualization:
    // <= -18 dBFS: low risk (0.0), 0 dBFS: full risk (1.0).
    const float clamped = std::clamp(dbfs, -18.0f, 0.0f);
    return (clamped + 18.0f) / 18.0f;
}

int ClipLitSegmentsFromDb(float dbfs) {
    constexpr int kClipSegments = 18;
    const float dangerUnit = ClipDangerUnitFromDb(dbfs);
    return std::clamp(static_cast<int>((dangerUnit * static_cast<float>(kClipSegments)) + 0.5f), 0, kClipSegments);
}

void SetStaticTextIfChanged(HWND ctl, std::wstring& cache, const std::wstring& nextText) {
    if (!ctl || cache == nextText) {
        return;
    }
    cache = nextText;
    SetWindowTextW(ctl, cache.c_str());
}

bool IsMusicMeterActive(const TelemetrySnapshot& t) {
    const float shownPeakDb = (t.musicSendPeakDbfs > -119.0f) ? t.musicSendPeakDbfs : t.musicPeakDbfs;
    const bool levelSuggestsSignal = (shownPeakDb > -96.0f) || (t.musicRmsDbfs > -100.0f);
    return t.musicActive || levelSuggestsSignal;
}

void UpdateMusicMeter(HWND hwnd) {
    HWND txt = GetDlgItem(hwnd, IDC_METER_TEXT);
    if (!txt) {
        return;
    }
    const TelemetrySnapshot t = g_lastTelemetry;
    const float shownPeakDb = (t.musicSendPeakDbfs > -119.0f) ? t.musicSendPeakDbfs : t.musicPeakDbfs;
    const bool meterActive = IsMusicMeterActive(t);

    float targetDb = -60.0f;
    if (meterActive) {
        targetDb = std::clamp(shownPeakDb, -60.0f, 0.0f);
    }
    if (targetDb > g_meterVisualDb) {
        g_meterVisualDb += (targetDb - g_meterVisualDb) * 0.70f;
    } else {
        g_meterVisualDb += (targetDb - g_meterVisualDb) * 0.30f;
    }
    g_meterVisualDb = std::clamp(g_meterVisualDb, -60.0f, 0.0f);

    if (targetDb >= g_meterHoldDb) {
        g_meterHoldDb = targetDb;
    } else {
        g_meterHoldDb = std::max(-60.0f, g_meterHoldDb - 0.6f);
    }

    if (!meterActive) {
        SetStaticTextIfChanged(txt, g_lastMusicMeterText, Tr(Str::NoSignal));
    } else {
        const wchar_t* grade = Tr(Str::LevelOk);
        if (shownPeakDb > -8.0f) {
            grade = Tr(Str::LevelHot);
        } else if (shownPeakDb > -16.0f) {
            grade = Tr(Str::LevelWarm);
        } else if (shownPeakDb < -35.0f) {
            grade = Tr(Str::LevelQuiet);
        }
        wchar_t meter[96];
        swprintf_s(meter, L"%.1f dBFS  |  %s", shownPeakDb, grade);
        SetStaticTextIfChanged(txt, g_lastMusicMeterText, meter);
    }

    const int meterWidth = std::max(1, static_cast<int>(g_rcMeter.right - g_rcMeter.left));
    const int levelPx = std::clamp(MeterDbToPixels(g_meterVisualDb, meterWidth), 0, meterWidth);
    const int holdPx = std::clamp(MeterDbToPixels(g_meterHoldDb, meterWidth), 0, meterWidth - 1);
    const bool meterChanged =
        (levelPx != g_lastMusicMeterLevelPx) ||
        (holdPx != g_lastMusicMeterHoldPx) ||
        (meterActive != g_lastMusicMeterActive);
    g_lastMusicMeterLevelPx = levelPx;
    g_lastMusicMeterHoldPx = holdPx;
    g_lastMusicMeterActive = meterActive;
    if (meterChanged) {
        if (g_page == kPageMix) {
            InvalidateRect(hwnd, &g_rcMeter, FALSE);
        }
    }

    const int clipLitSegments = ClipLitSegmentsFromDb(shownPeakDb);
    const bool clipChanged =
        (clipLitSegments != g_lastMusicClipLitSegments) ||
        (t.sourceClipRecent != g_lastMusicClipRecent);
    g_lastMusicClipLitSegments = clipLitSegments;
    g_lastMusicClipRecent = t.sourceClipRecent;
    if (clipChanged) {
        if (g_page == kPageMix) {
            InvalidateRect(hwnd, &g_rcMusicClip, FALSE);
        }
    }
}

void UpdateMicMeter(HWND hwnd) {
    HWND txt = GetDlgItem(hwnd, IDC_MIC_METER_TEXT);
    if (!txt) {
        return;
    }
    const TelemetrySnapshot t = g_lastTelemetry;
    float targetDb = -60.0f;
    if (t.micRmsDbfs > -119.0f) {
        targetDb = std::clamp(t.micRmsDbfs, -60.0f, 0.0f);
    }
    if (targetDb > g_micMeterVisualDb) {
        g_micMeterVisualDb += (targetDb - g_micMeterVisualDb) * 0.68f;
    } else {
        g_micMeterVisualDb += (targetDb - g_micMeterVisualDb) * 0.28f;
    }
    g_micMeterVisualDb = std::clamp(g_micMeterVisualDb, -60.0f, 0.0f);

    if (targetDb >= g_micMeterHoldDb) {
        g_micMeterHoldDb = targetDb;
    } else {
        g_micMeterHoldDb = std::max(-60.0f, g_micMeterHoldDb - 0.7f);
    }

    if (t.micRmsDbfs <= -119.0f) {
        SetStaticTextIfChanged(txt, g_lastMicMeterText, Tr(Str::NoSignal));
    } else {
        const wchar_t* grade = Tr(Str::LevelOk);
        if (t.micPeakDbfs > -8.0f) {
            grade = Tr(Str::LevelHot);
        } else if (t.micPeakDbfs > -16.0f) {
            grade = Tr(Str::LevelWarm);
        } else if (t.micPeakDbfs < -35.0f) {
            grade = Tr(Str::LevelQuiet);
        }
        wchar_t meter[96];
        swprintf_s(meter, L"%.1f dBFS  |  %s", t.micRmsDbfs, grade);
        SetStaticTextIfChanged(txt, g_lastMicMeterText, meter);
    }

    const int meterWidth = std::max(1, static_cast<int>(g_rcMicMeter.right - g_rcMicMeter.left));
    const int levelPx = std::clamp(MeterDbToPixels(g_micMeterVisualDb, meterWidth), 0, meterWidth);
    const int holdPx = std::clamp(MeterDbToPixels(g_micMeterHoldDb, meterWidth), 0, meterWidth - 1);
    const bool meterActive = t.micRmsDbfs > -119.0f;
    const bool meterChanged =
        (levelPx != g_lastMicMeterLevelPx) ||
        (holdPx != g_lastMicMeterHoldPx) ||
        (meterActive != g_lastMicMeterActive);
    g_lastMicMeterLevelPx = levelPx;
    g_lastMicMeterHoldPx = holdPx;
    g_lastMicMeterActive = meterActive;
    if (meterChanged) {
        if (g_page == kPageMix) {
            InvalidateRect(hwnd, &g_rcMicMeter, FALSE);
        }
    }

    const int clipLitSegments = ClipLitSegmentsFromDb(t.micPeakDbfs);
    const bool clipChanged =
        (clipLitSegments != g_lastMicClipLitSegments) ||
        (t.micClipRecent != g_lastMicClipRecent);
    g_lastMicClipLitSegments = clipLitSegments;
    g_lastMicClipRecent = t.micClipRecent;
    if (clipChanged) {
        if (g_page == kPageMix) {
            InvalidateRect(hwnd, &g_rcMicClip, FALSE);
        }
    }
}

void UpdateMonitorButton(HWND hwnd) {
    HWND btn = GetDlgItem(hwnd, IDC_MONITOR);
    if (!btn) {
        return;
    }
    const bool enabled = MicMixApp::Instance().IsMonitorEnabled();
    const wchar_t* next = Tr(enabled ? Str::MonitorOn : Str::MonitorOff);
    wchar_t current[64]{};
    GetWindowTextW(btn, current, static_cast<int>(std::size(current)));
    if (wcscmp(current, next) != 0) {
        SetWindowTextW(btn, next);
        InvalidateRect(btn, nullptr, FALSE);
    }
}

void UpdateGainLabel(HWND hwnd) {
    const int slider = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_GAIN), TBM_GETPOS, 0, 0));
    const float db = SliderToGainDb(slider);
    wchar_t buf[64];
    swprintf_s(buf, L"%+.1f dB", db);
    SetWindowTextW(GetDlgItem(hwnd, IDC_GAIN_VALUE), buf);
}

bool IsModifierOnlyKey(UINT vk) {
    return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
           vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
           vk == VK_LWIN || vk == VK_RWIN;
}

UINT ReadCurrentModifiers() {
    UINT mods = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) mods |= MOD_CONTROL;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) mods |= MOD_ALT;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) mods |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) mods |= MOD_WIN;
    return mods;
}

bool IsExtendedVk(UINT vk) {
    switch (vk) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_DIVIDE:
    case VK_RCONTROL:
    case VK_RMENU:
        return true;
    default:
        return false;
    }
}

std::wstring VkToText(UINT vk) {
    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    LONG lParam = static_cast<LONG>(scanCode << 16);
    if (IsExtendedVk(vk)) {
        lParam |= (1 << 24);
    }
    wchar_t keyName[128]{};
    if (GetKeyNameTextW(lParam, keyName, static_cast<int>(std::size(keyName))) > 0) {
        return keyName;
    }
    wchar_t fallback[32]{};
    swprintf_s(fallback, L"VK %u", vk);
    return fallback;
}

std::wstring FormatHotkeyText(UINT mods, UINT vk) {
    if (vk == 0) {
        return Tr(Str::HotkeyNotSet);
    }
    std::wstring out;
    if ((mods & MOD_CONTROL) != 0) out += std::wstring(Tr(Str::KeyCtrl)) + L" + ";
    if ((mods & MOD_ALT) != 0) out += L"Alt + ";
    if ((mods & MOD_SHIFT) != 0) out += std::wstring(Tr(Str::KeyShift)) + L" + ";
    if ((mods & MOD_WIN) != 0) out += L"Win + ";
    out += VkToText(vk);
    return out;
}

bool IsHotkeyConflict(UINT lhsMods, UINT lhsVk, UINT rhsMods, UINT rhsVk) {
    return lhsVk != 0 && rhsVk != 0 && lhsVk == rhsVk && lhsMods == rhsMods;
}

bool IsCapturingHotkey() {
    return g_hotkeyCaptureTarget != HotkeyCaptureTarget::None;
}

void UpdateMuteHotkeyLabel(HWND hwnd) {
    if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MusicMute) {
        return;
    }
    HWND label = GetDlgItem(hwnd, IDC_MUTE_HOTKEY_TEXT);
    if (!label) {
        return;
    }
    const std::wstring text = std::wstring(Tr(Str::HotkeyCurrentPrefix)) + FormatHotkeyText(g_muteHotkeyModifiers, g_muteHotkeyVk);
    SetWindowTextW(label, text.c_str());
}

void UpdateMicInputMuteHotkeyLabel(HWND hwnd) {
    if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MicInputMute) {
        return;
    }
    HWND label = GetDlgItem(hwnd, IDC_MIC_INPUT_HOTKEY_TEXT);
    if (!label) {
        return;
    }
    const std::wstring text = std::wstring(Tr(Str::HotkeyCurrentPrefix)) + FormatHotkeyText(g_micInputMuteHotkeyModifiers, g_micInputMuteHotkeyVk);
    SetWindowTextW(label, text.c_str());
}

void UpdateHotkeyLabels(HWND hwnd) {
    UpdateMuteHotkeyLabel(hwnd);
    UpdateMicInputMuteHotkeyLabel(hwnd);
}

void BeginHotkeyCapture(HWND hwnd, HotkeyCaptureTarget target) {
    g_hotkeyCaptureTarget = target;
    MicMixApp::Instance().SetGlobalHotkeyCaptureBlocked(true);
    int labelId = 0;
    if (target == HotkeyCaptureTarget::MusicMute) {
        labelId = IDC_MUTE_HOTKEY_TEXT;
    } else if (target == HotkeyCaptureTarget::MicInputMute) {
        labelId = IDC_MIC_INPUT_HOTKEY_TEXT;
    }
    if (labelId != 0) {
        SetWindowTextW(GetDlgItem(hwnd, labelId), Tr(Str::HotkeyPress));
    }
    InvalidateRect(GetDlgItem(hwnd, IDC_MUTE_HOTKEY_SET), nullptr, FALSE);
    InvalidateRect(GetDlgItem(hwnd, IDC_MIC_INPUT_HOTKEY_SET), nullptr, FALSE);
    SetFocus(hwnd);
}

void JoinSourceRefreshThread() {
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(g_sourceRefreshThreadMutex);
        if (!g_sourceRefreshThread.joinable()) {
            return;
        }
        worker = std::move(g_sourceRefreshThread);
    }
    worker.join();
}

bool StartSourceRefreshWorker(HWND hwnd, uint64_t seq) {
    if (!hwnd) {
        return false;
    }

    JoinSourceRefreshThread();
    try {
        std::thread worker([hwnd, seq]() {
            auto& app = MicMixApp::Instance();
            std::vector<LoopbackDeviceInfo> loopbacks = app.GetLoopbackDevices();
            std::vector<CaptureDeviceInfo> captureDevices = app.GetCaptureDevices();
            std::vector<AppProcessInfo> apps = app.GetAppProcesses();
            {
                std::lock_guard<std::mutex> lock(g_enumMutex);
                g_pendingLoopbacks = std::move(loopbacks);
                g_pendingCaptureDevices = std::move(captureDevices);
                g_pendingApps = std::move(apps);
            }
            if (!PostMessageW(hwnd, kMsgSourceRefreshDone, static_cast<WPARAM>(seq), 0)) {
                g_sourceRefreshInFlight.store(false, std::memory_order_release);
                g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
                const HWND currentHwnd = g_hwnd.load(std::memory_order_acquire);
                if (currentHwnd != nullptr) {
                    LogWarn("settings_window refresh completion post failed");
                }
            }
        });
        {
            std::lock_guard<std::mutex> lock(g_sourceRefreshThreadMutex);
            g_sourceRefreshThread = std::move(worker);
        }
        return true;
    } catch (const std::exception& ex) {
        g_sourceRefreshInFlight.store(false, std::memory_order_release);
        g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
        LogError(std::string("settings_window refresh thread start failed: ") + ex.what());
    } catch (...) {
        g_sourceRefreshInFlight.store(false, std::memory_order_release);
        g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
        LogError("settings_window refresh thread start failed: unknown");
    }
    return false;
}

void RequestSourceRefresh(HWND hwnd, bool reloadSettings) {
    if (!hwnd) {
        return;
    }
    if (reloadSettings) {
        g_sourceRefreshPendingReload.store(true, std::memory_order_release);
    }
    HWND refreshBtn = GetDlgItem(hwnd, IDC_RESCAN);
    if (refreshBtn) {
        EnableWindow(refreshBtn, FALSE);
    }
    SetStatusText(hwnd, Tr(Str::RefreshingSources));

    if (g_sourceRefreshInFlight.exchange(true, std::memory_order_acq_rel)) {
        g_sourceRefreshPending.store(true, std::memory_order_release);
        return;
    }
    const uint64_t seq = g_sourceRefreshSeq.fetch_add(1, std::memory_order_acq_rel) + 1ULL;
    g_sourceRefreshActiveSeq.store(seq, std::memory_order_release);
    if (!StartSourceRefreshWorker(hwnd, seq)) {
        g_sourceRefreshInFlight.store(false, std::memory_order_release);
        g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
        if (refreshBtn) {
            EnableWindow(refreshBtn, TRUE);
        }
        SetStatusText(hwnd, Tr(Str::RefreshFailed));
    }
}

void PopulateCombos(HWND hwnd) {
    g_sourceChoices.clear();
    g_lastValidSourceSel = -1;
    EnsureFallbackIcons();
    std::unordered_set<uint32_t> activeAppPids;

    auto addStaticItem = [](HWND combo, const wchar_t* text, INT_PTR data) {
        const LRESULT idx = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        if (idx >= 0) {
            SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(idx), static_cast<LPARAM>(data));
        }
    };

    auto addSelectableItem = [](HWND combo, const std::wstring& text, const SourceChoice& choice) {
        const LRESULT idx = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        if (idx >= 0) {
            g_sourceChoices.push_back(choice);
            const INT_PTR itemData = static_cast<INT_PTR>(g_sourceChoices.size() - 1);
            SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(idx), static_cast<LPARAM>(itemData));
        }
    };

    HWND source = GetDlgItem(hwnd, IDC_SOURCE);
    SendMessageW(source, CB_RESETCONTENT, 0, 0);

    addStaticItem(source, Tr(Str::SourceHeaderChannels), kSourceItemHeader);
    if (g_loopbacks.empty()) {
        addStaticItem(source, (std::wstring(L"  ") + Tr(Str::SourceNoChannels)).c_str(), kSourceItemPlaceholder);
    } else {
        for (const auto& d : g_loopbacks) {
            std::wstring label = L"  ";
            label += Utf8ToWide(d.name);
            if (d.isDefault) {
                label += Tr(Str::DefaultSuffix);
            }
            SourceChoice choice{};
            choice.type = SourceChoiceType::Loopback;
            choice.id = d.id;
            addSelectableItem(source, label, choice);
        }
    }

    addStaticItem(source, L"------------------------------", kSourceItemDivider);
    addStaticItem(source, Tr(Str::SourceHeaderApps), kSourceItemHeader);
    if (g_apps.empty()) {
        addStaticItem(source, (std::wstring(L"  ") + Tr(Str::SourceNoApps)).c_str(), kSourceItemPlaceholder);
    } else {
        for (const auto& p : g_apps) {
            std::wstring label = L"  ";
            label += Utf8ToWide(p.displayName);
            SourceChoice choice{};
            choice.type = SourceChoiceType::App;
            choice.id = std::to_string(p.pid);
            choice.processName = p.exeName;
            choice.pid = p.pid;
            addSelectableItem(source, label, choice);
            activeAppPids.insert(p.pid);
            EnsureAppIconForPid(p.pid);
        }
    }
    PruneAppIconCache(activeAppPids);

    HWND mic = GetDlgItem(hwnd, IDC_MIC_DEVICE);
    if (mic) {
        SendMessageW(mic, CB_RESETCONTENT, 0, 0);
        for (const auto& d : g_captureDevices) {
            std::wstring label = Utf8ToWide(d.name);
            if (d.isDefault) {
                label += Tr(Str::DefaultSuffix);
            }
            SendMessageW(mic, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
    }
}

void LoadSettings(HWND hwnd) {
    g_loadingUi = true;
    auto& app = MicMixApp::Instance();
    const MicMixSettings s = app.GetSettings();
    const std::wstring tsCaptureNameNorm = NormalizeDeviceName(Utf8ToWide(app.GetPreferredTsCaptureDeviceName()));

    int selectedComboIdx = -1;
    int firstLoopbackIdx = -1;
    int firstSelectableIdx = -1;

    HWND source = GetDlgItem(hwnd, IDC_SOURCE);
    const int count = static_cast<int>(SendMessageW(source, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        const LRESULT data = SendMessageW(source, CB_GETITEMDATA, static_cast<WPARAM>(i), 0);
        if (data < 0 || data == CB_ERR || static_cast<size_t>(data) >= g_sourceChoices.size()) {
            continue;
        }
        if (firstSelectableIdx < 0) {
            firstSelectableIdx = i;
        }
        const SourceChoice& choice = g_sourceChoices[static_cast<size_t>(data)];
        if (choice.type == SourceChoiceType::Loopback && firstLoopbackIdx < 0) {
            firstLoopbackIdx = i;
        }
        if (s.sourceMode == SourceMode::Loopback && choice.type == SourceChoiceType::Loopback && choice.id == s.loopbackDeviceId) {
            selectedComboIdx = i;
            break;
        }
        if (s.sourceMode == SourceMode::AppSession && choice.type == SourceChoiceType::App) {
            if (choice.id == s.appSessionId) {
                selectedComboIdx = i;
                break;
            }
            if (selectedComboIdx < 0 && !s.appProcessName.empty() && _stricmp(choice.processName.c_str(), s.appProcessName.c_str()) == 0) {
                selectedComboIdx = i;
            }
        }
    }
    if (selectedComboIdx < 0) {
        if (s.sourceMode == SourceMode::Loopback && firstLoopbackIdx >= 0) {
            selectedComboIdx = firstLoopbackIdx;
        } else {
            selectedComboIdx = firstSelectableIdx;
        }
    }
    if (selectedComboIdx >= 0) {
        SendMessageW(source, CB_SETCURSEL, selectedComboIdx, 0);
        g_lastValidSourceSel = selectedComboIdx;
    }

    int micSel = -1;
    int micDefaultIdx = -1;
    HWND micCombo = GetDlgItem(hwnd, IDC_MIC_DEVICE);
    for (size_t i = 0; i < g_captureDevices.size(); ++i) {
        const auto& d = g_captureDevices[i];
        if (d.isDefault && micDefaultIdx < 0) {
            micDefaultIdx = static_cast<int>(i);
        }
        if (!s.captureDeviceId.empty() && d.id == s.captureDeviceId) {
            micSel = static_cast<int>(i);
            break;
        }
        if (micSel < 0 && !tsCaptureNameNorm.empty()) {
            const std::wstring devNorm = NormalizeDeviceName(Utf8ToWide(d.name));
            if (devNorm == tsCaptureNameNorm) {
                micSel = static_cast<int>(i);
            }
        }
    }
    if (micSel < 0) {
        micSel = micDefaultIdx;
    }
    if (micCombo && micSel >= 0) {
        SendMessageW(micCombo, CB_SETCURSEL, micSel, 0);
    }

    SendMessageW(GetDlgItem(hwnd, IDC_GAIN), TBM_SETPOS, TRUE, GainDbToSlider(s.musicGainDb));
    SetOwnerCheckboxValue(IDC_FORCE_TX, s.forceTxEnabled);
    SetOwnerCheckboxValue(IDC_MIC_INPUT_MUTE, s.micInputMuted);
    SetOwnerCheckboxValue(IDC_MUTE, s.musicMuted);
    SetOwnerCheckboxValue(IDC_AUTOSTART, s.autostartEnabled);
    SetOwnerCheckboxValue(IDC_UPDATE_CHECK, s.uiUpdateCheck);
    InvalidateRect(GetDlgItem(hwnd, IDC_UPDATE_CHECK), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_FORCE_TX), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_MIC_INPUT_MUTE), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_MUTE), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_AUTOSTART), nullptr, TRUE);
    g_muteHotkeyModifiers = static_cast<UINT>(std::max(0, s.muteHotkeyModifiers));
    g_muteHotkeyVk = static_cast<UINT>(std::max(0, s.muteHotkeyVk));
    g_micInputMuteHotkeyModifiers = static_cast<UINT>(std::max(0, s.micInputMuteHotkeyModifiers));
    g_micInputMuteHotkeyVk = static_cast<UINT>(std::max(0, s.micInputMuteHotkeyVk));
    UpdateGainLabel(hwnd);
    UpdateHotkeyLabels(hwnd);
    if (s.uiThemeMode != g_themeMode) {
        g_themeMode = s.uiThemeMode;
        ApplyTheme(hwnd);
    }
    SyncMiniFromMain();
    g_loadingUi = false;
}

MicMixSettings CollectSettings(HWND hwnd) {
    auto s = MicMixApp::Instance().GetSettings();

    const int sourceSel = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), CB_GETCURSEL, 0, 0));
    if (sourceSel >= 0) {
        const LRESULT data = SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), CB_GETITEMDATA, static_cast<WPARAM>(sourceSel), 0);
        if (data >= 0 && data != CB_ERR && static_cast<size_t>(data) < g_sourceChoices.size()) {
            const SourceChoice& choice = g_sourceChoices[static_cast<size_t>(data)];
            if (choice.type == SourceChoiceType::Loopback) {
                s.sourceMode = SourceMode::Loopback;
                s.loopbackDeviceId = choice.id;
            } else {
                s.sourceMode = SourceMode::AppSession;
                s.appSessionId = choice.id;
                s.appProcessName = choice.processName;
            }
        }
    }

    const int micSel = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_MIC_DEVICE), CB_GETCURSEL, 0, 0));
    if (micSel >= 0 && static_cast<size_t>(micSel) < g_captureDevices.size()) {
        s.captureDeviceId = g_captureDevices[static_cast<size_t>(micSel)].id;
    }

    s.musicGainDb = SliderToGainDb(static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_GAIN), TBM_GETPOS, 0, 0)));
    s.forceTxEnabled = GetOwnerCheckboxValue(IDC_FORCE_TX);
    s.micInputMuted = GetOwnerCheckboxValue(IDC_MIC_INPUT_MUTE);
    s.musicMuted = GetOwnerCheckboxValue(IDC_MUTE);
    s.autostartEnabled = GetOwnerCheckboxValue(IDC_AUTOSTART);
    s.uiUpdateCheck = GetOwnerCheckboxValue(IDC_UPDATE_CHECK);
    s.muteHotkeyModifiers = static_cast<int>(g_muteHotkeyModifiers);
    s.muteHotkeyVk = static_cast<int>(g_muteHotkeyVk);
    s.micInputMuteHotkeyModifiers = static_cast<int>(g_micInputMuteHotkeyModifiers);
    s.micInputMuteHotkeyVk = static_cast<int>(g_micInputMuteHotkeyVk);
    s.uiThemeMode = g_themeMode;
    s.uiLastOpenTab = g_page;
    s.uiMiniMode = g_miniMode.load(std::memory_order_acquire);
    s.uiMiniPinned = g_miniPinned;
    s.uiMiniX = g_miniPosX;
    s.uiMiniY = g_miniPosY;
    s.uiLanguage = g_uiLanguage;
    return s;
}

void UpdateStatus(HWND hwnd, bool includeDetails) {
    auto& app = MicMixApp::Instance();
    const TelemetrySnapshot t = app.GetTelemetry();
    g_lastTelemetry = t;

    if (t.sourceClipEvents != g_lastMusicClipEvents) {
        wchar_t clipMusic[64]{};
        swprintf_s(clipMusic, Tr(Str::ClipEvents), static_cast<unsigned long long>(t.sourceClipEvents));
        SetWindowTextW(GetDlgItem(hwnd, IDC_MUSIC_CLIP_EVENTS), clipMusic);
        g_lastMusicClipEvents = t.sourceClipEvents;
    }
    if (t.micClipEvents != g_lastMicClipEvents) {
        wchar_t clipMic[64]{};
        swprintf_s(clipMic, Tr(Str::ClipEvents), static_cast<unsigned long long>(t.micClipEvents));
        SetWindowTextW(GetDlgItem(hwnd, IDC_MIC_CLIP_EVENTS), clipMic);
        g_lastMicClipEvents = t.micClipEvents;
    }

    UpdateMusicMeter(hwnd);
    UpdateMicMeter(hwnd);
    if (!includeDetails) {
        RefreshMini(false);
        return;
    }

    const MicMixSettings s = app.GetSettings();
    const SourceStatus st = app.GetSourceStatus();
    const bool sourceUp = IsSourceStateActive(st.state);
    SetControlEnabled(hwnd, IDC_MIC_INPUT_MUTE, sourceUp);
    SetControlEnabled(hwnd, IDC_MUTE, sourceUp);
    UpdateHeaderStatusBadgeState(hwnd, s, st);
    UpdateStatusCardText(hwnd, st);
    if (GetOwnerCheckboxValue(IDC_MUTE) != s.musicMuted) {
        SetOwnerCheckboxValue(IDC_MUTE, s.musicMuted);
        InvalidateRect(GetDlgItem(hwnd, IDC_MUTE), nullptr, TRUE);
    }
    if (GetOwnerCheckboxValue(IDC_MIC_INPUT_MUTE) != s.micInputMuted) {
        SetOwnerCheckboxValue(IDC_MIC_INPUT_MUTE, s.micInputMuted);
        InvalidateRect(GetDlgItem(hwnd, IDC_MIC_INPUT_MUTE), nullptr, TRUE);
    }
    std::string line1 = "State: " + SourceStateToString(st.state) + "  |  " + (st.message.empty() ? "idle" : st.message);
    if (!st.detail.empty()) {
        line1 += "  (" + st.detail + ")";
    }
    const float shownSendPeakDb = (t.musicSendPeakDbfs > -119.0f) ? t.musicSendPeakDbfs : t.musicPeakDbfs;
    char micBuf[32]{};
    sprintf_s(micBuf, "mic=%.1fdB", t.micRmsDbfs);
    char sendBuf[64]{};
    sprintf_s(sendBuf, "send=%.1fdB src=%.1fdB", shownSendPeakDb, t.musicPeakDbfs);
    const std::string resamplerText = (s.resamplerQuality < 0) ? "auto-cpu" : std::to_string(s.resamplerQuality);
    std::string line2 = "underrun=" + std::to_string(t.underruns) +
        "  overrun=" + std::to_string(t.overruns) +
        "  clip=" + std::to_string(t.clippedSamples) +
        "  resampler=" + resamplerText +
        "  " + micBuf +
        "  " + sendBuf +
        "  tag.micmix_active=" + std::to_string(t.musicActive ? 1 : 0);
    SetStatusText(hwnd, Utf8ToWide(line1 + "\r\n" + line2));
    UpdateMonitorButton(hwnd);
    RefreshMini(false);
}

void FlushDebouncedSettingsSave(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    KillTimer(hwnd, kTimerDebouncedSave);
    if (!g_saveDebouncePending.exchange(false, std::memory_order_acq_rel) || g_loadingUi) {
        return;
    }
    auto s = CollectSettings(hwnd);
    MicMixApp::Instance().ApplySettings(s, false, true);
}

void ApplyLiveSettings(HWND hwnd, bool restartSource, bool saveImmediately) {
    if (g_loadingUi) return;
    auto s = CollectSettings(hwnd);
    if (saveImmediately) {
        KillTimer(hwnd, kTimerDebouncedSave);
        g_saveDebouncePending.store(false, std::memory_order_release);
        MicMixApp::Instance().ApplySettings(s, restartSource, true);
    } else {
        MicMixApp::Instance().ApplySettings(s, restartSource, false);
        g_saveDebouncePending.store(true, std::memory_order_release);
        SetTimer(hwnd, kTimerDebouncedSave, kDebouncedSaveMs, nullptr);
    }
    UpdateStatus(hwnd);
}

void ApplyControlTheme(HWND hwnd) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(child, className, static_cast<int>(std::size(className)));
        if (_wcsicmp(className, WC_COMBOBOXW) == 0) {
            // "DarkMode_CFD" gives the combobox frame and drop arrow a dark rendering on
            // Windows 10 1809+; older systems silently fall back to the classic look.
            SetWindowTheme(child, g_theme.dark ? L"DarkMode_CFD" : L"Explorer", nullptr);
            COMBOBOXINFO cbi{};
            cbi.cbSize = sizeof(cbi);
            if (GetComboBoxInfo(child, &cbi) != FALSE && cbi.hwndList) {
                SetWindowTheme(cbi.hwndList, g_theme.dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            }
        }
        if (_wcsicmp(className, TRACKBAR_CLASSW) == 0) {
            // Keep custom slider visuals without native focus frame around the control.
            SetWindowTheme(child, L"", L"");
        }
        if (_wcsicmp(className, WC_BUTTONW) == 0) {
            // Every button is owner-drawn; strip visual styles so no themed hot/focus frame leaks through.
            SetWindowTheme(child, L"", L"");
        }
        return TRUE;
    }, 0);
}

void ApplyTheme(HWND hwnd) {
    SelectThemePalette(g_themeMode);
    if (!hwnd) {
        return;
    }
    ApplyWindowFrameTheme(hwnd);
    ApplyControlTheme(hwnd);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
    if (g_miniHwnd) {
        ApplyWindowFrameTheme(g_miniHwnd);
        ApplyControlTheme(g_miniHwnd);
        RedrawWindow(g_miniHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }
}

void SetThemeMode(HWND hwnd, UiThemeMode mode) {
    if (mode == g_themeMode) {
        return;
    }
    g_themeMode = mode;
    ApplyTheme(hwnd);
    ApplyLiveSettings(hwnd, false);
}

void ApplyFonts(HWND hwnd) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontBody), TRUE);
        return TRUE;
    }, 0);
    HFONT smallFont = g_fontSmall ? g_fontSmall : g_fontBody;
    SetControlFont(hwnd, IDC_VERSION, smallFont);
    SetControlFont(hwnd, IDC_REPO_LINK, smallFont);
    SetControlFont(hwnd, IDC_METER_TEXT, smallFont);
    SetControlFont(hwnd, IDC_MIC_METER_TEXT, smallFont);
    SetControlFont(hwnd, IDC_MUSIC_CLIP_EVENTS, g_fontClipInfo ? g_fontClipInfo : smallFont);
    SetControlFont(hwnd, IDC_MIC_CLIP_EVENTS, g_fontClipInfo ? g_fontClipInfo : smallFont);
    SetControlFont(hwnd, IDC_MUTE_HOTKEY_TEXT, smallFont);
    SetControlFont(hwnd, IDC_MIC_INPUT_HOTKEY_TEXT, smallFont);
}

void EndHotkeyCapture(HWND hwnd, bool refreshLabels) {
    g_hotkeyCaptureTarget = HotkeyCaptureTarget::None;
    MicMixApp::Instance().SetGlobalHotkeyCaptureBlocked(false);
    if (hwnd) {
        InvalidateRect(GetDlgItem(hwnd, IDC_MUTE_HOTKEY_SET), nullptr, FALSE);
        InvalidateRect(GetDlgItem(hwnd, IDC_MIC_INPUT_HOTKEY_SET), nullptr, FALSE);
    }
    if (refreshLabels) {
        UpdateHotkeyLabels(hwnd);
    }
}

// ---------------------------------------------------------------------------------------
// Main window (sidebar layout)
// ---------------------------------------------------------------------------------------

const wchar_t* PageTitle(int page) {
    switch (page) {
    case kPageSource: return Tr(Str::NavSource);
    case kPageMix: return Tr(Str::NavMix);
    case kPageHotkeys: return Tr(Str::NavHotkeys);
    case kPageAppearance: return Tr(Str::NavAppearance);
    default: return Tr(Str::NavDiagnostics);
    }
}

void ShowPage(HWND hwnd, int page, bool persist) {
    g_page = std::clamp(page, 0, kPageCount - 1);
    for (const auto& pc : g_pageControls) {
        HWND ctl = GetDlgItem(hwnd, pc.id);
        if (ctl) {
            ShowWindow(ctl, pc.page == g_page ? SW_SHOWNA : SW_HIDE);
        }
    }
    for (int i = 0; i < kPageCount; ++i) {
        InvalidateRect(GetDlgItem(hwnd, IDC_NAV_FIRST + i), nullptr, FALSE);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (persist) {
        ApplyLiveSettings(hwnd, false, false);
    }
}

// Paints the window scene. Only elements that touch `paintRc` are drawn: meter updates
// invalidate small rectangles many times per second, and the DC is clipped to them.
void PaintMainWindow(HDC dc, const RECT& clientRc, const RECT& paintRc) {
    const auto touches = [&paintRc](const RECT& r) {
        RECT overlap{};
        return IntersectRect(&overlap, &r, &paintRc) != FALSE;
    };
    FillRect(dc, &paintRc, g_brushBg);

    // Content panel with rounded top-left corner, like the Windows 11 settings app.
    RECT panel{ S(kSidebarWidthPx), S(10), clientRc.right + S(20), clientRc.bottom + S(20) };
    if (touches(panel)) {
        DrawRoundRect(dc, panel, S(8), FilledBordered(g_theme.content, g_theme.border));
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_theme.text);
    HGDIOBJ oldFont = SelectObject(dc, g_fontBrand ? g_fontBrand : g_fontBody);

    // Brand
    RECT logo{ S(20), S(22), S(48), S(50) };
    RECT brandRc{ S(58), S(22), S(kSidebarWidthPx - 10), S(50) };
    if (touches(logo) || touches(brandRc)) {
        DrawRoundRect(dc, logo, S(8), Filled(g_theme.accent));
        RECT logoIcon = logo;
        InflateRect(&logoIcon, -S(6), -S(6));
        DrawGlyph(dc, Glyph::Mic, logoIcon, g_theme.accentText, 2.0f);
        DrawTextW(dc, L"MicMix", -1, &brandRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    // Page title
    const int cx0 = S(kContentLeftPx);
    const int cx1 = S(kContentRightPx);
    RECT titleRc{ cx0, S(24), cx1, S(66) };
    if (touches(titleRc)) {
        SelectObject(dc, g_fontPageTitle ? g_fontPageTitle : g_fontBody);
        DrawTextW(dc, PageTitle(g_page), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    if (touches(g_rcHeaderBadge)) {
        DrawStatusCard(dc);
    }

    for (const auto& card : g_cards) {
        if (card.page == g_page && touches(card.rc)) {
            DrawCard(dc, card.rc);
        }
    }
    const int pad = S(kRowPadPx);
    for (const auto& row : g_rows) {
        if (row.page != g_page || !touches(row.rc)) {
            continue;
        }
        const bool hasSubtitle = row.subtitle != kNoText;
        RECT tr{ row.rc.left + pad, row.rc.top + S(13), row.textRight, row.rc.top + S(33) };
        if (!hasSubtitle) {
            tr = { row.rc.left + pad, row.rc.top, row.textRight, row.rc.bottom };
        }
        SetTextColor(dc, g_theme.text);
        SelectObject(dc, g_fontRowTitle ? g_fontRowTitle : g_fontBody);
        DrawTextW(dc, Tr(static_cast<Str>(row.title)), -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (row.subtitle >= 0) {
            RECT sr{ row.rc.left + pad, row.rc.top + S(33), row.textRight, row.rc.top + S(51) };
            SetTextColor(dc, g_theme.muted);
            SelectObject(dc, g_fontSmall ? g_fontSmall : g_fontBody);
            DrawTextW(dc, Tr(static_cast<Str>(row.subtitle)), -1, &sr, DT_SINGLELINE | DT_TOP | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (row.divider) {
            RECT line{ row.rc.left + 1, row.rc.bottom - 1, row.rc.right - 1, row.rc.bottom };
            FillSolidRect(dc, line, g_theme.divider);
        }
    }

    if (g_page == kPageMix) {
        if (touches(g_rcMeter)) {
            DrawLevelMeter(dc, g_rcMeter, IsMusicMeterActive(g_lastTelemetry), g_meterVisualDb, g_meterHoldDb);
        }
        if (touches(g_rcMicMeter)) {
            DrawLevelMeter(dc, g_rcMicMeter, g_lastTelemetry.micRmsDbfs > -119.0f, g_micMeterVisualDb, g_micMeterHoldDb);
        }
        const float shownMusicPeak = (g_lastTelemetry.musicSendPeakDbfs > -119.0f) ? g_lastTelemetry.musicSendPeakDbfs : g_lastTelemetry.musicPeakDbfs;
        if (touches(g_rcMusicClip)) {
            DrawClipStrip(dc, g_rcMusicClip, ClipDangerUnitFromDb(shownMusicPeak), g_lastTelemetry.sourceClipRecent);
        }
        if (touches(g_rcMicClip)) {
            DrawClipStrip(dc, g_rcMicClip, ClipDangerUnitFromDb(g_lastTelemetry.micPeakDbfs), g_lastTelemetry.micClipRecent);
        }
    } else if (g_page == kPageHotkeys) {
        for (const auto& card : g_cards) {
            if (card.page == kPageHotkeys) {
                RECT hint{ card.rc.left + S(2), card.rc.bottom + S(10), card.rc.right, card.rc.bottom + S(30) };
                SetTextColor(dc, g_theme.muted);
                SelectObject(dc, g_fontSmall ? g_fontSmall : g_fontBody);
                DrawTextW(dc, Tr(Str::HotkeyHint), -1, &hint, DT_SINGLELINE | DT_TOP | DT_NOPREFIX);
            }
        }
    } else if (g_page == kPageDiagnostics) {
        RECT statusBox = g_rcStatus;
        InflateRect(&statusBox, S(8), S(6));
        DrawRoundRect(dc, statusBox, S(6), FilledBordered(g_theme.statusBg, g_theme.statusBorder));
        SetTextColor(dc, g_theme.statusText);
        SelectObject(dc, g_fontMono ? g_fontMono : g_fontBody);
        RECT statusRc = g_rcStatus;
        InflateRect(&statusRc, -S(4), -S(2));
        DrawTextW(dc, g_lastStatusText.empty() ? L"State: Stopped" : g_lastStatusText.c_str(), -1, &statusRc,
                  DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
    }
    SelectObject(dc, oldFont);
}

void TogglePower(HWND hwnd) {
    auto& app = MicMixApp::Instance();
    if (IsSourceStateActive(app.GetSourceStatus().state)) {
        app.StopSource();
    } else {
        app.StartSource();
    }
    UpdateStatus(hwnd);
    InvalidateRect(GetDlgItem(hwnd, IDC_POWER), nullptr, FALSE);
    RefreshMini(true);
}

std::wstring GetComboSelectedText(HWND combo) {
    if (!combo) {
        return {};
    }
    const int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (sel < 0) {
        return {};
    }
    const LRESULT len = SendMessageW(combo, CB_GETLBTEXTLEN, static_cast<WPARAM>(sel), 0);
    if (len <= 0 || len == CB_ERR) {
        return {};
    }
    std::wstring text(static_cast<size_t>(len) + 1U, L'\0');
    const LRESULT copied = SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(text.data()));
    text.resize(copied > 0 ? static_cast<size_t>(copied) : 0U);
    return text;
}

// Status card headline and route line; repaints only when the text changes.
void UpdateStatusCardText(HWND hwnd, const SourceStatus& st) {
    std::wstring title;
    std::wstring subtitle;
    const std::wstring src = GetComboSelectedText(GetDlgItem(hwnd, IDC_SOURCE));
    std::wstring mic = GetComboSelectedText(GetDlgItem(hwnd, IDC_MIC_DEVICE));
    if (mic.empty()) {
        mic = Tr(Str::DefaultMicrophone);
    }
    const std::wstring route = (src.empty() ? std::wstring(Tr(Str::NoSourceSelected)) : src) + L"  \x2192  " + mic;
    switch (g_headerStatusBadgeState) {
    case HeaderStatusBadgeState::Active:
        title = Tr(Str::StatusSending);
        subtitle = route;
        break;
    case HeaderStatusBadgeState::Muted:
        title = Tr(Str::StatusMuted);
        subtitle = route;
        break;
    case HeaderStatusBadgeState::Error:
        title = Tr(Str::StatusError);
        subtitle = st.message.empty() ? std::wstring(Tr(Str::SourceErrorFallback)) : Utf8ToWide(st.message);
        break;
    default:
        title = Tr(Str::StatusOff);
        subtitle = src.empty() ? std::wstring(Tr(Str::StatusOffHint)) : Tr(Str::StatusReadyPrefix) + route;
        break;
    }
    if (title != g_statusTitle || subtitle != g_statusSubtitle) {
        g_statusTitle = title;
        g_statusSubtitle = subtitle;
        InvalidateRect(hwnd, &g_rcHeaderBadge, FALSE);
    }
}

// ---------------------------------------------------------------------------------------
// Update check
// ---------------------------------------------------------------------------------------

std::mutex g_updateMutex;
std::thread g_updateThread;
UpdateInfo g_updateInfo;
bool g_updateStarted = false;

void PublishUpdateInfo(const UpdateInfo& info) {
    {
        std::lock_guard<std::mutex> lock(g_updateMutex);
        g_updateInfo = info;
    }
    HWND hwnd = g_hwnd.load(std::memory_order_acquire);
    if (hwnd && info.available) {
        PostMessageW(hwnd, kMsgUpdateFound, 0, 0);
    }
}

UpdateInfo GetUpdateInfo() {
    std::lock_guard<std::mutex> lock(g_updateMutex);
    return g_updateInfo;
}

// Runs at most once per TeamSpeak session, in the background.
void StartUpdateCheck() {
    std::lock_guard<std::mutex> lock(g_updateMutex);
    if (g_updateStarted) {
        return;
    }
    g_updateStarted = true;
    try {
        g_updateThread = std::thread([] {
            PublishUpdateInfo(CheckForUpdate(MICMIX_VERSION, MICMIX_UPDATE_REPO));
        });
    } catch (...) {
        LogError("update check thread start failed");
    }
}

void StopUpdateCheck() {
    CancelUpdateCheck();
    std::thread worker;
    {
        std::lock_guard<std::mutex> lock(g_updateMutex);
        worker = std::move(g_updateThread);
    }
    if (worker.joinable()) {
        worker.join();
    }
}

void ShowUpdateHint(HWND hwnd) {
    HWND button = GetDlgItem(hwnd, IDC_UPDATE);
    if (!button) {
        return;
    }
    const UpdateInfo info = GetUpdateInfo();
    const bool show = info.available && GetOwnerCheckboxValue(IDC_UPDATE_CHECK);
    ShowWindow(button, show ? SW_SHOWNA : SW_HIDE);
    InvalidateRect(button, nullptr, FALSE);
}

void DrawUpdateButton(const DRAWITEMSTRUCT* dis) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.bg);
    const bool hover = dis->hwndItem == g_hoverControl;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    COLORREF fill = g_theme.accentSoft;
    if (pressed) {
        fill = MixColor(g_theme.accentSoft, g_theme.accent, 0.25f);
    } else if (hover) {
        fill = MixColor(g_theme.accentSoft, g_theme.accent, 0.12f);
    }
    DrawRoundRect(dis->hDC, rc, S(6), FilledBordered(fill, g_theme.accent));
    const COLORREF fg = g_theme.dark ? g_theme.accentHover : g_theme.accent;
    RECT icon{ rc.left + S(10), rc.top + ((rc.bottom - rc.top - S(16)) / 2), rc.left + S(26), rc.top + ((rc.bottom - rc.top - S(16)) / 2) + S(16) };
    DrawGlyph(dis->hDC, Glyph::Download, icon, fg, 2.0f);

    const UpdateInfo info = GetUpdateInfo();
    wchar_t text[96]{};
    swprintf_s(text, Tr(Str::UpdateAvailable), Utf8ToWide(info.version).c_str());
    RECT textRc{ icon.right + S(8), rc.top, rc.right - S(6), rc.bottom };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontButton ? g_fontButton : g_fontBody);
    DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

void OpenUpdatePage(HWND hwnd) {
    const UpdateInfo info = GetUpdateInfo();
    // The URL is built from the configured repository and a validated tag.
    if (info.available && info.url.rfind("https://github.com/", 0) == 0) {
        ShellExecuteW(hwnd, L"open", Utf8ToWide(info.url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

// ---------------------------------------------------------------------------------------
// Language
// ---------------------------------------------------------------------------------------

void PopulateLanguageCombo(HWND hwnd) {
    HWND combo = GetDlgItem(hwnd, IDC_LANGUAGE);
    if (!combo) {
        return;
    }
    const auto& langs = I18nLanguages();
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    // First entry: "Automatic (<language TeamSpeak/Windows resolves to>)".
    const std::string autoCode = I18nAutoCode();
    std::wstring autoName;
    for (const auto& l : langs) {
        if (autoCode == l.code) {
            autoName = l.nativeName;
        }
    }
    const std::wstring autoLabel = std::wstring(Tr(Str::LanguageAuto)) + (autoName.empty() ? L"" : L" (" + autoName + L")");
    LRESULT idx = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(autoLabel.c_str()));
    SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(idx), static_cast<LPARAM>(-1));
    int selected = 0;
    for (size_t i = 0; i < langs.size(); ++i) {
        idx = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(langs[i].nativeName));
        SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(idx), static_cast<LPARAM>(i));
        if (g_uiLanguage == langs[i].code) {
            selected = static_cast<int>(idx);
        }
    }
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

// Re-labels every control and repaints both windows in the active language.
void ApplyLanguageToUi(HWND hwnd) {
    const Str nav[kPageCount] = { Str::NavSource, Str::NavMix, Str::NavHotkeys, Str::NavAppearance, Str::NavDiagnostics };
    for (int i = 0; i < kPageCount; ++i) {
        SetWindowTextW(GetDlgItem(hwnd, IDC_NAV_FIRST + i), Tr(nav[i]));
    }
    const std::pair<int, Str> labels[] = {
        { IDC_OPEN_MINI, Str::MiniModeButton }, { IDC_RESCAN, Str::Refresh }, { IDC_SAVE, Str::Restart },
        { IDC_MUTE, Str::MuteMusic }, { IDC_MIC_INPUT_MUTE, Str::MuteMic },
        { IDC_MUTE_HOTKEY_SET, Str::HotkeySet }, { IDC_MIC_INPUT_HOTKEY_SET, Str::HotkeySet },
        { IDC_THEME_SYSTEM, Str::ThemeSystem }, { IDC_THEME_LIGHT, Str::ThemeLight }, { IDC_THEME_DARK, Str::ThemeDark },
    };
    for (const auto& [id, str] : labels) {
        SetWindowTextW(GetDlgItem(hwnd, id), Tr(str));
    }
    UpdateMonitorButton(hwnd);
    if (!IsCapturingHotkey()) {
        UpdateHotkeyLabels(hwnd);
    }
    // Force the periodic texts to be rewritten on the next status tick.
    g_lastMusicMeterText.clear();
    g_lastMicMeterText.clear();
    g_lastMusicClipEvents = std::numeric_limits<uint64_t>::max();
    g_lastMicClipEvents = std::numeric_limits<uint64_t>::max();
    g_windowTitleStateValid = false;
    g_statusTitle.clear();

    PopulateCombos(hwnd);
    LoadSettings(hwnd);
    PopulateLanguageCombo(hwnd);
    UpdateStatus(hwnd);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (g_miniHwnd) {
        SyncMiniFromMain();
        RedrawWindow(g_miniHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

void SetUiLanguage(HWND hwnd, const std::string& code) {
    if (code == g_uiLanguage) {
        return;
    }
    g_uiLanguage = code;
    I18nApply(g_uiLanguage);
    ApplyLiveSettings(hwnd, false);
    ApplyLanguageToUi(hwnd);
}

// ---------------------------------------------------------------------------------------
// DPI handling (per-monitor v2): rescale fonts, layout and window sizes on DPI changes.
// ---------------------------------------------------------------------------------------

// Windows 10 1607+ APIs, resolved at runtime so older systems keep working (unscaled).
void EnablePerMonitorDpiForThread() {
    using SetCtxFn = HANDLE(WINAPI*)(HANDLE);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    auto setCtx = user32 ? reinterpret_cast<SetCtxFn>(reinterpret_cast<void*>(GetProcAddress(user32, "SetThreadDpiAwarenessContext"))) : nullptr;
    if (!setCtx) {
        return;
    }
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (-4), falling back to PER_MONITOR_AWARE (-3).
    if (!setCtx(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)))) {
        setCtx(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-3)));
    }
}

SIZE WindowSizeForClient(int clientW, int clientH, DWORD style, int dpi) {
    RECT rc{ 0, 0, clientW, clientH };
    using AdjustForDpiFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
    static AdjustForDpiFn adjust = []() -> AdjustForDpiFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<AdjustForDpiFn>(reinterpret_cast<void*>(GetProcAddress(user32, "AdjustWindowRectExForDpi"))) : nullptr;
    }();
    if (adjust) {
        adjust(&rc, style, FALSE, 0, static_cast<UINT>(dpi));
    } else {
        AdjustWindowRectEx(&rc, style, FALSE, 0);
    }
    return { rc.right - rc.left, rc.bottom - rc.top };
}

void ApplyMiniFonts(HWND mini) {
    EnumChildWindows(mini, [](HWND child, LPARAM) -> BOOL {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g_fontBody), TRUE);
        return TRUE;
    }, 0);
}

struct DpiScale {
    int from = 96;
    int to = 96;
    int Px(int v) const { return MulDiv(v, to, from); }
    void Rect(RECT& r) const {
        r = { Px(r.left), Px(r.top), Px(r.right), Px(r.bottom) };
    }
};

void ScaleChildWindows(HWND parent, const DpiScale& scale) {
    struct Ctx {
        HWND parent;
        const DpiScale* scale;
    } ctx{ parent, &scale };
    EnumChildWindows(parent, [](HWND child, LPARAM lp) -> BOOL {
        const auto* c = reinterpret_cast<const Ctx*>(lp);
        if (GetParent(child) != c->parent) {
            return TRUE;
        }
        RECT rc{};
        GetWindowRect(child, &rc);
        MapWindowPoints(nullptr, c->parent, reinterpret_cast<LPPOINT>(&rc), 2);
        c->scale->Rect(rc);
        wchar_t cls[32]{};
        GetClassNameW(child, cls, static_cast<int>(std::size(cls)));
        if (_wcsicmp(cls, WC_COMBOBOXW) == 0) {
            // For a drop-down list the height passed to MoveWindow includes the open list.
            MoveWindow(child, rc.left, rc.top, rc.right - rc.left, S(300), FALSE);
            SendMessageW(child, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
            SendMessageW(child, CB_SETITEMHEIGHT, 0, S(24));
        } else {
            MoveWindow(child, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
}

// Moves both windows to a new DPI. `source` received WM_DPICHANGED and gets the
// suggested rectangle; the other window keeps its position and is resized.
void ApplyDpi(HWND source, int newDpi, const RECT* suggested) {
    HWND main = g_hwnd.load(std::memory_order_acquire);
    HWND mini = g_miniHwnd;
    if (newDpi <= 0 || newDpi == g_dpi) {
        if (source && suggested) {
            SetWindowPos(source, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return;
    }
    const DpiScale scale{ g_dpi, newDpi };
    g_dpi = newDpi;
    ReleaseFonts();
    EnsureUiResources();

    for (auto& row : g_rows) {
        scale.Rect(row.rc);
        row.textRight = scale.Px(row.textRight);
    }
    for (auto& card : g_cards) {
        scale.Rect(card.rc);
    }
    for (RECT* r : { &g_rcHeaderBadge, &g_rcMeter, &g_rcMicMeter, &g_rcMusicClip, &g_rcMicClip, &g_rcStatus,
                     &g_rcThemeSegment, &g_rcMiniPanel, &g_rcMiniGainText, &g_rcMiniMusicMeter, &g_rcMiniMicMeter,
                     &g_rcMiniHint }) {
        scale.Rect(*r);
    }

    const auto resize = [&](HWND w, int clientW, int clientH, DWORD style) {
        if (!w) {
            return;
        }
        if (w == source && suggested) {
            SetWindowPos(w, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return;
        }
        const SIZE sz = WindowSizeForClient(S(clientW), S(clientH), style, g_dpi);
        SetWindowPos(w, nullptr, 0, 0, sz.cx, sz.cy, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    };
    if (main) {
        ScaleChildWindows(main, scale);
        ApplyFonts(main);
        resize(main, kClientWidthPx, kClientHeightPx, kMainWindowStyle);
        RedrawWindow(main, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }
    if (mini) {
        ScaleChildWindows(mini, scale);
        ApplyMiniFonts(mini);
        resize(mini, kMiniClientWidthPx, kMiniClientHeightPx, kMiniWindowStyle);
        RedrawWindow(mini, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
    }
}

// After a window is shown on another monitor, bring the shared layout to that monitor's DPI.
void SyncDpiToWindow(HWND w) {
    if (w) {
        ApplyDpi(nullptr, static_cast<int>(GetDpiForWindow(w)), nullptr);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_dpi = GetDpiForWindow(hwnd);
        {
            const MicMixSettings initial = MicMixApp::Instance().GetSettings();
            g_themeMode = initial.uiThemeMode;
            g_page = std::clamp(initial.uiLastOpenTab, 0, kPageCount - 1);
            // Resolve "auto" again on every open so a changed TeamSpeak language is picked up.
            g_uiLanguage = initial.uiLanguage;
            I18nApply(g_uiLanguage);
        }
        g_hoverControl = nullptr;
        SelectThemePalette(g_themeMode);
        EnsureUiResources();
        g_hotkeyRefreshTick = 0;
        g_hotkeyCaptureTarget = HotkeyCaptureTarget::None;
        g_lastMusicClipEvents = std::numeric_limits<uint64_t>::max();
        g_lastMicClipEvents = std::numeric_limits<uint64_t>::max();
        g_lastMusicMeterLevelPx = -1;
        g_lastMusicMeterHoldPx = -1;
        g_lastMusicMeterActive = false;
        g_lastMicMeterLevelPx = -1;
        g_lastMicMeterHoldPx = -1;
        g_lastMicMeterActive = false;
        g_lastMusicClipLitSegments = -1;
        g_lastMusicClipRecent = false;
        g_lastMicClipLitSegments = -1;
        g_lastMicClipRecent = false;
        g_lastMusicMeterText.clear();
        g_lastMicMeterText.clear();
        g_headerStatusBadgeState = HeaderStatusBadgeState::Off;
        g_windowTitleActive = false;
        g_windowTitleStateValid = false;
        g_statusTitle = Tr(Str::StatusOff);
        g_statusSubtitle = Tr(Str::StatusOffHint);
        g_rows.clear();
        g_cards.clear();
        g_pageControls.clear();

        const int cx0 = S(kContentLeftPx);
        const int cx1 = S(kContentRightPx);
        const int pad = S(kRowPadPx);
        const int clientH = S(kClientHeightPx);

        const auto make = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, int page) -> HWND {
            HWND ctl = CreateWindowW(cls, text, WS_CHILD | style, x, y, w, h, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            if (page >= 0) {
                g_pageControls.push_back({ page, id });
            } else if (ctl) {
                ShowWindow(ctl, SW_SHOW);
            }
            return ctl;
        };
        int y = 0;
        int cardTop = 0;
        int lastRow = -1;
        const auto beginCard = [&](int top) {
            y = top;
            cardTop = top;
            lastRow = -1;
        };
        const auto addRow = [&](int page, int h, Str title, int subtitle, int textRight = 0) -> RECT {
            if (lastRow >= 0) {
                g_rows[static_cast<size_t>(lastRow)].divider = true;
            }
            RowDef row;
            row.page = page;
            row.rc = { cx0, y, cx1, y + S(h) };
            row.title = static_cast<int>(title);
            row.subtitle = subtitle;
            row.textRight = textRight > 0 ? textRight : cx1 - S(150);
            g_rows.push_back(row);
            lastRow = static_cast<int>(g_rows.size()) - 1;
            y += S(h);
            return row.rc;
        };
        const auto endCard = [&](int page) {
            g_cards.push_back({ page, { cx0, cardTop, cx1, y } });
            y += S(16);
        };
        const auto centerY = [](const RECT& r, int h) { return r.top + ((r.bottom - r.top - h) / 2); };

        // Sidebar ------------------------------------------------------------------
        const Str navNames[kPageCount] = { Str::NavSource, Str::NavMix, Str::NavHotkeys, Str::NavAppearance, Str::NavDiagnostics };
        for (int i = 0; i < kPageCount; ++i) {
            make(L"BUTTON", Tr(navNames[i]), WS_TABSTOP | BS_OWNERDRAW, S(10), S(72) + (i * S(42)), S(kSidebarWidthPx - 20), S(38), IDC_NAV_FIRST + i, -1);
        }
        make(L"BUTTON", Tr(Str::MiniModeButton), WS_TABSTOP | BS_OWNERDRAW, S(14), clientH - S(112), S(kSidebarWidthPx - 28), S(32), IDC_OPEN_MINI, -1);
        // Shown by ShowUpdateHint() once a newer release is known.
        HWND update = make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, S(14), clientH - S(158), S(kSidebarWidthPx - 28), S(36), IDC_UPDATE, -1);
        ShowWindow(update, SW_HIDE);
        const std::wstring versionText = Utf8ToWide(std::string("v") + MICMIX_VERSION + "  \xC2\xB7  by dabinuss");
        make(L"STATIC", versionText.c_str(), 0, S(20), clientH - S(64), S(kSidebarWidthPx - 30), S(18), IDC_VERSION, -1);
        make(L"STATIC", L"github.com/dabinuss/MicMix", SS_NOTIFY, S(20), clientH - S(44), S(kSidebarWidthPx - 30), S(18), IDC_REPO_LINK, -1);

        // Status card (all pages) ---------------------------------------------------
        g_rcHeaderBadge = { cx0, S(78), cx1, S(150) };
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(140), centerY(g_rcHeaderBadge, S(30)), S(140), S(30), IDC_POWER, -1);

        // Source page -----------------------------------------------------------------
        const int fieldX = cx0 + S(250);
        beginCard(S(166));
        RECT r = addRow(kPageSource, 64, Str::RowAudioSource, static_cast<int>(Str::RowAudioSourceSub), fieldX - S(12));
        const int refreshW = S(110);
        HWND source = make(L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, fieldX, centerY(r, S(30)), cx1 - pad - refreshW - S(8) - fieldX, S(360), IDC_SOURCE, kPageSource);
        SendMessageW(source, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
        SendMessageW(source, CB_SETITEMHEIGHT, 0, S(24));
        SendMessageW(source, CB_SETMINVISIBLE, 14, 0);
        make(L"BUTTON", Tr(Str::Refresh), WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - refreshW, centerY(r, S(32)), refreshW, S(32), IDC_RESCAN, kPageSource);
        r = addRow(kPageSource, 64, Str::RowMicrophone, static_cast<int>(Str::RowMicrophoneSub), fieldX - S(12));
        HWND mic = make(L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, fieldX, centerY(r, S(30)), cx1 - pad - fieldX, S(260), IDC_MIC_DEVICE, kPageSource);
        SendMessageW(mic, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
        SendMessageW(mic, CB_SETITEMHEIGHT, 0, S(24));
        SendMessageW(mic, CB_SETMINVISIBLE, 12, 0);
        r = addRow(kPageSource, 64, Str::RowAutostart, static_cast<int>(Str::RowAutostartSub));
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(48), centerY(r, S(30)), S(48), S(30), IDC_AUTOSTART, kPageSource);
        r = addRow(kPageSource, 64, Str::RowRestart, static_cast<int>(Str::RowRestartSub), cx1 - pad - S(130) - S(12));
        make(L"BUTTON", Tr(Str::Restart), WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(130), centerY(r, S(32)), S(130), S(32), IDC_SAVE, kPageSource);
        endCard(kPageSource);

        // Mix page --------------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageMix, 72, Str::RowVolume, static_cast<int>(Str::RowVolumeSub), fieldX - S(12));
        HWND gain = make(TRACKBAR_CLASSW, L"", WS_CLIPSIBLINGS | WS_TABSTOP, fieldX, centerY(r, S(30)), cx1 - pad - S(90) - fieldX, S(30), IDC_GAIN, kPageMix);
        SendMessageW(gain, TBM_SETRANGE, TRUE, MAKELONG(0, kMusicGainSliderMax));
        SendMessageW(gain, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(gain, TBM_SETLINESIZE, 0, 1);
        SendMessageW(gain, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
        make(L"STATIC", L"-15.0 dB", SS_RIGHT, cx1 - pad - S(80), centerY(r, S(20)), S(80), S(20), IDC_GAIN_VALUE, kPageMix);

        const int meterX = cx0 + S(200);
        const int meterTextW = S(128);
        const int meterRight = cx1 - pad - meterTextW - S(10);
        r = addRow(kPageMix, 76, Str::RowMusicLevel, static_cast<int>(Str::RowMusicLevelSub), meterX - S(12));
        g_rcMeter = { meterX, r.top + S(14), meterRight, r.top + S(40) };
        g_rcMusicClip = { meterX, r.top + S(46), meterRight, r.top + S(58) };
        make(L"STATIC", Tr(Str::NoSignal), SS_ENDELLIPSIS, cx1 - pad - meterTextW, r.top + S(17), meterTextW, S(18), IDC_METER_TEXT, kPageMix);
        make(L"STATIC", L"", 0, cx1 - pad - meterTextW, r.top + S(44), meterTextW, S(16), IDC_MUSIC_CLIP_EVENTS, kPageMix);
        r = addRow(kPageMix, 76, Str::RowMicLevel, static_cast<int>(Str::RowMicLevelSub), meterX - S(12));
        g_rcMicMeter = { meterX, r.top + S(14), meterRight, r.top + S(40) };
        g_rcMicClip = { meterX, r.top + S(46), meterRight, r.top + S(58) };
        make(L"STATIC", Tr(Str::NoSignal), SS_ENDELLIPSIS, cx1 - pad - meterTextW, r.top + S(17), meterTextW, S(18), IDC_MIC_METER_TEXT, kPageMix);
        make(L"STATIC", L"", 0, cx1 - pad - meterTextW, r.top + S(44), meterTextW, S(16), IDC_MIC_CLIP_EVENTS, kPageMix);
        r = addRow(kPageMix, 64, Str::RowForceTx, static_cast<int>(Str::RowForceTxSub));
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(48), centerY(r, S(30)), S(48), S(30), IDC_FORCE_TX, kPageMix);
        endCard(kPageMix);
        make(L"BUTTON", Tr(Str::MonitorOff), WS_TABSTOP | BS_OWNERDRAW, cx0, y, S(170), S(32), IDC_MONITOR, kPageMix);
        make(L"BUTTON", Tr(Str::MuteMic), WS_TABSTOP | BS_OWNERDRAW, cx1 - S(160), y, S(160), S(32), IDC_MIC_INPUT_MUTE, kPageMix);
        make(L"BUTTON", Tr(Str::MuteMusic), WS_TABSTOP | BS_OWNERDRAW, cx1 - S(160) - S(16) - S(170), y, S(170), S(32), IDC_MUTE, kPageMix);

        // Hotkeys page ----------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageHotkeys, 64, Str::MuteMusic, kTextFromControl);
        make(L"STATIC", L"", SS_ENDELLIPSIS, r.left + pad, r.top + S(33), S(380), S(18), IDC_MUTE_HOTKEY_TEXT, kPageHotkeys);
        make(L"BUTTON", Tr(Str::HotkeySet), WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(r, S(32)), S(110), S(32), IDC_MUTE_HOTKEY_SET, kPageHotkeys);
        r = addRow(kPageHotkeys, 64, Str::MuteMic, kTextFromControl);
        make(L"STATIC", L"", SS_ENDELLIPSIS, r.left + pad, r.top + S(33), S(380), S(18), IDC_MIC_INPUT_HOTKEY_TEXT, kPageHotkeys);
        make(L"BUTTON", Tr(Str::HotkeySet), WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(r, S(32)), S(110), S(32), IDC_MIC_INPUT_HOTKEY_SET, kPageHotkeys);
        endCard(kPageHotkeys);

        // Appearance page -------------------------------------------------------------
        beginCard(S(166));
        const int themeSegW = S(70);
        r = addRow(kPageAppearance, 72, Str::RowTheme, static_cast<int>(Str::RowThemeSub), cx1 - pad - (themeSegW * 3) - S(12));
        const int themeSegH = S(32);
        const int themeSegX = cx1 - pad - (themeSegW * 3);
        const int themeSegY = centerY(r, themeSegH);
        g_rcThemeSegment = { themeSegX, themeSegY, themeSegX + (themeSegW * 3), themeSegY + themeSegH };
        make(L"BUTTON", Tr(Str::ThemeSystem), WS_TABSTOP | BS_OWNERDRAW, themeSegX, themeSegY, themeSegW, themeSegH, IDC_THEME_SYSTEM, kPageAppearance);
        make(L"BUTTON", Tr(Str::ThemeLight), WS_TABSTOP | BS_OWNERDRAW, themeSegX + themeSegW, themeSegY, themeSegW, themeSegH, IDC_THEME_LIGHT, kPageAppearance);
        make(L"BUTTON", Tr(Str::ThemeDark), WS_TABSTOP | BS_OWNERDRAW, themeSegX + (themeSegW * 2), themeSegY, themeSegW, themeSegH, IDC_THEME_DARK, kPageAppearance);
        const int langW = S(240);
        r = addRow(kPageAppearance, 72, Str::RowLanguage, static_cast<int>(Str::RowLanguageSub), cx1 - pad - langW - S(12));
        HWND lang = make(L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, cx1 - pad - langW, centerY(r, S(30)), langW, S(360), IDC_LANGUAGE, kPageAppearance);
        SendMessageW(lang, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
        SendMessageW(lang, CB_SETITEMHEIGHT, 0, S(24));
        SendMessageW(lang, CB_SETMINVISIBLE, 14, 0);
        PopulateLanguageCombo(hwnd);
        r = addRow(kPageAppearance, 64, Str::RowUpdateCheck, static_cast<int>(Str::RowUpdateCheckSub));
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(48), centerY(r, S(30)), S(48), S(30), IDC_UPDATE_CHECK, kPageAppearance);
        endCard(kPageAppearance);

        // Diagnostics page ------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageDiagnostics, 132, Str::RowLiveStatus, kTextFromControl);
        g_rcStatus = { r.left + pad, r.top + S(42), r.right - pad, r.bottom - S(14) };
        endCard(kPageDiagnostics);

        ApplyWindowFrameTheme(hwnd);
        ApplyControlTheme(hwnd);
        InstallButtonHoverTracking(hwnd);
        ApplyFonts(hwnd);
        ShowPage(hwnd, g_page, false);
        PopulateCombos(hwnd);
        LoadSettings(hwnd);
        ShowUpdateHint(hwnd);
        RequestSourceRefresh(hwnd, true);
        UpdateMonitorButton(hwnd);
        SendMessageW(hwnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
        SetTimer(hwnd, kTimerStatusUpdate, kStatusTimerIntervalMs, nullptr);
        UpdateStatus(hwnd);
        return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, IDC_GAIN)) {
            SendMessageW(GetDlgItem(hwnd, IDC_GAIN), WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
            UpdateGainLabel(hwnd);
            // Repaint the whole slider so the accent fill follows the thumb on large jumps.
            InvalidateRect(GetDlgItem(hwnd, IDC_GAIN), nullptr, FALSE);
            SyncMiniFromMain();
            ApplyLiveSettings(hwnd, false, false);
        }
        return 0;
    case WM_MEASUREITEM: {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (!mis) {
            return FALSE;
        }
        if (mis->CtlID == IDC_SOURCE || mis->CtlID == IDC_MIC_DEVICE || mis->CtlID == IDC_LANGUAGE) {
            mis->itemHeight = static_cast<UINT>(S(24));
            return TRUE;
        }
        return FALSE;
    }
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) {
            return FALSE;
        }
        if (dis->CtlID == IDC_SOURCE) {
            DrawSourceComboItem(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_MIC_DEVICE || dis->CtlID == IDC_LANGUAGE) {
            DrawPlainComboItem(dis);
            return TRUE;
        }
        if (IsOwnerPushButtonControlId(static_cast<int>(dis->CtlID))) {
            DrawPushButton(dis);
            return TRUE;
        }
        if (IsThemeSegmentControlId(static_cast<int>(dis->CtlID))) {
            DrawThemeSegment(hwnd, dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_UPDATE) {
            DrawUpdateButton(dis);
            return TRUE;
        }
        if (IsOwnerCheckboxControlId(static_cast<int>(dis->CtlID))) {
            DrawOwnerCheckbox(dis);
            return TRUE;
        }
        if (IsNavControlId(static_cast<int>(dis->CtlID))) {
            DrawNavItem(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_POWER) {
            DrawPowerToggle(dis, IsSourceStateActive(MicMixApp::Instance().GetSourceStatus().state));
            return TRUE;
        }
        return FALSE;
    }
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (!hdr) {
            return 0;
        }
        if (hdr->idFrom == IDC_GAIN && hdr->code == NM_CUSTOMDRAW) {
            return DrawGainSliderCustom(reinterpret_cast<NMCUSTOMDRAW*>(lParam));
        }
        return 0;
    }
    case kMsgSourceRefreshDone: {
        const uint64_t completedSeq = static_cast<uint64_t>(wParam);
        const uint64_t activeSeq = g_sourceRefreshActiveSeq.load(std::memory_order_acquire);
        if (completedSeq != 0 && activeSeq != 0 && completedSeq != activeSeq) {
            return 0;
        }
        JoinSourceRefreshThread();
        {
            std::lock_guard<std::mutex> lock(g_enumMutex);
            g_loopbacks = g_pendingLoopbacks;
            g_captureDevices = g_pendingCaptureDevices;
            g_apps = g_pendingApps;
        }
        PopulateCombos(hwnd);
        if (g_sourceRefreshPendingReload.exchange(false, std::memory_order_acq_rel)) {
            LoadSettings(hwnd);
        }
        g_sourceRefreshInFlight.store(false, std::memory_order_release);
        g_sourceRefreshActiveSeq.store(0, std::memory_order_release);

        if (g_sourceRefreshPending.exchange(false, std::memory_order_acq_rel)) {
            const uint64_t nextSeq = g_sourceRefreshSeq.fetch_add(1, std::memory_order_acq_rel) + 1ULL;
            g_sourceRefreshInFlight.store(true, std::memory_order_release);
            g_sourceRefreshActiveSeq.store(nextSeq, std::memory_order_release);
            SetStatusText(hwnd, Tr(Str::RefreshingSources));
            if (!StartSourceRefreshWorker(hwnd, nextSeq)) {
                g_sourceRefreshInFlight.store(false, std::memory_order_release);
                g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
                HWND refreshBtn = GetDlgItem(hwnd, IDC_RESCAN);
                if (refreshBtn) {
                    EnableWindow(refreshBtn, TRUE);
                }
                SetStatusText(hwnd, Tr(Str::RefreshFailed));
            }
            return 0;
        }

        HWND refreshBtn = GetDlgItem(hwnd, IDC_RESCAN);
        if (refreshBtn) {
            EnableWindow(refreshBtn, TRUE);
        }
        UpdateStatus(hwnd);
        return 0;
    }
    case WM_DPICHANGED:
        ApplyDpi(hwnd, HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
        return 0;
    case kMsgUpdateFound:
        ShowUpdateHint(hwnd);
        return 0;
    case kMsgShowUi:
        if (g_miniMode.load(std::memory_order_acquire) && g_miniHwnd) {
            ShowWindow(g_miniHwnd, SW_SHOWNORMAL);
            SetForegroundWindow(g_miniHwnd);
        } else {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerDebouncedSave) {
            FlushDebouncedSettingsSave(hwnd);
            return 0;
        }
        if (wParam == kTimerStatusUpdate) {
            // Nothing on screen (minimized or hidden): poll slowly and skip all UI work.
            const bool mainShown = IsWindowVisible(hwnd) != FALSE && IsIconic(hwnd) == FALSE;
            const bool miniShown = g_miniHwnd && IsWindowVisible(g_miniHwnd) != FALSE && IsIconic(g_miniHwnd) == FALSE;
            if (!mainShown && !miniShown) {
                if (!g_statusTimerIdle) {
                    g_statusTimerIdle = true;
                    SetTimer(hwnd, kTimerStatusUpdate, kStatusTimerIdleMs, nullptr);
                }
                return 0;
            }
            if (g_statusTimerIdle) {
                g_statusTimerIdle = false;
                SetTimer(hwnd, kTimerStatusUpdate, kStatusTimerIntervalMs, nullptr);
            }
            const int tick = ++g_hotkeyRefreshTick;
            if ((tick % kHotkeyLabelRefreshEveryTicks) == 0) {
                UpdateHotkeyLabels(hwnd);
            }
            UpdateStatus(hwnd, (tick % kStatusDetailsEveryTicks) == 0);
            return 0;
        }
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (IsCapturingHotkey()) {
            const UINT vk = static_cast<UINT>(wParam);
            UINT* activeMods = nullptr;
            UINT* activeVk = nullptr;
            if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MusicMute) {
                activeMods = &g_muteHotkeyModifiers;
                activeVk = &g_muteHotkeyVk;
            } else if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MicInputMute) {
                activeMods = &g_micInputMuteHotkeyModifiers;
                activeVk = &g_micInputMuteHotkeyVk;
            }
            if (vk == VK_ESCAPE) {
                EndHotkeyCapture(hwnd);
                return 0;
            }
            if (vk == VK_DELETE || vk == VK_BACK) {
                EndHotkeyCapture(hwnd, false);
                if (activeMods) {
                    *activeMods = 0;
                }
                if (activeVk) {
                    *activeVk = 0;
                }
                ApplyLiveSettings(hwnd, false);
                UpdateHotkeyLabels(hwnd);
                return 0;
            }
            if (IsModifierOnlyKey(vk)) {
                return 0;
            }
            const UINT mods = ReadCurrentModifiers();
            if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MusicMute &&
                IsHotkeyConflict(mods, vk, g_micInputMuteHotkeyModifiers, g_micInputMuteHotkeyVk)) {
                EndHotkeyCapture(hwnd, false);
                MessageBeep(MB_ICONWARNING);
                SetStatusText(hwnd, Tr(Str::HotkeyConflictMic));
                UpdateHotkeyLabels(hwnd);
                return 0;
            }
            if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MicInputMute &&
                IsHotkeyConflict(mods, vk, g_muteHotkeyModifiers, g_muteHotkeyVk)) {
                EndHotkeyCapture(hwnd, false);
                MessageBeep(MB_ICONWARNING);
                SetStatusText(hwnd, Tr(Str::HotkeyConflictMusic));
                UpdateHotkeyLabels(hwnd);
                return 0;
            }
            EndHotkeyCapture(hwnd, false);
            if (activeMods) {
                *activeMods = mods;
            }
            if (activeVk) {
                *activeVk = vk;
            }
            ApplyLiveSettings(hwnd, false);
            UpdateHotkeyLabels(hwnd);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && IsCapturingHotkey()) {
            EndHotkeyCapture(hwnd);
            SetStatusText(hwnd, Tr(Str::HotkeyCancelled));
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_COMMAND:
        if (IsNavControlId(LOWORD(wParam)) && HIWORD(wParam) == BN_CLICKED) {
            ShowPage(hwnd, LOWORD(wParam) - IDC_NAV_FIRST, true);
            return 0;
        }
        switch (LOWORD(wParam)) {
        case IDC_REPO_LINK:
            if (HIWORD(wParam) == STN_CLICKED) {
                ShellExecuteW(hwnd, L"open", kRepoUrl, nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
            break;
        case IDC_RESCAN:
            RequestSourceRefresh(hwnd, true);
            return 0;
        case IDC_THEME_SYSTEM:
        case IDC_THEME_LIGHT:
        case IDC_THEME_DARK:
            if (HIWORD(wParam) == BN_CLICKED) {
                SetThemeMode(hwnd, ThemeModeForControlId(LOWORD(wParam)));
                for (int id : { IDC_THEME_SYSTEM, IDC_THEME_LIGHT, IDC_THEME_DARK }) {
                    InvalidateRect(GetDlgItem(hwnd, id), nullptr, FALSE);
                }
                return 0;
            }
            break;
        case IDC_POWER:
            if (HIWORD(wParam) == BN_CLICKED) {
                TogglePower(hwnd);
                return 0;
            }
            break;
        case IDC_OPEN_MINI:
            if (HIWORD(wParam) == BN_CLICKED) {
                EnterMiniMode(hwnd);
                return 0;
            }
            break;
        case IDC_UPDATE:
            if (HIWORD(wParam) == BN_CLICKED) {
                OpenUpdatePage(hwnd);
                return 0;
            }
            break;
        case IDC_SAVE:
            ApplyLiveSettings(hwnd, false, true);
            MicMixApp::Instance().StopSource();
            MicMixApp::Instance().StartSource();
            UpdateStatus(hwnd);
            return 0;
        case IDC_MONITOR:
            if (HIWORD(wParam) == BN_CLICKED) {
                MicMixApp::Instance().ToggleMonitor();
                UpdateMonitorButton(hwnd);
                return 0;
            }
            break;
        case IDC_MUTE_HOTKEY_SET:
            if (HIWORD(wParam) == BN_CLICKED) {
                BeginHotkeyCapture(hwnd, HotkeyCaptureTarget::MusicMute);
                return 0;
            }
            break;
        case IDC_MIC_INPUT_HOTKEY_SET:
            if (HIWORD(wParam) == BN_CLICKED) {
                BeginHotkeyCapture(hwnd, HotkeyCaptureTarget::MicInputMute);
                return 0;
            }
            break;
        case IDC_SOURCE:
            if (HIWORD(wParam) == CBN_DROPDOWN || HIWORD(wParam) == CBN_CLOSEUP) {
                InvalidateRect(reinterpret_cast<HWND>(lParam), nullptr, FALSE);
                return 0;
            }
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                const int sourceSel = static_cast<int>(SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), CB_GETCURSEL, 0, 0));
                const LRESULT data = SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), CB_GETITEMDATA, static_cast<WPARAM>(sourceSel), 0);
                if (data < 0 || data == CB_ERR || static_cast<size_t>(data) >= g_sourceChoices.size()) {
                    if (g_lastValidSourceSel >= 0) {
                        SendMessageW(GetDlgItem(hwnd, IDC_SOURCE), CB_SETCURSEL, g_lastValidSourceSel, 0);
                    }
                    return 0;
                }
                g_lastValidSourceSel = sourceSel;
                ApplyLiveSettings(hwnd, true);
                return 0;
            }
            break;
        case IDC_LANGUAGE:
            if (HIWORD(wParam) == CBN_DROPDOWN || HIWORD(wParam) == CBN_CLOSEUP) {
                InvalidateRect(reinterpret_cast<HWND>(lParam), nullptr, FALSE);
                return 0;
            }
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                HWND combo = reinterpret_cast<HWND>(lParam);
                const int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
                const LRESULT data = SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
                const auto& langs = I18nLanguages();
                const std::string code = (data >= 0 && static_cast<size_t>(data) < langs.size()) ? langs[static_cast<size_t>(data)].code : "auto";
                SetUiLanguage(hwnd, code);
                return 0;
            }
            break;
        case IDC_MIC_DEVICE:
            if (HIWORD(wParam) == CBN_DROPDOWN || HIWORD(wParam) == CBN_CLOSEUP) {
                InvalidateRect(reinterpret_cast<HWND>(lParam), nullptr, FALSE);
                return 0;
            }
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                ApplyLiveSettings(hwnd, false);
                return 0;
            }
            break;
        case IDC_FORCE_TX:
        case IDC_MIC_INPUT_MUTE:
        case IDC_MUTE:
        case IDC_AUTOSTART:
        case IDC_UPDATE_CHECK:
            if (HIWORD(wParam) == BN_CLICKED) {
                const int id = LOWORD(wParam);
                if ((id == IDC_MIC_INPUT_MUTE || id == IDC_MUTE) &&
                    IsWindowEnabled(GetDlgItem(hwnd, id)) == FALSE) {
                    return 0;
                }
                if (IsOwnerCheckboxControlId(id)) {
                    SetOwnerCheckboxValue(id, !GetOwnerCheckboxValue(id));
                    InvalidateRect(GetDlgItem(hwnd, id), nullptr, TRUE);
                }
                ApplyLiveSettings(hwnd, false);
                if (id == IDC_UPDATE_CHECK) {
                    if (GetOwnerCheckboxValue(IDC_UPDATE_CHECK)) {
                        StartUpdateCheck();
                    }
                    ShowUpdateHint(hwnd);
                }
                return 0;
            }
            break;
        default:
            break;
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        HWND ctl = reinterpret_cast<HWND>(lParam);
        const int id = GetDlgCtrlID(ctl);
        const COLORREF surface = SurfaceColorForControl(ctl);
        SetBkMode(hdc, TRANSPARENT);
        SetBkColor(hdc, surface);
        if (id == IDC_REPO_LINK) {
            SetTextColor(hdc, g_theme.link);
        } else if (id == IDC_VERSION || id == IDC_MUSIC_CLIP_EVENTS || id == IDC_MIC_CLIP_EVENTS ||
                   id == IDC_MUTE_HOTKEY_TEXT || id == IDC_MIC_INPUT_HOTKEY_TEXT ||
                   id == IDC_METER_TEXT || id == IDC_MIC_METER_TEXT) {
            SetTextColor(hdc, g_theme.muted);
        } else {
            SetTextColor(hdc, g_theme.text);
        }
        return reinterpret_cast<INT_PTR>(SurfaceBrushForColor(surface));
    }
    case WM_CTLCOLORBTN:
        // Owner-drawn buttons erase with this brush before WM_DRAWITEM; match their surface.
        return reinterpret_cast<INT_PTR>(SurfaceBrushForColor(SurfaceColorForControl(reinterpret_cast<HWND>(lParam))));
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, g_theme.text);
        SetBkColor(hdc, g_theme.inputBg);
        return reinterpret_cast<INT_PTR>(g_brushInput);
    }
    case WM_SETTINGCHANGE:
        if (g_themeMode == UiThemeMode::System && lParam != 0 &&
            lstrcmpiW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
            ApplyTheme(hwnd);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_SETCURSOR: {
        const HWND target = reinterpret_cast<HWND>(wParam);
        const UINT hit = LOWORD(lParam);
        if (target && hit == HTCLIENT) {
            const int id = GetDlgCtrlID(target);
            if (id == IDC_GAIN) {
                POINT cursorPt{};
                GetCursorPos(&cursorPt);
                const bool overThumb = IsPointOverGainThumb(target, cursorPt);
                const bool draggingThumb = (GetCapture() == target) && ((GetKeyState(VK_LBUTTON) & 0x8000) != 0);
                if (overThumb || draggingThumb) {
                    SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                    return TRUE;
                }
            }
            if (IsHandCursorControlId(id)) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT clientRc{};
        GetClientRect(hwnd, &clientRc);

        const int clientW = std::max(0, static_cast<int>(clientRc.right - clientRc.left));
        const int clientH = std::max(0, static_cast<int>(clientRc.bottom - clientRc.top));
        HDC buffer = g_mainBuffer.Acquire(hdc, clientW, clientH);
        HDC paintDc = buffer ? buffer : hdc;
        const int savedDc = SaveDC(paintDc);
        IntersectClipRect(paintDc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right, ps.rcPaint.bottom);
        PaintMainWindow(paintDc, clientRc, ps.rcPaint);
        RestoreDC(paintDc, savedDc);
        if (buffer) {
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                   buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        EndHotkeyCapture(hwnd, false);
        FlushDebouncedSettingsSave(hwnd);
        if (MicMixApp::Instance().IsMonitorEnabled()) {
            MicMixApp::Instance().SetMonitorEnabled(false);
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_mainBuffer.Release();
        if (g_miniHwnd) {
            DestroyWindow(g_miniHwnd);
            g_miniHwnd = nullptr;
        }
        g_miniMode.store(false, std::memory_order_release);
        EndHotkeyCapture(hwnd, false);
        if (MicMixApp::Instance().IsMonitorEnabled()) {
            MicMixApp::Instance().SetMonitorEnabled(false);
        }
        g_hwnd.store(nullptr, std::memory_order_release);
        g_sourceRefreshInFlight.store(false, std::memory_order_release);
        g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
        g_sourceRefreshPending.store(false, std::memory_order_release);
        g_sourceRefreshPendingReload.store(false, std::memory_order_release);
        g_saveDebouncePending.store(false, std::memory_order_release);
        g_hotkeyRefreshTick = 0;
        g_hotkeyCaptureTarget = HotkeyCaptureTarget::None;
        g_lastMusicClipEvents = std::numeric_limits<uint64_t>::max();
        g_lastMicClipEvents = std::numeric_limits<uint64_t>::max();
        g_lastMusicMeterLevelPx = -1;
        g_lastMusicMeterHoldPx = -1;
        g_lastMusicMeterActive = false;
        g_lastMicMeterLevelPx = -1;
        g_lastMicMeterHoldPx = -1;
        g_lastMicMeterActive = false;
        g_lastMusicClipLitSegments = -1;
        g_lastMusicClipRecent = false;
        g_lastMicClipLitSegments = -1;
        g_lastMicClipRecent = false;
        g_lastMusicMeterText.clear();
        g_lastMicMeterText.clear();
        g_headerStatusBadgeState = HeaderStatusBadgeState::Off;
        g_windowTitleActive = false;
        g_windowTitleStateValid = false;
        g_lastStatusText.clear();
        KillTimer(hwnd, kTimerStatusUpdate);
        KillTimer(hwnd, kTimerDebouncedSave);
        ReleaseUiResources();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void WindowThreadMain() {
    EnablePerMonitorDpiForThread();
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    Gdiplus::GdiplusStartupInput gdiplusInput;
    g_gdiplusReady = Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) == Gdiplus::Ok;
    struct GdiplusScope {
        ~GdiplusScope() {
            if (g_gdiplusReady) {
                Gdiplus::GdiplusShutdown(g_gdiplusToken);
                g_gdiplusReady = false;
                g_gdiplusToken = 0;
            }
        }
    } gdiplusScope;

    HINSTANCE hInst = GetPluginModuleHandle();
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MicMixSettingsWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON_SMALL));
    wc.hbrBackground = nullptr;
    UnregisterClassW(wc.lpszClassName, hInst);
    RegisterClassExW(&wc);

    {
        // Restore mini state before the first window exists.
        const MicMixSettings initial = MicMixApp::Instance().GetSettings();
        g_miniPinned = initial.uiMiniPinned;
        g_miniPosX = initial.uiMiniX;
        g_miniPosY = initial.uiMiniY;
    }
    const SIZE initialSize = WindowSizeForClient(S(kClientWidthPx), S(kClientHeightPx), kMainWindowStyle, g_dpi);
    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"MicMix Settings",
        kMainWindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        initialSize.cx,
        initialSize.cy,
        nullptr,
        nullptr,
        hInst,
        nullptr);
    if (!hwnd) {
        g_running.store(false, std::memory_order_release);
        return;
    }
    // WM_CREATE laid out the window at its monitor's DPI; size the frame to match.
    {
        const SIZE sz = WindowSizeForClient(S(kClientWidthPx), S(kClientHeightPx), kMainWindowStyle, g_dpi);
        SetWindowPos(hwnd, nullptr, 0, 0, sz.cx, sz.cy, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    }
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON))));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON_SMALL))));
    g_hwnd.store(hwnd, std::memory_order_release);
    if (MicMixApp::Instance().GetSettings().uiMiniMode) {
        EnterMiniMode(hwnd);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        HWND mainHwnd = g_hwnd.load(std::memory_order_acquire);
        if (mainHwnd &&
            IsCapturingHotkey() &&
            msg.hwnd != mainHwnd &&
            (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            SendMessageW(mainHwnd, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_running.store(false, std::memory_order_release);
}

} // namespace micmix_ui

using namespace micmix_ui;

SettingsWindowController& SettingsWindowController::Instance() {
    static SettingsWindowController instance;
    return instance;
}

void SettingsWindowController::Open() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_thread.joinable() && !g_running.load(std::memory_order_acquire)) {
        g_thread.join();
    }
    if (g_running.load(std::memory_order_acquire)) {
        HWND hwnd = g_hwnd.load(std::memory_order_acquire);
        if (hwnd) {
            PostMessageW(hwnd, kMsgShowUi, 0, 0);
        }
        return;
    }
    g_running.store(true, std::memory_order_release);
    if (MicMixApp::Instance().GetSettings().uiUpdateCheck) {
        StartUpdateCheck();
    }
    try {
        g_thread = std::thread(WindowThreadMain);
    } catch (const std::exception& ex) {
        g_running.store(false, std::memory_order_release);
        LogError(std::string("settings_window thread start failed: ") + ex.what());
    } catch (...) {
        g_running.store(false, std::memory_order_release);
        LogError("settings_window thread start failed: unknown");
    }
}

void SettingsWindowController::Close() {
    std::lock_guard<std::mutex> lock(g_mutex);
    StopUpdateCheck();
    if (MicMixApp::Instance().IsMonitorEnabled()) {
        MicMixApp::Instance().SetMonitorEnabled(false);
    }
    if (!g_running.load(std::memory_order_acquire)) {
        if (g_thread.joinable()) g_thread.join();
        JoinSourceRefreshThread();
        return;
    }
    HWND hwnd = g_hwnd.load(std::memory_order_acquire);
    if (hwnd) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
    JoinSourceRefreshThread();
}
