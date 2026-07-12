#pragma once
#include "pch.h"
#include "initialize.hpp"

using namespace SDK;
using namespace DX11_Base;

void initoffsets()
{
    g_Console = std::make_unique<Console>();
#if DEBUG
    g_Console->InitializeConsole("Debug Console");
    g_Console->printdbg("Updating offsets:\n", Console::Colors::yellow);
#endif
    Config.ClientBase = (DWORD64)GetModuleHandleA(NULL);
    auto GObject = signature("48 8B 05 ?? ?? ?? ?? 48 8B 0C C8 4C 8D 04 D1 EB 03").instruction(3).add(7).GetPointer() - Config.ClientBase;
    auto AppendString = signature("C3 48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 80").GetPointer() + 1 - Config.ClientBase;//
    auto ProcEvent = signature("40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 10 01 ? ? 48 8D").GetPointer() - Config.ClientBase;//
    auto TickAddress = signature("48 89 5C 24 ? 57 48 83 EC 60 48 8B F9 E8 ? ? ? ? 48 8B ? ? ? ? ? 48 8D").GetPointer() - Config.ClientBase;
    auto gworld = signature("48 8B 05 ? ? ? ? EB 05").instruction(3).add(7).GetPointer() - Config.ClientBase;


    Offsets::GObjects = GObject;
    Offsets::AppendString = AppendString;
    Offsets::ProcessEvent = ProcEvent;
    Config.offset_Tick = TickAddress;
    //gworld already set in config.cpp

    g_Console->printdbg("[+] ClientBase: %llx\n", Console::Colors::white, (unsigned long long)Config.ClientBase);
    g_Console->printdbg("[+] GWorld: %llx\n", Console::Colors::white, gworld);
    g_Console->printdbg("[+] GObject: %llx\n", Console::Colors::white, Offsets::GObjects);
    g_Console->printdbg("[+] AppendString: %llx\n", Console::Colors::white, Offsets::AppendString);
    g_Console->printdbg("[+] ProcEvent: %llx\n", Console::Colors::white, Offsets::ProcessEvent);
    g_Console->printdbg("[+] TickAddress: %llx\n\n", Console::Colors::white, Config.offset_Tick);
}
extern DWORD WINAPI MainThread_Initialize();
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  dwCallReason, LPVOID lpReserved)
{
    g_hModule = hModule;

    switch (dwCallReason) {
        case (DLL_PROCESS_ATTACH): {
            initoffsets();
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)MainThread_Initialize, g_hModule, NULL, NULL);
            break;
        }
        case (DLL_PROCESS_DETACH): {
            g_KillSwitch = TRUE;
            break;
        }
    }
    return TRUE;
}