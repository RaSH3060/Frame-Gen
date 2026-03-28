#include "menu.h"
#include "frame_interpolator.h"

// ImGui includes
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <Windows.h>
#include <vector>
#include <cmath>

// Win32 input handler for ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Menu::Menu(Config* config, FrameInterpolator* interpolator)
    : m_config(config)
    , m_interpolator(interpolator)
    , m_device(nullptr)
    , m_context(nullptr)
    , m_isOpen(false)
    , m_initialized(false)
    , m_currentTab(0)
    , m_fpsHistoryIndex(0)
    , m_showFPSOverlay(true)
    , m_menuWidth(400.0f)
    , m_menuHeight(500.0f)
{
    memset(m_fpsHistory, 0, sizeof(m_fpsHistory));
}

Menu::~Menu() {
    Shutdown();
}

bool Menu::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (m_initialized) return true;
    
    m_device = device;
    m_context = context;
    
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr; // Don't save to file
    
    // Setup style
    ApplyStyle();
    
    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(GetActiveWindow())) {
        return false;
    }
    
    if (!ImGui_ImplDX11_Init(device, context)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }
    
    m_initialized = true;
    return true;
}

void Menu::Shutdown() {
    if (!m_initialized) return;
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    m_initialized = false;
}

void Menu::Toggle() {
    m_isOpen = !m_isOpen;
    
    if (m_isOpen) {
        // Enable mouse cursor
        ImGuiIO& io = ImGui::GetIO();
        io.WantCaptureMouse = true;
        io.WantCaptureKeyboard = true;
    }
}

void Menu::Render(ID3D11RenderTargetView* renderTarget) {
    if (!m_initialized) return;
    
    BeginFrame();
    
    // Always show FPS overlay if enabled
    if (m_config->ShowFPS && !m_isOpen) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | 
                                  ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_NoFocusOnAppearing;
        
        if (ImGui::Begin("##FPSOverlay", nullptr, flags)) {
            float gameFPS = m_interpolator->GetGameFPS();
            float displayFPS = m_interpolator->GetDisplayFPS();
            
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Game: %.1f FPS", gameFPS);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Display: %.1f FPS", displayFPS);
            ImGui::Text("Multiplier: x%d", m_config->FrameMultiplier);
            
            // Update FPS history
            m_fpsHistory[m_fpsHistoryIndex] = displayFPS;
            m_fpsHistoryIndex = (m_fpsHistoryIndex + 1) % 100;
            
            // Mini graph
            ImGui::PlotLines("##fps", m_fpsHistory, 100, m_fpsHistoryIndex, 
                            nullptr, 0.0f, 200.0f, ImVec2(120, 40));
        }
        ImGui::End();
    }
    
    // Main menu
    if (m_isOpen) {
        RenderMainMenu();
    }
    
    EndFrame();
}

