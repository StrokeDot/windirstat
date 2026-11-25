// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "StorageFileCategory.h"

class CStorageFileTypeCard final : public CButton
{
public:
    void SetSummary(std::wstring_view title, std::wstring detail, double share, bool selected);
    void DrawItem(LPDRAWITEMSTRUCT item) override;

private:
    std::wstring m_title;
    std::wstring m_detail;
    double m_share = 0;
    bool m_selected = false;
};

class CStorageFileTypesPane final : public MessageTarget<CStorageFileTypesPane, CWnd>
{
public:
    bool CreatePane(CWnd* parent);
    void Refresh();
    void OnAppearanceChanged() override;
    void OnFontSizeChanged(int, int) override;
    static std::span<const RouteEntry> Routes();

private:
    struct Extension
    {
        std::wstring name;
        ULONGLONG files;
        ULONGLONG bytes;
    };

    struct Category
    {
        std::vector<Extension> extensions;
        ULONGLONG files = 0;
        ULONGLONG bytes = 0;
    };

    void Layout();
    void UpdateSelection();
    void OnSize(UINT, int, int);
    void OnPaint();
    void OnSelect(UINT id);
    void OnBrowse();
    bool OnEraseBkgnd(CDC*) { return true; }

    static constexpr auto categoryCount = static_cast<unsigned>(StorageFileCategory::Count);
    std::array<Category, categoryCount> m_categories;
    std::array<CStorageFileTypeCard, categoryCount> m_cards;
    CDarkHeaderListCtrl m_extensions;
    CButton m_browse;
    CRect m_summary;
    CRect m_detailHeader;
    ULONGLONG m_totalBytes = 0;
    unsigned m_selected = 0;
    bool m_ready = false;
};
