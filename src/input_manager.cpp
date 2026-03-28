#include "input_manager.h"
#include <algorithm>

InputManager* InputManager::s_instance = nullptr;

InputManager::InputManager()
    : m_mouseX(0)
    , m_mouseY(0)
    , m_mouseDeltaX(0)
    , m_mouseDeltaY(0)
    , m_blockGameInput(false)
    , m_rawInputInitialized(false)
    , m_originalWndProc(nullptr)
{
    s_instance = this;
    m_keyStates.reset();
    m_prevKeyStates.reset();
    m_mouseStates.reset();
    m_prevMouseStates.reset();
    
    InitializeRawInput();
}

InputManager::~InputManager() {
    if (m_originalWndProc && m_hwnd) {
        SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)m_originalWndProc);
    }
    s_instance = nullptr;
}

void InputManager::InitializeRawInput() {
    // Get active window
    m_hwnd = GetActiveWindow();
    if (!m_hwnd) {
        m_hwnd = GetForegroundWindow();
    }
    
    if (m_hwnd) {
        // Hook window procedure
        m_originalWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)&HookedWndProc);
        
        // Setup raw input for keyboard and mouse
        RAWINPUTDEVICE rid[2];
        
        // Keyboard
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage = 0x06;
        rid[0].dwFlags = RIDEV_NOLEGACY; // Skip legacy messages
        rid[0].hwndTarget = m_hwnd;
        
        // Mouse
        rid[1].usUsagePage = 0x01;
        rid[1].usUsage = 0x02;
        rid[1].dwFlags = 0;
        rid[1].hwndTarget = m_hwnd;
        
        if (RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE))) {
            m_rawInputInitialized = true;
        }
    }
}

LRESULT CALLBACK InputManager::HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputManager* self = s_instance;
    if (!self || !self->m_originalWndProc) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    
    // Handle raw input
    if (msg == WM_INPUT) {
        self->ProcessRawInput((HRAWINPUT)lParam);
    }
    
    // Handle mouse position
    if (msg == WM_MOUSEMOVE) {
        self->m_mouseX = LOWORD(lParam);
        self->m_mouseY = HIWORD(lParam);
    }
    
    // Handle mouse buttons
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        self->m_mouseStates[0] = true;
    }
    if (msg == WM_LBUTTONUP) {
        self->m_mouseStates[0] = false;
    }
    if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) {
        self->m_mouseStates[1] = true;
    }
    if (msg == WM_RBUTTONUP) {
        self->m_mouseStates[1] = false;
    }
    if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) {
        self->m_mouseStates[2] = true;
    }
    if (msg == WM_MBUTTONUP) {
        self->m_mouseStates[2] = false;
    }
    
    // Handle keyboard
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        self->m_keyStates[wParam] = true;
    }
    if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        self->m_keyStates[wParam] = false;
    }
    
    // Block input if menu is open
    if (self->m_blockGameInput) {
        // Allow only specific keys through
        switch (msg) {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_INPUT:
                return 0; // Block these messages
        }
    }
    
    return CallWindowProc(self->m_originalWndProc, hWnd, msg, wParam, lParam);
}

void InputManager::ProcessRawInput(HRAWINPUT hRawInput) {
    UINT size = 0;
    GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    
    if (size == 0) return;
    
    if (m_rawInputBuffer.size() < size) {
        m_rawInputBuffer.resize(size);
    }
    
    if (GetRawInputData(hRawInput, RID_INPUT, m_rawInputBuffer.data(), &size, 
                        sizeof(RAWINPUTHEADER)) == size) {
        RAWINPUT* raw = (RAWINPUT*)m_rawInputBuffer.data();
        
        if (raw->header.dwType == RIM_TYPEKEYBOARD) {
            USHORT keyCode = raw->data.keyboard.VKey;
            bool isDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
            
            if (keyCode < 256) {
                m_keyStates[keyCode] = isDown;
            }
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE) {
            m_mouseDeltaX += raw->data.mouse.lLastX;
            m_mouseDeltaY += raw->data.mouse.lLastY;
            
            // Handle mouse button changes
            USHORT flags = raw->data.mouse.usButtonFlags;
            if (flags & RI_MOUSE_BUTTON_1_DOWN) m_mouseStates[0] = true;
            if (flags & RI_MOUSE_BUTTON_1_UP) m_mouseStates[0] = false;
            if (flags & RI_MOUSE_BUTTON_2_DOWN) m_mouseStates[1] = true;
            if (flags & RI_MOUSE_BUTTON_2_UP) m_mouseStates[1] = false;
            if (flags & RI_MOUSE_BUTTON_3_DOWN) m_mouseStates[2] = true;
            if (flags & RI_MOUSE_BUTTON_3_UP) m_mouseStates[2] = false;
            if (flags & RI_MOUSE_BUTTON_4_DOWN) m_mouseStates[3] = true;
            if (flags & RI_MOUSE_BUTTON_4_UP) m_mouseStates[3] = false;
            if (flags & RI_MOUSE_BUTTON_5_DOWN) m_mouseStates[4] = true;
            if (flags & RI_MOUSE_BUTTON_5_UP) m_mouseStates[4] = false;
        }
    }
}

void InputManager::Update() {
    // Store previous states
    m_prevKeyStates = m_keyStates;
    m_prevMouseStates = m_mouseStates;
    
    // Reset deltas
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
    
    // Get current key states from Windows
    for (int i = 0; i < 256; i++) {
        SHORT state = GetAsyncKeyState(i);
        if (state & 0x8000) {
            m_keyStates[i] = true;
        } else {
            m_keyStates[i] = false;
        }
    }
    
    // Get mouse position
    POINT pt;
    if (GetCursorPos(&pt)) {
        if (ScreenToClient(m_hwnd, &pt)) {
            m_mouseX = pt.x;
            m_mouseY = pt.y;
        }
    }
    
    // Get mouse button states
    m_mouseStates[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    m_mouseStates[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    m_mouseStates[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
    m_mouseStates[3] = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
    m_mouseStates[4] = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
}

void InputManager::BlockGameInput(bool block) {
    m_blockGameInput = block;
}

bool InputManager::IsKeyDown(int keyCode) const {
    if (keyCode < 0 || keyCode >= 256) return false;
    return m_keyStates[keyCode];
}

bool InputManager::IsKeyJustPressed(int keyCode) {
    if (keyCode < 0 || keyCode >= 256) return false;
    return m_keyStates[keyCode] && !m_prevKeyStates[keyCode];
}

bool InputManager::IsKeyJustReleased(int keyCode) {
    if (keyCode < 0 || keyCode >= 256) return false;
    return !m_keyStates[keyCode] && m_prevKeyStates[keyCode];
}

bool InputManager::IsMouseButtonDown(int button) const {
    if (button < 0 || button >= 5) return false;
    return m_mouseStates[button];
}

bool InputManager::IsMouseButtonJustPressed(int button) {
    if (button < 0 || button >= 5) return false;
    return m_mouseStates[button] && !m_prevMouseStates[button];
}

void InputManager::GetMousePosition(int& x, int& y) const {
    x = m_mouseX;
    y = m_mouseY;
}

void InputManager::GetMouseDelta(int& dx, int& dy) {
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}
