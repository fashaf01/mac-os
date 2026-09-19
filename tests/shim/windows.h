// Minimal shim: lets the pure colour maths in Theme.cpp be unit-tested off Windows.
#pragma once
#include <cstdint>
typedef unsigned long DWORD;
typedef long LSTATUS;
typedef void* HKEY;
#define HKEY_CURRENT_USER ((HKEY)0)
#define ERROR_SUCCESS 0L
#define RRF_RT_REG_DWORD 0x00000018
inline LSTATUS RegGetValueW(HKEY, const wchar_t*, const wchar_t*, DWORD, void*, void*, DWORD*) {
    return 1; // "not found" -> exercises the fallback path
}
