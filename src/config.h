#pragma once

// Disable Windows min/max macros
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <map>

class Config {
public:
    // Frame Generation Settings
    int FrameMultiplier = 2;        // x2, x3, x4
    bool Enabled = true;
    bool MotionBlurReduction = true;
    bool ShowFPS = true;
    bool ShowOverlay = true;
    
    // Input Lag Compensation
    int InputLagCompensation = 1;   // 0=Off, 1=Low, 2=Medium, 3=High
    bool PredictInput = true;
    
    // Quality Settings
    int InterpolationQuality = 2;   // 0=Fast, 1=Balanced, 2=Quality
    bool UseMotionVectors = true;
    bool SmoothTransitions = true;
    
    // Menu Settings
    float MenuOpacity = 0.95f;
    int MenuKey = VK_DELETE;        // DEL key
    
    // Advanced
    bool VSync = false;
    int MaxPreRenderedFrames = 1;
    bool TripleBuffering = false;
    
    // Hotkeys
    int ToggleKey = VK_INSERT;
    int MenuToggleKey = VK_DELETE;
    
    Config() = default;
    
    void Load() {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string exePath(path);
        std::string configPath = exePath.substr(0, exePath.find_last_of("\\/")) + "\\framegen.ini";
        
        std::ifstream file(configPath);
        if (!file.is_open()) return;
        
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "FrameMultiplier") FrameMultiplier = std::stoi(value);
            else if (key == "Enabled") Enabled = (value == "1" || value == "true");
            else if (key == "MotionBlurReduction") MotionBlurReduction = (value == "1" || value == "true");
            else if (key == "ShowFPS") ShowFPS = (value == "1" || value == "true");
            else if (key == "ShowOverlay") ShowOverlay = (value == "1" || value == "true");
            else if (key == "InputLagCompensation") InputLagCompensation = std::stoi(value);
            else if (key == "PredictInput") PredictInput = (value == "1" || value == "true");
            else if (key == "InterpolationQuality") InterpolationQuality = std::stoi(value);
            else if (key == "UseMotionVectors") UseMotionVectors = (value == "1" || value == "true");
            else if (key == "SmoothTransitions") SmoothTransitions = (value == "1" || value == "true");
            else if (key == "MenuOpacity") MenuOpacity = std::stof(value);
            else if (key == "MenuKey") MenuKey = std::stoi(value);
            else if (key == "ToggleKey") ToggleKey = std::stoi(value);
            else if (key == "VSync") VSync = (value == "1" || value == "true");
            else if (key == "MaxPreRenderedFrames") MaxPreRenderedFrames = std::stoi(value);
            else if (key == "TripleBuffering") TripleBuffering = (value == "1" || value == "true");
        }
        
        file.close();
        m_modified = false;
    }
    
    void Save() {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string exePath(path);
        std::string configPath = exePath.substr(0, exePath.find_last_of("\\/")) + "\\framegen.ini";
        
        std::ofstream file(configPath);
        if (!file.is_open()) return;
        
        file << "[FrameGen Configuration]\n";
        file << "Enabled=" << (Enabled ? "1" : "0") << "\n";
        file << "FrameMultiplier=" << FrameMultiplier << "\n";
        file << "MotionBlurReduction=" << (MotionBlurReduction ? "1" : "0") << "\n";
        file << "ShowFPS=" << (ShowFPS ? "1" : "0") << "\n";
        file << "ShowOverlay=" << (ShowOverlay ? "1" : "0") << "\n";
        file << "\n[Input Lag]\n";
        file << "InputLagCompensation=" << InputLagCompensation << "\n";
        file << "PredictInput=" << (PredictInput ? "1" : "0") << "\n";
        file << "\n[Quality]\n";
        file << "InterpolationQuality=" << InterpolationQuality << "\n";
        file << "UseMotionVectors=" << (UseMotionVectors ? "1" : "0") << "\n";
        file << "SmoothTransitions=" << (SmoothTransitions ? "1" : "0") << "\n";
        file << "\n[Advanced]\n";
        file << "VSync=" << (VSync ? "1" : "0") << "\n";
        file << "MaxPreRenderedFrames=" << MaxPreRenderedFrames << "\n";
        file << "TripleBuffering=" << (TripleBuffering ? "1" : "0") << "\n";
        file << "\n[Hotkeys]\n";
        file << "MenuKey=" << MenuKey << "\n";
        file << "ToggleKey=" << ToggleKey << "\n";
        file << "MenuOpacity=" << MenuOpacity << "\n";
        
        file.close();
        m_modified = false;
    }
    
    bool IsModified() const { return m_modified; }
    void SetModified() { m_modified = true; }
    
private:
    bool m_modified = false;
};
