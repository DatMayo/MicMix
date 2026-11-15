// Drawing primitives (GDI+ with GDI fallback), icons, status card, meters and slider.

#include "settings_ui.h"

namespace micmix_ui {

Gdiplus::Color ToGdiColor(COLORREF c, BYTE alpha) {
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


ShapeStyle Filled(COLORREF fill) {
    ShapeStyle s;
    s.fill = fill;
    return s;
}

ShapeStyle FilledBordered(COLORREF fill, COLORREF border, float borderWidth) {
    ShapeStyle s;
    s.fill = fill;
    s.border = border;
    s.hasBorder = true;
    s.borderWidth = borderWidth;
    return s;
}

ShapeStyle Outline(COLORREF border, float borderWidth) {
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

// Uses the DC's built-in brush instead of creating a GDI brush per call.
void FillSolidRect(HDC hdc, const RECT& rc, COLORREF color) {
    const COLORREF previous = SetDCBrushColor(hdc, color);
    FillRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetDCBrushColor(hdc, previous);
}

// 1px rectangle outline with the DC's built-in pen.
void FrameSolidRect(HDC hdc, const RECT& rc, COLORREF color) {
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    const COLORREF previous = SetDCPenColor(hdc, color);
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SetDCPenColor(hdc, previous);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
}

// Back buffer reused across paints; recreated only when the size changes.
HDC BackBuffer::Acquire(HDC target, int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    if (dc && (width != w || height != h)) {
        Release();
    }
    if (!dc) {
        dc = CreateCompatibleDC(target);
        bmp = dc ? CreateCompatibleBitmap(target, width, height) : nullptr;
        if (!dc || !bmp) {
            Release();
            return nullptr;
        }
        oldBmp = SelectObject(dc, bmp);
        w = width;
        h = height;
    }
    return dc;
}

void BackBuffer::Release() {
    if (dc && oldBmp) {
        SelectObject(dc, oldBmp);
    }
    if (bmp) DeleteObject(bmp);
    if (dc) DeleteDC(dc);
    dc = nullptr;
    bmp = nullptr;
    oldBmp = nullptr;
    w = 0;
    h = 0;
}

void DrawCard(HDC hdc, const RECT& rc) {
    DrawRoundRect(hdc, rc, S(8), FilledBordered(g_theme.card, g_theme.border));
}

// Simple stroke icons on a 24x24 grid (same shapes as the design mockups).
void DrawGlyph(HDC hdc, Glyph glyph, const RECT& box, COLORREF color, float stroke) {
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

    // The window paints into a back buffer already, so draw directly.
    HDC drawDc = hdc;
    HDC memDc = nullptr;
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    const bool buffered = false;

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
        RECT guideRc{ x, barRc.top, x + 1, barRc.bottom };
        FillSolidRect(drawDc, guideRc, guideColor);

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

        const int holdX = barRc.left + holdPx;
        RECT holdRc{ holdX, barRc.top, std::min(static_cast<int>(barRc.right), holdX + std::max(1, S(2))), barRc.bottom };
        FillSolidRect(drawDc, holdRc, g_theme.meterHold);
    }

    FrameSolidRect(drawDc, barRc, g_theme.meterBorder);

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

    // The window paints into a back buffer already, so draw directly.
    HDC drawDc = hdc;
    HDC memDc = nullptr;
    HBITMAP memBmp = nullptr;
    HGDIOBJ oldBmp = nullptr;
    const bool buffered = false;

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

    FrameSolidRect(drawDc, rc, clipRecent ? g_theme.clipPulse : g_theme.clipBorder);

    SetBkMode(drawDc, TRANSPARENT);
    SetTextColor(drawDc, clipRecent ? g_theme.clipTextHot : g_theme.clipText);
    HGDIOBJ oldClipFont = SelectObject(drawDc, g_fontTiny ? g_fontTiny : (g_fontHint ? g_fontHint : g_fontSmall));
    RECT labelRc{ rc.left + S(5), rc.top, rc.right - S(52), rc.bottom };
    DrawTextW(drawDc, Tr(Str::ClipMeter), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

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
    SelectObject(drawDc, oldClipFont);

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

} // namespace micmix_ui
