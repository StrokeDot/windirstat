// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "StorageCacheIndex.h"

namespace
{
    enum class Location { Local, Roaming, Scan };

    struct CacheRule
    {
        Location location;
        std::wstring_view container;
        std::wstring_view folder;
        const wchar_t* application;
        std::wstring_view guidance;
        const wchar_t* helpUrl = nullptr;
        bool profiles = false;
    };

    constexpr auto keep = IDS_CACHE_KEEP;
    constexpr auto inApp = IDS_CACHE_IN_APP;
    constexpr auto shader = IDS_CACHE_SHADER;
    constexpr auto nvidia = L"https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/3D%20Settings/Manage_3D_Settings_%28reference%29.htm";
    constexpr auto adobe = L"https://helpx.adobe.com/premiere/desktop/troubleshooting/media-issues/clear-media-cache-using-preferences.html";
    constexpr auto chrome = L"https://support.google.com/chrome/answer/2392709";
    constexpr auto edge = L"https://support.microsoft.com/microsoft-edge/view-and-delete-browser-history-in-microsoft-edge-00cf7943-a9e1-975a-a33d-ac10ce454ca4";
    constexpr auto firefox = L"https://support.mozilla.org/kb/how-clear-firefox-cache";
    constexpr auto teams = L"https://learn.microsoft.com/microsoftteams/troubleshoot/teams-administration/clear-teams-cache";

    constexpr std::array rules
    {
        CacheRule{ Location::Local, L"NVIDIA", L"DXCache", L"NVIDIA", shader, nvidia },
        CacheRule{ Location::Local, L"NVIDIA", L"GLCache", L"NVIDIA", shader, nvidia },
        CacheRule{ Location::Local, L"NVIDIA Corporation", L"NV_Cache", L"NVIDIA", shader, nvidia },
        CacheRule{ Location::Local, L"AMD", L"DxCache", L"AMD", shader },
        CacheRule{ Location::Local, L"AMD", L"GLCache", L"AMD", shader },
        CacheRule{ Location::Local, L"", L"D3DSCache", L"DirectX", shader },
        CacheRule{ Location::Roaming, L"Adobe\\Common", L"Media Cache", L"Adobe Premiere", inApp, adobe },
        CacheRule{ Location::Roaming, L"Adobe\\Common", L"Media Cache Files", L"Adobe Premiere", inApp, adobe },
        CacheRule{ Location::Scan, L"", L"CacheClip", L"DaVinci Resolve", IDS_CACHE_RENDER,
            L"https://documents.blackmagicdesign.com/UserManuals/DaVinci_Resolve_17_Reference_Manual.pdf" },
        CacheRule{ Location::Scan, L"steamapps", L"shadercache", L"Steam", shader },
        CacheRule{ Location::Local, L"Google\\Chrome\\User Data", L"Cache", L"Google Chrome", inApp, chrome, true },
        CacheRule{ Location::Local, L"Google\\Chrome\\User Data", L"Code Cache", L"Google Chrome", inApp, chrome, true },
        CacheRule{ Location::Local, L"Microsoft\\Edge\\User Data", L"Cache", L"Microsoft Edge", inApp, edge, true },
        CacheRule{ Location::Local, L"Microsoft\\Edge\\User Data", L"Code Cache", L"Microsoft Edge", inApp, edge, true },
        CacheRule{ Location::Local, L"BraveSoftware\\Brave-Browser\\User Data", L"Cache", L"Brave", inApp, nullptr, true },
        CacheRule{ Location::Local, L"Vivaldi\\User Data", L"Cache", L"Vivaldi", inApp, nullptr, true },
        CacheRule{ Location::Local, L"Mozilla\\Firefox\\Profiles", L"cache2", L"Mozilla Firefox", inApp, firefox, true },
        CacheRule{ Location::Roaming, L"discord", L"Cache", L"Discord", keep },
        CacheRule{ Location::Roaming, L"discord", L"Code Cache", L"Discord", keep },
        CacheRule{ Location::Roaming, L"discord", L"GPUCache", L"Discord", keep },
        CacheRule{ Location::Roaming, L"Slack", L"Cache", L"Slack", inApp },
        CacheRule{ Location::Roaming, L"Code", L"Cache", L"Visual Studio Code", keep },
        CacheRule{ Location::Roaming, L"Code", L"Code Cache", L"Visual Studio Code", keep },
        CacheRule{ Location::Roaming, L"Microsoft\\Teams", L"Cache", L"Microsoft Teams", inApp, teams },
        CacheRule{ Location::Local, L"Packages\\MSTeams_8wekyb3d8bbwe\\LocalCache\\Microsoft\\MSTeams\\EBWebView\\Default",
            L"Cache", L"Microsoft Teams", inApp, teams },
    };

