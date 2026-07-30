//#include "pch.h"
#include "base.h"
#include "Archipelago.hpp"
#include <fstream>  // for debug log
#include <string>

HMODULE g_dinput = 0;
HANDLE g_hMainThread = NULL;
HANDLE g_hExitEvent = NULL;  // Set to signal MainThread to stop

// Simple debug log – write to a separate file so it doesn't interfere with
// Archipelago's log.  You can delete this once the crashes are gone.
static void DllLog(const char* msg) {
	// 1. Get the absolute path to Bayonetta.exe
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	std::string path(exePath);

	// 2. Chop off "Bayonetta.exe"
	size_t pos = path.find_last_of("\\/");
	if (pos != std::string::npos) {
		path = path.substr(0, pos + 1);
	}

	// 3. Aim it at our new folder
	std::string fullPath = path + "Archipelago Logs\\BayoHook_DllMain.log";

	std::ofstream f(fullPath, std::ios::app);
	if (f.is_open()) {
		f << msg << "\n";
	}
}

extern "C" {
	__declspec(dllexport) HRESULT WINAPI direct_input8_create(HINSTANCE hinst, DWORD dw_version, const IID& riidltf, LPVOID* ppv_out, LPUNKNOWN punk_outer) {
#pragma comment(linker, "/EXPORT:DirectInput8Create=_direct_input8_create@20")
		return ((decltype(direct_input8_create)*)GetProcAddress(g_dinput, "DirectInput8Create"))(hinst, dw_version, riidltf, ppv_out, punk_outer);
	}
}

DWORD WINAPI MainThread(LPVOID lpThreadParameter)
{
	wchar_t buffer[MAX_PATH]{ 0 };
	if (GetSystemDirectoryW(buffer, MAX_PATH) != 0) {
		if ((g_dinput = LoadLibraryW((std::wstring{ buffer } + L"\\dinput8.dll").c_str())) == NULL) {
			ExitProcess(0);
		}
	}

	// Small delay to let the game finish its own startup – avoids a race
	// where our hooks conflict with the game's early DInput init.
	Sleep(1000);

	Base::Data::hModule = (HMODULE)lpThreadParameter;
	Base::Init();

	// If Base::Init() returns (it should not), wait here for the exit signal
	// so we don't just fall through and terminate the thread unexpectedly.
	if (g_hExitEvent)
		WaitForSingleObject(g_hExitEvent, INFINITE);

	DllLog("MainThread: exiting cleanly");
	return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DllLog("DLL_PROCESS_ATTACH");
		g_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		g_hMainThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
		break;

	case DLL_PROCESS_DETACH:
		DllLog("DLL_PROCESS_DETACH - starting shutdown");

		// Signal all Archipelago threads to stop and clean up.
		Archipelago::Shutdown(lpReserved != nullptr);

		// Signal the main thread to exit.
		if (g_hExitEvent)
			SetEvent(g_hExitEvent);

		// If this is a normal DLL unload (not process termination), we can safely
		// wait for the main thread to finish before returning. During process
		// termination we must not wait (threads are already being killed).
		if (lpReserved == nullptr && g_hMainThread) {
			DllLog("Waiting for MainThread to exit...");
			DWORD waitResult = WaitForSingleObject(g_hMainThread, 5000);
			if (waitResult == WAIT_OBJECT_0) {
				DllLog("MainThread has exited.");
			}
			else if (waitResult == WAIT_TIMEOUT) {
				DllLog("WARNING: MainThread did not exit within 5 seconds.");
			}
			else {
				DllLog("ERROR: WaitForSingleObject failed.");
			}
			CloseHandle(g_hMainThread);
			g_hMainThread = NULL;
		}
		if (g_hExitEvent) {
			CloseHandle(g_hExitEvent);
			g_hExitEvent = NULL;
		}
		DllLog("DLL_PROCESS_DETACH - finished");
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}
	return TRUE;
}