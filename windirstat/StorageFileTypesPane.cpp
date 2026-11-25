// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "StorageAppearance.h"
#include "StorageFileTypesPane.h"

namespace
{
    constexpr std::array titles
    {
        IDS_TYPES_VIDEO, IDS_TYPES_AUDIO, IDS_TYPES_PICTURES, IDS_TYPES_DOCUMENTS,
        IDS_TYPES_ARCHIVES, IDS_TYPES_SOFTWARE, IDS_TYPES_DISKS, IDS_TYPES_OTHER
    };
    static_assert(titles.size() == static_cast<unsigned>(StorageFileCategory::Count));
}

void CStorageFileTypeCard::SetSummary(std::wstring_view title, std::wstring detail, double share, bool selected)
{
    m_title = title;
    m_detail = std::move(detail);
    m_share = share;
    m_selected = selected;
    SetText(m_detail.empty() ? m_title : m_title + L", " + m_detail);
    Invalidate(false);
}

void CStorageFileTypeCard::DrawItem(LPDRAWITEMSTRUCT item)
{
    auto dc = CDC::Borrow(item->hDC);
    const auto colors = GetStorageColors();
    const CRect box(item->rcItem);
    const bool active = m_selected || (item->itemState & ODS_SELECTED);
    dc.FillSolidRect(box, colors.background);
    const CPen outline(PS_SOLID, 1, active ? colors.accent : colors.line);
    const CBrush fill(active ? colors.selected : colors.surface);
    {
        const GdiObjectSelection pen(&dc, &outline);
        const GdiObjectSelection brush(&dc, &fill);
        dc.RoundRect(box, CPoint(ScaleForDpi(12), ScaleForDpi(12)));
    }

    const auto inset = ScaleForDpi(10);
    const auto centered = box.Height() <= ScaleForDpi(38);
    const CFont title(StorageFont(Handle(), ScaleForDpi(13), FW_SEMIBOLD));
    const CFont detail(StorageFont(Handle(), ScaleForDpi(11), FW_NORMAL));
    const auto headingTop = box.top + ScaleForDpi(centered ? 1 : 3);
    DrawStorageText(dc, title, active && DarkMode::IsHighContrastActive() ? colors.accent : colors.text, m_title,
        CRect(box.left + inset, headingTop, box.right - inset, headingTop + ScaleForDpi(16)));
    DrawStorageText(dc, detail, active && DarkMode::IsHighContrastActive() ? colors.accent : colors.secondary, m_detail,
        CRect(box.left + inset, headingTop + ScaleForDpi(16), box.right - inset, headingTop + ScaleForDpi(30)));
    if (m_share > 0 && !centered && !DarkMode::IsHighContrastActive())
    {
        const auto width = std::max(1, box.Width() - 2 * inset);
        const auto right = box.left + inset + std::max(1, static_cast<int>(width * m_share));
        const auto bottom = box.bottom - ScaleForDpi(6);
        dc.FillSolidRect(CRect(box.left + inset, bottom - ScaleForDpi(3), box.right - inset, bottom), colors.line);
        dc.FillSolidRect(CRect(box.left + inset, bottom - ScaleForDpi(3), right, bottom), colors.accent);
    }
    if ((item->itemState & ODS_FOCUS) && !(item->itemState & ODS_NOFOCUSRECT))
    {
        auto focus = box;
        focus.Deflate(ScaleForDpi(3), ScaleForDpi(3));
        DarkMode::DrawFocusRect(&dc, focus);
    }
}

std::span<const RouteEntry> CStorageFileTypesPane::Routes()
{
    static constexpr std::array routes
    {
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Control<&OnSelect>(BN_CLICKED, IDC_TYPES_CATEGORY_FIRST, IDC_TYPES_CATEGORY_LAST),
        Route::Control<&OnBrowse>(BN_CLICKED, IDC_TYPES_BROWSE)
    };
    return routes;
}

bool CStorageFileTypesPane::CreatePane(CWnd* parent)
{
    if (!CreateEx(WS_EX_CONTROLPARENT, nullptr, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CRect(), parent, IDC_TYPES_PANE)) return false;
    for (unsigned i = 0; i < m_cards.size(); ++i)
    {
        if (!m_cards[i].Create(Localization::Lookup(titles[i]).c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            BS_OWNERDRAW, CRect(), this, IDC_TYPES_CATEGORY_FIRST + i)) return false;
    }
    if (!m_extensions.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        CRect(), this, IDC_TYPES_EXTENSIONS) ||
        !m_browse.Create(Localization::Lookup(IDS_TYPES_BROWSE).c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            BS_PUSHBUTTON, CRect(), this, IDC_TYPES_BROWSE)) return false;

    m_extensions.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_extensions.InsertColumn(0, Localization::Lookup(IDS_COL_EXTENSION), LVCFMT_LEFT, ScaleForDpi(180));
    m_extensions.InsertColumn(1, Localization::Lookup(IDS_COL_FILES), LVCFMT_RIGHT, ScaleForDpi(110));
    m_extensions.InsertColumn(2, Localization::Lookup(IDS_COL_SIZE_LOGICAL), LVCFMT_RIGHT, ScaleForDpi(130));
    OnAppearanceChanged();
    return true;
}

