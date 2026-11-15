#pragma once

// Internal declarations shared by the settings UI translation units
// (settings_window.cpp, ui_theme.cpp, ui_draw.cpp, ui_controls.cpp, mini_window.cpp).

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
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// GDI+ expects min/max in its namespace; NOMINMAX hides the macros.
namespace Gdiplus {
using std::max;
using std::min;
}
#include <objidl.h>
#include <gdiplus.h>

#include "i18n.h"
#include "micmix_core.h"
#include "micmix_version.h"
#include "resource.h"

namespace micmix_ui {

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
    IDC_LANGUAGE = 1042,
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
constexpr int kNoText = -1;
constexpr int kTextFromControl = -2; // subtitle is a live STATIC control
struct RowDef {
    int page = 0;
    RECT rc{};
    int title = kNoText;     // Str id
    int subtitle = kNoText;  // Str id, kNoText or kTextFromControl
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


struct BackBuffer {
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    int w = 0;
    int h = 0;
    HDC Acquire(HDC target, int width, int height);
    void Release();
};

struct ShapeStyle {
    COLORREF fill = RGB(0, 0, 0);
    bool hasFill = true;
    COLORREF border = RGB(0, 0, 0);
    bool hasBorder = false;
    float borderWidth = 1.0f;
};

// ---- Shared state (defined in settings_window.cpp unless noted)
extern std::mutex g_mutex;
extern std::mutex g_enumMutex;
extern std::mutex g_sourceRefreshThreadMutex;
extern std::thread g_thread;
extern std::thread g_sourceRefreshThread;
extern std::atomic<bool> g_running;
extern std::atomic<HWND> g_hwnd;
extern std::vector<LoopbackDeviceInfo> g_loopbacks;
extern std::vector<CaptureDeviceInfo> g_captureDevices;
extern std::vector<AppProcessInfo> g_apps;
extern std::vector<SourceChoice> g_sourceChoices;
extern int g_lastValidSourceSel;
extern UiTheme g_theme;
extern UiThemeMode g_themeMode;
extern HWND g_hoverControl;
extern ULONG_PTR g_gdiplusToken;
extern bool g_gdiplusReady;
extern int g_dpi;
extern bool g_loadingUi;
extern int g_hotkeyRefreshTick;
extern HotkeyCaptureTarget g_hotkeyCaptureTarget;
extern UINT g_muteHotkeyModifiers;
extern UINT g_muteHotkeyVk;
extern UINT g_micInputMuteHotkeyModifiers;
extern UINT g_micInputMuteHotkeyVk;
extern std::vector<LoopbackDeviceInfo> g_pendingLoopbacks;
extern std::vector<CaptureDeviceInfo> g_pendingCaptureDevices;
extern std::vector<AppProcessInfo> g_pendingApps;
extern std::atomic_uint64_t g_sourceRefreshSeq;
extern std::atomic_uint64_t g_sourceRefreshActiveSeq;
extern std::atomic<bool> g_sourceRefreshInFlight;
extern std::atomic<bool> g_sourceRefreshPending;
extern std::atomic<bool> g_sourceRefreshPendingReload;
extern std::atomic<bool> g_saveDebouncePending;
extern std::unordered_map<uint32_t, HICON> g_appIconsByPid;
extern HICON g_loopbackFallbackIcon;
extern HICON g_appFallbackIcon;
extern bool g_ownerAutostartChecked;
extern bool g_ownerForceTxChecked;
extern bool g_ownerMicInputMuteChecked;
extern bool g_ownerMuteChecked;
extern HeaderStatusBadgeState g_headerStatusBadgeState;
extern bool g_windowTitleActive;
extern bool g_windowTitleStateValid;
extern HFONT g_fontBody;
extern HFONT g_fontSmall;
extern HFONT g_fontHint;
extern HFONT g_fontTiny;
extern HFONT g_fontClipInfo;
extern HFONT g_fontControlLarge;
extern HFONT g_fontTitle;
extern HFONT g_fontMono;
extern HFONT g_fontButton;
extern HFONT g_fontSection;
extern HBRUSH g_brushBg;
extern HBRUSH g_brushCard;
extern HBRUSH g_brushInput;
extern HBRUSH g_brushContent;
extern HFONT g_fontPageTitle;
extern HFONT g_fontRowTitle;
extern HFONT g_fontBrand;
extern std::vector<RowDef> g_rows;
extern std::vector<CardDef> g_cards;
extern std::vector<PageControl> g_pageControls;
extern int g_page;
extern HWND g_miniHwnd;
extern std::atomic<bool> g_miniMode;
extern bool g_miniPinned;
extern int g_miniMusicLit;
extern int g_miniMicLit;
extern RECT g_rcMiniPanel;
extern RECT g_rcMiniGainText;
extern RECT g_rcMiniMusicMeter;
extern RECT g_rcMiniMicMeter;
extern RECT g_rcMiniHint;
extern std::wstring g_statusTitle;
extern std::wstring g_statusSubtitle;
extern RECT g_rcHeaderBadge;
extern RECT g_rcMeter;
extern RECT g_rcMicMeter;
extern RECT g_rcMusicClip;
extern RECT g_rcMicClip;
extern RECT g_rcStatus;
extern RECT g_rcThemeSegment;
extern float g_meterVisualDb;
extern float g_meterHoldDb;
extern float g_micMeterVisualDb;
extern float g_micMeterHoldDb;
extern int g_lastMusicMeterLevelPx;
extern int g_lastMusicMeterHoldPx;
extern bool g_lastMusicMeterActive;
extern int g_lastMicMeterLevelPx;
extern int g_lastMicMeterHoldPx;
extern bool g_lastMicMeterActive;
extern int g_lastMusicClipLitSegments;
extern bool g_lastMusicClipRecent;
extern int g_lastMicClipLitSegments;
extern bool g_lastMicClipRecent;
extern std::wstring g_lastMusicMeterText;
extern std::wstring g_lastMicMeterText;
extern TelemetrySnapshot g_lastTelemetry;
extern uint64_t g_lastMusicClipEvents;
extern uint64_t g_lastMicClipEvents;
extern std::wstring g_lastStatusText;
extern int g_miniPosX;
extern std::string g_uiLanguage;
extern int g_miniPosY;
extern int g_miniLastFlags;

inline int S(int px) {
    return MulDiv(px, g_dpi, 96);
}

// ---- Functions
UiTheme MakeLightTheme();
UiTheme MakeDarkTheme();
COLORREF MixColor(COLORREF a, COLORREF b, float t);
bool IsOwnerCheckboxControlId(int id);
bool IsThemeSegmentControlId(int id);
bool IsOwnerPushButtonControlId(int id);
bool IsNavControlId(int id);
UiThemeMode ThemeModeForControlId(int id);
bool IsHandCursorControlId(int id);
bool IsPointOverGainThumb(HWND gainHwnd, const POINT& screenPt);
bool GetOwnerCheckboxValue(int id);
void SetOwnerCheckboxValue(int id, bool value);
std::wstring Utf8ToWide(const std::string& text);
void DestroyIconHandle(HICON& icon);
void ClearAppIconCache();
HICON CopySharedIcon(HICON sharedIcon);
HICON LoadStockSmallIcon(SHSTOCKICONID stockId);
bool TryGetProcessImagePath(uint32_t pid, std::wstring& outPath);
HICON TryLoadExeSmallIcon(const std::wstring& exePath);
void EnsureFallbackIcons();
void EnsureAppIconForPid(uint32_t pid);
void PruneAppIconCache(const std::unordered_set<uint32_t>& activePids);
std::wstring ToLowerWide(std::wstring text);
std::wstring NormalizeDeviceName(std::wstring text);
HINSTANCE GetPluginModuleHandle();
bool IsSystemDarkMode();
bool ResolveDarkMode(UiThemeMode mode);
void RebuildThemeBrushes();
void SelectThemePalette(UiThemeMode mode);
void ApplyWindowFrameTheme(HWND hwnd);
void EnsureUiResources();
void ReleaseFonts();
void ReleaseUiResources();
void SetControlFont(HWND hwnd, int id, HFONT font);
void SetStatusText(HWND hwnd, const std::wstring& text);
void SetControlEnabled(HWND hwnd, int controlId, bool enabled);
HeaderStatusBadgeState ResolveHeaderStatusBadgeState(const MicMixSettings& settings, const SourceStatus& status);
void UpdateWindowTitleState(HWND hwnd, bool active);
void UpdateHeaderStatusBadgeState(HWND hwnd, const MicMixSettings& settings, const SourceStatus& status);
int GainDbToSlider(float db);
float SliderToGainDb(int slider);
int MeterDbToPixels(float dbfs, int widthPx);
float MeterDbToUnit(float dbfs);
float ClipDangerUnitFromDb(float dbfs);
int ClipLitSegmentsFromDb(float dbfs);
void SetStaticTextIfChanged(HWND ctl, std::wstring& cache, const std::wstring& nextText);
bool IsMusicMeterActive(const TelemetrySnapshot& t);
void UpdateMusicMeter(HWND hwnd);
void UpdateMicMeter(HWND hwnd);
void UpdateMonitorButton(HWND hwnd);
void UpdateGainLabel(HWND hwnd);
bool IsModifierOnlyKey(UINT vk);
UINT ReadCurrentModifiers();
bool IsExtendedVk(UINT vk);
std::wstring VkToText(UINT vk);
std::wstring FormatHotkeyText(UINT mods, UINT vk);
bool IsHotkeyConflict(UINT lhsMods, UINT lhsVk, UINT rhsMods, UINT rhsVk);
bool IsCapturingHotkey();
void UpdateMuteHotkeyLabel(HWND hwnd);
void UpdateMicInputMuteHotkeyLabel(HWND hwnd);
void UpdateHotkeyLabels(HWND hwnd);
void BeginHotkeyCapture(HWND hwnd, HotkeyCaptureTarget target);
void JoinSourceRefreshThread();
bool StartSourceRefreshWorker(HWND hwnd, uint64_t seq);
void RequestSourceRefresh(HWND hwnd, bool reloadSettings);
void PopulateCombos(HWND hwnd);
void LoadSettings(HWND hwnd);
MicMixSettings CollectSettings(HWND hwnd);
void UpdateStatus(HWND hwnd, bool includeDetails = true);
void FlushDebouncedSettingsSave(HWND hwnd);
void ApplyLiveSettings(HWND hwnd, bool restartSource, bool saveImmediately = true);
void ApplyControlTheme(HWND hwnd);
void ApplyTheme(HWND hwnd);
void SetThemeMode(HWND hwnd, UiThemeMode mode);
void ApplyFonts(HWND hwnd);
Gdiplus::Color ToGdiColor(COLORREF c, BYTE alpha = 255);
void AddRoundRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius);
ShapeStyle Filled(COLORREF fill);
ShapeStyle FilledBordered(COLORREF fill, COLORREF border, float borderWidth = 1.0f);
ShapeStyle Outline(COLORREF border, float borderWidth = 1.0f);
void DrawRoundRect(HDC hdc, const RECT& rc, int radius, const ShapeStyle& style);
void DrawEllipseAA(HDC hdc, const RECT& rc, const ShapeStyle& style);
void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color);
void FrameSolidRect(HDC hdc, const RECT& rc, COLORREF color);
void PaintMainWindow(HDC dc, const RECT& clientRc, const RECT& paintRc);
void DrawCard(HDC hdc, const RECT& rc);
void DrawGlyph(HDC hdc, Glyph glyph, const RECT& box, COLORREF color, float stroke = 1.8f);
COLORREF SurfaceColorForControl(HWND ctl);
HBRUSH SurfaceBrushForColor(COLORREF color);
void DrawSwitch(HDC hdc, const RECT& track, bool on, bool hover, bool disabled);
void DrawStatusCard(HDC hdc);
void DrawLevelMeter(HDC hdc, const RECT& meterRect, bool active, float visualDb, float holdDb);
void EndHotkeyCapture(HWND hwnd, bool refreshLabels = true);
LRESULT DrawGainSliderCustom(const NMCUSTOMDRAW* cd);
void DrawClipStrip(HDC hdc, const RECT& rcStrip, float dangerUnit, bool clipRecent);
std::wstring GetComboItemText(const DRAWITEMSTRUCT* dis);
bool PaintComboItemBackground(const DRAWITEMSTRUCT* dis);
void DrawSourceComboItem(const DRAWITEMSTRUCT* dis);
void DrawPlainComboItem(const DRAWITEMSTRUCT* dis);
bool ShouldDrawFocusRing(const DRAWITEMSTRUCT* dis);
void DrawOwnerCheckbox(const DRAWITEMSTRUCT* dis);
void DrawPowerToggle(const DRAWITEMSTRUCT* dis, bool on);
Glyph NavGlyph(int page);
void DrawNavItem(const DRAWITEMSTRUCT* dis);
void DrawPushButton(const DRAWITEMSTRUCT* dis);
void DrawThemeSegment(HWND parent, const DRAWITEMSTRUCT* dis);
LRESULT CALLBACK ButtonHoverSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR);
void DrawComboChevron(HDC hdc, const RECT& area, COLORREF color);
void PaintComboField(HWND combo, HDC hdc);
LRESULT CALLBACK ComboSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR);
void InstallButtonHoverTracking(HWND hwnd);
const wchar_t* PageTitle(int page);
void ShowPage(HWND hwnd, int page, bool persist);
void TogglePower(HWND hwnd);
std::wstring GetComboSelectedText(HWND combo);
void UpdateStatusCardText(HWND hwnd, const SourceStatus& st);
int MiniLitSegments(float db, bool active);
std::wstring MiniHotkeyHint();
void RefreshMini(bool force);
void SyncMiniFromMain();
void DrawSegmentMeter(HDC dc, const RECT& rc, int lit);
void PaintMiniWindow(HDC dc, const RECT& client);
void DrawMiniPowerButton(const DRAWITEMSTRUCT* dis);
void DrawMiniSourceChip(const DRAWITEMSTRUCT* dis);
void DrawMiniIconButton(const DRAWITEMSTRUCT* dis, Glyph glyph, bool active);
void DrawMiniMuteButton(const DRAWITEMSTRUCT* dis, bool mic);
void ExitMiniMode(int page);
void ToggleMainMute(bool mic);
LRESULT CALLBACK MiniWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
HWND CreateMiniWindow(HWND main);
void EnterMiniMode(HWND main);
void PopulateLanguageCombo(HWND hwnd);
void ApplyLanguageToUi(HWND hwnd);
void SetUiLanguage(HWND hwnd, const std::string& code);
void EnablePerMonitorDpiForThread();
SIZE WindowSizeForClient(int clientW, int clientH, DWORD style, int dpi);
void ApplyMiniFonts(HWND mini);
void ApplyDpi(HWND source, int newDpi, const RECT* suggested);
void SyncDpiToWindow(HWND w);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void WindowThreadMain();

} // namespace micmix_ui
