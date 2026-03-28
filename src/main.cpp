/**
 * Frame Generation DLL for DirectX 11 Games
 */

#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

#include "MinHook.h"
#include "d3d11_hook.h"
#include "frame_interpolator.h"
#include "menu.h"
#include "input_manager.h"
#include "config.h"

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

// Logging to file in game directory - use simple path first
static const char* LOG_FILE = "framedll.log";
static bool g_logInitialized = false;

void LogMsg(const char* fmt, ...) {
    // Try multiple locations for log file
    const char* paths[] = {
        "framedll.log",                              // Current directory (game dir)
        "C:\\framedll.log",                          // Root C: for easy access
        ".\\framedll.log"                            // Explicit current dir
    };
    
    FILE* f = nullptr;
    for (const char* path : paths) {
        f = fopen(path, "a");
        if (f) break;
    }
    
    if (!f) return;  // Can't log anywhere
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fprintf(f, "\n");
    fflush(f);  // Force write
    fclose(f);

    // Also to debugger
    char buf[2048];
    va_start(args, fmt);
    vsprintf_s(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

bool Initialize();
void Shutdown();
DWORD WINAPI MainThread(LPVOID lpParam);

typedef HRESULT(WINAPI* D3D11CreateDeviceFn_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
typedef HRESULT(WINAPI* D3D11CreateDeviceAndSwapChainFn_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

static D3D11CreateDeviceFn_t s_origCreateDevice = nullptr;
static D3D11CreateDeviceAndSwapChainFn_t s_origCreateDeviceAndSwapChain = nullptr;
static HMODULE s_realD3D11 = nullptr;
static std::mutex s_loadMutex;

static void LoadOriginalD3D11() {
    if (s_realD3D11) return;
    std::lock_guard<std::mutex> lock(s_loadMutex);
    if (s_realD3D11) return;

    LogMsg("[INIT] Loading original d3d11.dll...");
    
    char sysDir[MAX_PATH];
    if (GetSystemDirectoryA(sysDir, MAX_PATH)) {
        char dllPath[MAX_PATH];
        snprintf(dllPath, MAX_PATH, "%s\\d3d11.dll", sysDir);
        LogMsg("[INIT] Trying: %s", dllPath);
        s_realD3D11 = LoadLibraryA(dllPath);
    }
    
    if (!s_realD3D11) {
        LogMsg("[INIT] Fallback to d3d11.dll");
        s_realD3D11 = LoadLibraryA("d3d11.dll");
    }

    if (s_realD3D11) {
        s_origCreateDevice = (D3D11CreateDeviceFn_t)GetProcAddress(s_realD3D11, "D3D11CreateDevice");
        s_origCreateDeviceAndSwapChain = (D3D11CreateDeviceAndSwapChainFn_t)GetProcAddress(s_realD3D11, "D3D11CreateDeviceAndSwapChain");
        LogMsg("[OK] Loaded original d3d11.dll at 0x%p", (void*)s_realD3D11);
        LogMsg("[OK] D3D11CreateDevice: 0x%p", (void*)s_origCreateDevice);
        LogMsg("[OK] D3D11CreateDeviceAndSwapChain: 0x%p", (void*)s_origCreateDeviceAndSwapChain);
    } else {
        LogMsg("[ERROR] Failed to load original d3d11.dll! Error: %lu", GetLastError());
    }
}

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

extern "C" {
    // Proxy functions - these are exported via exports.def with proper names
    __declspec(dllexport) HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType,
        HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
        UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext) 
    {
        LogMsg("[API] D3D11CreateDevice called");
        LoadOriginalD3D11();
        
        if (!s_realD3D11) {
            LogMsg("[ERROR] Original DLL not loaded!");
            return E_FAIL;
        }

        HRESULT hr = E_FAIL;
        if (s_origCreateDevice) {
            LogMsg("[API] Calling original...");
            hr = s_origCreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
        } else {
            auto fn = (D3D11CreateDeviceFn_t)GetProcAddress(s_realD3D11, "D3D11CreateDevice");
            if (fn) hr = fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
        }

        if (SUCCEEDED(hr))
            LogMsg("[OK] Device: 0x%p, Context: 0x%p", (void*)(ppDevice?*ppDevice:nullptr), (void*)(ppImmediateContext?*ppImmediateContext:nullptr));
        else
            LogMsg("[ERROR] Returned 0x%08lX", hr);
        
        return hr;
    }

    __declspec(dllexport) HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType,
        HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
        UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
        ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
    {
        LogMsg("[API] D3D11CreateDeviceAndSwapChain called");
        LoadOriginalD3D11();
        
        if (!s_realD3D11) {
            LogMsg("[ERROR] Original DLL not loaded!");
            return E_FAIL;
        }

        HRESULT hr = E_FAIL;
        if (s_origCreateDeviceAndSwapChain) {
            LogMsg("[API] Calling original...");
            hr = s_origCreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
        } else {
            auto fn = (D3D11CreateDeviceAndSwapChainFn_t)GetProcAddress(s_realD3D11, "D3D11CreateDeviceAndSwapChain");
            if (fn) hr = fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
        }

        if (SUCCEEDED(hr)) {
            LogMsg("[OK] Device: 0x%p, Context: 0x%p, SwapChain: 0x%p",
                (void*)(ppDevice?*ppDevice:nullptr), (void*)(ppImmediateContext?*ppImmediateContext:nullptr), (void*)(ppSwapChain?*ppSwapChain:nullptr));
        } else {
            LogMsg("[ERROR] Returned 0x%08lX", hr);
        }
        
        return hr;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        FrameGen::g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        LogMsg("========================================");
        LogMsg("FrameGen DLL Loaded!");
        char dllPath[MAX_PATH];
        GetModuleFileNameA(hModule, dllPath, MAX_PATH);
        LogMsg("DLL Path: %s", dllPath);
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        LogMsg("Game EXE: %s", exePath);
        LogMsg("========================================");
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        LogMsg("FrameGen DLL Unloading...");
        Shutdown();
        if (s_realD3D11) FreeLibrary(s_realD3D11);
        break;
    }
    return TRUE;
}

DWORD WINAPI MainThread(LPVOID) {
    Sleep(1000);
    if (!Initialize()) {
        LogMsg("[ERROR] Initialization failed!");
    }
    return 0;
}

bool Initialize() {
    if (FrameGen::g_initialized) return true;
    
    LogMsg("[INIT] Starting FrameGen initialization...");
    
    if (MH_Initialize() != MH_OK) {
        LogMsg("[ERROR] MinHook initialization failed!");
        return false;
    }
    
    FrameGen::g_config.Load();
    FrameGen::g_inputManager = new InputManager();
    FrameGen::g_interpolator = new FrameInterpolator(&FrameGen::g_config);
    FrameGen::g_menu = new Menu(&FrameGen::g_config, FrameGen::g_interpolator);
    FrameGen::g_d3dHook = new D3D11Hook(FrameGen::g_interpolator, FrameGen::g_menu, FrameGen::g_inputManager);
    
    if (!FrameGen::g_d3dHook->Initialize()) {
        LogMsg("[ERROR] D3D11Hook initialization failed!");
        return false;
    }
    
    FrameGen::g_initialized = true;
    LogMsg("[OK] FrameGen initialized successfully!");
    return true;
}

void Shutdown() {
    if (FrameGen::g_unloading) return;
    FrameGen::g_unloading = true;
    LogMsg("[SHUTDOWN] Cleaning up...");
    
    if (FrameGen::g_config.IsModified()) FrameGen::g_config.Save();
    if (FrameGen::g_d3dHook) { FrameGen::g_d3dHook->Shutdown(); delete FrameGen::g_d3dHook; FrameGen::g_d3dHook = nullptr; }
    if (FrameGen::g_menu) { delete FrameGen::g_menu; FrameGen::g_menu = nullptr; }
    if (FrameGen::g_interpolator) { delete FrameGen::g_interpolator; FrameGen::g_interpolator = nullptr; }
    if (FrameGen::g_inputManager) { delete FrameGen::g_inputManager; FrameGen::g_inputManager = nullptr; }
    
    MH_Uninitialize();
    FrameGen::g_initialized = false;
    LogMsg("[SHUTDOWN] Done.");
}
