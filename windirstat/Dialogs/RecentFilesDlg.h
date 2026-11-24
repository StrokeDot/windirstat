// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

class CWnd;

std::unique_ptr<CWnd> CreateRecentFilesPane(CWnd* parent);
void RefreshRecentFilesPane(CWnd& pane);
