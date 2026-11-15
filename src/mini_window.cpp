// Mini mode window.

#include "settings_ui.h"

namespace micmix_ui {

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
        return Tr(Str::MiniHotkeysHint);
    }
    std::wstring text;
    if (music) {
        text += FormatHotkeyText(g_muteHotkeyModifiers, g_muteHotkeyVk) + Tr(Str::MiniHotkeyMusic);
    }
    if (mic) {
        if (!text.empty()) text += L"  \xB7  ";
        text += FormatHotkeyText(g_micInputMuteHotkeyModifiers, g_micInputMuteHotkeyVk) + Tr(Str::MiniHotkeyMic);
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
        src = Tr(Str::MiniChooseSource);
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
    DrawTextW(dc, Tr(Str::RowVolume), -1, &g_rcMiniGainText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    HWND main = g_hwnd.load(std::memory_order_acquire);
    if (main) {
        wchar_t gainText[32]{};
        GetWindowTextW(GetDlgItem(main, IDC_GAIN_VALUE), gainText, static_cast<int>(std::size(gainText)));
        SetTextColor(dc, g_theme.text);
        DrawTextW(dc, gainText, -1, &g_rcMiniGainText, DT_SINGLELINE | DT_RIGHT | DT_VCENTER | DT_NOPREFIX);
    }
    SetTextColor(dc, g_theme.muted);
    RECT ml{ g_rcMiniPanel.left + S(14), g_rcMiniMusicMeter.top - S(5), g_rcMiniMusicMeter.left - S(6), g_rcMiniMusicMeter.bottom + S(5) };
    DrawTextW(dc, Tr(Str::MiniMusic), -1, &ml, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
    RECT cl{ g_rcMiniPanel.left + S(14), g_rcMiniMicMeter.top - S(5), g_rcMiniMicMeter.left - S(6), g_rcMiniMicMeter.bottom + S(5) };
    DrawTextW(dc, Tr(Str::MiniMic), -1, &cl, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);
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
    DrawTextW(dis->hDC, Tr(on ? Str::MiniLive : Str::MiniOff), -1, &textRc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
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
    const wchar_t* label = Tr(mic ? (muted ? Str::MicMuted : Str::MuteMic) : (muted ? Str::MusicMuted : Str::MuteMusic));
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
        make(L"BUTTON", Tr(Str::MiniChooseSource), WS_TABSTOP | BS_OWNERDRAW, 40, 202, 280, 30, IDC_MINI_SOURCE);
        g_rcMiniPanel = { S(18), S(248), S(342), S(370) };
        g_rcMiniGainText = { S(32), S(258), S(328), S(278) };
        HWND gain = make(TRACKBAR_CLASSW, L"", WS_TABSTOP, 26, 280, 308, 28, IDC_MINI_GAIN);
        SendMessageW(gain, TBM_SETRANGE, TRUE, MAKELONG(0, kMusicGainSliderMax));
        SendMessageW(gain, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(gain, TBM_SETLINESIZE, 0, 1);
        g_rcMiniMusicMeter = { S(100), S(318), S(328), S(326) };
        g_rcMiniMicMeter = { S(100), S(342), S(328), S(350) };
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

} // namespace micmix_ui
