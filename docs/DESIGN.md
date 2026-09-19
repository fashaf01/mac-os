# Design notes

## The colour system

Every colour is a **named role**, never a literal, and lives in
`src/design/Theme.h`. Nothing in the dock says "use #1E1E20"; it says
`theme_.panelFill`. Swapping the whole look is a data change in one file.

The roles:

| Role | What it is for |
|---|---|
| `panelFill` | The translucent glass of the dock strip |
| `panelStroke` | The hairline that defines its edge |
| `panelHighlight` | The inner top highlight that makes it read as glass |
| `panelShadow` | The drop shadow that lifts it off the wallpaper |
| `separator` | The divider before the Recycle Bin |
| `runningDot` | The indicator under an app that has a window open |
| `tooltipFill` / `tooltipStroke` / `tooltipText` | The hover label chip |
| `pressOverlay`, `accent` | Interaction feedback |

## Why the contrast is measured, not guessed

This is the part most Mac-look themes get wrong. They pick a flat grey, put
white text on it, and it falls apart over a light wallpaper.

Two things make that avoidable:

**1. Translucent surfaces have no fixed colour.** A panel at 62 % opacity over
a white wallpaper is a different colour than the same panel over a dark one.
So contrast has to be computed against the *composited* result, not the token.
`composite()` in `Theme.cpp` does the source-over blend; `contrastRatio()`
implements WCAG 2.1 relative luminance on the result.

**2. There is a worst case, and it can be tested.** Mid-grey (#808080) is the
hardest backdrop for both a light and a dark material — it gives the glass the
least help in either direction. Every text token is audited against the panel
composited over mid-grey, so if it passes there it passes on any wallpaper.

`auditContrast()` runs this at startup and writes the measured ratios to the
log. `tests/test_theme.cpp` runs it as an assertion, so a palette change that
breaks legibility fails the build rather than shipping.

Current measured ratios, worst case:

| | Dark | Light | Minimum |
|---|---|---|---|
| Tooltip text | 12.09:1 | 16.37:1 | 4.5:1 (WCAG AA body text) |
| Running dot | 6.01:1 | 5.00:1 | 3.0:1 (AA non-text) |
| Separator | 1.89:1 | 1.57:1 | 1.3:1 (decorative) |
| Panel hairline | 1.60:1 | 1.30:1 | 1.1:1 (decorative) |

The decorative minimums are deliberately low — a hairline that hit 4.5:1 would
look like a drawn border, not an edge. What matters is that the numbers are
*chosen and checked* rather than whatever fell out of picking a grey.

## Making translucency look like glass

A translucent rectangle on its own reads as a grey box. Three details turn it
into a pane:

1. **The inner top highlight.** A one-pixel light stroke inset along the top
   curve, at 10 % (dark) or 55 % (light). This is the single highest-value
   detail in the whole dock.
2. **A hairline edge**, not a border — one physical pixel, scaled with DPI.
3. **A shadow that is a ring, not a fill.** The panel is see-through, so a
   filled shadow behind it shows through and muddies the glass. MacDock draws
   expanding rounded-rectangle *strokes*, which leaves the middle clean.

## The magnification curve

The falloff is a raised cosine over the influence radius:

```
falloff(d) = d >= 1 ? 0 : 0.5 * (1 + cos(pi * d))
```

It is 1 under the cursor and 0 at the edge of the radius, and critically its
**slope is zero at both ends**. A linear or quadratic falloff has a slope
discontinuity at the boundary, which you see as the outermost icon popping as
the cursor crosses it.

Two other properties matter more than the curve itself:

**Scales are measured in resting space.** Distance is taken from where each
icon sits *unmagnified*. If you measured against magnified positions the
layout would chase itself — icons grow, which moves them, which changes their
distance, which changes how much they grow.

**The strip is anchored to the cursor.** As icons grow the whole strip gets
wider, and if you simply re-centred it, the icon you were aiming at would slide
out from under the pointer. Instead the layout finds the content coordinate
under the cursor in resting space, finds the same coordinate in magnified
space, and offsets the strip by the difference. The thing you point at stays
where you pointed. `tests/test_layout.cpp` asserts exactly this.

The resting and anchored layouts are then blended by the magnification spring,
so the dock eases in and out rather than snapping. The tests sweep the cursor
across the screen and across the blend and assert the layout never jumps.

**Icons grow upward out of a constant-height panel**, overhanging its top edge,
which is what the macOS dock does and why the window is taller than the panel.

## Why idle really is idle

The performance claim is structural, not an optimisation pass:

- `Spring::step()` returns `false` once it settles. `tickAnimation()` ORs the
  return values and calls `KillTimer` when they are all false. There is no
  animation timer running when nothing is moving.
- `invalidate()` posts at most one pending repaint, so a burst of mouse moves
  coalesces into a single frame.
- Running-app state is push-based (`SetWinEventHook`), and the hook does no
  work — it posts a message that starts a 180 ms debounce, because one app
  launch emits dozens of window events.
- Recycle Bin state is push-based too (`SHChangeNotifyRegister`). Checking it
  on a timer would have been three lines less code and a permanent wakeup.
- Icon extraction is the one genuinely slow operation, and it happens once per
  app per session.

The test suite asserts the springs and the bounce terminate, which is the
property the whole idle claim rests on.
