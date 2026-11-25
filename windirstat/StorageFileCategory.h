// Smart Storage
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <string_view>

enum class StorageFileCategory : unsigned
{
    Video, Audio, Pictures, Documents, Archives, Software, DiskImages, Other, Count
};

inline StorageFileCategory CategorizeExtension(const std::wstring_view extension)
{
    // The scanner normalizes extensions to lowercase, including the leading dot.
    constexpr std::array groups
    {
        std::wstring_view(L"|.mp4|.mkv|.webm|.avi|.mov|.m4v|.wmv|.mpg|.mpeg|.m2ts|.mts|.vob|.flv|.3gp|.mxf|.rmvb|"),
        std::wstring_view(L"|.mp3|.flac|.wav|.m4a|.aac|.ogg|.opus|.wma|.alac|.aif|.aiff|.amr|.mid|.midi|.ape|"),
        std::wstring_view(L"|.jpg|.jpeg|.png|.webp|.gif|.bmp|.tif|.tiff|.heic|.heif|.avif|.svg|.ico|.dng|.cr2|.cr3|.nef|.arw|.psd|.xcf|"),
        std::wstring_view(L"|.pdf|.txt|.rtf|.md|.doc|.docx|.xls|.xlsx|.ppt|.pptx|.odt|.ods|.odp|.csv|.epub|.mobi|.azw3|"),
        std::wstring_view(L"|.zip|.7z|.rar|.tar|.gz|.bz2|.xz|.tgz|.zst|.cab|"),
        std::wstring_view(L"|.exe|.msi|.msix|.msixbundle|.appx|.appxbundle|.dll|.sys|.com|.apk|.dmg|"),
        std::wstring_view(L"|.iso|.img|.vhd|.vhdx|.vmdk|.vdi|.qcow2|.wim|.esd|.chd|.nrg|"),
    };
    if (extension.empty() || extension.find(L'|') != std::wstring_view::npos) return StorageFileCategory::Other;
    for (unsigned i = 0; i < groups.size(); ++i)
    {
        const auto group = groups[i];
        const auto pos = group.find(extension);
        if (pos != std::wstring_view::npos && pos > 0 && group[pos - 1] == L'|' &&
            pos + extension.size() < group.size() && group[pos + extension.size()] == L'|')
            return static_cast<StorageFileCategory>(i);
    }
    return StorageFileCategory::Other;
}
