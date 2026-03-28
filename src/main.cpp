/**
 * Frame Generation DLL for DirectX 11 Games
 * Interpolates frames between rendered frames for smoother gameplay
 * 
 * Features:
 * - Frame interpolation (x2, x3, x4 multipliers)
 * - Motion vector based interpolation
 * - ImGui menu (Toggle: DEL key)
 * - Low input lag compensation
 */

// Disable Windows min/max macros BEFORE including any Windows headers
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <vector>
#include <deque>
#include <cmath>
#include <chrono>

#include "MinHook.h"
#include "d3d11_hook.h"
#include "frame_interpolator.h"
#include "menu.h"
#include "input_manager.h"
#include "config.h"

// Global state
namespace FrameGen {
    HMODULE g_hModule = nullptr;
    bool g_initialized = false;
    bool g_unloading = false;
    
    Config g_config;
    FrameInterpolator* g_interpolator = nullptr;
    Menu* g_menu = nullptr;
    InputManager* g_inputManager = nullptr;
    D3D11Hook* g_d3dHook = nullptr;
}

// Forward declarations
bool Initialize();
void Shutdown();
DWORD WINAPI MainThread(LPVOID lpParam);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        FrameGen::g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        Shutdown();
        break;
    }
    return TRUE;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    // Wait for game to fully load
    Sleep(3000);
    
    if (!Initialize()) {
        MessageBoxA(nullptr, "FrameGen: Failed to initialize!", "FrameGen Error", MB_ICONERROR);
        return 1;
    }
    
    return 0;
}

bool Initialize() {
    if (FrameGen::g_initialized) return true;
    
    // Initialize MinHook
    if (MH_Initialize() != MH_OK) {
        return false;
    }
    
    // Load config
    FrameGen::g_config.Load();
    
    // Create components
    FrameGen::g_inputManager = new InputManager();
    FrameGen::g_interpolator = new FrameInterpolator(&FrameGen::g_config);
    FrameGen::g_menu = new Menu(&FrameGen::g_config, FrameGen::g_interpolator);
    FrameGen::g_d3dHook = new D3D11Hook(
        FrameGen::g_interpolator,
        FrameGen::g_menu,
        FrameGen::g_inputManager
    );
    
    // Initialize D3D11 hook
    if (!FrameGen::g_d3dHook->Initialize()) {
        return false;
    }
    
    FrameGen::g_initialized = true;
    return true;
}

void Shutdown() {
    if (FrameGen::g_unloading) return;
    FrameGen::g_unloading = true;
    
    // Save config
    if (FrameGen::g_config.IsModified()) {
        FrameGen::g_config.Save();
    }
    
    // Cleanup in reverse order
    if (FrameGen::g_d3dHook) {
        FrameGen::g_d3dHook->Shutdown();
        delete FrameGen::g_d3dHook;
        FrameGen::g_d3dHook = nullptr;
    }
    
    if (FrameGen::g_menu) {
        delete FrameGen::g_menu;
        FrameGen::g_menu = nullptr;
    }
    
    if (FrameGen::g_interpolator) {
        delete FrameGen::g_interpolator;
        FrameGen::g_interpolator = nullptr;
    }
    
    if (FrameGen::g_inputManager) {
        delete FrameGen::g_inputManager;
        FrameGen::g_inputManager = nullptr;
    }
    
    MH_Uninitialize();
    FrameGen::g_initialized = false;
}
