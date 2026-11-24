// WinDirStat - Windows Directory Statistics
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class CWnd;

namespace SmartStorageUpdater
{
    void CheckNow(CWnd& parent);
    void Poll(CWnd& parent, bool canPrompt);
}
