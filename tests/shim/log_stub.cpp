#include "core/Log.h"
#include <cstdio>
#include <cwchar>
#include <cstdarg>
namespace md::log {
void init() {}
void shutdown() {}
void write(const wchar_t* fmt, ...) {
    va_list a; va_start(a, fmt); vwprintf(fmt, a); va_end(a); wprintf(L"\n");
}
}
