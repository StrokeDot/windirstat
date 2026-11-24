# Smart Storage interface and file activity

Open `recent-files-preview.html` in a browser to try the layout with sample data.

## Interface

Use the compact navigation and tables from [StrokeDot/simplewall](https://github.com/StrokeDot/simplewall/tree/custom-base) as a visual reference. The proposed dark palette uses a restrained green accent for navigation, focus, and storage totals. Keep the scanner, file tree, and existing cleanup commands available from Disk Overview. Give Recent Files and App caches persistent navigation entries.

Use Added and Deleted views. Both show the file name, location, event time, size when known, and the app responsible when captured. In Deleted, show the original path, the last known size, and one of three statuses. Use Moved to Recycle Bin, Deleted permanently, or Deletion unconfirmed. Permanent deletion includes a direct removal that bypasses the bin and a later purge of a recycled item. Show the sum of known sizes alongside the count of files with unknown sizes. Label this as historical file size.

Support last hour, last 24 hours, and last 7 days, plus drive, minimum-size, and text filters. Sort by event time or size. A file opens a details area with its full path. The default view is Added, showing the last 24 hours with newest first.

## App and installer branding

Smart Storage is the displayed product name. Use the selected Open Box logo for the multi-size Windows executable icon, setup and installed shortcuts, and Windows' installed-app listing. Use the Open Box palette across the prototype and native dark theme. Use the accent for active navigation, focus, and storage totals. Check that the logo reads clearly at 16, 24, and 32 pixels and that text has sufficient contrast.

The current executable icon is declared in `windirstat/windirstat.rc` and the MSI points at the same icon through `setup/msi/Defines.wxi`. Update the product display name in the application resources and installer metadata. Verify the finished MSI and installed shortcuts on a machine that also has upstream WinDirStat installed.

## Scan progress

Show elapsed active scan time, percentage when the scanner has a work estimate, and an approximate time remaining once the measured rate can support one. Pause freezes elapsed active time and removes the estimate. If the work is unbounded, the estimate is unavailable, the scanner stalls, or progress is recalculated, show elapsed time while the estimate recovers. Show Finalizing results when the scan workers have stopped and Smart Storage is reconciling, sorting, and preparing results. Report stopped and incomplete scans separately from complete scans.

## App caches

Add an App caches view alongside Disk overview and Recent files. Use the current scan's local results so moving a cache to another scanned volume does not require another full disk walk. Show the application, cache type, confirmed location, drive, allocated size when known, and the likely effect of clearing it. Report scan omissions and size uncertainty. Keep the drive filter optional so the default view covers every scanned drive.

- A built-in rule needs evidence for app ownership and the cache type. Resolve Windows user folders with known-folder APIs and support relocated directories from reliable app configuration or a user-confirmed path. Treat path or size heuristics as candidates for review. [Windows known folders](https://learn.microsoft.com/en-us/windows/win32/shell/known-folders)
- List NVIDIA shader caches as Keep by default. NVIDIA documents the DXCache, GLCache, and NV_Cache locations. Rebuilding compiled shaders can temporarily cause game stutter. [NVIDIA shader cache settings](https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/3D%20Settings/Manage_3D_Settings_%28reference%29.htm) · [NVIDIA cache locations](https://nvidia.custhelp.com/app/answers/detail/a_id/5735)
- List a confirmed Adobe Premiere media cache as App cleanup. Prefer Premiere's built-in cleanup for unused cache, including when the app points to a different drive. Show the cache's size separately from the amount that Premiere can reclaim. Project files, source footage, and preview or export folders need their own ownership evidence. [Adobe media cache locations](https://helpx.adobe.com/premiere/desktop/troubleshooting/media-issues/manage-media-cache.html) · [Adobe cleanup controls](https://helpx.adobe.com/premiere/desktop/troubleshooting/media-issues/clear-media-cache-using-preferences.html)
- Mark unfamiliar cache-named folders Needs review. A large size, old timestamp, or a directory name alone gives insufficient evidence that the owning app can regenerate the files.
- For a future direct cleanup rule, require vendor-supported regeneration, an explicit item review, and verified file identities and destinations at the time of action. Bypass Windows junctions and other reparse points, locked or changed files, and unavailable volumes. Prefer reversible cleanup where supported. Show which files were skipped and report actual recovered space after a rescan. [Windows reparse points](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points)

Validate the same app on two drives, a relocated cache, an unrelated folder named Cache, active and locked items, a junction outside a cache, duplicate paths to the same volume, shared or hard-linked files, partial scans, and a cache that is missing or has moved since detection. Start with vendor-managed cleanup actions and confirmed detection for the documented NVIDIA and Adobe cases.

## Windows implementation

1. Reuse the upstream watcher for opted-in local roots. Move its event collection and retained history out of the list control so collection can continue across view changes and rescans while the app runs. Keep the existing diagnostic watcher behavior available. Record observed timestamps and explicit gaps caused by overflow, inaccessible roots, and time when collection was off.
2. Persist activity locally in a bounded store. Index timestamps and reconcile file identity and renamed paths so a temporary browser download and its final rename count once. Preserve the original added time when a file is moved within the watched scope. Add a row for a move into the scope and label it accordingly. Refresh file metadata while a new file grows. File counts and current sizes should deduplicate hard links where identity is known. Report allocated bytes separately when available and make unknown sizes visible.
3. Investigate NTFS USN journal backfill for time when the app is closed. Track each opted-in volume's journal ID and last cursor. Show the available coverage when the journal has wrapped, been recreated, or is inaccessible. Explain that pre-monitoring results from creation timestamps are estimates if used. Running continuous collection at Windows sign-in would be a separate opt-in.
4. Capture writing app information prospectively through Windows process-aware file telemetry, with an explicit enablement and a bounded, local event store. Evaluate ETW and a Sysmon event source. Only display a source based on a matched event. A writing process identifies the app observed touching the file, and download URLs or a copy's original path require separate evidence. Keep unknown source visible.

## Deletion tracking

- Track removals in the same bounded, local history. Seed file identity, original path, and size from available scan or watcher metadata. A removal notification may arrive after the path disappears. Preserve fields already captured and show unknown values when metadata cannot be resolved.
- Correlate filesystem events with renames, moves, and Recycle Bin evidence by file identity where available. A directory removal notification alone is insufficient to distinguish moves out of scope from permanent deletion. Mark uncertain events as Deletion unconfirmed and omit confirmed ordinary moves from Deleted.
- Record the initial removal time and, if a recycled item is later purged, update the same entry with the permanent deletion time. Keep both timestamps so a later purge can still appear in the recent window. Handle restores and repeated removals as distinct activity. Deduplicate watcher and journal observations. Attribute removing apps and writing apps to their own observed events.
- Read available NTFS journal deletion and rename records on opted-in volumes to backfill gaps while the app was closed. Display missing coverage if records aged out, the journal changed, or a volume was inaccessible. Preserve any resolvable name and parent information when the original directory no longer exists. Journal history can document the event even when the file's contents cannot be recovered.

## Restore and recovery from Deleted

Keep the deletion event after restore or recovery. Show the original path, last known size, source volume, deletion timestamp, purge timestamp when available, and the action outcome. Display when each monitored location started reporting history and show gaps when coverage is uncertain. The history stores file metadata. Preserving file contents requires a separate backup. [Windows change journal records](https://learn.microsoft.com/en-us/windows/win32/fileio/change-journal-records)

- **In Recycle Bin.** Show Restore after verifying that the current user's Recycle Bin still contains the selected item. Use the Windows restore action for that item and prompt before writing. If the original path is occupied, preserve the existing file and offer a different location. Recheck the item before acting. If it was purged after review, update the row and offer recovery. Mark it Restored after verifying the resulting file. [Windows Recycle Bin restoration](https://support.microsoft.com/en-gb/onedrive/restore-your-onedrive-files)
- **Deleted permanently.** If a backup is available, direct the user to the original folder's previous versions or the backup provider. Otherwise offer Try recovery, passing the recorded volume and file name as a filter to the existing Recovery dialog. Revalidate the volume and present candidate matches before any copy. Recommend a separate physical drive for output and preserve existing destination files. Report recovered, partial, failed, or canceled outcomes based on the recovery result. Record a deletion as recoverable only when a verified copy or valid candidate supports it. Overwriting, SSD TRIM, or reused metadata can prevent recovery. [File History](https://support.microsoft.com/en-us/windows/experience/backup-recovery/backup-and-restore-with-file-history) · [Existing recovery engine](https://github.com/windirstat/windirstat/wiki/Recovering-Files) · [Windows File Recovery](https://support.microsoft.com/en-us/windows/experience/backup-recovery/windows-file-recovery)
- **Deletion unconfirmed.** Show the observed disappearance and any known original path. Keep the restore action unavailable while the evidence is insufficient. A rename or a correlated move should update the event instead of appearing as a confirmed deletion.

Validate a recycled file and a same-name file from a different folder, a file purged while selected, a name collision at the original location, a duplicate volume mount, an offline source, a separate recovery drive, partial file data, a cancellation, and a restoration followed by a second deletion. Exercise raw recovery against disposable volumes with known file contents. Keep intentional backups of important files independent of the deletion history.

## Verification before a Windows build is ready

Exercise file creation, a download that grows and is renamed, an in-scope move, a move in and out, app restart, large bursts, inaccessible folders, and a time window with zero results. Confirm watched folder and drive changes do not clear retained history. Check allocated and logical size on compressed or sparse files, and verify cleanup commands under the redesigned navigation.

For Deleted, test Explorer's Recycle Bin, Shift+Delete, an app deleting directly, emptying the bin, restoring and deleting again, an ordinary move that must not become a confirmed deletion, a file with unknown size or app, and deletion while the app is closed with both available and missing journal coverage. Check that a bin purge updates the entry and appears under the correct time filter while the file count stays accurate.
