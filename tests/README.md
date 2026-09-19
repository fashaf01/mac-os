# Tests

These cover the two pieces of MacDock that are pure computation, and therefore
the two pieces that can be verified without a Windows desktop in front of you:

| Suite | What it pins down |
|---|---|
| `test_layout` | The dock magnification curve, the anchoring that keeps the icon you point at under the pointer, panel geometry, hit testing, layout continuity while sweeping the cursor, and the spring/bounce animators terminating. |
| `test_theme` | WCAG 2.1 relative-luminance and contrast-ratio maths against known reference values, alpha compositing, and an audit proving every text token clears AA over the worst-case backdrop. |

`shim/windows.h` is a stub with just enough of the Win32 surface for
`Theme.cpp`'s colour maths to build off Windows. It is test-only and is never
part of the shipped binary.

## Running them

On Windows, once the main project is configured:

```
cmake -B build -S . -DMACDOCK_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Linux or macOS (the tests are portable C++20 even though the dock is not):

```
./tests/run.sh
```
