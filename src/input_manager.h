#pragma once

// Disable Windows min/max macros
#define NOMINMAX
#include <Windows.h>
#include <bitset>
#include <vector>

class InputManager {
public:
    InputManager();
    ~InputManager();
    
    void Update();
    void BlockGameInput(bool block);
    
    bool IsKeyDown(int keyCode) const;
    bool IsKeyJustPressed(int keyCode);
    bool IsKeyJustReleased(int keyCode);
    
    bool IsMouseButtonDown(int button) const;
    bool IsMouseButtonJustPressed(int button);
    
    void GetMousePosition(int& x, int& y) const;
    void GetMouseDelta(int& dx, int& dy);
    
    bool IsGameInputBlocked() const { return m_blockGameInput; }
    
private:
    std::bitset<256> m_keyStates;
    std::bitset<256> m_prevKeyStates;
    
    std::bitset<5> m_mouseStates;
    std::bitset<5> m_prevMouseStates;
    
    int m_mouseX;
    int m_mouseY;
    int m_mouseDeltaX;
    int m_mouseDeltaY;
    
    bool m_blockGameInput;
    
    // Raw input buffer
    std::vector<uint8_t> m_rawInputBuffer;
    bool m_rawInputInitialized;
    
    HWND m_hwnd;
    
    void InitializeRawInput();
    void ProcessRawInput(HRAWINPUT hRawInput);
    
    // Window procedure hook
    static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static InputManager* s_instance;
    
    WNDPROC m_originalWndProc;
};
