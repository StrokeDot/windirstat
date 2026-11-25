// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "StorageWorkspace.h"
#include "StorageAppearance.h"
#include "AppCachesPane.h"
#include "RecentFilesDlg.h"

namespace
{
    constexpr std::array pageTitles{ IDS_WORKSPACE_OVERVIEW, IDS_WORKSPACE_TYPES, IDS_WORKSPACE_CACHES, IDS_WORKSPACE_RECENT };
    constexpr std::array<UINT, 4> navigationIds{ IDC_WORKSPACE_OVERVIEW, IDC_WORKSPACE_TYPES,
        IDC_WORKSPACE_CACHES, IDC_WORKSPACE_RECENT };
}

void CStorageNavigationButton::SetCurrent(const bool current)
{
    if (m_current == current) return;
    m_current = current;
    Invalidate(false);
}

void CStorageNavigationButton::DrawItem(LPDRAWITEMSTRUCT item)
{
    auto dc = CDC::Borrow(item->hDC);
    const auto colors = GetStorageColors();
    CRect rect(item->rcItem);
    dc.FillSolidRect(rect, colors.rail);
    if (m_current || (item->itemState & ODS_SELECTED))
    {
        const CBrush selected(colors.selected);
        const StockObjectSelection noBorder(&dc, NULL_PEN);
        const GdiObjectSelection brush(&dc, &selected);
        dc.RoundRect(rect, CPoint(ScaleForDpi(8), ScaleForDpi(8)));
    }
    CRect label = rect;
    label.Deflate(ScaleForDpi(12), 0);
    const CFont font(StorageFont(Handle(), 0, m_current ? FW_SEMIBOLD : FW_NORMAL));
    DrawStorageText(dc, font, m_current ? colors.accent : colors.text, GetText(), label);
    if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT))
    {
        rect.Deflate(2, 2);
        DarkMode::DrawFocusRect(&dc, rect);
    }
}

std::span<const RouteEntry> CStorageWorkspace::Routes()
{
    static constexpr std::array routes
    {
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Control<&OnNavigate>(BN_CLICKED, IDC_WORKSPACE_OVERVIEW, IDC_WORKSPACE_RECENT),
        Route::Control<&OnNavigate>(BN_CLICKED, IDC_WORKSPACE_TYPES),
        Route::Control<&OnPause>(BN_CLICKED, IDC_WORKSPACE_PAUSE),
        Route::Control<&OnScanAction>(BN_CLICKED, IDC_WORKSPACE_SCAN_ACTION),
    };
    return routes;
}

bool CStorageWorkspace::CreateWorkspace(CWnd* frame)
{
    if (!CreateEx(WS_EX_CONTROLPARENT, nullptr, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(), frame, WDS_PANE_ID_BASE)) return false;
    for (unsigned i = 0; i < m_navigation.size(); ++i)
    {
        const std::wstring label = Localization::Lookup(pageTitles[i]);
        if (!m_navigation[i].Create(label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            CRect(), this, navigationIds[i])) return false;
    }
    if (!m_pause.Create(Localization::Lookup(IDS_WORKSPACE_PAUSE).c_str(),
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON, CRect(), this, IDC_WORKSPACE_PAUSE)) return false;
    if (!m_scanAction.Create(Localization::Lookup(IDS_WORKSPACE_NEW_SCAN).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(), this, IDC_WORKSPACE_SCAN_ACTION)) return false;
    m_navigation.front().SetCurrent(true);
    ClearScanStatus();
    return true;
}

void CStorageWorkspace::SetOverview(CWnd* overview)
{
    m_overview = overview;
    Layout();
}

