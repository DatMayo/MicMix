#include "settings_window.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// GDI+ expects min/max in its namespace; NOMINMAX hides the macros.
namespace Gdiplus {
using std::max;
using std::min;
}
#include <objidl.h>
#include <gdiplus.h>

#include "micmix_core.h"
#include "micmix_version.h"
#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Gdiplus.lib")

namespace {

enum ControlId {
    IDC_SOURCE = 1002,
    IDC_RESCAN = 1005,
    IDC_GAIN = 1006,
    IDC_GAIN_VALUE = 1007,
    IDC_FORCE_TX = 1008,
    IDC_MIC_INPUT_MUTE = 1009,
    IDC_MUTE = 1010,
    IDC_AUTOSTART = 1011,
    IDC_SAVE = 1014,
    IDC_METER_TEXT = 1019,
    IDC_MUTE_HOTKEY_SET = 1020,
    IDC_MUTE_HOTKEY_TEXT = 1021,
    IDC_MIC_INPUT_HOTKEY_SET = 1022,
    IDC_MIC_INPUT_HOTKEY_TEXT = 1023,
    IDC_MIC_DEVICE = 1024,
    IDC_MIC_METER_TEXT = 1025,
    IDC_MONITOR = 1026,
    IDC_VERSION = 1031,
    IDC_REPO_LINK = 1032,
    IDC_MUSIC_CLIP_EVENTS = 1033,
    IDC_MIC_CLIP_EVENTS = 1034,
    IDC_THEME_SYSTEM = 1035,
    IDC_THEME_LIGHT = 1036,
    IDC_THEME_DARK = 1037,
    IDC_POWER = 1040,
    IDC_OPEN_MINI = 1041,
    IDC_NAV_FIRST = 1050, // IDC_NAV_FIRST + page index
    IDC_NAV_LAST = 1054,
    // Mini window
    IDC_MINI_POWER = 1100,
    IDC_MINI_SOURCE = 1101,
    IDC_MINI_GAIN = 1102,
    IDC_MINI_MUTE_MUSIC = 1103,
    IDC_MINI_MUTE_MIC = 1104,
    IDC_MINI_PIN = 1105,
    IDC_MINI_EXPAND = 1106,
    IDC_MINI_CLOSE = 1107,
};

enum UiPage {
    kPageSource = 0,
    kPageMix = 1,
    kPageHotkeys = 2,
    kPageAppearance = 3,
    kPageDiagnostics = 4,
    kPageCount = 5,
};

enum class Glyph {
    Check, Pause, Power, Alert, Mic, Source, Mix, Keyboard, Contrast, Pulse,
    Pin, Expand, Close, Music, MicOff, Compact,
};

enum class SourceChoiceType {
    Loopback,
    App,
};

enum class HotkeyCaptureTarget {
    None,
    MusicMute,
    MicInputMute,
};

enum class HeaderStatusBadgeState {
    Active,
    Muted,
    Off,
    Error,
};

struct SourceChoice {
    SourceChoiceType type = SourceChoiceType::Loopback;
    std::string id;
    std::string processName;
    uint32_t pid = 0;
};

struct UiTheme {
    bool dark = false;
    // Surfaces
    COLORREF bg = RGB(241, 244, 248);
    COLORREF card = RGB(255, 255, 255);
    COLORREF cardShadow = RGB(226, 231, 239);
    COLORREF border = RGB(222, 227, 236);
    COLORREF text = RGB(28, 32, 40);
    COLORREF muted = RGB(100, 108, 122);
    COLORREF disabledText = RGB(156, 163, 175);
    COLORREF link = RGB(37, 99, 235);
    // Accent
    COLORREF accent = RGB(37, 99, 235);
    COLORREF accentHover = RGB(29, 78, 216);
    COLORREF accentPressed = RGB(30, 64, 175);
    COLORREF accentSecondary = RGB(124, 58, 237);
    COLORREF accentText = RGB(255, 255, 255);
    COLORREF accentSoft = RGB(219, 234, 254);
    // Buttons
    COLORREF buttonBg = RGB(246, 248, 251);
    COLORREF buttonHover = RGB(236, 240, 246);
    COLORREF buttonPressed = RGB(226, 232, 240);
    COLORREF buttonBorder = RGB(208, 215, 226);
    COLORREF buttonText = RGB(30, 35, 45);
    COLORREF buttonDisabledBg = RGB(243, 245, 248);
    // Inputs / lists
    COLORREF inputBg = RGB(255, 255, 255);
    COLORREF selectionBg = RGB(219, 234, 254);
    COLORREF selectionText = RGB(23, 37, 84);
    COLORREF comboHeaderText = RGB(100, 108, 122);
    COLORREF comboDividerText = RGB(190, 196, 206);
    COLORREF comboPlaceholderText = RGB(140, 146, 156);
    // Meters
    COLORREF meterBarBg = RGB(236, 240, 245);
    COLORREF meterBorder = RGB(214, 220, 229);
    COLORREF meterTickText = RGB(107, 114, 128);
    COLORREF meterTick = RGB(160, 168, 180);
    COLORREF meterGuide = RGB(214, 220, 229);
    COLORREF meterGuideWarn = RGB(205, 180, 100);
    COLORREF meterGuideHot = RGB(210, 140, 130);
    COLORREF meterGreen = RGB(34, 197, 94);
    COLORREF meterYellow = RGB(234, 179, 8);
    COLORREF meterRed = RGB(239, 68, 68);
    COLORREF meterHold = RGB(55, 65, 81);
    COLORREF clipOff = RGB(226, 231, 238);
    COLORREF clipBorder = RGB(214, 220, 229);
    COLORREF clipText = RGB(107, 114, 128);
    COLORREF clipTextHot = RGB(153, 27, 27);
    COLORREF clipPulse = RGB(220, 38, 38);
    // Slider / toggles
    COLORREF sliderTrack = RGB(214, 220, 229);
    COLORREF sliderThumb = RGB(255, 255, 255);
    COLORREF toggleOff = RGB(203, 210, 221);
    COLORREF toggleOffHover = RGB(186, 195, 209);
    COLORREF toggleKnob = RGB(255, 255, 255);
    // Status console
    COLORREF statusBg = RGB(246, 248, 251);
    COLORREF statusBorder = RGB(228, 232, 239);
    COLORREF statusText = RGB(51, 60, 75);
    // Fluent layout surfaces
    COLORREF content = RGB(250, 250, 250);
    COLORREF navSelected = RGB(234, 234, 234);
    COLORREF navHover = RGB(238, 238, 238);
    COLORREF divider = RGB(240, 240, 240);
    // State colors (icon background / foreground)
    COLORREF okBg = RGB(223, 246, 221);
    COLORREF okFg = RGB(16, 124, 16);
    COLORREF warnBg = RGB(255, 244, 206);
    COLORREF warnFg = RGB(157, 93, 0);
    COLORREF errBg = RGB(253, 231, 233);
    COLORREF errFg = RGB(196, 43, 28);
    COLORREF offBg = RGB(235, 235, 235);
    COLORREF offFg = RGB(95, 95, 95);
};

UiTheme MakeLightTheme() {
    UiTheme t;
    t.bg = RGB(243, 243, 243);
    t.content = RGB(250, 250, 250);
    t.card = RGB(255, 255, 255);
    t.cardShadow = RGB(236, 236, 236);
    t.border = RGB(229, 229, 229);
    t.divider = RGB(240, 240, 240);
    t.text = RGB(27, 27, 27);
    t.muted = RGB(95, 95, 95);
    t.disabledText = RGB(160, 160, 160);
    t.link = RGB(0, 95, 184);
    t.accent = RGB(0, 103, 192);
    t.accentHover = RGB(25, 117, 197);
    t.accentPressed = RGB(0, 84, 153);
    t.accentText = RGB(255, 255, 255);
    t.accentSoft = RGB(204, 228, 247);
    t.buttonBg = RGB(253, 253, 253);
    t.buttonHover = RGB(246, 246, 246);
    t.buttonPressed = RGB(240, 240, 240);
    t.buttonBorder = RGB(208, 208, 208);
    t.buttonText = RGB(27, 27, 27);
    t.buttonDisabledBg = RGB(245, 245, 245);
    t.inputBg = RGB(255, 255, 255);
    t.selectionBg = RGB(229, 241, 251);
    t.selectionText = RGB(27, 27, 27);
    t.navSelected = RGB(234, 234, 234);
    t.navHover = RGB(238, 238, 238);
    t.meterBarBg = RGB(236, 236, 236);
    t.meterBorder = RGB(224, 224, 224);
    t.meterGreen = RGB(16, 124, 16);
    t.meterYellow = RGB(193, 156, 0);
    t.meterRed = RGB(196, 43, 28);
    t.sliderTrack = RGB(196, 196, 196);
    t.toggleOff = RGB(255, 255, 255);
    t.toggleOffHover = RGB(243, 243, 243);
    t.statusBg = RGB(250, 250, 250);
    t.statusBorder = RGB(229, 229, 229);
    t.statusText = RGB(51, 51, 51);
    return t;
}

UiTheme MakeDarkTheme() {
    UiTheme t;
    t.dark = true;
    t.bg = RGB(32, 32, 32);
    t.content = RGB(39, 39, 39);
    t.card = RGB(45, 45, 45);
    t.cardShadow = RGB(28, 28, 28);
    t.border = RGB(58, 58, 58);
    t.divider = RGB(52, 52, 52);
    t.text = RGB(255, 255, 255);
    t.muted = RGB(207, 207, 207);
    t.disabledText = RGB(120, 120, 120);
    t.link = RGB(153, 235, 255);
    t.accent = RGB(76, 194, 255);
    t.accentHover = RGB(110, 207, 255);
    t.accentPressed = RGB(60, 170, 230);
    t.accentSecondary = RGB(139, 92, 246);
    t.accentText = RGB(0, 0, 0);
    t.accentSoft = RGB(26, 58, 82);
    t.buttonBg = RGB(55, 55, 55);
    t.buttonHover = RGB(61, 61, 61);
    t.buttonPressed = RGB(50, 50, 50);
    t.buttonBorder = RGB(72, 72, 72);
    t.buttonText = RGB(255, 255, 255);
    t.buttonDisabledBg = RGB(45, 45, 45);
    t.inputBg = RGB(51, 51, 51);
    t.selectionBg = RGB(30, 66, 96);
    t.selectionText = RGB(255, 255, 255);
    t.comboHeaderText = RGB(180, 180, 180);
    t.comboDividerText = RGB(90, 90, 90);
    t.comboPlaceholderText = RGB(140, 140, 140);
    t.navSelected = RGB(45, 45, 45);
    t.navHover = RGB(40, 40, 40);
    t.meterBarBg = RGB(30, 30, 30);
    t.meterBorder = RGB(62, 62, 62);
    t.meterTickText = RGB(170, 170, 170);
    t.meterTick = RGB(100, 100, 100);
    t.meterGuide = RGB(55, 55, 55);
    t.meterGuideWarn = RGB(110, 96, 50);
    t.meterGuideHot = RGB(116, 64, 60);
    t.meterGreen = RGB(108, 203, 95);
    t.meterYellow = RGB(252, 225, 0);
    t.meterRed = RGB(255, 153, 164);
    t.meterHold = RGB(240, 240, 240);
    t.clipOff = RGB(55, 55, 55);
    t.clipBorder = RGB(62, 62, 62);
    t.clipText = RGB(170, 170, 170);
    t.clipTextHot = RGB(255, 210, 210);
    t.sliderTrack = RGB(100, 100, 100);
    t.sliderThumb = RGB(69, 69, 69);
    t.toggleOff = RGB(39, 39, 39);
    t.toggleOffHover = RGB(50, 50, 50);
    t.toggleKnob = RGB(255, 255, 255);
    t.statusBg = RGB(32, 32, 32);
    t.statusBorder = RGB(58, 58, 58);
    t.statusText = RGB(220, 220, 220);
    t.okBg = RGB(30, 62, 32);
    t.okFg = RGB(108, 203, 95);
    t.warnBg = RGB(67, 53, 25);
    t.warnFg = RGB(252, 225, 0);
    t.errBg = RGB(68, 39, 38);
    t.errFg = RGB(255, 153, 164);
    t.offBg = RGB(58, 58, 58);
    t.offFg = RGB(207, 207, 207);
    return t;
}

COLORREF MixColor(COLORREF a, COLORREF b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto lerp = [t](int x, int y) {
        return static_cast<BYTE>(std::lround(static_cast<float>(x) + (static_cast<float>(y - x) * t)));
    };
    return RGB(lerp(GetRValue(a), GetRValue(b)), lerp(GetGValue(a), GetGValue(b)), lerp(GetBValue(a), GetBValue(b)));
}

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
bool g_ownerForceTxChecked = false;
bool g_ownerMicInputMuteChecked = false;
bool g_ownerMuteChecked = false;
HeaderStatusBadgeState g_headerStatusBadgeState = HeaderStatusBadgeState::Off;
bool g_windowTitleActive = false;
bool g_windowTitleStateValid = false;

constexpr INT_PTR kSourceItemHeader = -10;
constexpr INT_PTR kSourceItemDivider = -11;
constexpr INT_PTR kSourceItemPlaceholder = -12;
constexpr UINT kMsgSourceRefreshDone = WM_APP + 0x31;
constexpr UINT kMsgShowUi = WM_APP + 0x32;
constexpr UINT kTimerStatusUpdate = 1;
constexpr UINT kTimerDebouncedSave = 2;
constexpr UINT kStatusTimerIntervalMs = 40;
constexpr int kStatusDetailsEveryTicks = 3;
constexpr int kHotkeyLabelRefreshEveryTicks = 6;
constexpr UINT kDebouncedSaveMs = 300;
constexpr float kMusicGainMinDb = -30.0f;
constexpr float kMusicGainMaxDb = -2.0f;
constexpr int kMusicGainStepPerDb = 10;
constexpr int kMusicGainSliderMax = static_cast<int>((kMusicGainMaxDb - kMusicGainMinDb) * static_cast<float>(kMusicGainStepPerDb));
constexpr int kSourceIconSizePx = 16;
constexpr int kClientWidthPx = 900;
constexpr int kClientHeightPx = 620;
constexpr int kSidebarWidthPx = 210;
constexpr int kContentLeftPx = kSidebarWidthPx + 30;
constexpr int kContentRightPx = kClientWidthPx - 30;
constexpr int kRowPadPx = 18;
constexpr int kMiniClientWidthPx = 360;
constexpr int kMiniClientHeightPx = 476;
constexpr int kMiniMeterSegments = 24;
constexpr DWORD kMainWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
constexpr DWORD kMiniWindowStyle = WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN;

constexpr wchar_t kRepoUrl[] = L"https://github.com/dabinuss/MicMix";

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

struct RowDef {
    int page = 0;
    RECT rc{};
    const wchar_t* title = L"";
    const wchar_t* subtitle = L"";
    bool divider = false; // draw a divider below this row
    int textRight = 0;    // right edge for title/subtitle text
};
struct CardDef {
    int page = 0;
    RECT rc{};
};
struct PageControl {
    int page = 0;
    int id = 0;
};
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

int S(int px) {
    return MulDiv(px, g_dpi, 96);
}

void ApplyTheme(HWND hwnd);
void RefreshMini(bool force);
void SyncMiniFromMain();
void UpdateStatusCardText(HWND hwnd, const SourceStatus& st);
void ShowPage(HWND hwnd, int page, bool persist);
void ApplyDpi(HWND source, int newDpi, const RECT* suggested);
void SyncDpiToWindow(HWND w);
void ApplyMiniFonts(HWND mini);
int g_miniPosX = kUiPosUnset;
int g_miniPosY = kUiPosUnset;

bool IsOwnerCheckboxControlId(int id) {
    return id == IDC_AUTOSTART || id == IDC_FORCE_TX || id == IDC_MIC_INPUT_MUTE || id == IDC_MUTE;
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
    case IDC_FORCE_TX: return g_ownerForceTxChecked;
    case IDC_MIC_INPUT_MUTE: return g_ownerMicInputMuteChecked;
    case IDC_MUTE: return g_ownerMuteChecked;
    default: return false;
    }
}

