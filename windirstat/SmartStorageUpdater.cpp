// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pch.h"
#include "SmartStorageUpdater.h"
#include "UpdateManifest.h"
#include "Version.h"
#include "Dialogs/MessageBoxDlg.h"
#include "Dialogs/ProgressDlg.h"
#include <msi.h>
#include <msiquery.h>
#include <winhttp.h>

#pragma comment(lib, "msi.lib")
#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr std::wstring_view releaseHome = L"https://github.com/StrokeDot/windirstat/releases";
    constexpr auto manifestUrl = L"https://github.com/StrokeDot/windirstat/releases/latest/download/SmartStorage-x64.update";
    constexpr auto upgradeCode = L"{F254AD7A-05B6-498A-A01D-DC845C134B59}";
    constexpr std::array<unsigned, 4> currentVersion{ PRD_MAJVER, PRD_MINVER, PRD_PATCH, PRD_BUILD };

    struct UpdateCheck
    {
        std::future<std::optional<UpdateManifest>> result;
        bool started = false;
        bool interactive = false;
    };

    UpdateCheck check;

    std::optional<std::vector<char>> Download(std::wstring url, const size_t limit, CProgressDlg* progress = nullptr)
    {
        SmartPointer session(WinHttpCloseHandle, WinHttpOpen(L"Smart Storage Updater/1",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session) session = WinHttpOpen(L"Smart Storage Updater/1",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return std::nullopt;
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
        {
            protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
            if (!WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols)))
                return std::nullopt;
        }
        WinHttpSetTimeouts(session, 5000, 5000, 8000, 8000);
        const ULONGLONG deadline = GetTickCount64() + 60000;

        for (int redirects = 0; redirects <= 5; ++redirects)
        {
            if (progress && progress->IsCancelled()) return std::nullopt;
            URL_COMPONENTS parts{ .dwStructSize = sizeof(parts), .dwHostNameLength = DWORD(-1),
                .dwUserNameLength = DWORD(-1), .dwPasswordLength = DWORD(-1),
                .dwUrlPathLength = DWORD(-1), .dwExtraInfoLength = DWORD(-1) };
            if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS ||
                parts.nPort != INTERNET_DEFAULT_HTTPS_PORT || parts.dwUserNameLength || parts.dwPasswordLength ||
                !parts.dwHostNameLength || !parts.dwUrlPathLength) return std::nullopt;

            const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            if (host != L"github.com" && host != L"release-assets.githubusercontent.com" &&
                host != L"objects.githubusercontent.com") return std::nullopt;
            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

            SmartPointer connection(WinHttpCloseHandle, WinHttpConnect(session, host.c_str(), parts.nPort, 0));
            if (!connection) return std::nullopt;
            SmartPointer request(WinHttpCloseHandle, WinHttpOpenRequest(connection, L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE | WINHTTP_FLAG_ESCAPE_DISABLE | WINHTTP_FLAG_ESCAPE_DISABLE_QUERY));
            if (!request) return std::nullopt;
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            DWORD logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
            if (!WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy)) ||
                !WinHttpSetOption(request, WINHTTP_OPTION_AUTOLOGON_POLICY, &logon, sizeof(logon)) ||
                !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(request, nullptr)) return std::nullopt;

            DWORD status{}, statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) return std::nullopt;
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
            {
                std::array<wchar_t, 8192> next{};
                DWORD length = sizeof(next);
                if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                    next.data(), &length, WINHTTP_NO_HEADER_INDEX)) return std::nullopt;
                url = next.data();
                if (url.starts_with(L"/") && !url.starts_with(L"//")) url = L"https://" + host + url;
                continue;
            }
            if (status != 200) return std::nullopt;

            std::vector<char> bytes;
            std::array<char, 16384> chunk;
            while (GetTickCount64() < deadline)
            {
                if (progress && progress->IsCancelled()) return std::nullopt;
                DWORD count{};
                if (!WinHttpReadData(request, chunk.data(), static_cast<DWORD>(chunk.size()), &count))
                    return std::nullopt;
                if (count == 0) return bytes;
                if (count > limit - bytes.size()) return std::nullopt;
                bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
                if (progress) progress->SetProgress(bytes.size());
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::string Sha256(const std::vector<char>& bytes)
    {
        BCRYPT_ALG_HANDLE algorithm{};
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) return {};
        SmartPointer algorithmOwner([](const BCRYPT_ALG_HANDLE h) { BCryptCloseAlgorithmProvider(h, 0); }, algorithm);
        DWORD objectLength{}, resultLength{};
        if (!BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &resultLength, 0))) return {};
        std::vector<UCHAR> storage(objectLength);
        BCRYPT_HASH_HANDLE hash{};
        if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, storage.data(), objectLength, nullptr, 0, 0))) return {};
        SmartPointer hashOwner(BCryptDestroyHash, hash);
        std::array<UCHAR, 32> digest{};
        if (!BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                static_cast<ULONG>(bytes.size()), 0)) ||
            !BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0))) return {};
        std::string hex;
        hex.reserve(digest.size() * 2);
        for (const UCHAR byte : digest) hex += std::format("{:02x}", byte);
        return hex;
    }

    bool PackageMatches(const std::wstring& file, const std::wstring& expectedVersion)
    {
        MSIHANDLE rawDatabase{};
        if (MsiOpenDatabaseW(file.c_str(), MSIDBOPEN_READONLY, &rawDatabase) != ERROR_SUCCESS) return false;
        PMSIHANDLE database(rawDatabase);

        MSIHANDLE rawSummary{};
        if (MsiGetSummaryInformationW(database, nullptr, 0, &rawSummary) != ERROR_SUCCESS) return false;
        PMSIHANDLE summary(rawSummary);
        std::array<wchar_t, 512> platform{};
        DWORD max = static_cast<DWORD>(platform.size());
        if (MsiSummaryInfoGetPropertyW(summary, 7, nullptr, nullptr, nullptr, platform.data(), &max) != ERROR_SUCCESS ||
            !std::wstring_view(platform.data()).starts_with(L"x64;")) return false;

        MSIHANDLE rawView{};
        if (MsiDatabaseOpenViewW(database, L"SELECT `Property`, `Value` FROM `Property`", &rawView) != ERROR_SUCCESS)
            return false;
        PMSIHANDLE view(rawView);
        if (MsiViewExecute(view, 0) != ERROR_SUCCESS) return false;
        bool validVersion = false;
        bool validUpgrade = false;
        MSIHANDLE rawRecord{};
        while (MsiViewFetch(view, &rawRecord) == ERROR_SUCCESS)
        {
            PMSIHANDLE record(rawRecord);
            std::array<wchar_t, 256> property{}, value{};
            DWORD propertySize = static_cast<DWORD>(property.size()), valueSize = static_cast<DWORD>(value.size());
            if (MsiRecordGetStringW(record, 1, property.data(), &propertySize) != ERROR_SUCCESS ||
                MsiRecordGetStringW(record, 2, value.data(), &valueSize) != ERROR_SUCCESS) continue;
            if (std::wstring_view(property.data()) == L"ProductVersion") validVersion = expectedVersion == value.data();
            if (std::wstring_view(property.data()) == L"UpgradeCode") validUpgrade = _wcsicmp(value.data(), upgradeCode) == 0;
        }
        return validVersion && validUpgrade;
    }

    std::wstring StagePackage(const std::vector<char>& bytes)
    {
        std::array<wchar_t, MAX_PATH> temp{};
        const DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
        GUID id{};
        std::array<wchar_t, 40> guid{};
        if (!length || length >= temp.size() || FAILED(CoCreateGuid(&id)) ||
            !StringFromGUID2(id, guid.data(), static_cast<int>(guid.size()))) return {};
        const std::wstring path = std::format(L"{}SmartStorageUpdate-{}.msi", temp.data(), guid.data());
        SmartPointer file(CloseHandle, CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY, nullptr));
        if (!file.IsValid()) return {};
        DWORD written{};
        const bool valid = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(file);
        file.Release();
        if (valid) return path;
        DeleteFileW(path.c_str());
        return {};
    }

    bool IsRegisteredInstall()
    {
        std::array<wchar_t, 32768> registered{};
        DWORD size = sizeof(registered);
        return RegGetValueW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\SmartStorage.exe", nullptr,
            RRF_RT_REG_SZ, nullptr, registered.data(), &size) == ERROR_SUCCESS &&
            _wcsicmp(registered.data(), GetAppFileName().c_str()) == 0;
    }

    void ShowMessage(CWnd& parent, const std::wstring_view key)
    {
        CMessageBoxDlg::Show(Localization::Lookup(key), MB_OK, &parent);
    }

    void OfferUpdate(CWnd& parent, const UpdateManifest& update)
    {
        const std::wstring version(update.versionText.begin(), update.versionText.end());
        if (!IsRegisteredInstall())
        {
            if (CMessageBoxDlg::Show(Localization::Format(IDS_UPDATE_PORTABLEs, version),
                MB_YESNO | MB_ICONINFORMATION, &parent) == IDYES)
                ShellExecuteW(parent.Handle(), L"open", std::wstring(releaseHome).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }

        if (CMessageBoxDlg::Show(Localization::Format(IDS_UPDATE_AVAILABLEs, version),
            MB_YESNO | MB_ICONQUESTION, &parent) != IDYES) return;

        std::wstring file;
        CProgressDlg progress(update.bytes, CProgressDlg::Flags::None, &parent, [&](CProgressDlg* dlg)
        {
            try
            {
                const auto bytes = Download(std::format(
                    L"{}/download/smart-storage-v{}/SmartStorage-x64.msi", releaseHome, version), update.bytes, dlg);
                if (bytes && bytes->size() == update.bytes && !dlg->IsCancelled() && Sha256(*bytes) == update.sha256)
                    file = StagePackage(*bytes);
            }
            catch (...) { }
        });
        progress.ShowModal();
        if (progress.WasCancelled())
        {
            if (!file.empty()) DeleteFileW(file.c_str());
            return;
        }
        if (file.empty() || !PackageMatches(file, version))
        {
            if (!file.empty()) DeleteFileW(file.c_str());
            ShowMessage(parent, IDS_UPDATE_VERIFY_FAILED);
            return;
        }

        auto folder = GetAppFolder();
        while (folder.size() > 3 && folder.ends_with(L'\\')) folder.pop_back();
        // Quote the directory without leaving a trailing backslash before its closing quote.
        if (folder.ends_with(L'\\')) folder += L".";
        std::array<wchar_t, MAX_PATH> systemDirectory{};
        const UINT length = GetSystemDirectoryW(systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
        if (!length || length >= systemDirectory.size())
        {
            DeleteFileW(file.c_str());
            ShowMessage(parent, IDS_UPDATE_INSTALL_FAILED);
            return;
        }
        const std::wstring installer = std::format(L"{}\\msiexec.exe", systemDirectory.data());
        const std::wstring arguments = std::format(L"/i \"{}\" INSTALLFOLDER=\"{}\" /norestart", file, folder);
        const auto launched = ShellExecuteW(parent.Handle(), L"open", installer.c_str(), arguments.c_str(), nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(launched) <= 32)
        {
            DeleteFileW(file.c_str());
            ShowMessage(parent, IDS_UPDATE_INSTALL_FAILED);
            return;
        }
        parent.PostMessage(WM_CLOSE);
    }

    void StartCheck(const bool interactive)
    {
        check.started = true;
        check.interactive |= interactive;
        if (check.result.valid()) return;
        std::packaged_task<std::optional<UpdateManifest>()> task([]
        {
            const auto bytes = Download(manifestUrl, 1024);
            return bytes ? ParseUpdateManifest(std::string_view(bytes->data(), bytes->size())) : std::nullopt;
        });
        auto result = task.get_future();
        std::thread(std::move(task)).detach();
        check.result = std::move(result);
    }
}

void SmartStorageUpdater::CheckNow(CWnd& parent)
{
#ifdef _M_X64
    try { StartCheck(true); }
    catch (...) { ShowMessage(parent, IDS_UPDATE_CHECK_FAILED); }
#else
    ShowMessage(parent, IDS_UPDATE_X64_ONLY);
#endif
}

void SmartStorageUpdater::Poll(CWnd& parent, const bool canPrompt)
{
#ifdef _M_X64
    if (!check.started)
    {
        try { StartCheck(false); }
        catch (...) { }
    }
    if (!canPrompt || !check.result.valid() ||
        check.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const bool interactive = std::exchange(check.interactive, false);
    std::optional<UpdateManifest> update;
    try { update = check.result.get(); }
    catch (...) { }
    if (!update)
    {
        if (interactive) ShowMessage(parent, IDS_UPDATE_CHECK_FAILED);
    }
    else if (update->version > currentVersion) OfferUpdate(parent, *update);
    else if (interactive) ShowMessage(parent, IDS_UPDATE_CURRENT);
#else
    (void)parent;
    (void)canPrompt;
#endif
}
