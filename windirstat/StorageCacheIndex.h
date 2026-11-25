// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class CItem;

struct StorageCache
{
    std::wstring path;
    std::optional<unsigned long long> bytes;
    const wchar_t* application;
    std::wstring_view guidance;
    const wchar_t* helpUrl;
};

std::vector<StorageCache> FindProfileCaches();
std::vector<StorageCache> FindScannedCaches(const CItem& root);
