#pragma once

// Disable Windows min/max macros
#define NOMINMAX
#include <d3d11.h>
#include <vector>
#include <deque>
#include <chrono>
#include <cmath>
#include "config.h"

// Motion vector structure for frame interpolation
struct MotionVector {
    float x, y;
    float confidence;
};

// Frame data structure
struct FrameData {
    ID3D11Texture2D* texture;
    ID3D11RenderTargetView* rtv;
    ID3D11ShaderResourceView* srv;
    int width;
    int height;
    std::vector<std::vector<MotionVector>> motionVectors;
    std::chrono::high_resolution_clock::time_point timestamp;
    float deltaTime;
};

class FrameInterpolator {
public:
    FrameInterpolator(Config* config);
    ~FrameInterpolator();
    
    // Initialize resources
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height);
    
    // Process a new frame
    void ProcessFrame(ID3D11Texture2D* sourceTexture);
    
    // Get interpolated frame for display
    bool GetInterpolatedFrame(ID3D11Texture2D** outTexture);
    
    // Update configuration
    void UpdateConfig();
    
    // Get current FPS
    float GetGameFPS() const { return m_gameFPS; }
    float GetDisplayFPS() const { return m_displayFPS; }
    float GetFrameTime() const { return m_frameTime; }
    
    // Statistics
    int GetInterpolatedFramesCount() const { return m_interpolatedFramesCount; }
    int GetTotalFramesCount() const { return m_totalFramesCount; }
    
    // Reset state
    void Reset();
    
private:
    // Create textures for frame storage
    bool CreateTextures(int width, int height);
    
    // Calculate motion vectors between two frames
    void CalculateMotionVectors(FrameData* prev, FrameData* curr);
    
    // Interpolate a frame between two frames
    void InterpolateFrame(FrameData* prev, FrameData* curr, float t, ID3D11Texture2D* outTexture);
    
    // Simple motion estimation using block matching
    void EstimateMotion(ID3D11Texture2D* prev, ID3D11Texture2D* curr, std::vector<std::vector<MotionVector>>& motionVectors);
    
    // Apply motion vector to pixel
    void ApplyMotionVector(int x, int y, const MotionVector& mv, int width, int height, int& outX, int& outY);
    
    // Input prediction for reduced lag
    void PredictInput();
    
private:
    Config* m_config;
    ID3D11Device* m_device;
    ID3D11DeviceContext* m_context;
    
    int m_width;
    int m_height;
    bool m_initialized;
    
    // Frame buffer (double/triple buffering)
    static const int MAX_FRAMES = 4;
    FrameData m_frames[MAX_FRAMES];
    int m_currentFrameIndex;
    int m_prevFrameIndex;
    
    // Interpolation state
    int m_interpolationIndex;  // Current interpolation step
    int m_interpolationTarget; // Target steps based on multiplier
    
    // Timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    float m_frameTime;
    float m_gameFPS;
    float m_displayFPS;
    
    // Statistics
    int m_interpolatedFramesCount;
    int m_totalFramesCount;
    
    // Intermediate textures for interpolation
    ID3D11Texture2D* m_intermediateTexture;
    ID3D11RenderTargetView* m_intermediateRTV;
    ID3D11ShaderResourceView* m_intermediateSRV;
    
    // Pixel data buffers
    std::vector<uint8_t> m_pixelBuffer1;
    std::vector<uint8_t> m_pixelBuffer2;
    std::vector<uint8_t> m_outputBuffer;
    
    // Motion vector grid
    static const int MOTION_BLOCK_SIZE = 16;
    int m_motionGridWidth;
    int m_motionGridHeight;
    std::vector<std::vector<MotionVector>> m_motionVectors;
};
