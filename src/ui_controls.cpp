// Owner-drawn controls: dropdown items, switches, buttons, segments, hover tracking.

#include "settings_ui.h"

namespace micmix_ui {

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
    DrawTextW(dis->hDC, Tr(on ? Str::PowerOn : Str::PowerOff), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
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

} // namespace micmix_ui
