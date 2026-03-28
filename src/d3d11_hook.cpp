#include "d3d11_hook.h"
#include "config.h"
#include <vector>

// Global instance for static hook functions
D3D11Hook* D3D11Hook::s_instance = nullptr;

D3D11Hook::D3D11Hook(FrameInterpolator* interpolator, Menu* menu, InputManager* inputManager)
    : m_interpolator(interpolator)
    , m_menu(menu)
    , m_inputManager(inputManager)
    , m_device(nullptr)
    , m_context(nullptr)
    , m_mainRenderTargetView(nullptr)
    , m_backBuffer(nullptr)
    , m_originalPresent(nullptr)
    , m_originalResizeBuffers(nullptr)
    , m_originalDrawIndexed(nullptr)
    , m_originalDraw(nullptr)
    , m_initialized(false)
    , m_width(0)
    , m_height(0)
    , m_frameCount(0)
{
    s_instance = this;
}

D3D11Hook::~D3D11Hook() {
    Shutdown();
    s_instance = nullptr;
}

bool D3D11Hook::Initialize() {
    if (m_initialized) return true;
    
    // Get D3D11 function addresses by creating a temporary device
    if (!GetD3D11Addresses()) {
        return false;
    }
    
    // Create hooks
    if (MH_CreateHook(reinterpret_cast<LPVOID>(m_originalPresent), 
                      reinterpret_cast<LPVOID>(&HookedPresent),
                      reinterpret_cast<LPVOID*>(&m_originalPresent)) != MH_OK) {
        return false;
    }
    
    if (MH_CreateHook(reinterpret_cast<LPVOID>(m_originalResizeBuffers),
                      reinterpret_cast<LPVOID>(&HookedResizeBuffers),
                      reinterpret_cast<LPVOID*>(&m_originalResizeBuffers)) != MH_OK) {
        return false;
    }
    
    // Enable hooks
    if (MH_EnableHook(m_originalPresent) != MH_OK) {
        return false;
    }
    
    if (MH_EnableHook(m_originalResizeBuffers) != MH_OK) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

void D3D11Hook::Shutdown() {
    if (!m_initialized) return;
    
    MH_DisableHook(m_originalPresent);
    MH_DisableHook(m_originalResizeBuffers);
    
    ReleaseRenderTarget();
    
    m_initialized = false;
}

bool D3D11Hook::GetD3D11Addresses() {
    // Create temporary device and swap chain to get VTable addresses
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, 
                      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, 
                      L"FrameGenTemp", nullptr };
    RegisterClassEx(&wc);
    
    HWND hwnd = CreateWindow(wc.lpszClassName, L"FrameGenTemp", WS_OVERLAPPEDWINDOW,
                             0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    
    if (!hwnd) return false;
    
    // Create D3D11 device and swap chain
    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 2;
    scDesc.BufferDesc.Width = 100;
    scDesc.BufferDesc.Height = 100;
    scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferDesc.RefreshRate.Numerator = 60;
    scDesc.BufferDesc.RefreshRate.Denominator = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hwnd;
    scDesc.SampleDesc.Count = 1;
    scDesc.Windowed = TRUE;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    ID3D11Device* tempDevice = nullptr;
    ID3D11DeviceContext* tempContext = nullptr;
    IDXGISwapChain* tempSwapChain = nullptr;
    
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                0, nullptr, 0, D3D11_SDK_VERSION,
                                                &scDesc, &tempSwapChain, &tempDevice,
                                                &featureLevel, &tempContext);
    
    if (FAILED(hr)) {
        DestroyWindow(hwnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }
    
    // Get VTable addresses
    void** swapChainVTable = *(void***)tempSwapChain;
    m_originalPresent = (Present_t)swapChainVTable[8];      // Present is at index 8
    m_originalResizeBuffers = (ResizeBuffers_t)swapChainVTable[13]; // ResizeBuffers is at index 13
    
    // Get device context VTable
    void** contextVTable = *(void***)tempContext;
    m_originalDrawIndexed = (DrawIndexed_t)contextVTable[12]; // DrawIndexed
    m_originalDraw = (Draw_t)contextVTable[13];              // Draw
    
    // Cleanup
    tempSwapChain->Release();
    tempDevice->Release();
    tempContext->Release();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    
    return m_originalPresent != nullptr;
}

bool D3D11Hook::CreateRenderTarget(IDXGISwapChain* pSwapChain) {
    if (m_mainRenderTargetView) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
    if (m_backBuffer) {
        m_backBuffer->Release();
        m_backBuffer = nullptr;
    }
    
    HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_backBuffer);
    if (FAILED(hr)) return false;
    
    hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (LPVOID*)&m_device);
    if (FAILED(hr)) {
        m_backBuffer->Release();
        m_backBuffer = nullptr;
        return false;
    }
    
    m_device->GetImmediateContext(&m_context);
    
    // Get dimensions
    D3D11_TEXTURE2D_DESC desc;
    m_backBuffer->GetDesc(&desc);
    m_width = desc.Width;
    m_height = desc.Height;
    
    // Create render target view
    hr = m_device->CreateRenderTargetView(m_backBuffer, nullptr, &m_mainRenderTargetView);
    if (FAILED(hr)) {
        m_backBuffer->Release();
        m_backBuffer = nullptr;
        return false;
    }
    
    return true;
}

