# Recent Files and interface redesign

Open `recent-files-preview.html` in a browser to try the layout with sample data.

## Interface

Use the compact navigation and tables from [StrokeDot/simplewall](https://github.com/StrokeDot/simplewall/tree/custom-base) as a visual reference. The proposed dark palette uses a restrained green accent for navigation, focus, and storage totals. Keep the scanner, file tree, treemap, and existing cleanup commands available from Disk Overview. Recent Files gets its own persistent navigation entry.

Show the file name, location, current size, when it was added, and the writing app when captured. Support last hour, last 24 hours, and last 7 days, plus drive, minimum-size, and text filters. Sort by added time or size. A file opens a details area with its full path. Initial selection is the last 24 hours ordered by newest first.

## Windows implementation

1. Reuse the upstream watcher for opted-in local roots. Move its event collection and retained history out of the list control so collection can continue across view changes and rescans while the app runs. Keep the existing diagnostic watcher behavior available. Record observed timestamps and explicit gaps caused by overflow, inaccessible roots, and time when collection was off.
2. Persist activity locally in a bounded store. Index timestamps and reconcile file identity and renamed paths so a temporary browser download and its final rename count once. Preserve the original added time when a file is moved within the watched scope. Add a row for a move into the scope and label it accordingly. Refresh file metadata while a new file grows. File counts and current sizes should deduplicate hard links where identity is known. Report allocated bytes separately when available and make unknown sizes visible.
3. Investigate NTFS USN journal backfill for time when the app is closed. Track each opted-in volume's journal ID and last cursor. Show the available coverage when the journal has wrapped, been recreated, or is inaccessible. Explain that pre-monitoring results from creation timestamps are estimates if used. Running continuous collection at Windows sign-in would be a separate opt-in.
4. Capture writing app information prospectively through Windows process-aware file telemetry, with an explicit enablement and a bounded, local event store. Evaluate ETW and a Sysmon event source. Only display a source based on a matched event. A writing process identifies the app observed touching the file, and download URLs or a copy's original path require separate evidence. Keep unknown source visible.

## Verification before a Windows build is ready

Exercise file creation, a download that grows and is renamed, an in-scope move, a move in and out, deletion, app restart, large bursts, inaccessible folders, and a time window with zero results. Confirm watched folder and drive changes do not clear retained history. Check allocated and logical size on compressed or sparse files, and verify cleanup commands under the redesigned navigation.
