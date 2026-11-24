// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "pch.h"

class CStorageNavigationButton final : public CButton
{
public:
    void SetCurrent(bool current);
    void DrawItem(LPDRAWITEMSTRUCT item) override;

private:
    bool m_current = false;
};

class CStorageWorkspace final : public MessageTarget<CStorageWorkspace, CWnd>
{
public:
    enum class Page : unsigned { Overview, Caches, Recent };

    bool CreateWorkspace(CWnd* frame);
    void SetOverview(CWnd* overview);
    void ShowPage(Page page);
    void RefreshCurrentPage();
    void SetScanStatus(std::wstring status, std::wstring target, std::optional<int> percent, bool canPause, bool paused);
    void ClearScanStatus();
    void OnAppearanceChanged() override;
    void OnFontSizeChanged(int, int) override;
    static std::span<const RouteEntry> Routes();

private:
    void Layout();
    void OnSize(UINT, int, int);
    void OnPaint();
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnNavigate(UINT command);
    void OnPause();

    std::array<CStorageNavigationButton, 3> m_navigation;
    CButton m_pause;
    CWnd* m_overview{};
    std::unique_ptr<CWnd> m_caches;
    std::unique_ptr<CWnd> m_recent;
    Page m_page = Page::Overview;
    CRect m_rail;
    CRect m_brand;
    CRect m_scan;
    CRect m_heading;
    CRect m_content;
    std::wstring m_status;
    std::wstring m_target;
    std::optional<int> m_percent;
    bool m_canPause = false;
    bool m_paused = false;
};