void D3D11Hook::ReleaseRenderTarget() {
    if (m_mainRenderTargetView) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
    if (m_backBuffer) {
        m_backBuffer->Release();
        m_backBuffer = nullptr;
    }
    m_context = nullptr;
    m_device = nullptr;
}

HRESULT __stdcall D3D11Hook::HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    D3D11Hook* self = s_instance;
    if (!self) return self->m_originalPresent(pSwapChain, SyncInterval, Flags);
    
    // Initialize on first call
    if (!self->m_mainRenderTargetView) {
        if (!self->CreateRenderTarget(pSwapChain)) {
            return self->m_originalPresent(pSwapChain, SyncInterval, Flags);
        }
        
        // Initialize interpolator
        self->m_interpolator->Initialize(self->m_device, self->m_context, self->m_width, self->m_height);
        
        // Initialize menu
        self->m_menu->Initialize(self->m_device, self->m_context);
    }
    
    // Get config
    Config* config = self->m_menu->GetConfig();
    
    // Process input
    self->m_inputManager->Update();
    
    // Check for menu toggle
    if (self->m_inputManager->IsKeyJustPressed(config->MenuKey)) {
        self->m_menu->Toggle();
    }
    
    // Check for enable toggle
    if (self->m_inputManager->IsKeyJustPressed(config->ToggleKey)) {
        config->Enabled = !config->Enabled;
        config->SetModified();
    }
    
    // Frame generation logic
    static ID3D11Texture2D* lastFrameTexture = nullptr;
    static bool generatingInterpolatedFrames = false;
    static int interpolatedFramesRemaining = 0;
    
    if (config->Enabled && !self->m_menu->IsOpen()) {
        // Get backbuffer texture
        ID3D11Texture2D* currentFrame = self->m_backBuffer;
        
        if (currentFrame) {
            // Process the real frame
            self->m_interpolator->ProcessFrame(currentFrame);
            
            // Get interpolated frame if available
            ID3D11Texture2D* outputFrame = nullptr;
            if (self->m_interpolator->GetInterpolatedFrame(&outputFrame)) {
                // Copy interpolated frame to backbuffer if it's not the real frame
                if (outputFrame != currentFrame && outputFrame != nullptr) {
                    self->m_context->CopyResource(self->m_backBuffer, outputFrame);
                }
            }
        }
    }
    
    // Draw menu on top
    if (self->m_menu->IsOpen()) {
        self->m_menu->Render(self->m_mainRenderTargetView);
        
        // Block game input when menu is open
        self->m_inputManager->BlockGameInput(true);
    } else {
        self->m_inputManager->BlockGameInput(false);
    }
    
    // Call original Present
    return self->m_originalPresent(pSwapChain, SyncInterval, Flags);
}

HRESULT __stdcall D3D11Hook::HookedResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount,
                                                   UINT Width, UINT Height, DXGI_FORMAT NewFormat,
                                                   UINT SwapChainFlags) {
    D3D11Hook* self = s_instance;
    if (!self) return self->m_originalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    
    // Release resources before resize
    self->ReleaseRenderTarget();
    self->m_menu->Shutdown();
    
    // Call original ResizeBuffers
    HRESULT hr = self->m_originalResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    
    if (SUCCEEDED(hr)) {
        // Recreate resources
        self->CreateRenderTarget(pSwapChain);
        self->m_interpolator->Initialize(self->m_device, self->m_context, Width, Height);
        self->m_menu->Initialize(self->m_device, self->m_context);
    }
    
    return hr;
}

void __stdcall D3D11Hook::HookedDrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount,
                                             UINT StartIndexLocation, INT BaseVertexLocation) {
    D3D11Hook* self = s_instance;
    if (self && self->m_originalDrawIndexed) {
        self->m_originalDrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
    }
}

void __stdcall D3D11Hook::HookedDraw(ID3D11DeviceContext* pContext, UINT VertexCount,
                                      UINT StartVertexLocation) {
    D3D11Hook* self = s_instance;
    if (self && self->m_originalDraw) {
        self->m_originalDraw(pContext, VertexCount, StartVertexLocation);
    }
}
