// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "AppCachesPane.h"
#include "Layout.h"
#include "StorageCacheIndex.h"

namespace
{
    struct CacheRow
    {
        std::array<std::wstring, 4> columns;
        const wchar_t* guide;
    };

    class AppCachesPane final : public MessageTarget<AppCachesPane, CLayoutDialog>
    {
    public:
        explicit AppCachesPane(CWnd* parent) : MessageTarget(IDD_APP_CACHES_PANE, nullptr, parent) {}
        static std::span<const RouteEntry> Routes();
        void Refresh();

    protected:
        bool OnInitDialog() override;
        void OnCancel() override {}
        void OnOK() override {}

    private:
        void OpenFolder();
        void OpenGuide();
        void SetActions();
        void OnSelection(NMHDR*, LRESULT*);
        void OnDisplayInfo(NMHDR* header, LRESULT* result);
        HBRUSH OnCtlColor(CDC*, WindowRef, UINT);
        CDarkHeaderListCtrl m_list;
        std::vector<CacheRow> m_rows;
    };

    std::span<const RouteEntry> AppCachesPane::Routes()
    {
        static constexpr std::array routes
        {
            Route::Control<&Refresh>(BN_CLICKED, IDC_APP_CACHES_REFRESH),
            Route::Control<&OpenFolder>(BN_CLICKED, IDC_APP_CACHES_OPEN),
            Route::Control<&OpenGuide>(BN_CLICKED, IDC_APP_CACHES_GUIDE),
            Route::Notify<&OnSelection>(LVN_ITEMCHANGED, IDC_APP_CACHES_LIST),
            Route::Notify<&OnSelection>(LVN_ODSTATECHANGED, IDC_APP_CACHES_LIST),
            Route::Notify<&OnDisplayInfo>(LVN_GETDISPINFO, IDC_APP_CACHES_LIST),
            Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        };
        return routes;
    }