void CStorageWorkspace::ShowPage(const Page page)
{
    if (page == Page::Types && !m_types)
    {
        auto types = std::make_unique<CStorageFileTypesPane>();
        if (!types->CreatePane(this)) return;
        m_types = std::move(types);
        DarkMode::AdjustControls(m_types->Handle());
        m_types->OnAppearanceChanged();
    }
    if (page == Page::Recent && !m_recent) m_recent = CreateRecentFilesPane(this);
    if (page == Page::Caches && !m_caches) m_caches = CreateAppCachesPane(this);
    if ((page == Page::Recent && !m_recent) || (page == Page::Caches && !m_caches)) return;
    m_page = page;

    if (m_overview) m_overview->ShowWindow(page == Page::Overview ? SW_SHOWNA : SW_HIDE);
    if (m_types) m_types->ShowWindow(page == Page::Types ? SW_SHOWNA : SW_HIDE);
    if (m_recent) m_recent->ShowWindow(page == Page::Recent ? SW_SHOWNA : SW_HIDE);
    if (m_caches) m_caches->ShowWindow(page == Page::Caches ? SW_SHOWNA : SW_HIDE);
    for (unsigned i = 0; i < m_navigation.size(); ++i)
        m_navigation[i].SetCurrent(i == static_cast<unsigned>(page));
    Layout();
    RefreshCurrentPage();
}

void CStorageWorkspace::RefreshCurrentPage()
{
    m_previousScanSettled = CWinDirStatModel::Get()->IsScanSettled();
    if (m_page == Page::Types && m_types) m_types->Refresh();
    if (m_page == Page::Recent && m_recent) RefreshRecentFilesPane(*m_recent);
    if (m_page == Page::Caches && m_caches) RefreshAppCachesPane(*m_caches);
    m_resultsDirty = false;
}

void CStorageWorkspace::PollScanResults()
{
    if (m_page != Page::Types && m_page != Page::Caches) return;
    const bool settled = CWinDirStatModel::Get()->IsScanSettled();
    if (settled != m_previousScanSettled || (settled && m_resultsDirty)) RefreshCurrentPage();
}

void CStorageWorkspace::Layout()
{
    if (!Handle()) return;
    const CRect bounds = GetClientRect();
    const int margin = ScaleForDpi(17), navHeight = ScaleForDpi(36), brandHeight = ScaleForDpi(54);
    const bool compact = bounds.Width() < ScaleForDpi(680);
    const int railWidth = ScaleForDpi(177);
    m_rail = compact ? CRect(0, 0, bounds.right, brandHeight + navHeight + margin) :
        CRect(0, 0, railWidth, bounds.bottom);
    m_brand = CRect(margin, 0, compact ? bounds.right - margin : railWidth - margin, brandHeight);
    const int width = compact ? std::max(0, (bounds.Width() - margin * 2) / static_cast<int>(m_navigation.size())) :
        railWidth - margin * 2;
    for (unsigned i = 0; i < m_navigation.size(); ++i)
    {
        if (!m_navigation[i].Handle()) continue;
        m_navigation[i].SetWindowPos(nullptr, margin + (compact ? static_cast<int>(i) * width : 0),
            brandHeight + (compact ? 0 : static_cast<int>(i) * (navHeight + ScaleForDpi(3))),
            width, navHeight, SWP_NOACTIVATE | SWP_NOZORDER);
    }

    const int left = (compact ? 0 : railWidth) + margin;
    const int right = std::max(left, static_cast<int>(bounds.right) - margin);
    const int top = (compact ? m_rail.bottom : 0) + margin;
    m_scan = CRect(left, top, right, top + ScaleForDpi(72));
    m_heading = CRect(left, m_scan.bottom + ScaleForDpi(5), right, m_scan.bottom + ScaleForDpi(42));
    m_content = CRect(left, m_heading.bottom, right, std::max(m_heading.bottom, bounds.bottom - margin));

    if (m_pause.Handle()) m_pause.SetWindowPos(nullptr, std::max(left, right - ScaleForDpi(163)),
        top + ScaleForDpi(10), ScaleForDpi(74), ScaleForDpi(26), SWP_NOACTIVATE | SWP_NOZORDER);
    if (m_scanAction.Handle()) m_scanAction.SetWindowPos(nullptr, std::max(left, right - ScaleForDpi(85)),
        top + ScaleForDpi(10), ScaleForDpi(73), ScaleForDpi(26), SWP_NOACTIVATE | SWP_NOZORDER);
    for (CWnd* view : { m_overview, static_cast<CWnd*>(m_types.get()), m_caches.get(), m_recent.get() })
    {
        if (view && view->Handle()) view->SetWindowPos(nullptr, m_content.left, m_content.top,
            m_content.Width(), m_content.Height(), SWP_NOACTIVATE | SWP_NOZORDER);
    }
    Invalidate(false);
}

void CStorageWorkspace::OnSize(UINT, int, int) { Layout(); }

