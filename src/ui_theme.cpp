// Light/dark palettes, system theme detection, fonts and brushes.

#include "settings_ui.h"

namespace micmix_ui {

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

} // namespace micmix_ui
