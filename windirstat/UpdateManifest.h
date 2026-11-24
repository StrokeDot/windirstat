// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

struct UpdateManifest
{
    std::array<unsigned, 4> version{};
    std::string versionText;
    std::size_t bytes{};
    std::string sha256;
};

inline constexpr std::size_t MaxSmartStorageInstallerSize = 64u * 1024u * 1024u;

inline std::optional<UpdateManifest> ParseUpdateManifest(std::string_view input)
{
    auto takeLine = [&input]
    {
        const auto end = input.find('\n');
        auto value = input.substr(0, end);
        input.remove_prefix(end == std::string_view::npos ? input.size() : end + 1);
        if (value.ends_with('\r')) value.remove_suffix(1);
        return value;
    };

    if (takeLine() != "SmartStorage-x64") return std::nullopt;
    auto versionText = takeLine();
    const auto bytesText = takeLine();
    const auto hash = takeLine();
    if (!input.empty() || hash.size() != 64 || !std::ranges::all_of(hash, [](const char ch)
        { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); })) return std::nullopt;

    UpdateManifest update;
    update.versionText = versionText;
    update.sha256 = hash;
    for (std::size_t index = 0; index < update.version.size(); ++index)
    {
        const auto separator = versionText.find('.');
        const auto field = versionText.substr(0, separator);
        if (field.empty() || (field.size() > 1 && field.front() == '0') ||
            ((separator == std::string_view::npos) != (index == update.version.size() - 1))) return std::nullopt;
        auto& value = update.version[index];
        const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
        if (error != std::errc{} || end != field.data() + field.size() || value > (index < 2 ? 255u : 65535u))
            return std::nullopt;
        versionText.remove_prefix(separator == std::string_view::npos ? field.size() : separator + 1);
    }
    if (bytesText.empty()) return std::nullopt;
    const auto [end, error] = std::from_chars(bytesText.data(), bytesText.data() + bytesText.size(), update.bytes);
    if (error != std::errc{} || end != bytesText.data() + bytesText.size() ||
        update.bytes == 0 || update.bytes > MaxSmartStorageInstallerSize) return std::nullopt;
    return update;
}
