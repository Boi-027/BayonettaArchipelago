#pragma once
#include <atomic> // <-- This fixes the errors

struct IDirect3DDevice9;

namespace Archipelago {
    extern volatile bool g_apWitchTimeLocked;
    int32_t __cdecl FilterMoveID(int32_t moveId);

    void Init();
    void Poll();
    void DrawTab();
    void DrawOverlay();
    void Shutdown(bool isProcessTerminating);
    void ToggleGui();
    void ShowGui();
    void HideGui();
    void InitLogoTexture(IDirect3DDevice9* pDevice);
}