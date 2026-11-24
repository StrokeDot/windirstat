# Smart Storage interface and file activity

Open `recent-files-preview.html` in a browser to try the layout with sample data.

## Interface

Use the compact navigation and tables from [StrokeDot/simplewall](https://github.com/StrokeDot/simplewall/tree/custom-base) as a visual reference. The proposed dark palette uses a restrained green accent for navigation, focus, and storage totals. Keep the scanner, file tree, treemap, and existing cleanup commands available from Disk Overview. Recent Files gets its own persistent navigation entry.

Use Added and Deleted views. Both show the file name, location, event time, size when known, and the app responsible when captured. In Deleted, show the original path, the last known size, and one of three statuses. Use Moved to Recycle Bin, Deleted permanently, or Deletion unconfirmed. Permanent deletion includes a direct removal that bypasses the bin and a later purge of a recycled item. Show the sum of known sizes alongside the count of files with unknown sizes. Label this as historical file size.

Support last hour, last 24 hours, and last 7 days, plus drive, minimum-size, and text filters. Sort by event time or size. A file opens a details area with its full path. The default view is Added, showing the last 24 hours with newest first.

## App and installer branding

Smart Storage is the displayed product name. Use the selected logo for the multi-size Windows executable icon, setup and installed shortcuts, and Windows' installed-app listing. Apply the selected logo and palette to the prototype before carrying them into the desktop interface. Use the accent for active navigation, focus, and storage totals. Check that the logo reads clearly at 16, 24, and 32 pixels and that text has sufficient contrast.

The current executable icon is declared in `windirstat/windirstat.rc` and the MSI points at the same icon through `setup/msi/Defines.wxi`. Update the product display name in the application resources and installer metadata. Verify the finished MSI and installed shortcuts on a machine that also has upstream WinDirStat installed.

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

## Verification before a Windows build is ready

Exercise file creation, a download that grows and is renamed, an in-scope move, a move in and out, app restart, large bursts, inaccessible folders, and a time window with zero results. Confirm watched folder and drive changes do not clear retained history. Check allocated and logical size on compressed or sparse files, and verify cleanup commands under the redesigned navigation.

For Deleted, test Explorer's Recycle Bin, Shift+Delete, an app deleting directly, emptying the bin, restoring and deleting again, an ordinary move that must not become a confirmed deletion, a file with unknown size or app, and deletion while the app is closed with both available and missing journal coverage. Check that a bin purge updates the entry and appears under the correct time filter while the file count stays accurate.
