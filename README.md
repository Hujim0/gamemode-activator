# Game State Monitor

A small, headless Windows C program that detects foreground fullscreen activity
and publishes it to a state file. WSL reads the same file through a symlink:
no Linux daemon, PowerShell polling, logs, or marker process is needed.

Fork of [Marcelo's Rainmeter Game Mode Activator](https://github.com/marcelofrau/rainmeter-gamemode-activator),
adapted for sharing game state with WSL. Rainmeter is not required. This fork
replaces the original tray app and process marker with a file interface;
existing Rainmeter skins that watch `gamemode_active.exe` need updating.

## State file

`gamestatus` is created beside `gamestate-monitor.exe`:

```ini
active=true
game="ZenlessZoneZero"
updated_at=1792618200
```

- `active=true`: the foreground fullscreen window passed the exclusions.
- `active=false`: no qualifying fullscreen window; `game` is empty.
- `active=unknown`: the detector is ending through a normal window close or
  Windows session shutdown. Forced termination cannot update the file.
- `game`: the window title, encoded as UTF-8. Double quotes, backslashes,
  dollar signs, and backticks are escaped; control characters become spaces.
- `updated_at`: Unix timestamp in UTC, refreshed at least every 30 seconds
  while the detector is running successfully.

The default detection interval is two seconds. State or title changes are
written at the next sample; otherwise only the heartbeat rewrites the file.
Each update replaces a temporary file in the same directory, so readers see
one complete snapshot. A failed write is retried on the next sample.

Consumers should treat a missing file, a timestamp older than 90 seconds, or a
future timestamp after a clock adjustment as unknown. Check freshness before
using `active`: the last snapshot survives crashes and Windows restarts.
The file is data; parse the fields rather than executing it as a shell script.

## Build

Use a MinGW-w64 C compiler. No resource compiler or third party libraries are
needed. Builds produce `dist/gamestate-monitor.exe`.

On Windows, with MinGW-w64 `gcc` on PATH:

```powershell
.\build.ps1
# Or select a compiler:
.\build.ps1 -Compiler clang
```

The selected compiler must target Windows and provide MinGW-w64 headers and
libraries. A native Linux compiler cannot build this Windows application.

Cross compile from Linux:

```sh
make
# Or with llvm-mingw:
make CC=/path/to/llvm-mingw/bin/x86_64-w64-mingw32-clang
```

To cross compile in Docker without installing MinGW-w64 on the host, use Docker
Buildx:

```sh
make build-docker
```

This builds the Windows x64 executable and its blacklist data file into
`dist-docker/`. Docker is only the build environment; the result is a regular
Windows executable, not a containerized service. Existing files under `dist/`
are left as-is. The default builder platform is Linux x64; to run the builder on
another Docker architecture, use `make build-docker DOCKER_PLATFORM=linux/arm64`.
The builder uses the official Debian Trixie image and its
[MinGW-w64 x64 POSIX C compiler package](https://packages.debian.org/trixie/gcc-mingw-w64-x86-64-posix).
The scratch output stage exports only the executable and blacklist file; no
runtime image is created.

Manual build from the repository root:

```sh
mkdir -p dist
x86_64-w64-mingw32-gcc -Os -Wall -Wextra -Wpedantic -std=c11 \
  -s -municode -mwindows -o dist/gamestate-monitor.exe \
  src/main.c src/detector.c -luser32
```

## Windows installation

1. Copy `dist/gamestate-monitor.exe` to a writable directory such as
   `%LOCALAPPDATA%\GameModeActivator`. Avoid protected folders like Program Files.
2. Optionally copy `gamemode_blacklist.txt` beside it.
3. Run the executable. It has no console or tray icon. A second instance in
   the same Windows session exits immediately.
4. To start at sign-in, open `shell:startup` and create a shortcut to the executable.
   It must run in your interactive desktop session to see the foreground window.

Optional arguments:

```powershell
.\gamestate-monitor.exe --interval 3
.\gamestate-monitor.exe --once
```

`--interval` accepts an integer from 1 through 60 seconds. `--once` publishes a
single snapshot and exits; its timestamp will become stale. Exit codes are 0
for success, 1 for runtime failure, and 2 for invalid arguments. Since this is
a GUI subsystem executable, it prints no console output. Inspect the state
file to confirm operation. Use Task Manager to end the background process;
that forced termination leaves its last snapshot in place.

## WSL installation

Replace `YOUR_WINDOWS_USER` with your Windows profile directory name:

```sh
ln -s /mnt/c/Users/YOUR_WINDOWS_USER/AppData/Local/GameModeActivator/gamestatus \
  ~/.gamestatus
cat ~/.gamestatus
```

If you used the earlier Linux log reader, disable it first so it cannot replace
the symlink. Preserve its old file if needed:

```sh
systemctl --user disable --now gamestatus.service
mv ~/.gamestatus ~/.gamestatus.old
ln -s /mnt/c/Users/YOUR_WINDOWS_USER/AppData/Local/GameModeActivator/gamestatus \
  ~/.gamestatus
```

For the earlier Windows installation, remove its Startup shortcut and end the
old `rainmeter-gamemode-activator-*` and `gamemode_active` processes before
starting this executable. The new build does not use their logs or binaries.

No WSL service or startup command is needed. Reopen the file for each read so
you see atomic replacements. Linux inotify notifications for Windows-originated
writes on `/mnt/c` are unreliable; read the file when your application needs
state. For a gateway serving many requests, cache the parsed state inside the
request handler for up to two seconds, sharing a single refresh across requests.
On the development host, 3 rounds of 1,000 small-file open/read/close operations
measured about 4.5 ms per read on `/mnt/c`, versus 0.003 ms on Linux storage.
These are host-specific measurements, not guarantees. Check heartbeat freshness
even when serving cached state; a failed refresh should become unknown.
The state file itself does not limit resources, unload models, or block
requests; consumers can implement those policies separately.

## Detection and exclusions

The detector compares the foreground window bounds with the full bounds of
that window's monitor. This works on secondary monitors and does not assume
the primary display. DPI awareness avoids coordinate virtualization.

Chrome, Firefox, and common browsers, desktop tools, launchers, and system
components are ignored. Add more executable names to `gamemode_blacklist.txt`
(one per line, case insensitive; `#` starts a comment line). Exclusions load at
startup. The built-in list lives in `src/detector.c`.

Fullscreen is a heuristic: some fullscreen apps can qualify without being
games, and unusual game windows can be missed. Alt-tabbing away deactivates
state at the next sample. There is no debounce. Processes whose names cannot
be queried are excluded. Titles are limited to 255 UTF-16 code units.

## Footprint

One Windows process sleeps in the Windows message loop between timer samples.
It does not scan process modules, spawn helpers, or start WSL. The file is only
a few hundred bytes. Logging and tray resources are excluded from the build.
No WSL process runs with the symlink setup. Memory use depends on the Windows
version, compiler, and shared library accounting; measurements of the original
tray executable do not describe this build.

## License and origin

MIT. Original copyright and license are preserved in [LICENSE](LICENSE).
Repository: [Hujim0/gamemode-activator](https://github.com/Hujim0/gamemode-activator).
