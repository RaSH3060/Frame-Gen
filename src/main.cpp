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
#include <cstdio>

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

// D3D11 proxy exports - forward to real d3d11.dll
typedef HRESULT(WINAPI* D3D11CreateDevice_t)(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
);

typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChain_t)(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
    IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext
);

static D3D11CreateDevice_t s_originalD3D11CreateDevice = nullptr;
static D3D11CreateDeviceAndSwapChain_t s_originalD3D11CreateDeviceAndSwapChain = nullptr;
static HMODULE s_realD3D11 = nullptr;

static void LoadOriginalD3D11() {
    if (s_realD3D11) return;
    
    // Get system directory
    char systemDir[MAX_PATH];
    GetSystemDirectoryA(systemDir, MAX_PATH);
    
    // Build path to real d3d11.dll
    char realD3D11Path[MAX_PATH];
    snprintf(realD3D11Path, MAX_PATH, "%s\\d3d11.dll", systemDir);
    
    s_realD3D11 = LoadLibraryA(realD3D11Path);
    if (!s_realD3D11) {
        // Fallback - try loading without path
        s_realD3D11 = LoadLibraryA("d3d11.dll");
    }
    
    if (s_realD3D11) {
        s_originalD3D11CreateDevice = (D3D11CreateDevice_t)GetProcAddress(s_realD3D11, "D3D11CreateDevice");
        s_originalD3D11CreateDeviceAndSwapChain = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(s_realD3D11, "D3D11CreateDeviceAndSwapChain");
        
        OutputDebugStringA("FrameGen: Loaded original D3D11 functions\n");
    } else {
        OutputDebugStringA("FrameGen: Failed to load original d3d11.dll\n");
    }
}

extern "C" {
    HRESULT WINAPI D3D11CreateDevice(
        IDXGIAdapter* pAdapter,
        D3D_DRIVER_TYPE DriverType,
        HMODULE Software,
        UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels,
        UINT FeatureLevels,
        UINT SDKVersion,
        ID3D11Device** ppDevice,
        D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext
    ) {
        LoadOriginalD3D11();
        if (s_originalD3D11CreateDevice) {
            return s_originalD3D11CreateDevice(pAdapter, DriverType, Software, Flags, 
                                               pFeatureLevels, FeatureLevels, SDKVersion,
                                               ppDevice, pFeatureLevel, ppImmediateContext);
        }
        // Fallback - try to load real d3d11.dll again and call directly
        char systemDir[MAX_PATH];
        GetSystemDirectoryA(systemDir, MAX_PATH);
        char realD3D11Path[MAX_PATH];
        snprintf(realD3D11Path, MAX_PATH, "%s\\d3d11.dll", systemDir);
        HMODULE realD3D11 = LoadLibraryA(realD3D11Path);
        if (realD3D11) {
            D3D11CreateDevice_t func = (D3D11CreateDevice_t)GetProcAddress(realD3D11, "D3D11CreateDevice");
            if (func) {
                return func(pAdapter, DriverType, Software, Flags, 
                           pFeatureLevels, FeatureLevels, SDKVersion,
                           ppDevice, pFeatureLevel, ppImmediateContext);
            }
        }
        return E_FAIL;
    }

    HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
        IDXGIAdapter* pAdapter,
        D3D_DRIVER_TYPE DriverType,
        HMODULE Software,
        UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels,
        UINT FeatureLevels,
        UINT SDKVersion,
        const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
        IDXGISwapChain** ppSwapChain,
        ID3D11Device** ppDevice,
        D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext
    ) {
        LoadOriginalD3D11();
        if (s_originalD3D11CreateDeviceAndSwapChain) {
            return s_originalD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags,
                                                           pFeatureLevels, FeatureLevels, SDKVersion,
                                                           pSwapChainDesc, ppSwapChain, ppDevice,
                                                           pFeatureLevel, ppImmediateContext);
        }
        // Fallback - try to load real d3d11.dll again and call directly
        char systemDir[MAX_PATH];
        GetSystemDirectoryA(systemDir, MAX_PATH);
        char realD3D11Path[MAX_PATH];
        snprintf(realD3D11Path, MAX_PATH, "%s\\d3d11.dll", systemDir);
        HMODULE realD3D11 = LoadLibraryA(realD3D11Path);
        if (realD3D11) {
            D3D11CreateDeviceAndSwapChain_t func = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(realD3D11, "D3D11CreateDeviceAndSwapChain");
            if (func) {
                return func(pAdapter, DriverType, Software, Flags,
                           pFeatureLevels, FeatureLevels, SDKVersion,
                           pSwapChainDesc, ppSwapChain, ppDevice,
                           pFeatureLevel, ppImmediateContext);
            }
        }
        return E_FAIL;
    }
}

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
    // Wait for D3D11 to be loaded and game to fully initialize
    // Don't wait too long - let the game start first
    Sleep(1000);
    
    // Initialize in background - don't block if it fails
    if (!Initialize()) {
        // Log error but don't show MessageBox - it can crash some games
        OutputDebugStringA("FrameGen: Failed to initialize!\n");
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