void CStorageWorkspace::OnAppearanceChanged() { Layout(); }

void CStorageWorkspace::OnFontSizeChanged(int, int) { Layout(); }

void CStorageWorkspace::OnNavigate(const UINT command)
{
    ShowPage(static_cast<Page>(std::ranges::find(navigationIds, command) - navigationIds.begin()));
}

void CStorageWorkspace::OnPause()
{
    if (m_canPause) GetParent().PostMessage(WM_COMMAND, m_paused ? ID_SCAN_RESUME : ID_SCAN_SUSPEND);
}

void CStorageWorkspace::OnScanAction()
{
    GetParent().PostMessage(WM_COMMAND, m_canPause ? ID_SCAN_STOP : ID_FILE_SELECT);
}

void CStorageWorkspace::SetScanStatus(std::wstring status, std::wstring target, std::optional<int> percent,
    const bool canPause, const bool paused)
{
    if (m_status == status && m_target == target && m_percent == percent &&
        m_canPause == canPause && m_paused == paused) return;
    m_status = std::move(status);
    m_target = std::move(target);
    m_percent = percent;
    m_canPause = canPause;
    m_paused = paused;
    if (m_pause.Handle())
    {
        m_pause.SetText(Localization::Lookup(paused ? IDS_RESUME : IDS_WORKSPACE_PAUSE));
        m_pause.ShowWindow(canPause ? SW_SHOWNA : SW_HIDE);
    }
    if (m_scanAction.Handle())
        m_scanAction.SetText(Localization::Lookup(canPause ? IDS_WORKSPACE_STOP : IDS_WORKSPACE_NEW_SCAN));
    if (Handle()) ::InvalidateRect(Handle(), &m_scan, FALSE);
}

void CStorageWorkspace::ClearScanStatus()
{
    SetScanStatus(Localization::Lookup(IDS_WORKSPACE_READY), Localization::Lookup(IDS_WORKSPACE_READY_HINT),
        std::nullopt, false, false);
}

void CStorageWorkspace::OnPaint()
{
    CPaintDC dc(this);
    const auto colors = GetStorageColors();
    dc.FillSolidRect(GetClientRect(), colors.background);
    dc.FillSolidRect(m_rail, colors.rail);
    const CFont brandFont(StorageFont(Handle(), ScaleForDpi(16), FW_SEMIBOLD));
    const CFont statusFont(StorageFont(Handle(), ScaleForDpi(14), FW_SEMIBOLD));
    const CFont headingFont(StorageFont(Handle(), ScaleForDpi(23), FW_SEMIBOLD));
    DrawStorageText(dc, brandFont, colors.text, L"Smart Storage", m_brand);
    if (m_scan.Width() < ScaleForDpi(30)) return;

    const CPen border(PS_SOLID, 1, colors.line);
    const CBrush surface(colors.surface);
    {
        const GdiObjectSelection pen(&dc, &border);
        const GdiObjectSelection brush(&dc, &surface);
        dc.RoundRect(m_scan, CPoint(ScaleForDpi(11), ScaleForDpi(11)));
    }
    const int step = ScaleForDpi(12);
    const int right = std::max(m_scan.left + step, m_scan.right - ScaleForDpi(m_canPause ? 170 : 90));
    DrawStorageText(dc, statusFont, colors.text, m_status,
        CRect(m_scan.left + step, m_scan.top + ScaleForDpi(8), right, m_scan.top + ScaleForDpi(33)));
    DrawStorageText(dc, GetAppFont(Handle()), colors.secondary, m_target,
        CRect(m_scan.left + step, m_scan.top + ScaleForDpi(35), m_scan.right - step, m_scan.top + ScaleForDpi(55)));
    if (m_percent)
    {
        const CRect track(m_scan.left + step, m_scan.top + ScaleForDpi(58), m_scan.right - step, m_scan.top + ScaleForDpi(63));
        dc.FillSolidRect(track, colors.line);
        CRect completed = track;
        completed.right = track.left + track.Width() * std::clamp(*m_percent, 0, 100) / 100;
        if (completed.Width() > 0) dc.FillSolidRect(completed, colors.accent);
    }
    DrawStorageText(dc, headingFont, colors.text, Localization::Lookup(pageTitles[static_cast<unsigned>(m_page)]), m_heading);
}