void CStorageFileTypesPane::Refresh()
{
    m_categories = {};
    m_totalBytes = 0;
    auto* model = CWinDirStatModel::Get();
    m_ready = model->IsScanSettled();
    if (m_ready)
    {
        for (const auto& [extension, record] : *model->GetExtensionData())
        {
            const auto files = record.GetFiles();
            if (files == 0) continue;
            const auto bytes = record.GetBytes();
            auto& category = m_categories[static_cast<unsigned>(CategorizeExtension(extension))];
            category.extensions.push_back({ extension, files, bytes });
            category.files += files;
            category.bytes += bytes;
            m_totalBytes += bytes;
        }
        if (!m_categories[m_selected].files)
            m_selected = static_cast<unsigned>(std::ranges::max_element(m_categories, {}, &Category::files) -
                m_categories.begin());
        for (auto& category : m_categories)
            std::ranges::sort(category.extensions, [](const Extension& a, const Extension& b)
            {
                return a.bytes != b.bytes ? a.bytes > b.bytes : a.name < b.name;
            });
    }
    UpdateSelection();
}

void CStorageFileTypesPane::UpdateSelection()
{
    for (unsigned i = 0; i < m_cards.size(); ++i)
    {
        const auto& group = m_categories[i];
        std::wstring detail;
        if (m_ready)
            detail = FormatSizeSuffixes(group.bytes) + L" · " + (group.files == 1 ?
                Localization::Lookup(IDS_TYPES_ONE_FILE) :
                Localization::Format(IDS_TYPES_FILE_COUNTs, FormatCount(group.files)));
        m_cards[i].SetSummary(Localization::Lookup(titles[i]), std::move(detail),
            m_totalBytes ? static_cast<double>(group.bytes) / static_cast<double>(m_totalBytes) : 0,
            m_selected == i);
    }

    const ScopedRedrawPause redraw(&m_extensions);
    m_extensions.SendNativeMessage(LVM_DELETEALLITEMS);
    int index = 0;
    for (const auto& extension : m_categories[m_selected].extensions)
    {
        auto name = extension.name.empty() ? Localization::Lookup(IDS_TYPES_NO_EXTENSION) : extension.name;
        LVITEMW item{ .mask = LVIF_TEXT, .iItem = index++, .pszText = name.data() };
        const auto row = m_extensions.SendNativeMessage(LVM_INSERTITEMW, 0, &item);
        if (row == -1) break;
        auto files = FormatCount(extension.files);
        auto bytes = FormatSizeSuffixes(extension.bytes);
        item.iSubItem = 1;
        item.pszText = files.data();
        m_extensions.SendNativeMessage(LVM_SETITEMTEXTW, row, &item);
        item.iSubItem = 2;
        item.pszText = bytes.data();
        m_extensions.SendNativeMessage(LVM_SETITEMTEXTW, row, &item);
    }
    m_browse.EnableWindow(m_ready && m_categories[m_selected].files != 0);
    Invalidate(false);
}

void CStorageFileTypesPane::Layout()
{
    const auto area = GetClientRect();
    const auto gap = ScaleForDpi(7);
    const int columns = area.Width() >= ScaleForDpi(540) ? 4 : 2;
    const int rows = static_cast<int>(m_cards.size()) / columns;
    const int height = ScaleForDpi(area.Height() < ScaleForDpi(rows * 50 + 95) ? 34 : 48);
    const int width = std::max(0, (area.Width() - (columns - 1) * gap) / columns);
    m_summary = CRect(0, 0, area.right, ScaleForDpi(23));
    for (int i = 0; i < static_cast<int>(m_cards.size()); ++i)
        if (m_cards[i].Handle()) m_cards[i].MoveWindow((i % columns) * (width + gap),
            m_summary.bottom + (i / columns) * (height + gap), width, height);

    const int top = m_summary.bottom + rows * (height + gap);
    const int buttonWidth = ScaleForDpi(110);
    m_detailHeader = CRect(0, top, std::max(0, static_cast<int>(area.right) - buttonWidth - gap), top + ScaleForDpi(30));
    if (m_browse.Handle()) m_browse.MoveWindow(std::max(0, static_cast<int>(area.right) - buttonWidth),
        top, buttonWidth, ScaleForDpi(25));
    if (m_extensions.Handle())
    {
        const int listTop = m_detailHeader.bottom;
        m_extensions.MoveWindow(0, listTop, area.Width(), std::max(0, static_cast<int>(area.bottom) - listTop));
        m_extensions.SetColumnWidth(0, std::max(ScaleForDpi(120), area.Width() - ScaleForDpi(257)));
    }
    Invalidate(false);
}

void CStorageFileTypesPane::OnSize(UINT, int, int) { Layout(); }

void CStorageFileTypesPane::OnFontSizeChanged(int, int) { Layout(); }

void CStorageFileTypesPane::OnAppearanceChanged()
{
    if (!m_extensions.Handle()) return;
    const auto colors = GetStorageColors();
    m_extensions.SetBkColor(colors.surface);
    m_extensions.SetTextBkColor(colors.surface);
    m_extensions.SetTextColor(colors.text);
    Layout();
}

void CStorageFileTypesPane::OnPaint()
{
    CPaintDC dc(this);
    const auto colors = GetStorageColors();
    dc.FillSolidRect(GetClientRect(), colors.background);
    const auto summary = m_ready ? Localization::Format(IDS_TYPES_SUMMARYs, FormatSizeSuffixes(m_totalBytes)) :
        Localization::Lookup(IDS_TYPES_WAITING);
    DrawStorageText(dc, GetAppFont(Handle()), colors.secondary, summary, m_summary);
    const CFont heading(StorageFont(Handle(), ScaleForDpi(14), FW_SEMIBOLD));
    DrawStorageText(dc, heading, colors.text, Localization::Lookup(titles[m_selected]), m_detailHeader);
}

void CStorageFileTypesPane::OnSelect(UINT id)
{
    m_selected = id - IDC_TYPES_CATEGORY_FIRST;
    UpdateSelection();
}

void CStorageFileTypesPane::OnBrowse()
{
    if (m_ready && m_categories[m_selected].files)
        CMainFrame::Get()->BrowseFileCategory(static_cast<StorageFileCategory>(m_selected));
}
