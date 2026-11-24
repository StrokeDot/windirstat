// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "Layout.h"
#include "RecentFilesDlg.h"
#include "MessageBoxDlg.h"
#include <shlguid.h>

namespace
{
    const PROPERTYKEY originalFolder{ FMTID_Displaced, PID_DISPLACED_FROM };
    const PROPERTYKEY deletedTime{ FMTID_Displaced, PID_DISPLACED_DATE };

    struct DeletedItem
    {
        CComPtr<IShellItem> item;
        FILETIME deleted{};
        std::array<std::wstring, 4> columns;
    };

    class RecentFilesDlg final : public MessageTarget<RecentFilesDlg, CLayoutDialog>
    {
    public:
        explicit RecentFilesDlg(CWnd* parent) : MessageTarget(IDD_RECENT_FILES, nullptr, parent) {}
        static std::span<const RouteEntry> Routes();
        void Refresh();

    protected:
        bool OnInitDialog() override;

    private:
        void OnOK() override {}
        void OnCancel() override {}
        void OpenRecycleBin();
        void OpenRecovery();
        void OnSelectionChanged(NMHDR*, LRESULT* result);
        void OnDisplayInfo(NMHDR* header, LRESULT* result);
        HBRUSH OnCtlColor(CDC* dc, WindowRef window, UINT type);

        CDarkHeaderListCtrl m_list;
        std::vector<DeletedItem> m_items;
    };

    std::span<const RouteEntry> RecentFilesDlg::Routes()
    {
        static constexpr std::array entries
        {
            Route::Control<&Refresh>(BN_CLICKED, IDC_RECENT_REFRESH),
            Route::Control<&OpenRecycleBin>(BN_CLICKED, IDC_RECENT_RECYCLE_BIN),
            Route::Control<&OpenRecovery>(BN_CLICKED, IDC_RECENT_RECOVERY),
            Route::Notify<&OnSelectionChanged>(LVN_ITEMCHANGED, IDC_RECENT_LIST),
            Route::Notify<&OnSelectionChanged>(LVN_ODSTATECHANGED, IDC_RECENT_LIST),
            Route::Notify<&OnDisplayInfo>(LVN_GETDISPINFO, IDC_RECENT_LIST),
            Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        };
        return entries;
    }

    bool RecentFilesDlg::OnInitDialog()
    {
        CDialog::OnInitDialog();
        Localization::UpdateDialogs(*this);
        DarkMode::AdjustControls(Handle());
        m_list.SubclassDlgItem(IDC_RECENT_LIST, this);
        m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);

        const std::array headers{ IDS_COL_NAME, IDS_RECENT_ORIGINAL_FOLDER, IDS_RECENT_DELETED_AT, IDS_COL_SIZE_LOGICAL };
        const std::array widths{ 200, 230, 150, 100 };
        for (size_t i = 0; i < headers.size(); ++i)
            m_list.InsertColumn(static_cast<int>(i), Localization::Lookup(headers[i]),
                i == 3 ? LVCFMT_RIGHT : LVCFMT_LEFT, ScaleForDpi(widths[i]));

