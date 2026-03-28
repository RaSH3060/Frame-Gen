#pragma once

// Disable Windows min/max macros
#define NOMINMAX
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include "MinHook.h"
#include "frame_interpolator.h"
#include "menu.h"
#include "input_manager.h"

class D3D11Hook {
public:
    D3D11Hook(FrameInterpolator* interpolator, Menu* menu, InputManager* inputManager);
    ~D3D11Hook();
    
    bool Initialize();
    void Shutdown();
    
private:
    // Original function pointers
    typedef HRESULT(__stdcall* Present_t)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    typedef HRESULT(__stdcall* ResizeBuffers_t)(IDXGISwapChain* pSwapChain, UINT BufferCount, 
                                                  UINT Width, UINT Height, DXGI_FORMAT NewFormat, 
                                                  UINT SwapChainFlags);
    typedef void(__stdcall* DrawIndexed_t)(ID3D11DeviceContext* pContext, UINT IndexCount, 
                                            UINT StartIndexLocation, INT BaseVertexLocation);
    typedef void(__stdcall* Draw_t)(ID3D11DeviceContext* pContext, UINT VertexCount, 
                                     UINT StartVertexLocation);
    
    // Hooked functions
    static HRESULT __stdcall HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
    static HRESULT __stdcall HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, 
                                                  UINT Width, UINT Height, DXGI_FORMAT NewFormat, 
                                                  UINT SwapChainFlags);
    static void __stdcall HookedDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, 
                                             UINT StartIndexLocation, INT BaseVertexLocation);
    static void __stdcall HookedDraw(ID3D11DeviceContext* pContext, UINT VertexCount, 
                                      UINT StartVertexLocation);
    
    // Helper functions
    bool CreateRenderTarget(IDXGISwapChain* pSwapChain);
    void ReleaseRenderTarget();
    bool GetD3D11Addresses();
    
private:
    FrameInterpolator* m_interpolator;
    Menu* m_menu;
    InputManager* m_inputManager;
    
    // D3D11 objects
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    ID3D11RenderTargetView* m_mainRenderTargetView;
    ID3D11Texture2D* m_backBuffer;
    
    // Original function pointers
    Present_t m_originalPresent;
    ResizeBuffers_t m_originalResizeBuffers;
    DrawIndexed_t m_originalDrawIndexed;
    Draw_t m_originalDraw;
    
    // Initialization state
    bool m_initialized;
    int m_width;
    int m_height;
    
    // Frame counter for interpolation
    int m_frameCount;
    
    // Static instance for hooks
    static D3D11Hook* s_instance;
};