    bool SamePath(std::wstring_view a, std::wstring_view b)
    {
        return a.size() == b.size() && (a.empty() || CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
            b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL);
    }

    std::wstring_view LastComponent(std::wstring_view path)
    {
        if (const auto i = path.find_last_of(L"\\"); i != std::wstring_view::npos) path.remove_prefix(i + 1);
        return path;
    }

    std::wstring_view Parent(std::wstring_view path)
    {
        const auto i = path.find_last_of(L"\\");
        return i == std::wstring_view::npos ? std::wstring_view{} : path.substr(0, i);
    }

    bool MatchesContainer(const CacheRule& rule, std::wstring_view path)
    {
        if (rule.container.empty()) return true;
        auto base = Parent(path);
        if (rule.profiles) base = Parent(base);
        if (base.size() < rule.container.size()) return false;
        const auto start = base.size() - rule.container.size();
        if (!SamePath(base.substr(start), rule.container) || (start && base[start - 1] != L'\\')) return false;
        // Generic profile paths such as "Code\Cache" must be under AppData.
        return rule.location == Location::Scan || rule.container.find(L'\\') != std::wstring_view::npos ||
            (start && (SamePath(LastComponent(Parent(base)), L"Roaming") ||
                       SamePath(LastComponent(Parent(base)), L"Local")));
    }

    StorageCache MakeCache(const CacheRule& rule, std::wstring path, std::optional<ULONGLONG> bytes = {})
    {
        return { std::move(path), bytes, rule.application, rule.guidance, rule.helpUrl };
    }
}

std::vector<StorageCache> FindProfileCaches()
{
    CComHeapPtr<wchar_t> local, roaming;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &local);
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DONT_VERIFY, nullptr, &roaming);
    std::vector<StorageCache> caches;
    auto append = [&caches](const CacheRule& rule, const std::filesystem::path& base)
    {
        const auto path = (base / rule.folder).wstring();
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
            caches.push_back(MakeCache(rule, path));
    };
    for (const auto& rule : rules)
    {
        if (rule.location == Location::Scan) continue;
        const wchar_t* profile = rule.location == Location::Local ? local.m_pData : roaming.m_pData;
        if (!profile) continue;
        const auto base = std::filesystem::path(profile) / rule.container;
        if (!rule.profiles)
        {
            append(rule, base);
            continue;
        }
        std::error_code error;
        for (std::filesystem::directory_iterator it(base, error), end; !error && it != end; it.increment(error))
            if (it->is_directory(error)) append(rule, it->path());
    }
    return caches;
}

std::vector<StorageCache> FindScannedCaches(const CItem& root)
{
    std::vector<StorageCache> caches;
    std::vector pending{ &root };
    while (!pending.empty())
    {
        const auto* item = pending.back();
        pending.pop_back();
        if (item->IsTypeOrFlag(ITF_REMOTE, ITF_MTP, ITF_WSL)) continue;
        if (item->IsTypeOrFlag(IT_DIRECTORY))
        {
            auto name = item->GetNameView();
            while (name.ends_with(L'\\')) name.remove_suffix(1);
            name = LastComponent(name);
            std::wstring path;
            for (const auto& rule : rules)
            {
                if (!SamePath(name, rule.folder)) continue;
                if (path.empty())
                {
                    path = item->GetPath();
                    while (path.ends_with(L'\\')) path.pop_back();
                }
                if (path.empty() || !MatchesContainer(rule, path)) continue;
                const auto bytes = item->IsDone() && item->HasSizePhysical() &&
                    item->GetScanResult().outcome == ScanOutcome::Complete
                    ? std::optional{ item->GetSizePhysical() } : std::nullopt;
                caches.push_back(MakeCache(rule, std::move(path), bytes));
                break;
            }
        }
        if (!item->IsLeaf())
            for (const auto* child : item->GetChildren())
                if (child->IsTypeOrFlag(IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY)) pending.push_back(child);
    }
    return caches;
}
