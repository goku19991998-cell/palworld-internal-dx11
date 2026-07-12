// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"
#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

// SDK ( shit takes way to long to compile, now a static library )
#define static_assert(...)
#include "SDK.hpp"
#undef static_assert

#endif //PCH_H