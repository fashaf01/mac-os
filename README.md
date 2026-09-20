# MacDock

A macOS-style desktop shell for Windows, written in native C++ so it costs
less than the thing it replaces.

This repository is **Phase 1 + 2**: the rendering core, the design system, and
a working Dock. The menu bar and desktop layer come next — see
[the roadmap](docs/ROADMAP.md).

## Why native

Most "Mac theme for Windows" tools are Electron apps. An Electron dock ships a
whole browser to draw sixty pixels of glass, which is why they sit at 300–700 MB
of RAM and show up in Task Manager next to your actual work.

MacDock is raw Win32 with Direct2D and DirectComposition. Concretely:

| | RAM | Idle CPU |
|---|---|---|
| Typical Electron dock | 300–700 MB | 2–8 % |
| **MacDock (target)** | **20–45 MB** | **~0 %** |
| Windows `explorer.exe`, for scale | 80–200 MB | ~0.3 % |

The idle number is the important one, and it is not an accident:

- **Nothing polls.** Running-app state comes from `SetWinEventHook`. Recycle
  Bin state comes from `SHChangeNotifyRegister`. Light/dark comes from
  `WM_SETTINGCHANGE`. There is no background thread and no scanning loop.
- **The frame timer only exists while something moves.** Springs report when
  they settle, and the animation timer is killed the moment they do. At rest
  the process is asleep in `GetMessage`.
- **The GPU does the compositing.** DirectComposition blends the dock; the CPU
  is not in the per-frame path at all.
- **Icons are extracted once** and cached as GPU bitmaps.
- Single process. No helper service, no updater, no tray daemon.

To be straight about it: nothing can use *zero* RAM, and this does not modify
Windows itself. It is a normal user-mode program. It just happens to be a
cheap one.

## What works now

- **Replaces the Windows taskbar** rather than sitting beside it. The taskbar
  is switched to auto-hide (which frees the screen space it reserved) and its
  window is hidden (so it does not slide back in and fight the dock for the
  bottom edge). Explorer keeps running throughout, and the taskbar is put back
  exactly as it was when MacDock exits.
- **Dock** with pinned apps, running-app indicators, and temporary tiles for
  running apps you have not pinned.
- **Cursor magnification** with the raised-cosine falloff and the anchoring
  that keeps the icon you point at underneath the pointer while everything
  around it grows.
- **Launch bounce**, press feedback, and hover labels.
- **Painted glass** — translucent panel with an inner top highlight and a
  hairline edge. Real DWM blur is parked; see
  [the roadmap](docs/ROADMAP.md#real-backdrop-blur--parked-and-why).
- **Light and dark themes** that follow Windows, with a contrast-audited
  palette (see [the design notes](docs/DESIGN.md)).
- **Work-area reservation**, so maximized windows stop at the dock instead of
  sliding under it.
- **Recycle Bin** tile with empty/full state and an Empty command.
- Per-monitor DPI, right-click menus, a tray icon to quit, and an INI file you
  can edit.

## Verification status

Honest accounting of what has and has not been run:

- ✅ **Builds with MSVC 19.51** on `windows-latest`, Debug and Release, on
  every push. Also links clean under GCC/mingw at `-Wall -Wextra -Wshadow`.
- ✅ **Tests pass in CI** on both toolchains — the magnification maths
  (including the property that the icon under the cursor stays under the
  cursor) and the WCAG contrast maths.
- ✅ **A prebuilt `MacDock.exe`** is attached to every green Release run in
  [Actions](https://github.com/fashaf01/mac-os/actions), so you can try it
  without building.
- ⚠️ **Not yet run on a real desktop.** Visual behaviour — blur, shadow
  softness, magnification feel, icon sharpness — needs eyes on a screen. The
  RAM and CPU figures above are design targets, not measurements; Phase 5
  replaces them with real numbers.

## Build

Windows 10 1809+ or Windows 11, Visual Studio 2022 with the *Desktop
development with C++* workload. Full steps in [BUILD.md](BUILD.md).

```
cmake -B build -S .
cmake --build build --config Release
build\Release\MacDock.exe
```

Quit it by right-clicking the divider next to the Recycle Bin → **Quit
MacDock**. (The notification area goes away with the taskbar, so that menu —
not the tray icon — is the dock's own control panel.)

**If you ever lose the taskbar:** right-click that same divider → *Show
Windows taskbar*. If MacDock is not running at all, press
<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Esc</kbd>, find **Windows Explorer** in
the list, and click **Restart**. Nothing is permanent — the taskbar is only
hidden, never removed.

## Tests

The magnification maths and the contrast maths are pure computation and are
covered by tests that run on any platform:

```
./tests/run.sh          # Linux/macOS
cmake -B build -S . -DMACDOCK_BUILD_TESTS=ON && ctest --test-dir build -C Release
```

See [tests/README.md](tests/README.md) for what they pin down.

## Settings

First run writes `%APPDATA%\MacDock\settings.ini` and pins whichever common
apps are actually installed. Edit it and pick *Reload settings* from the tray
menu. Every key is documented in [BUILD.md](BUILD.md#settings).

## Known limits

- **Window title bars are untouched.** macOS traffic lights on other apps'
  windows would need injection or hooks into processes we do not own, which
  antivirus flags and Windows updates break. It is deliberately out of scope.
- **The Start menu still belongs to Windows.** The <kbd>Win</kbd> key still
  opens it, and it still looks like Windows. A macOS-style menu bar is Phase 3.
- **No real backdrop blur yet.** The panel is translucent and layered, but
  what is behind it is not blurred. The reason, and the route that should
  work, are in [the roadmap](docs/ROADMAP.md#real-backdrop-blur--parked-and-why).
- **No Apple assets.** Icons, fonts and artwork here are our own or the
  system's. This reimplements a layout and a set of interactions, not Apple's
  copyrighted design assets.

## Licence

See [LICENSE](LICENSE).
