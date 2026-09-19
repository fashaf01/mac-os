#!/usr/bin/env sh
# Builds and runs the portable test suites with any C++20 compiler.
set -e
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${TMPDIR:-/tmp}/macdock-tests"
cxx="${CXX:-g++}"
mkdir -p "$out"

echo "== test_layout =="
"$cxx" -std=c++20 -Wall -Wextra -I "$root/src" \
    "$root/tests/test_layout.cpp" "$root/src/dock/DockLayout.cpp" \
    -o "$out/test_layout"
"$out/test_layout"

echo
echo "== test_theme =="
"$cxx" -std=c++20 -Wall -Wextra -I "$root/src" -I "$root/tests/shim" \
    "$root/tests/test_theme.cpp" "$root/src/design/Theme.cpp" \
    "$root/tests/shim/log_stub.cpp" \
    -o "$out/test_theme"
"$out/test_theme"
