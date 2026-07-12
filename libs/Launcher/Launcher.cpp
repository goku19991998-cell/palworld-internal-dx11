#include "pch.h"
#include "Launcher.h"


int ExitWithErrorMsg(const char* eMSG, DWORD eCODE)
{
	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, eCODE, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
	std::string errorMsg(messageBuffer, size);
	LocalFree(messageBuffer);

	char buf[0x512];
	sprintf_s(buf, "Palworld Launcher encountered an error and will now exit.\n\nMSG: %s\nSRC: %s", errorMsg.c_str(), eMSG);
	MessageBoxA(nullptr, buf, "Palworld Launcher Fatal Error", MB_ICONWARNING);
	return EXIT_FAILURE;
}

std::string GetCurrentPath()
{
	char buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}

bool IsGameRunning(const wchar_t* procName, DWORD* dwPID)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return false;

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(pe32);
	if (!Process32First(hSnap, &pe32))
	{
		CloseHandle(hSnap);
		return false;
	}

	do
	{
		if (_wcsicmp(pe32.szExeFile, procName))
			continue;

		*dwPID = pe32.th32ProcessID;
		CloseHandle(hSnap);
		return true;

	} while (Process32Next(hSnap, &pe32));
	
	CloseHandle(hSnap);
	return false;
}

struct FindWindowData
{
	DWORD procID;
	HWND hwnd;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	FindWindowData* data = reinterpret_cast<FindWindowData*>(lParam);
	DWORD windowProcID = 0;
	GetWindowThreadProcessId(hwnd, &windowProcID);

	// A real, top-level, visible, unowned window - the kind the engine creates
	// once it's actually ready to render, not some invisible helper window that
	// can exist moments after the process starts.
	if (windowProcID == data->procID && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr)
	{
		data->hwnd = hwnd;
		return FALSE;
	}
	return TRUE;
}

// The process appears (and PROCESS_ALL_ACCESS becomes obtainable) long before the
// engine has finished initializing D3D11 - a fixed sleep after process detection
// was unreliable (sometimes too short, always an arbitrary guess). Waiting for the
// game's actual main window to exist is a real readiness signal instead.
bool WaitForMainWindow(DWORD procID, int timeoutSeconds)
{
	for (int i = 0; i < timeoutSeconds; i++)
	{
		FindWindowData data{ procID, nullptr };
		EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
		if (data.hwnd != nullptr)
			return true;

		Sleep(1000);
	}
	return false;
}

bool Inject(HANDLE hProc)
{
	void* addr = VirtualAllocEx(hProc, NULL, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!addr)
		return false;

	std::string path = GetCurrentPath() + "\\" + DLL_NAME;
	if (!WriteProcessMemory(hProc, addr, path.c_str(), strlen(path.c_str()) + 1, NULL))
	{
		VirtualFreeEx(hProc, addr, 0, MEM_RELEASE);
		CloseHandle(hProc);
		return false;
	}

	HANDLE hThread = CreateRemoteThread(hProc, 0, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA), addr, 0, 0);
	if (!hThread)
	{
		VirtualFreeEx(hProc, addr, 0, MEM_RELEASE);
		CloseHandle(hProc);
		return false;
	}

	WaitForSingleObject(hThread, INFINITE);

	// The remote thread's exit code is LoadLibraryA's return value (the loaded
	// HMODULE, or 0 on failure) - without checking it, a failed LoadLibraryA
	// (missing dependency, blocked DLL, etc.) would silently report success.
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);

	CloseHandle(hThread);
	VirtualFreeEx(hProc, addr, 0, MEM_RELEASE);
	CloseHandle(hProc);
	return exitCode != 0;
}


//	main function
int exec()
{
	// Waits up to 5 minutes for the game to appear, checking once a second. If it
	// never shows up, exits silently (no message box) instead of erroring out -
	// this lets the launcher be started speculatively (e.g. alongside Steam)
	// without nagging if the game just isn't being played this time.
	constexpr int kMaxWaitSeconds = 5 * 60;
	int rTick = 0;
	DWORD procID;
	while (!IsGameRunning(PROCESS_NAME, &procID))
	{
		Sleep(1000);
		rTick++;

		if (rTick >= kMaxWaitSeconds)
			return 0;
	}

	// Wait for the game's actual window instead of guessing a fixed delay - see
	// WaitForMainWindow. Gives up to 2 minutes after the process appears; if the
	// window never shows up in that time, exits silently (no message box) rather
	// than injecting blind into a not-yet-ready engine.
	if (!WaitForMainWindow(procID, 120))
		return 0;

	// Small buffer so the swapchain/device finishes setting up right after the
	// window itself appears.
	Sleep(3000);

	HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, false, procID);
	if (!hProc)
		return ExitWithErrorMsg("", GetLastError());

	if (!Inject(hProc))
		return ExitWithErrorMsg("", GetLastError());

	return 0;
}