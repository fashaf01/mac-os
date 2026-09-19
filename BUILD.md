# Building and running MacDock

## Requirements

- Windows 10 version 1809 or later, or Windows 11
- **Visual Studio 2022 Community** (free) with the
  *Desktop development with C++* workload — that gives you MSVC, CMake and
  the Windows SDK in one install
- No other dependencies. Nothing to download, no package manager, no runtime
  to install alongside the exe.

## Build

From a *Developer Command Prompt for VS 2022*, in the repository root:

```
cmake -B build -S .
cmake --build build --config Release
```

The result is a single self-contained file:

```
build\Release\MacDock.exe
```

The C runtime is linked statically by default, so you can copy that one file
anywhere. Pass `-DMACDOCK_STATIC_RUNTIME=OFF` if you would rather not.

### From the Visual Studio IDE

*File → Open → Folder…* on the repository root. Visual Studio reads
`CMakeLists.txt` directly; pick the `x64-Release` configuration and press F5.

### Debug build

```
cmake --build build --config Debug
```

A debug build writes `%LOCALAPPDATA%\MacDock\macdock.log`, including the
startup contrast audit.

## Running

Double-click `MacDock.exe`. It appears at the bottom of your primary monitor
and puts an icon in the notification area.

- **Left-click** a tile — launch the app, or bring it forward. Clicking an app
  that already has several windows cycles through them.
- **Right-click** a tile — Open, Show in File Explorer, Keep in / Remove from
  Dock.
- **Right-click the tray icon** — open the settings file, reload settings, or
  quit.

Only one copy runs at a time; launching it again is a no-op.

### Start it with Windows

Press <kbd>Win</kbd>+<kbd>R</kbd>, run `shell:startup`, and put a shortcut to
`MacDock.exe` in the folder that opens.

### Uninstall

Quit from the tray icon and delete the exe. The dock releases its reserved
screen space on exit. To remove its settings too, delete
`%APPDATA%\MacDock`.

## Settings

`%APPDATA%\MacDock\settings.ini`, written on first run. Change it, then pick
*Reload settings* from the tray menu.

| Key | Default | Meaning |
|---|---|---|
| `theme` | `auto` | `auto` follows Windows, or force `dark` / `light` |
| `iconSize` | `52` | Resting icon size in logical pixels (24–128) |
| `magnificationOn` | `true` | Turn cursor magnification off entirely |
| `magnification` | `1.75` | Peak scale under the cursor (1.0–3.0) |
| `influenceRadius` | `2.4` | How far magnification reaches, in icon widths |
| `itemGap` | `8` | Space between tiles |
| `panelPadding` | `8` | Inset between the icons and the panel edge |
| `bottomMargin` | `8` | Gap between the panel and the screen edge |
| `reserveWorkArea` | `true` | Keep maximized windows off the dock |
| `autoHide` | `false` | Slide the dock away until the cursor reaches the edge |
| `showRunningDots` | `true` | Dot under apps that have a window open |
| `showRunningApps` | `true` | Temporary tiles for running apps you have not pinned |
| `showLabels` | `true` | Name label above the hovered tile |
| `showRecycleBin` | `true` | Recycle Bin tile at the end |
| `backdropBlur` | `true` | Real DWM blur behind the panel; `false` uses painted glass |
| `pin` | — | One absolute path per line, in dock order |

`pin` lines are the dock's contents and order. They are rewritten whenever you
use *Keep in Dock* or *Remove from Dock*.

## Troubleshooting

**Nothing appears.** Check `%LOCALAPPDATA%\MacDock\macdock.log`. A Direct3D
failure there means the graphics driver needs updating; MacDock falls back to
WARP software rendering but says so in the log.

**No blur, just a flat translucent bar.** Either transparency effects are off
(*Settings → Personalisation → Colours → Transparency effects*), or the
undocumented blur API is unavailable on your build. The painted fallback is
intentional, not a failure. Set `backdropBlur = false` to stop it trying.

**Maximized windows cover the dock.** `reserveWorkArea` must be `true`, and
Explorer sometimes reclaims the work area after a crash — restarting MacDock
re-registers it. `autoHide = true` deliberately does not reserve space.

**Icons are blurry when magnified.** That app only ships a small icon;
Windows has nothing larger to give. MacDock already asks for the 256 px
jumbo variant first.

**Wrong monitor.** The dock currently attaches to the primary monitor.
Multi-monitor placement is on the roadmap.
