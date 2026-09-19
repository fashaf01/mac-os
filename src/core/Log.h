#pragma once

namespace md::log {

// Writes to the debugger output, and (in debug builds) to
// %LOCALAPPDATA%\MacDock\macdock.log. Never touches disk on a Release
// build unless a log line is actually emitted, so the idle cost is zero.
void init();
void shutdown();
void write(const wchar_t* fmt, ...);

} // namespace md::log

#define MD_LOG(...) ::md::log::write(__VA_ARGS__)
