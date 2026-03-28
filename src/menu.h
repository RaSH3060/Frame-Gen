#pragma once

// Disable Windows min/max macros
#define NOMINMAX
#include <d3d11.h>
#include <string>
#include "config.h"

// Forward declaration for ImGui
struct ImGuiContext;

class FrameInterpolator;

class Menu {
public:
    Menu(Config* config, FrameInterpolator* interpolator);
    ~Menu();
    
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    
    void Render(ID3D11RenderTargetView* renderTarget);
    
    void Toggle();
    bool IsOpen() const { return m_isOpen; }
    
    Config* GetConfig() const { return m_config; }
    
private:
    void BeginFrame();
    void EndFrame();
    
    void RenderMainMenu();
    void RenderSettingsTab();
    void RenderPerformanceTab();
    void RenderAdvancedTab();
    void RenderHelpTab();
    
    void ApplyStyle();
    
private:
    Config* m_config;
    FrameInterpolator* m_interpolator;
    
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    
    bool m_isOpen;
    bool m_initialized;
    
    // Menu state
    int m_currentTab;
    float m_fpsHistory[100];
    int m_fpsHistoryIndex;
    
    // Window state
    bool m_showFPSOverlay;
    
    // Style
    float m_menuWidth;
    float m_menuHeight;
};