        m_list.SetBkColor(DarkMode::SystemColor(COLOR_WINDOW));
        m_list.SetTextBkColor(DarkMode::SystemColor(COLOR_WINDOW));
        m_list.SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));
        m_layout.AddControl(IDC_RECENT_LIST, 0, 0, 1, 1);
        m_layout.AddControl(IDC_RECENT_STATUS, 0, 1, 1, 0);
        for (const int id : { IDC_RECENT_REFRESH, IDC_RECENT_RECYCLE_BIN, IDC_RECENT_RECOVERY })
            m_layout.AddControl(id, 1, 1, 0, 0);
        m_layout.OnInitDialog(true);

        return true;
    }

    void RecentFilesDlg::Refresh()
    {
        m_list.SetItemCountEx(0, 0);
        m_items.clear();
        CComPtr<IShellItem> bin;
        CComPtr<IEnumShellItems> items;
        HRESULT result = SHGetKnownFolderItem(FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&bin));
        if (SUCCEEDED(result)) result = bin->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items));
        if (SUCCEEDED(result) && items)
        {
            for (;;)
            {
                CComPtr<IShellItem> item;
                result = items->Next(1, &item, nullptr);
                if (result != S_OK) break;

                CComHeapPtr<wchar_t> name;
                if (FAILED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &name)) || !name) continue;

                DeletedItem entry;
                entry.item = item;
                entry.columns[0] = name.m_pData;
                if (const CComQIPtr<IShellItem2> details(item); details)
                {
                    CComHeapPtr<wchar_t> folder;
                    if (SUCCEEDED(details->GetString(originalFolder, &folder)) && folder)
                        entry.columns[1] = folder.m_pData;
                    if (details->GetFileTime(deletedTime, &entry.deleted) == S_OK)
                        entry.columns[2] = FormatFileTime(entry.deleted);
                    ULONGLONG size{};
                    if (details->GetUInt64(PKEY_Size, &size) == S_OK)
                        entry.columns[3] = FormatSizeSuffixes(size);
                }
                m_items.push_back(std::move(entry));
            }
        }

        std::ranges::stable_sort(m_items, [](const DeletedItem& a, const DeletedItem& b)
        {
            return CompareFileTime(&a.deleted, &b.deleted) > 0;
        });
        m_list.SetItemCountEx(static_cast<int>(m_items.size()), 0);
        SetText(IDC_RECENT_STATUS, FAILED(result) ? Localization::Lookup(IDS_RECENT_INCOMPLETE) :
            m_items.empty() ? Localization::Lookup(IDS_RECENT_EMPTY) :
            Localization::Format(IDS_RECENT_COUNTs, FormatCount(m_items.size())));
        SetText(IDC_RECENT_RECYCLE_BIN, Localization::Lookup(IDS_RECENT_OPEN_BIN));
    }

    void RecentFilesDlg::OpenRecycleBin()
    {
        const int row = m_list.GetFirstSelectedIndex();
        if (row >= 0 && static_cast<size_t>(row) < m_items.size())
        {
            SmartPointer<PIDLIST_ABSOLUTE, decltype(&CoTaskMemFree)> selection(&CoTaskMemFree);
            if (SUCCEEDED(SHGetIDListFromObject(m_items[row].item, &selection)) &&
                SUCCEEDED(SHOpenFolderAndSelectItems(selection.Get(), 0, nullptr, 0))) return;
        }
        ShellExecuteW(Handle(), L"open", L"explorer.exe", L"shell:RecycleBinFolder", nullptr, SW_SHOWNORMAL);
    }

    void RecentFilesDlg::OpenRecovery()
    {
        if (IsElevationActive()) ::PostMessageW(GetAncestor(Handle(), GA_ROOT), WM_COMMAND, ID_TOOLS_RECOVERY, 0);
        else CMessageBoxDlg::Show(Localization::Lookup(IDS_RECENT_NEEDS_ELEVATION), MB_OK, this);
    }

    void RecentFilesDlg::OnSelectionChanged(NMHDR*, LRESULT* result)
    {
        *result = 0;
        SetText(IDC_RECENT_RECYCLE_BIN, Localization::Lookup(
            m_list.GetFirstSelectedIndex() < 0 ? IDS_RECENT_OPEN_BIN : IDS_RECENT_SHOW_IN_BIN));
    }

    void RecentFilesDlg::OnDisplayInfo(NMHDR* header, LRESULT* result)
    {
        *result = 0;
        auto& item = reinterpret_cast<NMLVDISPINFOW*>(header)->item;
        if ((item.mask & LVIF_TEXT) == 0 || item.iItem < 0 ||
            static_cast<size_t>(item.iItem) >= m_items.size() || item.iSubItem < 0 || item.iSubItem >= 4) return;
        item.pszText = m_items[item.iItem].columns[item.iSubItem].data();
    }

    HBRUSH RecentFilesDlg::OnCtlColor(CDC* dc, WindowRef window, UINT type)
    {
        const HBRUSH brush = DarkMode::OnCtlColor(dc, type);
        return brush ? brush : CDialog::OnCtlColor(dc, window, type);
    }
}

std::unique_ptr<CWnd> CreateRecentFilesPane(CWnd* parent)
{
    auto pane = std::make_unique<RecentFilesDlg>(parent);
    if (!CreateDialogParamW(GetAppInstance(), MAKEINTRESOURCEW(IDD_RECENT_FILES), parent->Handle(),
        FrameworkDialogProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(pane.get())))) return {};
    return pane;
}

void RefreshRecentFilesPane(CWnd& pane) { static_cast<RecentFilesDlg&>(pane).Refresh(); }