void SetOwnerCheckboxValue(int id, bool value) {
    switch (id) {
    case IDC_AUTOSTART: g_ownerAutostartChecked = value; break;
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

bool IsSystemDarkMode() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS rc = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    return rc == ERROR_SUCCESS && value == 0;
}

bool ResolveDarkMode(UiThemeMode mode) {
    switch (mode) {
    case UiThemeMode::Dark:
        return true;
    case UiThemeMode::Light:
        return false;
    default:
        return IsSystemDarkMode();
    }
}

void RebuildThemeBrushes() {
    if (g_brushBg) { DeleteObject(g_brushBg); g_brushBg = nullptr; }
    if (g_brushCard) { DeleteObject(g_brushCard); g_brushCard = nullptr; }
    if (g_brushInput) { DeleteObject(g_brushInput); g_brushInput = nullptr; }
    if (g_brushContent) { DeleteObject(g_brushContent); g_brushContent = nullptr; }
    g_brushContent = CreateSolidBrush(g_theme.content);
    g_brushBg = CreateSolidBrush(g_theme.bg);
    g_brushCard = CreateSolidBrush(g_theme.card);
    g_brushInput = CreateSolidBrush(g_theme.inputBg);
}

void SelectThemePalette(UiThemeMode mode) {
    g_theme = ResolveDarkMode(mode) ? MakeDarkTheme() : MakeLightTheme();
    RebuildThemeBrushes();
}

void ApplyWindowFrameTheme(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static DwmSetWindowAttributeFn setAttr = []() -> DwmSetWindowAttributeFn {
        HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!dwm) {
            return nullptr;
        }
        return reinterpret_cast<DwmSetWindowAttributeFn>(GetProcAddress(dwm, "DwmSetWindowAttribute"));
    }();
    if (!setAttr) {
        return;
    }
    constexpr DWORD kDwmUseImmersiveDarkMode = 20;       // Windows 10 20H1+ / Windows 11
    constexpr DWORD kDwmUseImmersiveDarkModeLegacy = 19; // Windows 10 1809 - 1909
    constexpr DWORD kDwmWindowCornerPreference = 33;     // Windows 11
    constexpr DWORD kDwmBorderColor = 34;                // Windows 11
    constexpr DWORD kDwmCaptionColor = 35;               // Windows 11
    constexpr DWORD kDwmTextColor = 36;                  // Windows 11
    const BOOL dark = g_theme.dark ? TRUE : FALSE;
    if (FAILED(setAttr(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark)))) {
        setAttr(hwnd, kDwmUseImmersiveDarkModeLegacy, &dark, sizeof(dark));
    }
    const int roundCorners = 2; // DWMWCP_ROUND
    setAttr(hwnd, kDwmWindowCornerPreference, &roundCorners, sizeof(roundCorners));
    const COLORREF caption = g_theme.bg;
    const COLORREF captionText = g_theme.text;
    const COLORREF border = g_theme.border;
    setAttr(hwnd, kDwmCaptionColor, &caption, sizeof(caption));
    setAttr(hwnd, kDwmTextColor, &captionText, sizeof(captionText));
    setAttr(hwnd, kDwmBorderColor, &border, sizeof(border));
}