    bool AppCachesPane::OnInitDialog()
    {
        CDialog::OnInitDialog();
        Localization::UpdateDialogs(*this);
        DarkMode::AdjustControls(Handle());
        m_list.SubclassDlgItem(IDC_APP_CACHES_LIST, this);
        m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        const std::array fields{ IDS_CACHE_APP, IDS_CACHE_LOCATION, IDS_COL_SIZE_PHYSICAL, IDS_CACHE_EFFECT };
        const std::array widths{ 130, 260, 110, 140 };
        for (std::size_t i = 0; i < fields.size(); ++i)
            m_list.InsertColumn(static_cast<int>(i), Localization::Lookup(fields[i]),
                i == 2 ? LVCFMT_RIGHT : LVCFMT_LEFT, ScaleForDpi(widths[i]));
        m_list.SetBkColor(DarkMode::SystemColor(COLOR_WINDOW));
        m_list.SetTextBkColor(DarkMode::SystemColor(COLOR_WINDOW));
        m_list.SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));

        m_layout.AddControl(IDC_APP_CACHES_LIST, 0, 0, 1, 1);
        m_layout.AddControl(IDC_APP_CACHES_STATUS, 0, 1, 1, 0);
        for (const int id : { IDC_APP_CACHES_REFRESH, IDC_APP_CACHES_OPEN, IDC_APP_CACHES_GUIDE })
            m_layout.AddControl(id, 1, 1, 0, 0);
        m_layout.OnInitDialog(true);
        SetActions();
        return true;
    }

    void AppCachesPane::Refresh()
    {
        m_list.SetItemCountEx(0, 0);
        m_rows.clear();
        const auto* model = CWinDirStatModel::Get();
        const bool settled = model->IsScanSettled();
        auto caches = FindProfileCaches();
        if (settled)
        {
            for (const auto& scanned : model->GetScannedCaches())
            {
                const auto existing = std::ranges::find_if(caches, [&scanned](const auto& cache)
                {
                    return CompareStringOrdinal(cache.path.c_str(), -1, scanned.path.c_str(), -1, TRUE) == CSTR_EQUAL;
                });
                if (existing == caches.end()) caches.push_back(scanned);
                else existing->bytes = scanned.bytes;
            }
            // Known folders can have a custom location with no recognizable parent names.
            for (auto& cache : caches)
            {
                if (cache.bytes) continue;
                const auto* item = model->GetRootItem()->FindItemByPath(cache.path, false, IT_DIRECTORY);
                if (item && item->IsDone() && item->HasSizePhysical() &&
                    item->GetScanResult().outcome == ScanOutcome::Complete) cache.bytes = item->GetSizePhysical();
            }
        }
        std::ranges::sort(caches, [](const StorageCache& a, const StorageCache& b)
        {
            if (a.bytes != b.bytes) return a.bytes > b.bytes;
            return CompareStringOrdinal(a.path.c_str(), -1, b.path.c_str(), -1, TRUE) == CSTR_LESS_THAN;
        });
        for (auto& cache : caches)
        {
            const auto size = cache.bytes ? FormatSizeSuffixes(*cache.bytes) : Localization::Lookup(IDS_CACHE_SIZE_PENDING);
            m_rows.push_back({ { cache.application, std::move(cache.path), size, Localization::Lookup(cache.guidance) },
                cache.helpUrl });
        }
        m_list.SetItemCountEx(static_cast<int>(m_rows.size()), 0);
        SetText(IDC_APP_CACHES_STATUS, Localization::Lookup(m_rows.empty() ? IDS_CACHE_EMPTY : IDS_CACHE_SCAN_SCOPE));
        SetActions();
    }

    void AppCachesPane::SetActions()
    {
        const int index = m_list.GetFirstSelectedIndex();
        const bool selected = index >= 0 && static_cast<std::size_t>(index) < m_rows.size();
        GetDlgItem(IDC_APP_CACHES_OPEN).EnableWindow(selected);
        GetDlgItem(IDC_APP_CACHES_GUIDE).EnableWindow(selected && m_rows[index].guide);
    }

    void AppCachesPane::OpenFolder()
    {
        const int index = m_list.GetFirstSelectedIndex();
        if (index >= 0 && static_cast<std::size_t>(index) < m_rows.size())
            ShellExecuteW(Handle(), L"open", m_rows[index].columns[1].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void AppCachesPane::OpenGuide()
    {
        const int index = m_list.GetFirstSelectedIndex();
        if (index >= 0 && static_cast<std::size_t>(index) < m_rows.size() && m_rows[index].guide)
            ShellExecuteW(Handle(), L"open", m_rows[index].guide, nullptr, nullptr, SW_SHOWNORMAL);
    }

    void AppCachesPane::OnSelection(NMHDR*, LRESULT* result)
    {
        *result = 0;
        SetActions();
    }

    void AppCachesPane::OnDisplayInfo(NMHDR* header, LRESULT* result)
    {
        *result = 0;
        auto& item = reinterpret_cast<NMLVDISPINFOW*>(header)->item;
        if ((item.mask & LVIF_TEXT) == 0 || item.iItem < 0 ||
            static_cast<std::size_t>(item.iItem) >= m_rows.size() || item.iSubItem < 0 || item.iSubItem >= 4) return;
        item.pszText = m_rows[item.iItem].columns[item.iSubItem].data();
    }

    HBRUSH AppCachesPane::OnCtlColor(CDC* dc, WindowRef window, UINT type)
    {
        const HBRUSH brush = DarkMode::OnCtlColor(dc, type);
        return brush ? brush : CDialog::OnCtlColor(dc, window, type);
    }
}

std::unique_ptr<CWnd> CreateAppCachesPane(CWnd* parent)
{
    auto pane = std::make_unique<AppCachesPane>(parent);
    if (!CreateDialogParamW(GetAppInstance(), MAKEINTRESOURCEW(IDD_APP_CACHES_PANE), parent->Handle(),
        FrameworkDialogProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(pane.get())))) return {};
    return pane;
}

void RefreshAppCachesPane(CWnd& pane) { static_cast<AppCachesPane&>(pane).Refresh(); }
