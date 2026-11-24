// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "AppCachesPane.h"
#include "Layout.h"

namespace
{
    struct CacheFolder
    {
        const KNOWNFOLDERID& profile;
        std::wstring_view relative;
        std::wstring_view application;
        std::wstring_view policy;
        const wchar_t* guide;
    };

    constexpr auto nvidiaGuide = L"https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/3D%20Settings/Manage_3D_Settings_%28reference%29.htm";
    constexpr auto adobeGuide = L"https://helpx.adobe.com/premiere/desktop/troubleshooting/media-issues/clear-media-cache-using-preferences.html";
    const std::array knownCaches{
        CacheFolder{ FOLDERID_LocalAppData, L"NVIDIA\\DXCache", L"NVIDIA", IDS_CACHE_KEEP, nvidiaGuide },
        CacheFolder{ FOLDERID_LocalAppData, L"NVIDIA\\GLCache", L"NVIDIA", IDS_CACHE_KEEP, nvidiaGuide },
        CacheFolder{ FOLDERID_LocalAppData, L"NVIDIA Corporation\\NV_Cache", L"NVIDIA", IDS_CACHE_KEEP, nvidiaGuide },
        CacheFolder{ FOLDERID_RoamingAppData, L"Adobe\\Common\\Media Cache", L"Adobe Premiere", IDS_CACHE_IN_APP, adobeGuide },
        CacheFolder{ FOLDERID_RoamingAppData, L"Adobe\\Common\\Media Cache Files", L"Adobe Premiere", IDS_CACHE_IN_APP, adobeGuide },
    };

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
        const CItem* scanned = model->IsScanSettled() ? model->GetRootItem() : nullptr;
        for (const CacheFolder& cache : knownCaches)
        {
            CComHeapPtr<wchar_t> profile;
            if (FAILED(SHGetKnownFolderPath(cache.profile, KF_FLAG_DONT_VERIFY, nullptr, &profile)) || !profile)
                continue;
            const auto path = (std::filesystem::path(profile.m_pData) / cache.relative).wstring();
            const DWORD attributes = GetFileAttributesW(path.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            const CItem* item = scanned ? scanned->FindItemByPath(path, false, IT_DIRECTORY) : nullptr;
            const auto size = item && item->IsDone() && item->HasSizePhysical()
                ? FormatSizeSuffixes(item->GetSizePhysical()) : Localization::Lookup(IDS_CACHE_SIZE_PENDING);
            m_rows.push_back({ { std::wstring(cache.application), path, size, Localization::Lookup(cache.policy) },
                cache.guide });
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
        GetDlgItem(IDC_APP_CACHES_GUIDE).EnableWindow(selected);
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
        if (index >= 0 && static_cast<std::size_t>(index) < m_rows.size())
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