void EnsureUiResources() {
    if (!g_brushBg || !g_brushCard || !g_brushInput || !g_brushContent) RebuildThemeBrushes();
    if (!g_fontPageTitle) g_fontPageTitle = CreateFontW(-S(26), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    if (!g_fontRowTitle) g_fontRowTitle = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontBrand) g_fontBrand = CreateFontW(-S(15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    if (!g_fontButton) g_fontButton = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    if (!g_fontSection) g_fontSection = CreateFontW(-S(11), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    if (!g_fontBody) g_fontBody = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontSmall) g_fontSmall = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontHint) g_fontHint = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontTitle) g_fontTitle = CreateFontW(-S(24), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Semibold");
    if (!g_fontTiny) g_fontTiny = CreateFontW(-S(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontClipInfo) g_fontClipInfo = CreateFontW(-S(10), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontControlLarge) g_fontControlLarge = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!g_fontMono) g_fontMono = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    EnsureFallbackIcons();
}

void ReleaseFonts() {
    for (HFONT* f : { &g_fontBody, &g_fontSmall, &g_fontHint, &g_fontTiny, &g_fontClipInfo, &g_fontControlLarge,
                      &g_fontTitle, &g_fontMono, &g_fontButton, &g_fontSection, &g_fontPageTitle, &g_fontRowTitle,
                      &g_fontBrand }) {
        if (*f) {
            DeleteObject(*f);
            *f = nullptr;
        }
    }
}

void ReleaseUiResources() {
    ClearAppIconCache();
    DestroyIconHandle(g_loopbackFallbackIcon);
    DestroyIconHandle(g_appFallbackIcon);
    ReleaseFonts();
    if (g_brushBg) { DeleteObject(g_brushBg); g_brushBg = nullptr; }
    if (g_brushCard) { DeleteObject(g_brushCard); g_brushCard = nullptr; }
    if (g_brushInput) { DeleteObject(g_brushInput); g_brushInput = nullptr; }
    if (g_brushContent) { DeleteObject(g_brushContent); g_brushContent = nullptr; }
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
    SetWindowTextW(hwnd, active ? L"MicMix Settings - ACTIVE" : L"MicMix Settings - OFF");
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
        SetStaticTextIfChanged(txt, g_lastMusicMeterText, L"No signal");
    } else {
        const wchar_t* grade = L"OK";
        if (shownPeakDb > -8.0f) {
            grade = L"Hot";
        } else if (shownPeakDb > -16.0f) {
            grade = L"Warm";
        } else if (shownPeakDb < -35.0f) {
            grade = L"Quiet";
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
        SetStaticTextIfChanged(txt, g_lastMicMeterText, L"No signal");
    } else {
        const wchar_t* grade = L"OK";
        if (t.micPeakDbfs > -8.0f) {
            grade = L"Hot";
        } else if (t.micPeakDbfs > -16.0f) {
            grade = L"Warm";
        } else if (t.micPeakDbfs < -35.0f) {
            grade = L"Quiet";
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
    const wchar_t* next = enabled ? L"Monitor Mix: On" : L"Monitor Mix: Off";
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
        return L"Not set";
    }
    std::wstring out;
    if ((mods & MOD_CONTROL) != 0) out += L"Ctrl + ";
    if ((mods & MOD_ALT) != 0) out += L"Alt + ";
    if ((mods & MOD_SHIFT) != 0) out += L"Shift + ";
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
    const std::wstring text = L"Current: " + FormatHotkeyText(g_muteHotkeyModifiers, g_muteHotkeyVk);
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
    const std::wstring text = L"Current: " + FormatHotkeyText(g_micInputMuteHotkeyModifiers, g_micInputMuteHotkeyVk);
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
        SetWindowTextW(GetDlgItem(hwnd, labelId), L"Current: Press key... (Esc=Cancel, Del=Clear)");
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
    SetStatusText(hwnd, L"Refreshing source list...");

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
        SetStatusText(hwnd, L"Refresh failed to start");
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

    addStaticItem(source, L"Audio Channels:", kSourceItemHeader);
    if (g_loopbacks.empty()) {
        addStaticItem(source, L"  (no audio channels found)", kSourceItemPlaceholder);
    } else {
        for (const auto& d : g_loopbacks) {
            std::wstring label = L"  ";
            label += Utf8ToWide(d.name);
            if (d.isDefault) {
                label += L" (Default)";
            }
            SourceChoice choice{};
            choice.type = SourceChoiceType::Loopback;
            choice.id = d.id;
            addSelectableItem(source, label, choice);
        }
    }

    addStaticItem(source, L"------------------------------", kSourceItemDivider);
    addStaticItem(source, L"Running Apps:", kSourceItemHeader);
    if (g_apps.empty()) {
        addStaticItem(source, L"  (no running app found)", kSourceItemPlaceholder);
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
                label += L" (Default)";
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
    return s;
}

void UpdateStatus(HWND hwnd, bool includeDetails = true) {
    auto& app = MicMixApp::Instance();
    const TelemetrySnapshot t = app.GetTelemetry();
    g_lastTelemetry = t;

    if (t.sourceClipEvents != g_lastMusicClipEvents) {
        wchar_t clipMusic[64]{};
        swprintf_s(clipMusic, L"CLIP Events: %llu", static_cast<unsigned long long>(t.sourceClipEvents));
        SetWindowTextW(GetDlgItem(hwnd, IDC_MUSIC_CLIP_EVENTS), clipMusic);
        g_lastMusicClipEvents = t.sourceClipEvents;
    }
    if (t.micClipEvents != g_lastMicClipEvents) {
        wchar_t clipMic[64]{};
        swprintf_s(clipMic, L"CLIP Events: %llu", static_cast<unsigned long long>(t.micClipEvents));
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

void ApplyLiveSettings(HWND hwnd, bool restartSource, bool saveImmediately = true) {
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

Gdiplus::Color ToGdiColor(COLORREF c, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
}

void AddRoundRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius) {
    radius = std::min(radius, std::min(r.Width, r.Height) * 0.5f);
    if (radius <= 0.5f) {
        path.AddRectangle(r);
        return;
    }
    const float d = radius * 2.0f;
    path.AddArc(r.X, r.Y, d, d, 180.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270.0f, 90.0f);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

struct ShapeStyle {
    COLORREF fill = RGB(0, 0, 0);
    bool hasFill = true;
    COLORREF border = RGB(0, 0, 0);
    bool hasBorder = false;
    float borderWidth = 1.0f;
};

ShapeStyle Filled(COLORREF fill) {
    ShapeStyle s;
    s.fill = fill;
    return s;
}

ShapeStyle FilledBordered(COLORREF fill, COLORREF border, float borderWidth = 1.0f) {
    ShapeStyle s;
    s.fill = fill;
    s.border = border;
    s.hasBorder = true;
    s.borderWidth = borderWidth;
    return s;
}

ShapeStyle Outline(COLORREF border, float borderWidth = 1.0f) {
    ShapeStyle s;
    s.hasFill = false;
    s.border = border;
    s.hasBorder = true;
    s.borderWidth = borderWidth;
    return s;
}

// Anti-aliased rounded rectangle via GDI+, with a plain GDI fallback when GDI+ is unavailable.
void DrawRoundRect(HDC hdc, const RECT& rc, int radius, const ShapeStyle& style) {
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        return;
    }
    if (!g_gdiplusReady) {
        HPEN pen = style.hasBorder ? CreatePen(PS_SOLID, std::max(1, static_cast<int>(style.borderWidth)), style.border) : nullptr;
        HBRUSH brush = style.hasFill ? CreateSolidBrush(style.fill) : nullptr;
        HGDIOBJ oldPen = SelectObject(hdc, pen ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
        HGDIOBJ oldBrush = SelectObject(hdc, brush ? static_cast<HGDIOBJ>(brush) : GetStockObject(HOLLOW_BRUSH));
        RoundRect(hdc, rc.left, rc.top, rc.right + (pen ? 0 : 1), rc.bottom + (pen ? 0 : 1), radius * 2, radius * 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    // Pixel centers sit on integer coordinates: shift by half a pixel so the shape covers
    // exactly the pixels of `rc` and 1px borders land on whole pixels.
    const Gdiplus::RectF r(
        static_cast<Gdiplus::REAL>(rc.left) - 0.5f,
        static_cast<Gdiplus::REAL>(rc.top) - 0.5f,
        static_cast<Gdiplus::REAL>(rc.right - rc.left),
        static_cast<Gdiplus::REAL>(rc.bottom - rc.top));
    if (style.hasFill) {
        Gdiplus::GraphicsPath path;
        AddRoundRectPath(path, r, static_cast<float>(radius));
        Gdiplus::SolidBrush brush(ToGdiColor(style.fill));
        g.FillPath(&brush, &path);
    }
    if (style.hasBorder) {
        const float inset = style.borderWidth * 0.5f;
        const Gdiplus::RectF br(r.X + inset, r.Y + inset, r.Width - style.borderWidth, r.Height - style.borderWidth);
        Gdiplus::GraphicsPath path;
        AddRoundRectPath(path, br, std::max(0.0f, static_cast<float>(radius) - inset));
        Gdiplus::Pen pen(ToGdiColor(style.border), style.borderWidth);
        g.DrawPath(&pen, &path);
    }
}

void DrawEllipseAA(HDC hdc, const RECT& rc, const ShapeStyle& style) {
    if (rc.right <= rc.left || rc.bottom <= rc.top) {
        return;
    }
    if (!g_gdiplusReady) {
        DrawRoundRect(hdc, rc, std::min(rc.right - rc.left, rc.bottom - rc.top) / 2, style);
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    // Pixel centers sit on integer coordinates: shift by half a pixel so the shape covers
    // exactly the pixels of `rc` and 1px borders land on whole pixels.
    const Gdiplus::RectF r(
        static_cast<Gdiplus::REAL>(rc.left) - 0.5f,
        static_cast<Gdiplus::REAL>(rc.top) - 0.5f,
        static_cast<Gdiplus::REAL>(rc.right - rc.left),
        static_cast<Gdiplus::REAL>(rc.bottom - rc.top));
    if (style.hasFill) {
        Gdiplus::SolidBrush brush(ToGdiColor(style.fill));
        g.FillEllipse(&brush, r);
    }
    if (style.hasBorder) {
        const float inset = style.borderWidth * 0.5f;
        Gdiplus::Pen pen(ToGdiColor(style.border), style.borderWidth);
        g.DrawEllipse(&pen, r.X + inset, r.Y + inset, r.Width - style.borderWidth, r.Height - style.borderWidth);
    }
}

void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    FillRect(hdc, &rc, b);
    DeleteObject(b);
}

void DrawCard(HDC hdc, const RECT& rc) {
    DrawRoundRect(hdc, rc, S(8), FilledBordered(g_theme.card, g_theme.border));
}

// Simple stroke icons on a 24x24 grid (same shapes as the design mockups).
void DrawGlyph(HDC hdc, Glyph glyph, const RECT& box, COLORREF color, float stroke = 1.8f) {
    if (!g_gdiplusReady || box.right <= box.left || box.bottom <= box.top) {
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float size = static_cast<float>(std::min(box.right - box.left, box.bottom - box.top));
    const float sc = size / 24.0f;
    const float ox = static_cast<float>(box.left) + ((static_cast<float>(box.right - box.left) - size) * 0.5f);
    const float oy = static_cast<float>(box.top) + ((static_cast<float>(box.bottom - box.top) - size) * 0.5f);
    const auto X = [&](float v) { return ox + (v * sc); };
    const auto Y = [&](float v) { return oy + (v * sc); };
    Gdiplus::Pen pen(ToGdiColor(color), std::max(1.0f, stroke * sc));
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush brush(ToGdiColor(color));
    const auto L = [&](float x1, float y1, float x2, float y2) { g.DrawLine(&pen, X(x1), Y(y1), X(x2), Y(y2)); };
    const auto Arc = [&](float x, float y, float w, float h, float start, float sweep) {
        g.DrawArc(&pen, X(x), Y(y), w * sc, h * sc, start, sweep);
    };
    const auto RR = [&](float x, float y, float w, float h, float r) {
        Gdiplus::GraphicsPath path;
        AddRoundRectPath(path, Gdiplus::RectF(X(x), Y(y), w * sc, h * sc), r * sc);
        g.DrawPath(&pen, &path);
    };
    const auto Dot = [&](float x, float y, float r) { g.FillEllipse(&brush, X(x - r), Y(y - r), 2 * r * sc, 2 * r * sc); };
    switch (glyph) {
    case Glyph::Check: L(5, 12, 10, 17); L(10, 17, 20, 7); break;
    case Glyph::Pause: L(9, 6, 9, 18); L(15, 6, 15, 18); break;
    case Glyph::Power: L(12, 3, 12, 11); Arc(4.5f, 5, 15, 15, 300, 300); break;
    case Glyph::Alert: L(12, 6, 12, 13); Dot(12, 17.5f, 1.4f); break;
    case Glyph::Mic: RR(9, 3, 6, 11, 3); Arc(5, 4, 14, 14, 0, 180); L(12, 18, 12, 21); break;
    case Glyph::MicOff: RR(9, 3, 6, 11, 3); Arc(5, 4, 14, 14, 0, 180); L(12, 18, 12, 21); L(3, 3, 21, 21); break;
    case Glyph::Source: L(4, 6, 20, 6); L(4, 12, 14, 12); L(4, 18, 10, 18); break;
    case Glyph::Mix:
        L(6, 4, 6, 20); L(12, 4, 12, 20); L(18, 4, 18, 20);
        L(4, 9, 8, 9); L(10, 15, 14, 15); L(16, 7, 20, 7);
        break;
    case Glyph::Keyboard:
        RR(3, 6, 18, 12, 2);
        Dot(7, 10, 0.9f); Dot(11, 10, 0.9f); Dot(15, 10, 0.9f); Dot(17.5f, 10, 0.9f);
        L(7, 14, 17, 14);
        break;
    case Glyph::Contrast:
        g.DrawEllipse(&pen, X(4), Y(4), 16 * sc, 16 * sc);
        g.FillPie(&brush, X(4), Y(4), 16 * sc, 16 * sc, 90, 180);
        break;
    case Glyph::Pulse: {
        const Gdiplus::PointF pts[6] = {
            { X(3), Y(12) }, { X(7), Y(12) }, { X(10), Y(5) }, { X(14), Y(19) }, { X(17), Y(12) }, { X(21), Y(12) } };
        g.DrawLines(&pen, pts, 6);
        break;
    }
    case Glyph::Pin: {
        L(12, 16, 12, 21);
        const Gdiplus::PointF pts[7] = {
            { X(8), Y(3) }, { X(16), Y(3) }, { X(15), Y(9) }, { X(18), Y(13) }, { X(6), Y(13) }, { X(9), Y(9) }, { X(8), Y(3) } };
        g.DrawLines(&pen, pts, 7);
        break;
    }
    case Glyph::Expand: RR(4, 4, 16, 16, 2); L(4, 9, 20, 9); break;
    case Glyph::Close: L(6, 6, 18, 18); L(18, 6, 6, 18); break;
    case Glyph::Music:
        L(9, 18, 9, 5); L(9, 5, 20, 3); L(20, 3, 20, 16);
        g.DrawEllipse(&pen, X(3), Y(15), 6 * sc, 6 * sc);
        g.DrawEllipse(&pen, X(14), Y(13), 6 * sc, 6 * sc);
        break;
    case Glyph::Compact: RR(3, 5, 18, 14, 2); RR(12, 11, 6, 5, 1); break;
    }
}

// Background color behind a child control: the card, sidebar or content surface it sits on.
COLORREF SurfaceColorForControl(HWND ctl) {
    HWND parent = GetParent(ctl);
    RECT rc{};
    GetWindowRect(ctl, &rc);
    MapWindowPoints(nullptr, parent, reinterpret_cast<LPPOINT>(&rc), 2);
    const POINT pt{ (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 };
    if (parent && parent == g_miniHwnd) {
        return PtInRect(&g_rcMiniPanel, pt) ? g_theme.card : g_theme.content;
    }
    if (pt.x < S(kSidebarWidthPx)) {
        return g_theme.bg;
    }
    if (PtInRect(&g_rcHeaderBadge, pt)) {
        return g_theme.card;
    }
    for (const auto& card : g_cards) {
        if (card.page == g_page && PtInRect(&card.rc, pt)) {
            return g_theme.card;
        }
    }
    return g_theme.content;
}

HBRUSH SurfaceBrushForColor(COLORREF color) {
    if (color == g_theme.card) return g_brushCard;
    if (color == g_theme.bg) return g_brushBg;
    return g_brushContent;
}

void DrawSwitch(HDC hdc, const RECT& track, bool on, bool hover, bool disabled) {
    const int trackH = track.bottom - track.top;
    COLORREF fill = on ? (hover ? g_theme.accentHover : g_theme.accent) : (hover ? g_theme.toggleOffHover : g_theme.toggleOff);
    COLORREF knob = on ? g_theme.accentText : g_theme.muted;
    if (disabled) {
        fill = on ? MixColor(g_theme.accent, g_theme.card, 0.6f) : g_theme.buttonDisabledBg;
        knob = g_theme.disabledText;
    }
    if (on) {
        DrawRoundRect(hdc, track, trackH / 2, Filled(fill));
    } else {
        DrawRoundRect(hdc, track, trackH / 2, FilledBordered(fill, disabled ? g_theme.border : g_theme.muted));
    }
    const int knobSize = hover && !disabled ? trackH - S(8) : trackH - S(10);
    const int inset = (trackH - knobSize) / 2;
    const int knobLeft = on ? (track.right - S(4) - knobSize) : (track.left + S(4));
    RECT k{ knobLeft, track.top + inset, knobLeft + knobSize, track.top + inset + knobSize };
    DrawEllipseAA(hdc, k, Filled(knob));
}

struct StatusVisual {
    Glyph glyph = Glyph::Power;
    COLORREF bg = RGB(0, 0, 0);
    COLORREF fg = RGB(0, 0, 0);
};

StatusVisual GetStatusVisual(HeaderStatusBadgeState state) {
    switch (state) {
    case HeaderStatusBadgeState::Active: return { Glyph::Check, g_theme.okBg, g_theme.okFg };
    case HeaderStatusBadgeState::Muted: return { Glyph::Pause, g_theme.warnBg, g_theme.warnFg };
    case HeaderStatusBadgeState::Error: return { Glyph::Alert, g_theme.errBg, g_theme.errFg };
    default: return { Glyph::Power, g_theme.offBg, g_theme.offFg };
    }
}

// Status card at the top of every page: state icon, headline and route.
void DrawStatusCard(HDC hdc) {
    const RECT& rc = g_rcHeaderBadge;
    if (rc.right <= rc.left) {
        return;
    }
    DrawCard(hdc, rc);
    const StatusVisual v = GetStatusVisual(g_headerStatusBadgeState);
    const int d = S(40);
    RECT circle{ rc.left + S(kRowPadPx), rc.top + ((rc.bottom - rc.top - d) / 2), rc.left + S(kRowPadPx) + d, rc.top + ((rc.bottom - rc.top - d) / 2) + d };
    DrawEllipseAA(hdc, circle, Filled(v.bg));
    RECT iconRc = circle;
    InflateRect(&iconRc, -S(10), -S(10));
    DrawGlyph(hdc, v.glyph, iconRc, v.fg, 2.2f);

    SetBkMode(hdc, TRANSPARENT);
    const int textLeft = circle.right + S(16);
    const int textRight = rc.right - S(150);
    RECT titleRc{ textLeft, rc.top + S(15), textRight, rc.top + S(36) };
    SetTextColor(hdc, g_theme.text);
    HGDIOBJ oldFont = SelectObject(hdc, g_fontButton ? g_fontButton : g_fontBody);
    DrawTextW(hdc, g_statusTitle.c_str(), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RECT subRc{ textLeft, rc.top + S(36), textRight, rc.bottom - S(12) };
    SetTextColor(hdc, g_theme.muted);
    SelectObject(hdc, g_fontSmall ? g_fontSmall : g_fontBody);
    DrawTextW(hdc, g_statusSubtitle.c_str(), -1, &subRc, DT_SINGLELINE | DT_TOP | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

void DrawLevelMeter(HDC hdc, const RECT& meterRect, bool active, float visualDb, float holdDb) {
    if (meterRect.right <= meterRect.left || meterRect.bottom <= meterRect.top) {
        return;
    }
    const int targetWidth = static_cast<int>(meterRect.right - meterRect.left);
    const int targetHeight = static_cast<int>(meterRect.bottom - meterRect.top);
    if (targetWidth <= 2 || targetHeight <= 2) {
        return;
    }

    HDC drawDc = hdc;
    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    bool buffered = false;
    if (memDc) {
        memBmp = CreateCompatibleBitmap(hdc, targetWidth, targetHeight);
        if (memBmp) {
            oldBmp = SelectObject(memDc, memBmp);
            drawDc = memDc;
            buffered = true;
        } else {
            DeleteDC(memDc);
            memDc = nullptr;
        }
    }

    RECT rc = buffered ? RECT{ 0, 0, targetWidth, targetHeight } : meterRect;
    const int left = static_cast<int>(rc.left);
    const int right = static_cast<int>(rc.right);
    const int width = right - left;
    const int height = static_cast<int>(rc.bottom - rc.top);
    if (width <= 2 || height <= 2) {
        if (buffered) {
            SelectObject(memDc, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDc);
        }
        return;
    }

    constexpr float kYellowStartDb = -24.0f;
    constexpr float kRedStartDb = -12.0f;
    const int scaleBandH = std::clamp(S(10), 7, std::max(7, height - S(8)));
    RECT scaleRc = rc;
    scaleRc.bottom = std::min(rc.bottom, rc.top + scaleBandH);
    RECT barRc = rc;
    barRc.top = std::min(rc.bottom - 1, scaleRc.bottom);

    FillSolidRect(drawDc, rc, g_theme.card);
    FillSolidRect(drawDc, barRc, g_theme.meterBarBg);

    const std::array<int, 7> tickDb = { -60, -36, -24, -18, -12, -6, 0 };
    SetBkMode(drawDc, TRANSPARENT);
    SetTextColor(drawDc, g_theme.meterTickText);
    HGDIOBJ oldFont = SelectObject(drawDc, g_fontTiny ? g_fontTiny : (g_fontHint ? g_fontHint : g_fontSmall));
    for (size_t i = 0; i < tickDb.size(); ++i) {
        const int db = tickDb[i];
        int x = left + MeterDbToPixels(static_cast<float>(db), width);
        x = std::clamp(x, left, right - 1);

        const COLORREF guideColor = (db == -24)
            ? g_theme.meterGuideWarn
            : (db == -12 || db == 0)
                ? g_theme.meterGuideHot
                : g_theme.meterGuide;
        HPEN guidePen = CreatePen(PS_SOLID, 1, guideColor);
        HGDIOBJ oldGuidePen = SelectObject(drawDc, guidePen);
        MoveToEx(drawDc, x, barRc.top, nullptr);
        LineTo(drawDc, x, barRc.bottom);
        SelectObject(drawDc, oldGuidePen);
        DeleteObject(guidePen);

        RECT tickRc{ x, scaleRc.bottom - S(3), x + 1, scaleRc.bottom };
        FillSolidRect(drawDc, tickRc, g_theme.meterTick);

        wchar_t label[8]{};
        swprintf_s(label, L"%d", db);
        SIZE textSize{};
        if (GetTextExtentPoint32W(drawDc, label, lstrlenW(label), &textSize) != 0) {
            int textLeft = x - (textSize.cx / 2);
            if (i == 0) {
                textLeft = left + S(2);
            } else if (i == (tickDb.size() - 1)) {
                textLeft = right - textSize.cx - S(2);
            } else {
                const int labelMinX = left + S(2);
                const int labelMaxX = right - static_cast<int>(textSize.cx) - S(2);
                textLeft = std::clamp(textLeft, labelMinX, labelMaxX);
            }
            TextOutW(drawDc, textLeft, scaleRc.top, label, lstrlenW(label));
        }
    }
    SelectObject(drawDc, oldFont);

    const int barWidth = std::max(1, static_cast<int>(barRc.right - barRc.left));
    const int levelPx = std::clamp(MeterDbToPixels(visualDb, width), 0, width);
    const int holdPx = std::clamp(MeterDbToPixels(holdDb, width), 0, width - 1);
    const int greenEnd = std::clamp(MeterDbToPixels(kYellowStartDb, barWidth), 0, barWidth);
    const int yellowEnd = std::clamp(MeterDbToPixels(kRedStartDb, barWidth), 0, barWidth);

    if (active && levelPx > 0) {
        RECT seg = barRc;
        seg.right = barRc.left + std::min(levelPx, greenEnd);
        if (seg.right > seg.left) FillSolidRect(drawDc, seg, g_theme.meterGreen);

        if (levelPx > greenEnd) {
            seg.left = barRc.left + greenEnd;
            seg.right = barRc.left + std::min(levelPx, yellowEnd);
            if (seg.right > seg.left) FillSolidRect(drawDc, seg, g_theme.meterYellow);
        }
        if (levelPx > yellowEnd) {
            seg.left = barRc.left + yellowEnd;
            seg.right = barRc.left + levelPx;
            if (seg.right > seg.left) FillSolidRect(drawDc, seg, g_theme.meterRed);
        }

        HPEN holdPen = CreatePen(PS_SOLID, std::max(1, S(2)), g_theme.meterHold);
        HGDIOBJ oldPen = SelectObject(drawDc, holdPen);
        MoveToEx(drawDc, barRc.left + holdPx, barRc.top, nullptr);
        LineTo(drawDc, barRc.left + holdPx, barRc.bottom);
        SelectObject(drawDc, oldPen);
        DeleteObject(holdPen);
    }

    HPEN border = CreatePen(PS_SOLID, 1, g_theme.meterBorder);
    HGDIOBJ oldPen = SelectObject(drawDc, border);
    HGDIOBJ oldBrush = SelectObject(drawDc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(drawDc, barRc.left, barRc.top, barRc.right, barRc.bottom);
    SelectObject(drawDc, oldBrush);
    SelectObject(drawDc, oldPen);
    DeleteObject(border);

    if (buffered) {
        BitBlt(
            hdc,
            meterRect.left,
            meterRect.top,
            targetWidth,
            targetHeight,
            memDc,
            0,
            0,
            SRCCOPY);
        SelectObject(memDc, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDc);
    }
}

void EndHotkeyCapture(HWND hwnd, bool refreshLabels = true) {
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

LRESULT DrawGainSliderCustom(const NMCUSTOMDRAW* cd) {
    if (!cd) {
        return CDRF_DODEFAULT;
    }
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }
    if (cd->dwDrawStage != CDDS_ITEMPREPAINT) {
        return CDRF_DODEFAULT;
    }

    const DWORD item = static_cast<DWORD>(cd->dwItemSpec);
    const RECT rc = cd->rc;
    if (item == TBCD_TICS) {
        // Modern flat look: no tick marks.
        return CDRF_SKIPDEFAULT;
    }
    if (item == TBCD_CHANNEL) {
        const int trackH = std::max(2, S(4));
        const int centerY = (rc.top + rc.bottom) / 2;
        RECT track{ rc.left, centerY - (trackH / 2), rc.right, centerY - (trackH / 2) + trackH };
        DrawRoundRect(cd->hdc, track, trackH / 2, Filled(g_theme.sliderTrack));

        RECT thumbRc{};
        SendMessageW(cd->hdr.hwndFrom, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumbRc));
        RECT filled = track;
        filled.right = std::clamp(static_cast<int>((thumbRc.left + thumbRc.right) / 2), static_cast<int>(track.left), static_cast<int>(track.right));
        if (filled.right > filled.left) {
            DrawRoundRect(cd->hdc, filled, trackH / 2, Filled(g_theme.accent));
        }
        return CDRF_SKIPDEFAULT;
    }
    if (item == TBCD_THUMB) {
        RECT thumb = rc;
        const bool hot = (cd->uItemState & CDIS_HOT) != 0;
        const bool pressed = (cd->uItemState & CDIS_SELECTED) != 0;
        const int w = thumb.right - thumb.left;
        const COLORREF ring = pressed ? g_theme.accentPressed : (hot ? g_theme.accentHover : g_theme.accent);
        DrawRoundRect(cd->hdc, thumb, w / 2, FilledBordered(g_theme.sliderThumb, ring, static_cast<float>(std::max(1, S(2)))));
        return CDRF_SKIPDEFAULT;
    }
    return CDRF_DODEFAULT;
}

void DrawClipStrip(HDC hdc, const RECT& rcStrip, float dangerUnit, bool clipRecent) {
    if (rcStrip.right <= rcStrip.left || rcStrip.bottom <= rcStrip.top) {
        return;
    }
    const int targetWidth = static_cast<int>(rcStrip.right - rcStrip.left);
    const int targetHeight = static_cast<int>(rcStrip.bottom - rcStrip.top);
    if (targetWidth <= 2 || targetHeight <= 2) {
        return;
    }

    HDC drawDc = hdc;
    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    bool buffered = false;
    if (memDc) {
        memBmp = CreateCompatibleBitmap(hdc, targetWidth, targetHeight);
        if (memBmp) {
            oldBmp = SelectObject(memDc, memBmp);
            drawDc = memDc;
            buffered = true;
        } else {
            DeleteDC(memDc);
            memDc = nullptr;
        }
    }

    RECT rc = buffered ? RECT{ 0, 0, targetWidth, targetHeight } : rcStrip;
    FillSolidRect(drawDc, rc, g_theme.card);

    const int width = rc.right - rc.left;
    constexpr int kSegments = 18;
    const int segGap = std::max(1, S(1));
    const int litSegments = std::clamp(static_cast<int>(std::floor(dangerUnit * static_cast<float>(kSegments) + 0.5f)), 0, kSegments);
    const COLORREF offColor = g_theme.clipOff;
    const COLORREF clipColor = g_theme.clipPulse;

    for (int i = 0; i < kSegments; ++i) {
        const int segLeft = rc.left + ((i * width) / kSegments);
        const int segRightFull = rc.left + (((i + 1) * width) / kSegments);
        int segRight = segRightFull;
        if (i < (kSegments - 1)) {
            segRight = std::max(segLeft + 1, segRightFull - segGap);
        }
        RECT seg{ segLeft, rc.top + S(2), segRight, rc.bottom - S(2) };
        float t = static_cast<float>(i) / static_cast<float>(kSegments - 1);
        COLORREF onColor = RGB(
            static_cast<BYTE>(84 + (154.0f * t)),
            static_cast<BYTE>(176 - (126.0f * t)),
            static_cast<BYTE>(86 - (40.0f * t)));
        COLORREF color = (i < litSegments) ? onColor : offColor;
        FillSolidRect(drawDc, seg, color);
    }

    HPEN border = CreatePen(PS_SOLID, 1, clipRecent ? g_theme.clipPulse : g_theme.clipBorder);
    HGDIOBJ oldPen = SelectObject(drawDc, border);
    HGDIOBJ oldBrush = SelectObject(drawDc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(drawDc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(drawDc, oldBrush);
    SelectObject(drawDc, oldPen);
    DeleteObject(border);

    SetBkMode(drawDc, TRANSPARENT);
    SetTextColor(drawDc, clipRecent ? g_theme.clipTextHot : g_theme.clipText);
    SelectObject(drawDc, g_fontTiny ? g_fontTiny : (g_fontHint ? g_fontHint : g_fontSmall));
    RECT labelRc{ rc.left + S(5), rc.top, rc.right - S(52), rc.bottom };
    DrawTextW(drawDc, L"Clip Meter", -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

    RECT inner{ rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1 };
    RECT pulse{};
    const int pulseWidth = S(44);
    if (clipRecent) {
        pulse = { std::max(inner.left, inner.right - pulseWidth), inner.top, inner.right, inner.bottom };
        FillSolidRect(drawDc, pulse, clipColor);
    }

    if (clipRecent) {
        SetTextColor(drawDc, RGB(255, 255, 255));
        SelectObject(drawDc, g_fontTiny ? g_fontTiny : (g_fontHint ? g_fontHint : g_fontSmall));
        const wchar_t* tag = L"CLIP";
        DrawTextW(drawDc, tag, -1, &pulse, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    if (buffered) {
        BitBlt(
            hdc,
            rcStrip.left,
            rcStrip.top,
            targetWidth,
            targetHeight,
            memDc,
            0,
            0,
            SRCCOPY);
        SelectObject(memDc, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDc);
    }
}

std::wstring GetComboItemText(const DRAWITEMSTRUCT* dis) {
    std::wstring itemText;
    const LRESULT textLen = SendMessageW(dis->hwndItem, CB_GETLBTEXTLEN, dis->itemID, 0);
    if (textLen > 0 && textLen != CB_ERR) {
        itemText.resize(static_cast<size_t>(textLen) + 1U);
        const LRESULT copied = SendMessageW(
            dis->hwndItem,
            CB_GETLBTEXT,
            dis->itemID,
            reinterpret_cast<LPARAM>(itemText.data()));
        if (copied != CB_ERR && copied >= 0) {
            itemText.resize(static_cast<size_t>(copied));
        } else {
            itemText.clear();
        }
    }
    return itemText;
}

// Returns true when the item is highlighted inside the open drop-down list.
bool PaintComboItemBackground(const DRAWITEMSTRUCT* dis) {
    const bool selected = (dis->itemState & ODS_SELECTED) != 0;
    const bool editField = (dis->itemState & ODS_COMBOBOXEDIT) != 0;
    const bool highlight = selected && !editField;
    FillSolidRect(dis->hDC, dis->rcItem, g_theme.inputBg);
    if (highlight) {
        RECT pill = dis->rcItem;
        InflateRect(&pill, -S(3), -S(1));
        DrawRoundRect(dis->hDC, pill, S(5), Filled(g_theme.selectionBg));
    }
    return highlight;
}

void DrawSourceComboItem(const DRAWITEMSTRUCT* dis) {
    if (!dis || dis->itemID == static_cast<UINT>(-1)) {
        return;
    }
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const std::wstring itemText = GetComboItemText(dis);

    RECT rc = dis->rcItem;
    const bool highlight = PaintComboItemBackground(dis);

    const LRESULT itemData = SendMessageW(dis->hwndItem, CB_GETITEMDATA, dis->itemID, 0);
    const bool isSelectableSource = !(itemData < 0 || itemData == CB_ERR || static_cast<size_t>(itemData) >= g_sourceChoices.size());

    HICON icon = nullptr;
    if (isSelectableSource) {
        const SourceChoice& choice = g_sourceChoices[static_cast<size_t>(itemData)];
        if (choice.type == SourceChoiceType::Loopback) {
            icon = g_loopbackFallbackIcon;
        } else {
            EnsureAppIconForPid(choice.pid);
            if (auto it = g_appIconsByPid.find(choice.pid); it != g_appIconsByPid.end()) {
                icon = it->second;
            } else {
                icon = g_appFallbackIcon;
            }
        }
    }

    int textLeft = rc.left + S(10);
    const int iconSize = S(kSourceIconSizePx);
    if (icon) {
        const int iconY = rc.top + ((rc.bottom - rc.top - iconSize) / 2);
        DrawIconEx(dis->hDC, rc.left + S(8), iconY, icon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
        textLeft = rc.left + S(8) + iconSize + S(8);
    }

    SetBkMode(dis->hDC, TRANSPARENT);
    HGDIOBJ font = g_fontBody ? static_cast<HGDIOBJ>(g_fontBody) : GetStockObject(DEFAULT_GUI_FONT);
    if (disabled) {
        SetTextColor(dis->hDC, g_theme.disabledText);
    } else if (itemData == kSourceItemHeader) {
        SetTextColor(dis->hDC, g_theme.comboHeaderText);
        if (g_fontSection) font = g_fontSection;
    } else if (itemData == kSourceItemDivider) {
        SetTextColor(dis->hDC, g_theme.comboDividerText);
    } else if (itemData == kSourceItemPlaceholder) {
        SetTextColor(dis->hDC, g_theme.comboPlaceholderText);
    } else {
        SetTextColor(dis->hDC, highlight ? g_theme.selectionText : g_theme.text);
    }
    HGDIOBJ oldFont = SelectObject(dis->hDC, font);

    RECT textRc = rc;
    textRc.left = textLeft;
    textRc.right -= S(6);
    DrawTextW(dis->hDC, itemText.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

void DrawPlainComboItem(const DRAWITEMSTRUCT* dis) {
    if (!dis || dis->itemID == static_cast<UINT>(-1)) {
        return;
    }
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const std::wstring itemText = GetComboItemText(dis);
    const bool highlight = PaintComboItemBackground(dis);

    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, disabled ? g_theme.disabledText : (highlight ? g_theme.selectionText : g_theme.text));
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontBody ? static_cast<HGDIOBJ>(g_fontBody) : GetStockObject(DEFAULT_GUI_FONT));
    RECT textRc = dis->rcItem;
    textRc.left += S(10);
    textRc.right -= S(6);
    DrawTextW(dis->hDC, itemText.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

bool ShouldDrawFocusRing(const DRAWITEMSTRUCT* dis) {
    return (dis->itemState & ODS_FOCUS) != 0 && (dis->itemState & ODS_NOFOCUSRECT) == 0;
}

// Owner-drawn on/off switch (replaces the classic checkbox glyph). Without window text the
// switch sits at the right edge (settings rows); with text it leads the label.
void DrawOwnerCheckbox(const DRAWITEMSTRUCT* dis) {
    if (!dis) {
        return;
    }
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, SurfaceColorForControl(dis->hwndItem));

    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool checked = GetOwnerCheckboxValue(dis->CtlID);
    const bool hover = !disabled && dis->hwndItem == g_hoverControl;

    wchar_t text[256]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    const bool hasText = text[0] != L'\0';

    const int trackW = S(40);
    const int trackH = S(20);
    const int trackTop = rc.top + ((rc.bottom - rc.top - trackH) / 2);
    const int trackLeft = hasText ? rc.left + S(2) : rc.right - S(2) - trackW;
    RECT track{ trackLeft, trackTop, trackLeft + trackW, trackTop + trackH };
    DrawSwitch(dis->hDC, track, checked, hover, disabled);
    if (ShouldDrawFocusRing(dis)) {
        RECT ring = track;
        InflateRect(&ring, S(3), S(3));
        DrawRoundRect(dis->hDC, ring, (trackH / 2) + S(3), Outline(g_theme.text, static_cast<float>(std::max(1, S(1)))));
    }

    if (hasText) {
        RECT textRc = rc;
        textRc.left = track.right + S(10);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, disabled ? g_theme.disabledText : g_theme.text);
        HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontBody);
        DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dis->hDC, oldFont);
    }
}

// Master on/off switch in the status card: "On"/"Off" label followed by the switch.
void DrawPowerToggle(const DRAWITEMSTRUCT* dis, bool on) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, SurfaceColorForControl(dis->hwndItem));
    const bool hover = dis->hwndItem == g_hoverControl;
    const int trackW = S(40);
    const int trackH = S(20);
    RECT track{ rc.right - S(2) - trackW, rc.top + ((rc.bottom - rc.top - trackH) / 2), rc.right - S(2), rc.top + ((rc.bottom - rc.top - trackH) / 2) + trackH };
    DrawSwitch(dis->hDC, track, on, hover, false);
    if (ShouldDrawFocusRing(dis)) {
        RECT ring = track;
        InflateRect(&ring, S(3), S(3));
        DrawRoundRect(dis->hDC, ring, (trackH / 2) + S(3), Outline(g_theme.text, static_cast<float>(std::max(1, S(1)))));
    }
    RECT textRc{ rc.left, rc.top, track.left - S(12), rc.bottom };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, g_theme.text);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontBody);
    DrawTextW(dis->hDC, on ? L"On" : L"Off", -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

Glyph NavGlyph(int page) {
    switch (page) {
    case kPageSource: return Glyph::Source;
    case kPageMix: return Glyph::Mix;
    case kPageHotkeys: return Glyph::Keyboard;
    case kPageAppearance: return Glyph::Contrast;
    default: return Glyph::Pulse;
    }
}

void DrawNavItem(const DRAWITEMSTRUCT* dis) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.bg);
    const int page = static_cast<int>(dis->CtlID) - IDC_NAV_FIRST;
    const bool selected = page == g_page;
    const bool hover = dis->hwndItem == g_hoverControl;
    if (selected || hover) {
        DrawRoundRect(dis->hDC, rc, S(5), Filled(selected ? g_theme.navSelected : g_theme.navHover));
    }
    if (selected) {
        RECT bar{ rc.left, rc.top + S(10), rc.left + S(3), rc.bottom - S(10) };
        DrawRoundRect(dis->hDC, bar, S(2), Filled(g_theme.accent));
    }
    RECT icon{ rc.left + S(14), rc.top + ((rc.bottom - rc.top - S(16)) / 2), rc.left + S(30), rc.top + ((rc.bottom - rc.top - S(16)) / 2) + S(16) };
    DrawGlyph(dis->hDC, NavGlyph(page), icon, g_theme.text, 1.8f);
    wchar_t text[64]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    RECT textRc{ rc.left + S(42), rc.top, rc.right - S(8), rc.bottom };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, g_theme.text);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontBody);
    DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

enum class ButtonKind {
    Primary,
    Secondary,
    Active,
};

ButtonKind ResolveButtonKind(int id) {
    if (id == IDC_MONITOR && MicMixApp::Instance().IsMonitorEnabled()) {
        return ButtonKind::Active;
    }
    if ((id == IDC_MUTE_HOTKEY_SET && g_hotkeyCaptureTarget == HotkeyCaptureTarget::MusicMute) ||
        (id == IDC_MIC_INPUT_HOTKEY_SET && g_hotkeyCaptureTarget == HotkeyCaptureTarget::MicInputMute)) {
        return ButtonKind::Active;
    }
    return ButtonKind::Secondary;
}

void DrawPushButtonEx(const DRAWITEMSTRUCT* dis, ButtonKind kind, Glyph* glyph) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, SurfaceColorForControl(dis->hwndItem));

    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool hover = !disabled && dis->hwndItem == g_hoverControl;
    const int radius = S(5);

    COLORREF fill = g_theme.buttonBg;
    COLORREF border = g_theme.buttonBorder;
    COLORREF textColor = g_theme.buttonText;
    bool hasBorder = true;
    switch (kind) {
    case ButtonKind::Primary:
        fill = pressed ? g_theme.accentPressed : (hover ? g_theme.accentHover : g_theme.accent);
        textColor = g_theme.accentText;
        hasBorder = false;
        break;
    case ButtonKind::Active:
        fill = pressed ? MixColor(g_theme.accentSoft, g_theme.accent, 0.25f)
                       : (hover ? MixColor(g_theme.accentSoft, g_theme.accent, 0.12f) : g_theme.accentSoft);
        border = g_theme.accent;
        textColor = g_theme.dark ? g_theme.accentHover : g_theme.accent;
        break;
    case ButtonKind::Secondary:
    default:
        fill = pressed ? g_theme.buttonPressed : (hover ? g_theme.buttonHover : g_theme.buttonBg);
        break;
    }
    if (disabled) {
        fill = g_theme.buttonDisabledBg;
        border = g_theme.border;
        textColor = g_theme.disabledText;
        hasBorder = true;
    }

    DrawRoundRect(dis->hDC, rc, radius, hasBorder ? FilledBordered(fill, border) : Filled(fill));
    if (ShouldDrawFocusRing(dis)) {
        RECT ring = rc;
        InflateRect(&ring, -S(2), -S(2));
        DrawRoundRect(dis->hDC, ring, std::max(1, radius - S(2)), Outline(kind == ButtonKind::Primary ? g_theme.accentText : g_theme.text, static_cast<float>(std::max(1, S(1)))));
    }

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    RECT textRc = rc;
    if (pressed) {
        OffsetRect(&textRc, 0, S(1));
    }
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, textColor);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontBody);
    if (glyph) {
        SIZE ts{};
        GetTextExtentPoint32W(dis->hDC, text, lstrlenW(text), &ts);
        const int iconSize = S(16);
        const int total = iconSize + S(8) + ts.cx;
        const int left = textRc.left + ((textRc.right - textRc.left - total) / 2);
        RECT icon{ left, textRc.top + ((textRc.bottom - textRc.top - iconSize) / 2), left + iconSize, textRc.top + ((textRc.bottom - textRc.top - iconSize) / 2) + iconSize };
        DrawGlyph(dis->hDC, *glyph, icon, textColor, 1.8f);
        textRc.left = icon.right + S(8);
        DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    } else {
        DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(dis->hDC, oldFont);
}

void DrawPushButton(const DRAWITEMSTRUCT* dis) {
    if (!dis) {
        return;
    }
    if (dis->CtlID == IDC_OPEN_MINI) {
        Glyph g = Glyph::Compact;
        DrawPushButtonEx(dis, ButtonKind::Secondary, &g);
        return;
    }
    DrawPushButtonEx(dis, ResolveButtonKind(static_cast<int>(dis->CtlID)), nullptr);
}

// One segment of the System / Light / Dark selector. Each segment paints the part of the
// shared container that lies inside its own window, so the three buttons read as one control.
void DrawThemeSegment(HWND parent, const DRAWITEMSTRUCT* dis) {
    if (!dis) {
        return;
    }
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.card);

    RECT ctlRc{};
    GetWindowRect(dis->hwndItem, &ctlRc);
    MapWindowPoints(nullptr, parent, reinterpret_cast<LPPOINT>(&ctlRc), 2);
    RECT container = g_rcThemeSegment;
    OffsetRect(&container, -ctlRc.left, -ctlRc.top);
    DrawRoundRect(dis->hDC, container, S(9), FilledBordered(g_theme.buttonBg, g_theme.buttonBorder));

    const int id = static_cast<int>(dis->CtlID);
    const bool selected = ThemeModeForControlId(id) == g_themeMode;
    const bool hover = dis->hwndItem == g_hoverControl;
    RECT pill = rc;
    InflateRect(&pill, -S(3), -S(3));
    COLORREF textColor = g_theme.muted;
    if (selected) {
        DrawRoundRect(dis->hDC, pill, S(6), Filled(g_theme.accent));
        textColor = g_theme.accentText;
    } else if (hover) {
        DrawRoundRect(dis->hDC, pill, S(6), Filled(g_theme.buttonHover));
        textColor = g_theme.text;
    }
    if (ShouldDrawFocusRing(dis)) {
        DrawRoundRect(dis->hDC, pill, S(6), Outline(selected ? g_theme.accentText : g_theme.accent, static_cast<float>(std::max(1, S(1)))));
    }

    wchar_t text[32]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, textColor);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontButton ? g_fontButton : g_fontBody);
    DrawTextW(dis->hDC, text, -1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

// Hover tracking for owner-drawn buttons. Also maps double clicks to plain clicks so quick
// repeated toggling is never swallowed (owner-drawn buttons report BN_DOUBLECLICKED otherwise).
LRESULT CALLBACK ButtonHoverSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (g_hoverControl != hwnd) {
            HWND previous = g_hoverControl;
            g_hoverControl = hwnd;
            if (previous && IsWindow(previous)) {
                InvalidateRect(previous, nullptr, FALSE);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        if (g_hoverControl == hwnd) {
            g_hoverControl = nullptr;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_LBUTTONDBLCLK:
        msg = WM_LBUTTONDOWN;
        break;
    case WM_NCDESTROY:
        if (g_hoverControl == hwnd) {
            g_hoverControl = nullptr;
        }
        RemoveWindowSubclass(hwnd, ButtonHoverSubclassProc, subclassId);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void DrawComboChevron(HDC hdc, const RECT& area, COLORREF color) {
    const int cx = (area.left + area.right) / 2;
    const int cy = (area.top + area.bottom) / 2;
    const int w = S(4);
    const int h = S(2);
    if (!g_gdiplusReady) {
        HPEN pen = CreatePen(PS_SOLID, std::max(1, S(2) - 1), color);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, cx - w, cy - h, nullptr);
        LineTo(hdc, cx, cy + h);
        LineTo(hdc, cx + w + 1, cy - h - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
        return;
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(ToGdiColor(color), static_cast<Gdiplus::REAL>(std::max(1.0f, static_cast<float>(S(3)) * 0.5f)));
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    const Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(static_cast<Gdiplus::REAL>(cx - w), static_cast<Gdiplus::REAL>(cy - h)),
        Gdiplus::PointF(static_cast<Gdiplus::REAL>(cx), static_cast<Gdiplus::REAL>(cy + h)),
        Gdiplus::PointF(static_cast<Gdiplus::REAL>(cx + w), static_cast<Gdiplus::REAL>(cy - h)),
    };
    g.DrawLines(&pen, pts, 3);
}

// Fully custom-painted combobox field (frame, current item and chevron). Native themed
// rendering cannot be made dark reliably on every Windows version, so we own every pixel.
void PaintComboField(HWND combo, HDC hdc) {
    RECT client{};
    GetClientRect(combo, &client);
    const int w = client.right - client.left;
    const int h = client.bottom - client.top;
    if (w <= 0 || h <= 0) {
        return;
    }
    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP memBmp = memDc ? CreateCompatibleBitmap(hdc, w, h) : nullptr;
    HDC drawDc = hdc;
    HGDIOBJ oldBmp = nullptr;
    if (memDc && memBmp) {
        oldBmp = SelectObject(memDc, memBmp);
        drawDc = memDc;
    }

    const bool enabled = IsWindowEnabled(combo) != FALSE;
    const bool focused = GetFocus() == combo;
    const bool dropped = SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) != FALSE;
    const bool hover = enabled && g_hoverControl == combo;

    FillSolidRect(drawDc, client, SurfaceColorForControl(combo));
    COLORREF border = g_theme.buttonBorder;
    if (focused || dropped) {
        border = g_theme.accent;
    } else if (hover) {
        border = MixColor(g_theme.buttonBorder, g_theme.accent, 0.45f);
    }
    DrawRoundRect(drawDc, client, S(5), FilledBordered(enabled ? g_theme.inputBg : g_theme.buttonDisabledBg, border,
                                                       static_cast<float>((focused || dropped) ? std::max(1, S(2) - 1) : 1)));

    const int arrowW = S(28);
    RECT arrowRc{ client.right - arrowW, client.top, client.right, client.bottom };
    DrawComboChevron(drawDc, arrowRc, enabled ? g_theme.muted : g_theme.disabledText);

    const int sel = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (sel >= 0) {
        DRAWITEMSTRUCT dis{};
        dis.CtlType = ODT_COMBOBOX;
        dis.CtlID = static_cast<UINT>(GetDlgCtrlID(combo));
        dis.itemID = static_cast<UINT>(sel);
        dis.itemAction = ODA_DRAWENTIRE;
        dis.itemState = ODS_COMBOBOXEDIT | (enabled ? 0U : static_cast<UINT>(ODS_DISABLED));
        dis.hwndItem = combo;
        dis.hDC = drawDc;
        dis.rcItem = { client.left + S(2), client.top + S(2), arrowRc.left, client.bottom - S(2) };
        dis.itemData = static_cast<ULONG_PTR>(SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(sel), 0));
        if (dis.CtlID == IDC_SOURCE) {
            DrawSourceComboItem(&dis);
        } else {
            DrawPlainComboItem(&dis);
        }
    }

    if (drawDc != hdc) {
        BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
        SelectObject(memDc, oldBmp);
    }
    if (memBmp) DeleteObject(memBmp);
    if (memDc) DeleteDC(memDc);
}

LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintComboField(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        PaintComboField(hwnd, reinterpret_cast<HDC>(wParam));
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_ENABLE:
    case CB_SETCURSEL: {
        const LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        InvalidateRect(hwnd, nullptr, FALSE);
        return result;
    }
    case WM_MOUSEMOVE:
        if (g_hoverControl != hwnd) {
            HWND previous = g_hoverControl;
            g_hoverControl = hwnd;
            if (previous && IsWindow(previous)) {
                InvalidateRect(previous, nullptr, FALSE);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;
    case WM_MOUSELEAVE:
        if (g_hoverControl == hwnd) {
            // The native combobox tracks the mouse itself and can cancel our tracking, which
            // produces a leave while the cursor is still inside. Re-arm instead of repainting.
            POINT pt{};
            RECT rc{};
            GetCursorPos(&pt);
            GetWindowRect(hwnd, &rc);
            if (PtInRect(&rc, pt)) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                break;
            }
            g_hoverControl = nullptr;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_NCDESTROY:
        if (g_hoverControl == hwnd) {
            g_hoverControl = nullptr;
        }
        RemoveWindowSubclass(hwnd, ComboSubclassProc, subclassId);
        break;
    default:
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void InstallButtonHoverTracking(HWND hwnd) {
    EnumChildWindows(hwnd, [](HWND child, LPARAM) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(child, className, static_cast<int>(std::size(className)));
        if (_wcsicmp(className, WC_BUTTONW) == 0) {
            SetWindowSubclass(child, ButtonHoverSubclassProc, 1, 0);
        }
        if (_wcsicmp(className, WC_COMBOBOXW) == 0) {
            SetWindowSubclass(child, ComboSubclassProc, 2, 0);
        }
        return TRUE;
    }, 0);
}

// ---------------------------------------------------------------------------------------
// Main window (sidebar layout)
// ---------------------------------------------------------------------------------------

const wchar_t* PageTitle(int page) {
    switch (page) {
    case kPageSource: return L"Source";
    case kPageMix: return L"Mix";
    case kPageHotkeys: return L"Hotkeys";
    case kPageAppearance: return L"Appearance";
    default: return L"Diagnostics";
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

void PaintMainWindow(HDC dc, const RECT& clientRc) {
    FillRect(dc, &clientRc, g_brushBg);

    // Content panel with rounded top-left corner, like the Windows 11 settings app.
    RECT panel{ S(kSidebarWidthPx), S(10), clientRc.right + S(20), clientRc.bottom + S(20) };
    DrawRoundRect(dc, panel, S(8), FilledBordered(g_theme.content, g_theme.border));

    // Brand
    RECT logo{ S(20), S(22), S(48), S(50) };
    DrawRoundRect(dc, logo, S(8), Filled(g_theme.accent));
    RECT logoIcon = logo;
    InflateRect(&logoIcon, -S(6), -S(6));
    DrawGlyph(dc, Glyph::Mic, logoIcon, g_theme.accentText, 2.0f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_theme.text);
    HGDIOBJ oldFont = SelectObject(dc, g_fontBrand ? g_fontBrand : g_fontBody);
    RECT brandRc{ S(58), S(22), S(kSidebarWidthPx - 10), S(50) };
    DrawTextW(dc, L"MicMix", -1, &brandRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Page title
    const int cx0 = S(kContentLeftPx);
    const int cx1 = S(kContentRightPx);
    SelectObject(dc, g_fontPageTitle ? g_fontPageTitle : g_fontBody);
    RECT titleRc{ cx0, S(24), cx1, S(66) };
    DrawTextW(dc, PageTitle(g_page), -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    DrawStatusCard(dc);

    for (const auto& card : g_cards) {
        if (card.page == g_page) {
            DrawCard(dc, card.rc);
        }
    }
    const int pad = S(kRowPadPx);
    for (const auto& row : g_rows) {
        if (row.page != g_page) {
            continue;
        }
        const bool hasSubtitle = row.subtitle == nullptr || row.subtitle[0] != L'\0';
        RECT tr{ row.rc.left + pad, row.rc.top + S(13), row.textRight, row.rc.top + S(33) };
        if (!hasSubtitle) {
            tr = { row.rc.left + pad, row.rc.top, row.textRight, row.rc.bottom };
        }
        SetTextColor(dc, g_theme.text);
        SelectObject(dc, g_fontRowTitle ? g_fontRowTitle : g_fontBody);
        DrawTextW(dc, row.title, -1, &tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (row.subtitle && row.subtitle[0] != L'\0') {
            RECT sr{ row.rc.left + pad, row.rc.top + S(33), row.textRight, row.rc.top + S(51) };
            SetTextColor(dc, g_theme.muted);
            SelectObject(dc, g_fontSmall ? g_fontSmall : g_fontBody);
            DrawTextW(dc, row.subtitle, -1, &sr, DT_SINGLELINE | DT_TOP | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        if (row.divider) {
            RECT line{ row.rc.left + 1, row.rc.bottom - 1, row.rc.right - 1, row.rc.bottom };
            FillSolidRect(dc, line, g_theme.divider);
        }
    }

    if (g_page == kPageMix) {
        DrawLevelMeter(dc, g_rcMeter, IsMusicMeterActive(g_lastTelemetry), g_meterVisualDb, g_meterHoldDb);
        DrawLevelMeter(dc, g_rcMicMeter, g_lastTelemetry.micRmsDbfs > -119.0f, g_micMeterVisualDb, g_micMeterHoldDb);
        const float shownMusicPeak = (g_lastTelemetry.musicSendPeakDbfs > -119.0f) ? g_lastTelemetry.musicSendPeakDbfs : g_lastTelemetry.musicPeakDbfs;
        DrawClipStrip(dc, g_rcMusicClip, ClipDangerUnitFromDb(shownMusicPeak), g_lastTelemetry.sourceClipRecent);
        DrawClipStrip(dc, g_rcMicClip, ClipDangerUnitFromDb(g_lastTelemetry.micPeakDbfs), g_lastTelemetry.micClipRecent);
    } else if (g_page == kPageHotkeys) {
        for (const auto& card : g_cards) {
            if (card.page == kPageHotkeys) {
                RECT hint{ card.rc.left + S(2), card.rc.bottom + S(10), card.rc.right, card.rc.bottom + S(30) };
                SetTextColor(dc, g_theme.muted);
                SelectObject(dc, g_fontSmall ? g_fontSmall : g_fontBody);
                DrawTextW(dc, L"Click Set\x2026 and press the key combination. Esc cancels, Del clears.", -1, &hint, DT_SINGLELINE | DT_TOP | DT_NOPREFIX);
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
        mic = L"default microphone";
    }
    const std::wstring route = (src.empty() ? std::wstring(L"No source selected") : src) + L"  \x2192  " + mic;
    switch (g_headerStatusBadgeState) {
    case HeaderStatusBadgeState::Active:
        title = L"MicMix is sending music";
        subtitle = route;
        break;
    case HeaderStatusBadgeState::Muted:
        title = L"Music is muted";
        subtitle = route;
        break;
    case HeaderStatusBadgeState::Error:
        title = L"Source error";
        subtitle = Utf8ToWide(st.message.empty() ? std::string("The source stopped unexpectedly") : st.message);
        break;
    default:
        title = L"MicMix is off";
        subtitle = src.empty() ? L"Choose a source, then switch MicMix on" : L"Ready: " + route;
        break;
    }
    if (title != g_statusTitle || subtitle != g_statusSubtitle) {
        g_statusTitle = title;
        g_statusSubtitle = subtitle;
        InvalidateRect(hwnd, &g_rcHeaderBadge, FALSE);
    }
}

// ---------------------------------------------------------------------------------------
// Mini mode window
// ---------------------------------------------------------------------------------------

int g_miniLastFlags = -1;

int MiniLitSegments(float db, bool active) {
    if (!active) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround(MeterDbToUnit(db) * static_cast<float>(kMiniMeterSegments))), 0, kMiniMeterSegments);
}

std::wstring MiniHotkeyHint() {
    const bool music = g_muteHotkeyVk != 0;
    const bool mic = g_micInputMuteHotkeyVk != 0;
    if (!music && !mic) {
        return L"Set hotkeys in the main window";
    }
    std::wstring text;
    if (music) {
        text += FormatHotkeyText(g_muteHotkeyModifiers, g_muteHotkeyVk) + L" music";
    }
    if (mic) {
        if (!text.empty()) text += L"  \xB7  ";
        text += FormatHotkeyText(g_micInputMuteHotkeyModifiers, g_micInputMuteHotkeyVk) + L" mic";
    }
    return text;
}

// Repaints the parts of the mini window whose state changed. `force` repaints buttons too.
void RefreshMini(bool force) {
    HWND mini = g_miniHwnd;
    HWND main = g_hwnd.load(std::memory_order_acquire);
    if (!mini || !main || !IsWindowVisible(mini)) {
        return;
    }
    const int musicLit = MiniLitSegments(g_meterVisualDb, IsMusicMeterActive(g_lastTelemetry));
    const int micLit = MiniLitSegments(g_micMeterVisualDb, g_lastTelemetry.micRmsDbfs > -119.0f);
    if (musicLit != g_miniMusicLit) {
        g_miniMusicLit = musicLit;
        InvalidateRect(mini, &g_rcMiniMusicMeter, FALSE);
    }
    if (micLit != g_miniMicLit) {
        g_miniMicLit = micLit;
        InvalidateRect(mini, &g_rcMiniMicMeter, FALSE);
    }
    const bool power = IsSourceStateActive(MicMixApp::Instance().GetSourceStatus().state);
    const int flags = (power ? 1 : 0) |
        (GetOwnerCheckboxValue(IDC_MUTE) ? 2 : 0) |
        (GetOwnerCheckboxValue(IDC_MIC_INPUT_MUTE) ? 4 : 0) |
        (IsWindowEnabled(GetDlgItem(main, IDC_MUTE)) ? 8 : 0) |
        (IsWindowEnabled(GetDlgItem(main, IDC_MIC_INPUT_MUTE)) ? 16 : 0);
    if (force || flags != g_miniLastFlags) {
        g_miniLastFlags = flags;
        EnableWindow(GetDlgItem(mini, IDC_MINI_MUTE_MUSIC), (flags & 8) ? TRUE : FALSE);
        EnableWindow(GetDlgItem(mini, IDC_MINI_MUTE_MIC), (flags & 16) ? TRUE : FALSE);
        InvalidateRect(GetDlgItem(mini, IDC_MINI_POWER), nullptr, FALSE);
        InvalidateRect(GetDlgItem(mini, IDC_MINI_MUTE_MUSIC), nullptr, FALSE);
        InvalidateRect(GetDlgItem(mini, IDC_MINI_MUTE_MIC), nullptr, FALSE);
    }
    if (force) {
        InvalidateRect(mini, &g_rcMiniHint, FALSE);
    }
}

void SyncMiniFromMain() {
    HWND mini = g_miniHwnd;
    HWND main = g_hwnd.load(std::memory_order_acquire);
    if (!mini || !main) {
        return;
    }
    HWND mainGain = GetDlgItem(main, IDC_GAIN);
    HWND miniGain = GetDlgItem(mini, IDC_MINI_GAIN);
    const LRESULT pos = SendMessageW(mainGain, TBM_GETPOS, 0, 0);
    if (SendMessageW(miniGain, TBM_GETPOS, 0, 0) != pos) {
        SendMessageW(miniGain, TBM_SETPOS, TRUE, pos);
        InvalidateRect(miniGain, nullptr, FALSE);
    }
    InvalidateRect(mini, &g_rcMiniGainText, FALSE);
    std::wstring src = GetComboSelectedText(GetDlgItem(main, IDC_SOURCE));
    if (src.empty()) {
        src = L"Choose source";
    }
    SetWindowTextW(GetDlgItem(mini, IDC_MINI_SOURCE), src.c_str());
    InvalidateRect(GetDlgItem(mini, IDC_MINI_SOURCE), nullptr, FALSE);
    RefreshMini(true);
}

void DrawSegmentMeter(HDC dc, const RECT& rc, int lit) {
    const int w = rc.right - rc.left;
    const int gap = std::max(1, S(2));
    for (int i = 0; i < kMiniMeterSegments; ++i) {
        const int l = rc.left + ((i * w) / kMiniMeterSegments);
        const int r = rc.left + (((i + 1) * w) / kMiniMeterSegments) - (i < kMiniMeterSegments - 1 ? gap : 0);
        const bool on = i < lit;
        COLORREF c = g_theme.meterBarBg;
        if (on) {
            c = i >= 21 ? g_theme.meterRed : (i >= 16 ? g_theme.meterYellow : g_theme.accent);
        }
        RECT seg{ l, rc.top, std::max(l + 1, r), rc.bottom };
        DrawRoundRect(dc, seg, S(1), Filled(c));
    }
}

void PaintMiniWindow(HDC dc, const RECT& client) {
    FillSolidRect(dc, client, g_theme.content);
    // 1px frame (Windows 10 draws no border around popups).
    HPEN pen = CreatePen(PS_SOLID, 1, g_theme.border);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, client.left, client.top, client.right, client.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_theme.text);
    HGDIOBJ oldFont = SelectObject(dc, g_fontBrand ? g_fontBrand : g_fontBody);
    RECT brand{ S(18), S(15), S(120), S(45) };
    DrawTextW(dc, L"MicMix", -1, &brand, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SIZE bs{};
    GetTextExtentPoint32W(dc, L"MicMix", 6, &bs);
    SetTextColor(dc, g_theme.muted);
    SelectObject(dc, g_fontSmall ? g_fontSmall : g_fontBody);
    RECT sub{ S(18) + bs.cx + S(8), S(15), S(230), S(45) };
    DrawTextW(dc, L"TeamSpeak", -1, &sub, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    DrawRoundRect(dc, g_rcMiniPanel, S(12), FilledBordered(g_theme.card, g_theme.border));
    SetTextColor(dc, g_theme.muted);
    DrawTextW(dc, L"Music volume", -1, &g_rcMiniGainText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    HWND main = g_hwnd.load(std::memory_order_acquire);
    if (main) {
        wchar_t gainText[32]{};
        GetWindowTextW(GetDlgItem(main, IDC_GAIN_VALUE), gainText, static_cast<int>(std::size(gainText)));
        SetTextColor(dc, g_theme.text);
        DrawTextW(dc, gainText, -1, &g_rcMiniGainText, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
    }
    SetTextColor(dc, g_theme.muted);
    RECT ml{ g_rcMiniPanel.left + S(14), g_rcMiniMusicMeter.top - S(5), g_rcMiniMusicMeter.left - S(6), g_rcMiniMusicMeter.bottom + S(5) };
    DrawTextW(dc, L"Music", -1, &ml, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    RECT cl{ g_rcMiniPanel.left + S(14), g_rcMiniMicMeter.top - S(5), g_rcMiniMicMeter.left - S(6), g_rcMiniMicMeter.bottom + S(5) };
    DrawTextW(dc, L"Mic", -1, &cl, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    DrawSegmentMeter(dc, g_rcMiniMusicMeter, std::max(0, g_miniMusicLit));
    DrawSegmentMeter(dc, g_rcMiniMicMeter, std::max(0, g_miniMicLit));

    DrawTextW(dc, MiniHotkeyHint().c_str(), -1, &g_rcMiniHint, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

void DrawMiniPowerButton(const DRAWITEMSTRUCT* dis) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.content);
    const bool on = IsSourceStateActive(MicMixApp::Instance().GetSourceStatus().state);
    const bool hover = dis->hwndItem == g_hoverControl;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const int d = std::min(rc.right - rc.left, rc.bottom - rc.top);
    RECT outer{ rc.left + ((rc.right - rc.left - d) / 2), rc.top + ((rc.bottom - rc.top - d) / 2), 0, 0 };
    outer.right = outer.left + d;
    outer.bottom = outer.top + d;
    DrawEllipseAA(dis->hDC, outer, Filled(on ? MixColor(g_theme.accent, g_theme.content, 0.7f) : g_theme.border));
    RECT inner = outer;
    InflateRect(&inner, -S(7), -S(7));
    COLORREF fill = on ? (pressed ? g_theme.accentPressed : (hover ? g_theme.accentHover : g_theme.accent))
                       : (pressed ? g_theme.buttonPressed : (hover ? g_theme.buttonHover : g_theme.card));
    DrawEllipseAA(dis->hDC, inner, Filled(fill));
    if (ShouldDrawFocusRing(dis)) {
        DrawEllipseAA(dis->hDC, outer, Outline(g_theme.text, static_cast<float>(std::max(1, S(1)))));
    }
    const COLORREF fg = on ? g_theme.accentText : g_theme.text;
    const int icon = S(34);
    const int cx = (inner.left + inner.right) / 2;
    const int cy = (inner.top + inner.bottom) / 2;
    RECT iconRc{ cx - (icon / 2), cy - icon + S(4), cx + (icon / 2), cy + S(4) };
    DrawGlyph(dis->hDC, Glyph::Power, iconRc, fg, 2.4f);
    RECT textRc{ inner.left, cy + S(8), inner.right, cy + S(30) };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontButton ? g_fontButton : g_fontBody);
    DrawTextW(dis->hDC, on ? L"LIVE" : L"OFF", -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

void DrawMiniSourceChip(const DRAWITEMSTRUCT* dis) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.content);
    const bool hover = dis->hwndItem == g_hoverControl;
    wchar_t text[256]{};
    GetWindowTextW(dis->hwndItem, text, static_cast<int>(std::size(text)));
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontSmall ? g_fontSmall : g_fontBody);
    SIZE ts{};
    GetTextExtentPoint32W(dis->hDC, text, lstrlenW(text), &ts);
    const int contentW = std::min(static_cast<int>(rc.right - rc.left), S(18) + S(8) + static_cast<int>(ts.cx) + S(8) + S(12) + S(24));
    RECT pill{ rc.left + ((rc.right - rc.left - contentW) / 2), rc.top, 0, rc.bottom };
    pill.right = pill.left + contentW;
    DrawRoundRect(dis->hDC, pill, (rc.bottom - rc.top) / 2, FilledBordered(hover ? g_theme.buttonHover : g_theme.card, g_theme.border));
    RECT iconBox{ pill.left + S(8), rc.top + ((rc.bottom - rc.top - S(18)) / 2), pill.left + S(26), rc.top + ((rc.bottom - rc.top - S(18)) / 2) + S(18) };
    DrawRoundRect(dis->hDC, iconBox, S(5), Filled(g_theme.accentSoft));
    RECT note = iconBox;
    InflateRect(&note, -S(3), -S(3));
    DrawGlyph(dis->hDC, Glyph::Music, note, g_theme.dark ? g_theme.accentHover : g_theme.accent, 2.4f);
    RECT textRc{ iconBox.right + S(8), rc.top, pill.right - S(26), rc.bottom };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, g_theme.text);
    DrawTextW(dis->hDC, text, -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    RECT chevron{ pill.right - S(24), rc.top, pill.right - S(8), rc.bottom };
    DrawComboChevron(dis->hDC, chevron, g_theme.muted);
    SelectObject(dis->hDC, oldFont);
}

void DrawMiniIconButton(const DRAWITEMSTRUCT* dis, Glyph glyph, bool active) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.content);
    const bool hover = dis->hwndItem == g_hoverControl;
    const COLORREF fill = active ? g_theme.accentSoft : (hover ? g_theme.buttonHover : g_theme.card);
    DrawRoundRect(dis->hDC, rc, S(8), FilledBordered(fill, active ? g_theme.accent : g_theme.border));
    RECT icon = rc;
    InflateRect(&icon, -S(8), -S(8));
    DrawGlyph(dis->hDC, glyph, icon, active ? (g_theme.dark ? g_theme.accentHover : g_theme.accent) : g_theme.text, 2.0f);
}

void DrawMiniMuteButton(const DRAWITEMSTRUCT* dis, bool mic) {
    RECT rc = dis->rcItem;
    FillSolidRect(dis->hDC, rc, g_theme.content);
    const bool muted = GetOwnerCheckboxValue(mic ? IDC_MIC_INPUT_MUTE : IDC_MUTE);
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool hover = !disabled && dis->hwndItem == g_hoverControl;
    COLORREF fill = hover ? g_theme.buttonHover : g_theme.card;
    COLORREF border = g_theme.border;
    COLORREF fg = g_theme.text;
    if (muted) {
        fill = hover ? MixColor(g_theme.errBg, g_theme.errFg, 0.1f) : g_theme.errBg;
        border = g_theme.errFg;
        fg = g_theme.errFg;
    }
    if (disabled) {
        fill = g_theme.buttonDisabledBg;
        border = g_theme.border;
        fg = g_theme.disabledText;
    }
    DrawRoundRect(dis->hDC, rc, S(10), FilledBordered(fill, border));
    if (ShouldDrawFocusRing(dis)) {
        RECT ring = rc;
        InflateRect(&ring, -S(3), -S(3));
        DrawRoundRect(dis->hDC, ring, S(8), Outline(g_theme.text, static_cast<float>(std::max(1, S(1)))));
    }
    const wchar_t* label = mic ? (muted ? L"Mic muted" : L"Mute mic") : (muted ? L"Music muted" : L"Mute music");
    const Glyph glyph = mic ? (muted ? Glyph::MicOff : Glyph::Mic) : Glyph::Music;
    HGDIOBJ oldFont = SelectObject(dis->hDC, g_fontButton ? g_fontButton : g_fontBody);
    SIZE ts{};
    GetTextExtentPoint32W(dis->hDC, label, lstrlenW(label), &ts);
    const int iconSize = S(16);
    const int total = iconSize + S(8) + ts.cx;
    const int left = rc.left + ((rc.right - rc.left - total) / 2);
    RECT icon{ left, rc.top + ((rc.bottom - rc.top - iconSize) / 2), left + iconSize, rc.top + ((rc.bottom - rc.top - iconSize) / 2) + iconSize };
    DrawGlyph(dis->hDC, glyph, icon, fg, 2.0f);
    RECT textRc{ icon.right + S(8), rc.top, rc.right, rc.bottom };
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);
    DrawTextW(dis->hDC, label, -1, &textRc, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dis->hDC, oldFont);
}

void ExitMiniMode(int page) {
    HWND main = g_hwnd.load(std::memory_order_acquire);
    g_miniMode.store(false, std::memory_order_release);
    if (g_miniHwnd) {
        ShowWindow(g_miniHwnd, SW_HIDE);
    }
    if (main) {
        if (page >= 0) {
            ShowPage(main, page, true);
        }
        ShowWindow(main, SW_SHOW);
        SyncDpiToWindow(main);
        SetForegroundWindow(main);
        ApplyLiveSettings(main, false);
    }
}

void ToggleMainMute(bool mic) {
    HWND main = g_hwnd.load(std::memory_order_acquire);
    const int id = mic ? IDC_MIC_INPUT_MUTE : IDC_MUTE;
    if (!main || IsWindowEnabled(GetDlgItem(main, id)) == FALSE) {
        return;
    }
    SetOwnerCheckboxValue(id, !GetOwnerCheckboxValue(id));
    InvalidateRect(GetDlgItem(main, id), nullptr, TRUE);
    ApplyLiveSettings(main, false);
    RefreshMini(true);
}

LRESULT CALLBACK MiniWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        const auto make = [&](const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
            return CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
        };
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 240, 14, 30, 30, IDC_MINI_PIN);
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 276, 14, 30, 30, IDC_MINI_EXPAND);
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 312, 14, 30, 30, IDC_MINI_CLOSE);
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 114, 58, 132, 132, IDC_MINI_POWER);
        make(L"BUTTON", L"Choose source", WS_TABSTOP | BS_OWNERDRAW, 40, 202, 280, 30, IDC_MINI_SOURCE);
        g_rcMiniPanel = { S(18), S(248), S(342), S(370) };
        g_rcMiniGainText = { S(32), S(258), S(328), S(278) };
        HWND gain = make(TRACKBAR_CLASSW, L"", WS_TABSTOP, 26, 280, 308, 28, IDC_MINI_GAIN);
        SendMessageW(gain, TBM_SETRANGE, TRUE, MAKELONG(0, kMusicGainSliderMax));
        SendMessageW(gain, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(gain, TBM_SETLINESIZE, 0, 1);
        g_rcMiniMusicMeter = { S(78), S(318), S(328), S(326) };
        g_rcMiniMicMeter = { S(78), S(342), S(328), S(350) };
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 18, 386, 157, 48, IDC_MINI_MUTE_MUSIC);
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 185, 386, 157, 48, IDC_MINI_MUTE_MIC);
        g_rcMiniHint = { S(18), S(444), S(342), S(466) };
        ApplyControlTheme(hwnd);
        InstallButtonHoverTracking(hwnd);
        ApplyMiniFonts(hwnd);
        SendMessageW(hwnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
        return 0;
    }
    case WM_DPICHANGED:
        ApplyDpi(hwnd, HIWORD(wParam), reinterpret_cast<const RECT*>(lParam));
        return 0;
    case WM_EXITSIZEMOVE: {
        // Remember where the user dragged the mini window.
        RECT rc{};
        GetWindowRect(hwnd, &rc);
        g_miniPosX = rc.left;
        g_miniPosY = rc.top;
        HWND main = g_hwnd.load(std::memory_order_acquire);
        if (main) {
            ApplyLiveSettings(main, false, false);
        }
        return 0;
    }
    case WM_NCHITTEST: {
        const LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
        // Borderless window: drag it by any empty area.
        return hit == HTCLIENT ? HTCAPTION : hit;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, IDC_MINI_GAIN)) {
            HWND main = g_hwnd.load(std::memory_order_acquire);
            if (main) {
                const LRESULT pos = SendMessageW(reinterpret_cast<HWND>(lParam), TBM_GETPOS, 0, 0);
                HWND mainGain = GetDlgItem(main, IDC_GAIN);
                SendMessageW(mainGain, TBM_SETPOS, TRUE, pos);
                InvalidateRect(mainGain, nullptr, FALSE);
                UpdateGainLabel(main);
                ApplyLiveSettings(main, false, false);
            }
            InvalidateRect(reinterpret_cast<HWND>(lParam), nullptr, FALSE);
            InvalidateRect(hwnd, &g_rcMiniGainText, FALSE);
        }
        return 0;
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr && hdr->idFrom == IDC_MINI_GAIN && hdr->code == NM_CUSTOMDRAW) {
            return DrawGainSliderCustom(reinterpret_cast<NMCUSTOMDRAW*>(lParam));
        }
        return 0;
    }
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) {
            return FALSE;
        }
        switch (dis->CtlID) {
        case IDC_MINI_POWER: DrawMiniPowerButton(dis); return TRUE;
        case IDC_MINI_SOURCE: DrawMiniSourceChip(dis); return TRUE;
        case IDC_MINI_PIN: DrawMiniIconButton(dis, Glyph::Pin, g_miniPinned); return TRUE;
        case IDC_MINI_EXPAND: DrawMiniIconButton(dis, Glyph::Expand, false); return TRUE;
        case IDC_MINI_CLOSE: DrawMiniIconButton(dis, Glyph::Close, false); return TRUE;
        case IDC_MINI_MUTE_MUSIC: DrawMiniMuteButton(dis, false); return TRUE;
        case IDC_MINI_MUTE_MIC: DrawMiniMuteButton(dis, true); return TRUE;
        default: return FALSE;
        }
    }
    case WM_COMMAND: {
        if (HIWORD(wParam) != BN_CLICKED) {
            return 0;
        }
        HWND main = g_hwnd.load(std::memory_order_acquire);
        switch (LOWORD(wParam)) {
        case IDC_MINI_POWER:
            if (main) TogglePower(main);
            return 0;
        case IDC_MINI_SOURCE:
            ExitMiniMode(kPageSource);
            return 0;
        case IDC_MINI_EXPAND:
            ExitMiniMode(-1);
            return 0;
        case IDC_MINI_CLOSE:
            if (main) PostMessageW(main, WM_CLOSE, 0, 0);
            return 0;
        case IDC_MINI_PIN:
            g_miniPinned = !g_miniPinned;
            SetWindowPos(hwnd, g_miniPinned ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            InvalidateRect(GetDlgItem(hwnd, IDC_MINI_PIN), nullptr, FALSE);
            if (main) ApplyLiveSettings(main, false);
            return 0;
        case IDC_MINI_MUTE_MUSIC:
            ToggleMainMute(false);
            return 0;
        case IDC_MINI_MUTE_MIC:
            ToggleMainMute(true);
            return 0;
        default:
            return 0;
        }
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HWND ctl = reinterpret_cast<HWND>(lParam);
        const COLORREF surface = SurfaceColorForControl(ctl);
        SetBkColor(reinterpret_cast<HDC>(wParam), surface);
        return reinterpret_cast<INT_PTR>(SurfaceBrushForColor(surface));
    }
    case WM_SETCURSOR: {
        const HWND target = reinterpret_cast<HWND>(wParam);
        if (target && target != hwnd && LOWORD(lParam) == HTCLIENT && IsHandCursorControlId(GetDlgCtrlID(target))) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client{};
        GetClientRect(hwnd, &client);
        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP bmp = memDc ? CreateCompatibleBitmap(hdc, client.right, client.bottom) : nullptr;
        if (memDc && bmp) {
            HGDIOBJ old = SelectObject(memDc, bmp);
            PaintMiniWindow(memDc, client);
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                   memDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            SelectObject(memDc, old);
        } else {
            PaintMiniWindow(hdc, client);
        }
        if (bmp) DeleteObject(bmp);
        if (memDc) DeleteDC(memDc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        ExitMiniMode(-1);
        return 0;
    case WM_DESTROY:
        if (g_miniHwnd == hwnd) {
            g_miniHwnd = nullptr;
        }
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

HWND CreateMiniWindow(HWND main) {
    HINSTANCE hInst = GetPluginModuleHandle();
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = MiniWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"MicMixMiniWindow";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON_SMALL));
        UnregisterClassW(wc.lpszClassName, hInst);
        registered = RegisterClassExW(&wc) != 0;
    }
    RECT mainRc{};
    GetWindowRect(main, &mainRc);
    const int w = S(kMiniClientWidthPx);
    const int h = S(kMiniClientHeightPx);
    int x = mainRc.right - w;
    int y = mainRc.top;
    // Restore the saved position only while it is still on a connected monitor.
    if (g_miniPosX != kUiPosUnset && g_miniPosY != kUiPosUnset) {
        const POINT probe{ g_miniPosX + S(40), g_miniPosY + S(20) };
        if (MonitorFromPoint(probe, MONITOR_DEFAULTTONULL) != nullptr) {
            x = g_miniPosX;
            y = g_miniPosY;
        }
    }
    const DWORD exStyle = WS_EX_APPWINDOW | (g_miniPinned ? WS_EX_TOPMOST : 0);
    HWND mini = CreateWindowExW(exStyle, L"MicMixMiniWindow", L"MicMix", kMiniWindowStyle,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (mini) {
        ApplyWindowFrameTheme(mini);
    }
    return mini;
}

void EnterMiniMode(HWND main) {
    if (!g_miniHwnd) {
        g_miniHwnd = CreateMiniWindow(main);
        if (!g_miniHwnd) {
            return;
        }
    }
    g_miniMode.store(true, std::memory_order_release);
    g_miniMusicLit = -1;
    g_miniMicLit = -1;
    ShowWindow(main, SW_HIDE);
    ShowWindow(g_miniHwnd, SW_SHOW);
    SyncDpiToWindow(g_miniHwnd);
    SetForegroundWindow(g_miniHwnd);
    SyncMiniFromMain();
    ApplyLiveSettings(main, false);
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
        g_statusTitle = L"MicMix is off";
        g_statusSubtitle = L"Choose a source, then switch MicMix on";
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
        const auto addRow = [&](int page, int h, const wchar_t* title, const wchar_t* subtitle, int textRight = 0) -> RECT {
            if (lastRow >= 0) {
                g_rows[static_cast<size_t>(lastRow)].divider = true;
            }
            RowDef row;
            row.page = page;
            row.rc = { cx0, y, cx1, y + S(h) };
            row.title = title;
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
        const wchar_t* navNames[kPageCount] = { L"Source", L"Mix", L"Hotkeys", L"Appearance", L"Diagnostics" };
        for (int i = 0; i < kPageCount; ++i) {
            make(L"BUTTON", navNames[i], WS_TABSTOP | BS_OWNERDRAW, S(10), S(72) + (i * S(42)), S(kSidebarWidthPx - 20), S(38), IDC_NAV_FIRST + i, -1);
        }
        make(L"BUTTON", L"Mini mode", WS_TABSTOP | BS_OWNERDRAW, S(14), clientH - S(112), S(kSidebarWidthPx - 28), S(32), IDC_OPEN_MINI, -1);
        const std::wstring versionText = Utf8ToWide(std::string("v") + MICMIX_VERSION + "  \xC2\xB7  by dabinuss");
        make(L"STATIC", versionText.c_str(), 0, S(20), clientH - S(64), S(kSidebarWidthPx - 30), S(18), IDC_VERSION, -1);
        make(L"STATIC", L"github.com/dabinuss/MicMix", SS_NOTIFY, S(20), clientH - S(44), S(kSidebarWidthPx - 30), S(18), IDC_REPO_LINK, -1);

        // Status card (all pages) ---------------------------------------------------
        g_rcHeaderBadge = { cx0, S(78), cx1, S(150) };
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(g_rcHeaderBadge, S(30)), S(110), S(30), IDC_POWER, -1);

        // Source page -----------------------------------------------------------------
        const int fieldX = cx0 + S(250);
        beginCard(S(166));
        RECT r = addRow(kPageSource, 64, L"Audio source", L"App or output device to mix in", fieldX - S(12));
        const int refreshW = S(90);
        HWND source = make(L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, fieldX, centerY(r, S(30)), cx1 - pad - refreshW - S(8) - fieldX, S(360), IDC_SOURCE, kPageSource);
        SendMessageW(source, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
        SendMessageW(source, CB_SETITEMHEIGHT, 0, S(24));
        SendMessageW(source, CB_SETMINVISIBLE, 14, 0);
        make(L"BUTTON", L"Refresh", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - refreshW, centerY(r, S(32)), refreshW, S(32), IDC_RESCAN, kPageSource);
        r = addRow(kPageSource, 64, L"Microphone", L"Device the music is mixed into", fieldX - S(12));
        HWND mic = make(L"COMBOBOX", L"", WS_VSCROLL | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, fieldX, centerY(r, S(30)), cx1 - pad - fieldX, S(260), IDC_MIC_DEVICE, kPageSource);
        SendMessageW(mic, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), S(24));
        SendMessageW(mic, CB_SETITEMHEIGHT, 0, S(24));
        SendMessageW(mic, CB_SETMINVISIBLE, 12, 0);
        r = addRow(kPageSource, 64, L"Start with TeamSpeak", L"Enable MicMix automatically when TeamSpeak starts");
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(48), centerY(r, S(30)), S(48), S(30), IDC_AUTOSTART, kPageSource);
        r = addRow(kPageSource, 64, L"Restart source", L"Reconnect the selected source after device changes");
        make(L"BUTTON", L"Restart", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(r, S(32)), S(110), S(32), IDC_SAVE, kPageSource);
        endCard(kPageSource);

        // Mix page --------------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageMix, 72, L"Music volume", L"Max \x2212" L"2 dB to avoid clipping", fieldX - S(12));
        HWND gain = make(TRACKBAR_CLASSW, L"", WS_CLIPSIBLINGS | WS_TABSTOP, fieldX, centerY(r, S(30)), cx1 - pad - S(90) - fieldX, S(30), IDC_GAIN, kPageMix);
        SendMessageW(gain, TBM_SETRANGE, TRUE, MAKELONG(0, kMusicGainSliderMax));
        SendMessageW(gain, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(gain, TBM_SETLINESIZE, 0, 1);
        SendMessageW(gain, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);
        make(L"STATIC", L"-15.0 dB", SS_RIGHT, cx1 - pad - S(80), centerY(r, S(20)), S(80), S(20), IDC_GAIN_VALUE, kPageMix);

        const int meterX = cx0 + S(200);
        const int meterTextW = S(128);
        const int meterRight = cx1 - pad - meterTextW - S(10);
        r = addRow(kPageMix, 76, L"Music level", L"Sent level and clipping", meterX - S(12));
        g_rcMeter = { meterX, r.top + S(14), meterRight, r.top + S(40) };
        g_rcMusicClip = { meterX, r.top + S(46), meterRight, r.top + S(58) };
        make(L"STATIC", L"No signal", SS_ENDELLIPSIS, cx1 - pad - meterTextW, r.top + S(17), meterTextW, S(18), IDC_METER_TEXT, kPageMix);
        make(L"STATIC", L"CLIP Events: 0", 0, cx1 - pad - meterTextW, r.top + S(44), meterTextW, S(16), IDC_MUSIC_CLIP_EVENTS, kPageMix);
        r = addRow(kPageMix, 76, L"Mic level", L"Only while connected", meterX - S(12));
        g_rcMicMeter = { meterX, r.top + S(14), meterRight, r.top + S(40) };
        g_rcMicClip = { meterX, r.top + S(46), meterRight, r.top + S(58) };
        make(L"STATIC", L"No signal", SS_ENDELLIPSIS, cx1 - pad - meterTextW, r.top + S(17), meterTextW, S(18), IDC_MIC_METER_TEXT, kPageMix);
        make(L"STATIC", L"CLIP Events: 0", 0, cx1 - pad - meterTextW, r.top + S(44), meterTextW, S(16), IDC_MIC_CLIP_EVENTS, kPageMix);
        r = addRow(kPageMix, 64, L"Send music when mic is silent", L"Music keeps sending while you are not speaking");
        make(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(48), centerY(r, S(30)), S(48), S(30), IDC_FORCE_TX, kPageMix);
        endCard(kPageMix);
        make(L"BUTTON", L"Monitor Mix: Off", WS_TABSTOP | BS_OWNERDRAW, cx0, y, S(150), S(32), IDC_MONITOR, kPageMix);
        make(L"BUTTON", L"Mute mic", WS_TABSTOP | BS_OWNERDRAW, cx1 - S(120), y, S(120), S(32), IDC_MIC_INPUT_MUTE, kPageMix);
        make(L"BUTTON", L"Mute music", WS_TABSTOP | BS_OWNERDRAW, cx1 - S(120) - S(16) - S(130), y, S(130), S(32), IDC_MUTE, kPageMix);

        // Hotkeys page ----------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageHotkeys, 64, L"Mute music", nullptr);
        make(L"STATIC", L"Current: Not set", SS_ENDELLIPSIS, r.left + pad, r.top + S(33), S(380), S(18), IDC_MUTE_HOTKEY_TEXT, kPageHotkeys);
        make(L"BUTTON", L"Set\x2026", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(r, S(32)), S(110), S(32), IDC_MUTE_HOTKEY_SET, kPageHotkeys);
        r = addRow(kPageHotkeys, 64, L"Mute mic input", nullptr);
        make(L"STATIC", L"Current: Not set", SS_ENDELLIPSIS, r.left + pad, r.top + S(33), S(380), S(18), IDC_MIC_INPUT_HOTKEY_TEXT, kPageHotkeys);
        make(L"BUTTON", L"Set\x2026", WS_TABSTOP | BS_OWNERDRAW, cx1 - pad - S(110), centerY(r, S(32)), S(110), S(32), IDC_MIC_INPUT_HOTKEY_SET, kPageHotkeys);
        endCard(kPageHotkeys);

        // Appearance page -------------------------------------------------------------
        beginCard(S(166));
        const int themeSegW = S(70);
        r = addRow(kPageAppearance, 72, L"App theme", L"System follows your Windows app mode", cx1 - pad - (themeSegW * 3) - S(12));
        const int themeSegH = S(32);
        const int themeSegX = cx1 - pad - (themeSegW * 3);
        const int themeSegY = centerY(r, themeSegH);
        g_rcThemeSegment = { themeSegX, themeSegY, themeSegX + (themeSegW * 3), themeSegY + themeSegH };
        make(L"BUTTON", L"System", WS_TABSTOP | BS_OWNERDRAW, themeSegX, themeSegY, themeSegW, themeSegH, IDC_THEME_SYSTEM, kPageAppearance);
        make(L"BUTTON", L"Light", WS_TABSTOP | BS_OWNERDRAW, themeSegX + themeSegW, themeSegY, themeSegW, themeSegH, IDC_THEME_LIGHT, kPageAppearance);
        make(L"BUTTON", L"Dark", WS_TABSTOP | BS_OWNERDRAW, themeSegX + (themeSegW * 2), themeSegY, themeSegW, themeSegH, IDC_THEME_DARK, kPageAppearance);
        endCard(kPageAppearance);

        // Diagnostics page ------------------------------------------------------------
        beginCard(S(166));
        r = addRow(kPageDiagnostics, 132, L"Live status", nullptr);
        g_rcStatus = { r.left + pad, r.top + S(42), r.right - pad, r.bottom - S(14) };
        endCard(kPageDiagnostics);

        ApplyWindowFrameTheme(hwnd);
        ApplyControlTheme(hwnd);
        InstallButtonHoverTracking(hwnd);
        ApplyFonts(hwnd);
        ShowPage(hwnd, g_page, false);
        PopulateCombos(hwnd);
        LoadSettings(hwnd);
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
        if (mis->CtlID == IDC_SOURCE || mis->CtlID == IDC_MIC_DEVICE) {
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
        if (dis->CtlID == IDC_MIC_DEVICE) {
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
        if (dis->CtlID == IDC_AUTOSTART || dis->CtlID == IDC_FORCE_TX ||
            dis->CtlID == IDC_MIC_INPUT_MUTE || dis->CtlID == IDC_MUTE) {
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
            SetStatusText(hwnd, L"Refreshing source list...");
            if (!StartSourceRefreshWorker(hwnd, nextSeq)) {
                g_sourceRefreshInFlight.store(false, std::memory_order_release);
                g_sourceRefreshActiveSeq.store(0, std::memory_order_release);
                HWND refreshBtn = GetDlgItem(hwnd, IDC_RESCAN);
                if (refreshBtn) {
                    EnableWindow(refreshBtn, TRUE);
                }
                SetStatusText(hwnd, L"Refresh failed to start");
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
                SetStatusText(hwnd, L"Hotkey conflict: already assigned to Mic Input Mute.");
                UpdateHotkeyLabels(hwnd);
                return 0;
            }
            if (g_hotkeyCaptureTarget == HotkeyCaptureTarget::MicInputMute &&
                IsHotkeyConflict(mods, vk, g_muteHotkeyModifiers, g_muteHotkeyVk)) {
                EndHotkeyCapture(hwnd, false);
                MessageBeep(MB_ICONWARNING);
                SetStatusText(hwnd, L"Hotkey conflict: already assigned to Music Mute.");
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
            SetStatusText(hwnd, L"Hotkey capture cancelled.");
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

        HDC paintDc = hdc;
        HDC memDc = nullptr;
        HBITMAP memBmp = nullptr;
        HGDIOBJ oldBmp = nullptr;
        bool bufferedPaint = false;
        const int clientW = std::max(0, static_cast<int>(clientRc.right - clientRc.left));
        const int clientH = std::max(0, static_cast<int>(clientRc.bottom - clientRc.top));
        if (clientW > 0 && clientH > 0) {
            memDc = CreateCompatibleDC(hdc);
            if (memDc) {
                memBmp = CreateCompatibleBitmap(hdc, clientW, clientH);
                if (memBmp) {
                    oldBmp = SelectObject(memDc, memBmp);
                    paintDc = memDc;
                    bufferedPaint = true;
                } else {
                    DeleteDC(memDc);
                    memDc = nullptr;
                }
            }
        }

        PaintMainWindow(paintDc, clientRc);

        if (bufferedPaint) {
            const int copyW = std::max(0, static_cast<int>(ps.rcPaint.right - ps.rcPaint.left));
            const int copyH = std::max(0, static_cast<int>(ps.rcPaint.bottom - ps.rcPaint.top));
            if (copyW > 0 && copyH > 0) {
                BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, copyW, copyH, memDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            }
            SelectObject(memDc, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDc);
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

} // namespace

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
