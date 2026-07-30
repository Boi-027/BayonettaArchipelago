#pragma once
// Forced-include file for Archipelago.cpp ONLY (set via that file's
// Project Properties -> C/C++ -> Advanced -> Forced Include File).
//
// This exists to guarantee winsock2.h is processed before windows.h in
// Archipelago.cpp's translation unit, no matter what the project-wide
// pch.h forced-include setting is doing. Putting these guards in
// Archipelago.cpp's own text was not sufficient - which means something
// (most likely a stale/misapplied PCH setting) was still injecting
// windows.h ahead of that file's own first lines. A per-file Forced
// Include File setting takes priority and is processed before the
// file's own text is compiled, so this closes that gap directly.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
