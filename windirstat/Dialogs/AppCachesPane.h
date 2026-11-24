// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <memory>

class CWnd;
std::unique_ptr<CWnd> CreateAppCachesPane(CWnd* parent);
void RefreshAppCachesPane(CWnd& pane);
