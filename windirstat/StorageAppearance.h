// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct StorageColors
{
    COLORREF background, rail, surface, line, text, secondary, accent, selected;
};

inline StorageColors GetStorageColors()
{
    if (DarkMode::IsHighContrastActive())
    {
        const auto background = GetSysColor(COLOR_WINDOW);
        const auto text = GetSysColor(COLOR_WINDOWTEXT);
        return { background, background, background, text, text, text,
            GetSysColor(COLOR_HIGHLIGHTTEXT), GetSysColor(COLOR_HIGHLIGHT) };
    }
    if (DarkMode::IsDarkModeActive()) return { RGB(24,27,27), RGB(20,24,23), RGB(34,39,38),
        RGB(58,67,64), RGB(237,242,240), RGB(178,191,185), RGB(136,221,182), RGB(41,69,59) };
    return { RGB(247,249,248), RGB(235,242,238), RGB(255,255,255),
        RGB(208,220,213), RGB(25,41,33), RGB(75,94,83), RGB(32,99,74), RGB(218,239,225) };
}

inline void DrawStorageText(CDC& dc, HFONT font, COLORREF color, std::wstring_view text, CRect rect)
{
    const GdiObjectSelection selection(&dc, font);
    SetTextColor(dc.Handle(), color);
    SetBkMode(dc.Handle(), TRANSPARENT);
    DrawTextW(dc.Handle(), text.data(), static_cast<int>(text.size()), &rect,
        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

inline LOGFONTW StorageFont(HWND window, int pixels, LONG weight)
{
    LOGFONTW font{};
    GetObjectW(GetAppFont(window), sizeof(font), &font);
    if (pixels > 0) font.lfHeight = -pixels;
    font.lfWeight = weight;
    return font;
}