void Menu::BeginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Menu::EndFrame() {
    ImGui::Render();
    
    // Render ImGui draw data
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Menu::RenderMainMenu() {
    ImGui::SetNextWindowSize(ImVec2(m_menuWidth, m_menuHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    
    if (!ImGui::Begin("FrameGen Settings", &m_isOpen, windowFlags)) {
        ImGui::End();
        return;
    }
    
    // Tab bar
    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Settings")) {
            RenderSettingsTab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Performance")) {
            RenderPerformanceTab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Advanced")) {
            RenderAdvancedTab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Help")) {
            RenderHelpTab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    // Status bar at bottom
    ImGui::Separator();
    
    float gameFPS = m_interpolator->GetGameFPS();
    float displayFPS = m_interpolator->GetDisplayFPS();
    
    ImGui::Text("Status: %s", m_config->Enabled ? "Enabled" : "Disabled");
    ImGui::SameLine(200);
    ImGui::Text("FPS: %.0f -> %.0f", gameFPS, displayFPS);
    
    ImGui::End();
}

void Menu::RenderSettingsTab() {
    ImGui::Spacing();
    
    // Enable/Disable
    if (ImGui::Checkbox("Enable Frame Generation", &m_config->Enabled)) {
        m_config->SetModified();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Frame Multiplier
    ImGui::Text("Frame Multiplier:");
    
    int multiplier = m_config->FrameMultiplier;
    
    // Radio buttons for x2, x3, x4
    ImGui::Columns(3, "multicols", false);
    
    if (ImGui::RadioButton("x2", multiplier == 2)) {
        m_config->FrameMultiplier = 2;
        m_config->SetModified();
        m_interpolator->UpdateConfig();
    }
    ImGui::NextColumn();
    
    if (ImGui::RadioButton("x3", multiplier == 3)) {
        m_config->FrameMultiplier = 3;
        m_config->SetModified();
        m_interpolator->UpdateConfig();
    }
    ImGui::NextColumn();
    
    if (ImGui::RadioButton("x4", multiplier == 4)) {
        m_config->FrameMultiplier = 4;
        m_config->SetModified();
        m_interpolator->UpdateConfig();
    }
    ImGui::Columns(1);
    
    ImGui::Spacing();
    
    // Slider for fine control
    ImGui::SliderInt("Multiplier", &m_config->FrameMultiplier, 2, 4);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
        m_interpolator->UpdateConfig();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Input Lag Compensation
    ImGui::Text("Input Lag Compensation:");
    
    const char* lagLevels[] = { "Off", "Low", "Medium", "High" };
    int lagLevel = m_config->InputLagCompensation;
    
    ImGui::Combo("##LagComp", &lagLevel, lagLevels, IM_ARRAYSIZE(lagLevels));
    if (lagLevel != m_config->InputLagCompensation) {
        m_config->InputLagCompensation = lagLevel;
        m_config->SetModified();
    }
    
    ImGui::Checkbox("Predict Input", &m_config->PredictInput);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Display options
    ImGui::Text("Display Options:");
    
    if (ImGui::Checkbox("Show FPS Overlay", &m_config->ShowFPS)) {
        m_config->SetModified();
    }
    
    if (ImGui::Checkbox("Show Overlay", &m_config->ShowOverlay)) {
        m_config->SetModified();
    }
    
    if (ImGui::Checkbox("Motion Blur Reduction", &m_config->MotionBlurReduction)) {
        m_config->SetModified();
    }
}

void Menu::RenderPerformanceTab() {
    ImGui::Spacing();
    
    // FPS Graph
    ImGui::Text("Display FPS History:");
    
    // Update FPS history
    float displayFPS = m_interpolator->GetDisplayFPS();
    m_fpsHistory[m_fpsHistoryIndex] = displayFPS;
    m_fpsHistoryIndex = (m_fpsHistoryIndex + 1) % 100;
    
    char overlay[32];
    sprintf(overlay, "Current: %.1f FPS", displayFPS);
    
    ImGui::PlotLines("##FPSGraph", m_fpsHistory, 100, m_fpsHistoryIndex, 
                     overlay, 0.0f, 200.0f, ImVec2(-1, 80));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Statistics
    ImGui::Text("Statistics:");
    
    float gameFPS = m_interpolator->GetGameFPS();
    float frameTime = m_interpolator->GetFrameTime() * 1000.0f; // Convert to ms
    int interpFrames = m_interpolator->GetInterpolatedFramesCount();
    int totalFrames = m_interpolator->GetTotalFramesCount();
    
    ImGui::Columns(2, "statscols", false);
    
    ImGui::Text("Game FPS:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%.1f", gameFPS);
    ImGui::NextColumn();
    
    ImGui::Text("Display FPS:");
    ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%.1f", displayFPS);
    ImGui::NextColumn();
    
    ImGui::Text("Frame Time:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", frameTime);
    ImGui::NextColumn();
    
    ImGui::Text("Multiplier:");
    ImGui::NextColumn();
    ImGui::Text("x%d", m_config->FrameMultiplier);
    ImGui::NextColumn();
    
    ImGui::Text("Interpolated Frames:");
    ImGui::NextColumn();
    ImGui::Text("%d", interpFrames);
    ImGui::NextColumn();
    
    ImGui::Text("Total Frames:");
    ImGui::NextColumn();
    ImGui::Text("%d", totalFrames);
    ImGui::Columns(1);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Quality settings
    ImGui::Text("Interpolation Quality:");
    
    const char* qualityLevels[] = { "Fast", "Balanced", "Quality" };
    int quality = m_config->InterpolationQuality;
    
    ImGui::Combo("##Quality", &quality, qualityLevels, IM_ARRAYSIZE(qualityLevels));
    if (quality != m_config->InterpolationQuality) {
        m_config->InterpolationQuality = quality;
        m_config->SetModified();
    }
    
    ImGui::Checkbox("Use Motion Vectors", &m_config->UseMotionVectors);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
    }
    
    ImGui::Checkbox("Smooth Transitions", &m_config->SmoothTransitions);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
    }
}

void Menu::RenderAdvancedTab() {
    ImGui::Spacing();
    
    ImGui::Text("Advanced Settings");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Warning: These settings may affect stability");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // VSync
    if (ImGui::Checkbox("VSync", &m_config->VSync)) {
        m_config->SetModified();
    }
    
    // Max Pre-rendered Frames
    ImGui::SliderInt("Max Pre-rendered Frames", &m_config->MaxPreRenderedFrames, 0, 4);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
    }
    
    // Triple Buffering
    if (ImGui::Checkbox("Triple Buffering", &m_config->TripleBuffering)) {
        m_config->SetModified();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Hotkey settings
    ImGui::Text("Hotkeys:");
    
    // Menu Key
    ImGui::Text("Menu Toggle Key:");
    ImGui::SameLine();
    const char* keyName = "DEL";
    switch (m_config->MenuKey) {
        case VK_DELETE: keyName = "DEL"; break;
        case VK_INSERT: keyName = "INS"; break;
        case VK_HOME: keyName = "HOME"; break;
        case VK_END: keyName = "END"; break;
        case VK_F1: keyName = "F1"; break;
        case VK_F2: keyName = "F2"; break;
        case VK_F12: keyName = "F12"; break;
    }
    
    if (ImGui::Button(keyName)) {
        // Key capture would be implemented here
    }
    
    ImGui::Spacing();
    
    // Menu Opacity
    ImGui::SliderFloat("Menu Opacity", &m_config->MenuOpacity, 0.5f, 1.0f);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_config->SetModified();
        ApplyStyle();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Save/Load
    if (ImGui::Button("Save Settings")) {
        m_config->Save();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Load Settings")) {
        m_config->Load();
        ApplyStyle();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Reset Defaults")) {
        *m_config = Config();
        m_config->SetModified();
        ApplyStyle();
    }
}

void Menu::RenderHelpTab() {
    ImGui::Spacing();
    
    ImGui::Text("FrameGen - Frame Generation for DirectX 11 Games");
    ImGui::Spacing();
    
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("How to use:");
    ImGui::BulletText("Place d3d11.dll in the game folder");
    ImGui::BulletText("Start the game - frame generation starts automatically");
    ImGui::BulletText("Press DEL to open/close the menu");
    ImGui::BulletText("Press INS to toggle frame generation on/off");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Frame Multipliers:");
    ImGui::BulletText("x2: 60 FPS -> 120 FPS (Recommended)");
    ImGui::BulletText("x3: 60 FPS -> 180 FPS (High performance)");
    ImGui::BulletText("x4: 60 FPS -> 240 FPS (For high refresh rate monitors)");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Input Lag Compensation:");
    ImGui::BulletText("Off: No compensation, maximum smoothness");
    ImGui::BulletText("Low: Slight prediction for faster response");
    ImGui::BulletText("Medium: Balanced prediction");
    ImGui::BulletText("High: Maximum responsiveness for competitive games");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Tips:");
    ImGui::BulletText("Lower game FPS before frame gen for better input lag");
    ImGui::BulletText("Use VSync OFF in game for best results");
    ImGui::BulletText("Higher quality = more GPU usage");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Keyboard Shortcuts:");
    ImGui::BulletText("DEL - Toggle menu");
    ImGui::BulletText("INS - Toggle frame generation");
}

void Menu::ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Dark theme with custom colors
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    
    style.Alpha = m_config->MenuOpacity;
    
    // Colors - using green/teal theme (avoiding blue/indigo)
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.30f, 0.25f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.35f, 0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.50f, 0.40f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.28f, 0.80f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.50f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.60f, 0.45f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.17f, 0.90f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.28f, 0.24f, 0.95f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.35f, 0.30f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.25f, 0.20f, 0.90f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.55f, 0.40f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.60f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.20f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.40f, 0.30f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.85f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.70f, 0.50f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.85f, 0.60f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.40f, 0.35f, 0.80f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.52f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.70f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.35f, 0.70f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.45f, 0.85f, 0.60f, 1.00f);
}
