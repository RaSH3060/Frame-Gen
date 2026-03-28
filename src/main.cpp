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
static CRITICAL_SECTION g_initCS;
static volatile LONG g_initLock = 0;
static bool g_csInitialized = false;

static void LoadOriginalD3D11() {
    if (s_realD3D11) return;
    
    // Initialize critical section using interlocked for thread safety
    if (!g_csInitialized) {
        if (InterlockedCompareExchange(&g_initLock, 1, 0) == 0) {
            InitializeCriticalSection(&g_initCS);
            g_csInitialized = true;
        } else {
            // Wait for other thread to initialize
            while (!g_csInitialized) Sleep(1);
        }
    }
    
    EnterCriticalSection(&g_initCS);
    
    // Double-check after acquiring lock
    if (s_realD3D11) {
        LeaveCriticalSection(&g_initCS);
        return;
    }
    
    // Get system directory
    char systemDir[MAX_PATH];
    GetSystemDirectoryA(systemDir, MAX_PATH);
    
    // Build path to real d3d11.dll
    char realD3D11Path[MAX_PATH];
    snprintf(realD3D11Path, MAX_PATH, "%s\\d3d11.dll", systemDir);
    
    s_realD3D11 = LoadLibraryA(realD3D11Path);
    if (!s_realD3D11) {
        s_realD3D11 = LoadLibraryA("d3d11.dll");
    }
    
    if (s_realD3D11) {
        s_originalD3D11CreateDevice = (D3D11CreateDevice_t)GetProcAddress(s_realD3D11, "D3D11CreateDevice");
        s_originalD3D11CreateDeviceAndSwapChain = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(s_realD3D11, "D3D11CreateDeviceAndSwapChain");
        
        char msg[256];
        sprintf_s(msg, sizeof(msg), "FrameGen: Loaded original d3d11.dll at %p\n", (void*)s_realD3D11);
        OutputDebugStringA(msg);
    } else {
        OutputDebugStringA("FrameGen: CRITICAL - Failed to load original d3d11.dll!\n");
    }
    
    LeaveCriticalSection(&g_initCS);
}

// Stub implementations for additional exports
extern "C" {
    HRESULT WINAPI D3D11CoreCreateDevice() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11CoreCreateLayeredDevice() { return E_NOTIMPL; }
    SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize() { return 0; }
    HRESULT WINAPI D3D11CoreRegisterLayers() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11CreateDeviceForD3D12() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11On12CreateDevice() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11Reflect() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11SerializeStateBlock() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11DisassembleStateBlock() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11DisassembleEffect() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11CompileEffectFromMemory() { return E_NOTIMPL; }
    HRESULT WINAPI D3D11CreateBlob() { return E_NOTIMPL; }
}

// Proxy function implementations - MUST match exports.def
extern "C" {
    __declspec(dllexport) HRESULT WINAPI D3D11CreateDevice(
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
        OutputDebugStringA("FrameGen: D3D11CreateDevice called\n");
        
        LoadOriginalD3D11();
        
        HRESULT hr = E_FAIL;
        
        if (s_originalD3D11CreateDevice) {
            hr = s_originalD3D11CreateDevice(pAdapter, DriverType, Software, Flags, 
                                               pFeatureLevels, FeatureLevels, SDKVersion,
                                               ppDevice, pFeatureLevel, ppImmediateContext);
        } else if (s_realD3D11) {
            D3D11CreateDevice_t func = (D3D11CreateDevice_t)GetProcAddress(s_realD3D11, "D3D11CreateDevice");
            if (func) {
                hr = func(pAdapter, DriverType, Software, Flags, 
                           pFeatureLevels, FeatureLevels, SDKVersion,
                           ppDevice, pFeatureLevel, ppImmediateContext);
            }
        }
        
        char msg[256];
        sprintf_s(msg, sizeof(msg), "FrameGen: D3D11CreateDevice returned %08X\n", hr);
        OutputDebugStringA(msg);
        
        return hr;
    }

    __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
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
        OutputDebugStringA("FrameGen: D3D11CreateDeviceAndSwapChain called\n");
        
        LoadOriginalD3D11();
        
        HRESULT hr = E_FAIL;
        
        if (s_originalD3D11CreateDeviceAndSwapChain) {
            hr = s_originalD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags,
                                                           pFeatureLevels, FeatureLevels, SDKVersion,
                                                           pSwapChainDesc, ppSwapChain, ppDevice,
                                                           pFeatureLevel, ppImmediateContext);
        } else if (s_realD3D11) {
            D3D11CreateDeviceAndSwapChain_t func = (D3D11CreateDeviceAndSwapChain_t)GetProcAddress(s_realD3D11, "D3D11CreateDeviceAndSwapChain");
            if (func) {
                hr = func(pAdapter, DriverType, Software, Flags,
                           pFeatureLevels, FeatureLevels, SDKVersion,
                           pSwapChainDesc, ppSwapChain, ppDevice,
                           pFeatureLevel, ppImmediateContext);
            }
        }
        
        char msg[256];
        sprintf_s(msg, sizeof(msg), "FrameGen: D3D11CreateDeviceAndSwapChain returned %08X\n", hr);
        OutputDebugStringA(msg);
        
        return hr;
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
        if (g_csInitialized) {
            DeleteCriticalSection(&g_initCS);
        }
        break;
    }
    return TRUE;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    Sleep(1000);
    
    if (!Initialize()) {
        OutputDebugStringA("FrameGen: Failed to initialize!\n");
    }
    
    return 0;
}

bool Initialize() {
    if (FrameGen::g_initialized) return true;
    
    if (MH_Initialize() != MH_OK) {
        return false;
    }
    
    FrameGen::g_config.Load();
    
    FrameGen::g_inputManager = new InputManager();
    FrameGen::g_interpolator = new FrameInterpolator(&FrameGen::g_config);
    FrameGen::g_menu = new Menu(&FrameGen::g_config, FrameGen::g_interpolator);
    FrameGen::g_d3dHook = new D3D11Hook(
        FrameGen::g_interpolator,
        FrameGen::g_menu,
        FrameGen::g_inputManager
    );
    
    if (!FrameGen::g_d3dHook->Initialize()) {
        return false;
    }
    
    FrameGen::g_initialized = true;
    return true;
}

void Shutdown() {
    if (FrameGen::g_unloading) return;
    FrameGen::g_unloading = true;
    
    if (FrameGen::g_config.IsModified()) {
        FrameGen::g_config.Save();
    }
    
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
