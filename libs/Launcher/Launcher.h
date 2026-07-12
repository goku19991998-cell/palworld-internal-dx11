#pragma once
#include "pch.h"

static const wchar_t* PROCESS_NAME = (isXbox) ? L"Palworld-WinGDK-Shipping.exe" : L"Palworld-Win64-Shipping.exe";
static const char* DLL_NAME = "NetCrack-PalWorld.dll";

int ExitWithErrorMsg(const char* eMSG, DWORD eCODE);
std::string GetCurrentPath();
bool IsGameRunning(const wchar_t* procName, DWORD* dwPID);
bool WaitForMainWindow(DWORD procID, int timeoutSeconds);
int exec();
