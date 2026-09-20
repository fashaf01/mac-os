# Roadmap

## Phase 1 — rendering core ✅

DirectComposition + Direct2D window, GPU device management with loss recovery,
DirectWrite text, per-monitor DPI, role-based design tokens with a measured
contrast audit, spring and bounce animators that report when they settle.

## Phase 2 — the Dock ✅

Pinned and running tiles, cursor magnification with cursor anchoring, launch
bounce, press feedback, hover labels, running indicators, Recycle Bin, work-area
reservation, translucent glass with optional DWM blur, tray icon, right-click
menus, INI settings.

## Phase 3 — the menu bar

A 24 px bar across the top of the screen:

- Left: app menu, the active application's name in semibold, and its menus.
- Right: status items — clock with date, battery with charge state, Wi-Fi with
  signal strength, volume, and a Control Centre popover.
- Battery via `RegisterPowerSettingNotification`, Wi-Fi via the WLAN API's
  notification callback, volume via `IAudioEndpointVolume` callbacks. All
  push-based; the clock is the only timer, and it fires once a minute.
- Reuses the Phase 1 composition window and design tokens as-is.

## Phase 4 — the desktop layer (started)

- ✅ Hide the Windows taskbar and hand its screen space to the dock, restoring
  it on exit, on log-off and on demand from the dock's own menu.
- Intercept the Win key so Start does not appear over the desktop.
- Desktop icon grid with snap-to-grid and rubber-band selection.
- macOS-style desktop context menu.
- Handle Explorer reclaiming the work area after a restart.

## Phase 5 — polish and shipping

- A settings app, so the INI file is not the only interface.
- Autostart registration and a clean uninstaller.
- Multi-monitor placement; the dock currently attaches to the primary monitor.
- A profiling pass against the RAM and idle-CPU targets in the README, with the
  numbers published rather than estimated.

## Deliberately out of scope

**Traffic lights on other apps' windows.** Repainting title bars of processes
we do not own needs injection or global hooks. Antivirus flags it, Windows
updates break it, and it can take other applications down with it. The cost is
not worth the three buttons.

**Full shell replacement** (`Winlogon\Shell`, booting instead of
`explorer.exe`) is a real option and would use *less* total RAM, since Explorer
would never start. The rendering core is deliberately host-agnostic so this can
be added as a second host target later. It is not a v1 feature: a crash there
means a black desktop, and it would require reimplementing file browsing, tray
icons and global hotkeys from scratch.
