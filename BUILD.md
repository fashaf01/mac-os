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

Double-click `MacDock.exe`. The Windows taskbar disappears and the dock takes
the bottom of your primary monitor.

The <kbd>Win</kbd> key still opens the Start menu, so nothing becomes
unreachable.

- **Left-click** a tile — launch the app, or bring it forward. Clicking an app
  that already has several windows cycles through them.
- **Right-click** a tile — Open, Show in File Explorer, Keep in / Remove from
  Dock.
- **Right-click the divider** (the line next to the Recycle Bin) — show/hide
  the Windows taskbar, open the settings file, reload settings, or quit. This
  is the dock's own menu, and it stays reachable when the notification area is
  hidden along with the taskbar.

Only one copy runs at a time; launching it again is a no-op.

### Start it with Windows

Press <kbd>Win</kbd>+<kbd>R</kbd>, run `shell:startup`, and put a shortcut to
`MacDock.exe` in the folder that opens.

### Getting the Windows taskbar back

Three ways, in order of convenience:

1. **Right-click the divider** next to the Recycle Bin → **Show Windows
   taskbar**. The setting is saved, so it stays off next launch.
2. **Quit MacDock** — it restores the taskbar on the way out, including on
   log-off and shutdown.
3. **If MacDock was force-killed** — closing Visual Studio while debugging,
   End Task, a crash — it never ran its cleanup. Two ways back:
   - Set `hideWindowsTaskbar = false` in the settings file and launch MacDock
     again. It notices a taskbar left hidden by a previous run and puts it
     back, using the state stored in `savedTaskbarState`.
   - Or press <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Esc</kbd>, find **Windows
     Explorer**, click **Restart**.

> **Note if you debug from Visual Studio:** stopping the debugger *terminates*
> MacDock rather than closing it, so its cleanup never runs. Quit from the
> dock's own menu first, then stop debugging.

The taskbar is only ever *hidden* — auto-hide plus `ShowWindow(SW_HIDE)`.
Explorer keeps running the whole time and nothing is deleted or reconfigured
permanently.

### Uninstall

Quit from the dock menu (which restores your taskbar) and delete the exe. The dock releases its reserved
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
| `hideWindowsTaskbar` | `true` | Replace the Windows taskbar instead of sitting beside it |
| `savedTaskbarState` | `-1` | Written by MacDock, not by you: the taskbar's setting before it was hidden, so a killed run can still be undone |
| `pin` | — | One absolute path per line, in dock order |

`pin` lines are the dock's contents and order. They are rewritten whenever you
use *Keep in Dock* or *Remove from Dock*.

## Troubleshooting

**Nothing appears.** Check `%LOCALAPPDATA%\MacDock\macdock.log`. A Direct3D
failure there means the graphics driver needs updating; MacDock falls back to
WARP software rendering but says so in the log.

**No blur behind the dock.** Expected for now — the panel is translucent but
what is behind it is not blurred. See
[the roadmap](docs/ROADMAP.md#real-backdrop-blur--parked-and-why).

**The dock is an empty bar with no icons.** That was a real bug, fixed: a blur
pane owned by the dock window was covering them. If you still see it, send
`%LOCALAPPDATA%\MacDock\macdock.log` — it lists every tile and every icon
that failed to load.

**Maximized windows cover the dock.** `reserveWorkArea` must be `true`, and
Explorer sometimes reclaims the work area after a crash — restarting MacDock
re-registers it. `autoHide = true` deliberately does not reserve space.

**Icons are blurry when magnified.** That app only ships a small icon;
Windows has nothing larger to give. MacDock already asks for the 256 px
jumbo variant first.

**Wrong monitor.** The dock currently attaches to the primary monitor.
Multi-monitor placement is on the roadmap.
