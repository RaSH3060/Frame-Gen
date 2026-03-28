#include "frame_interpolator.h"
#include <algorithm>
#include <cstring>

// Undefine Windows macros that conflict with std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

FrameInterpolator::FrameInterpolator(Config* config)
    : m_config(config)
    , m_device(nullptr)
    , m_context(nullptr)
    , m_width(0)
    , m_height(0)
    , m_initialized(false)
    , m_currentFrameIndex(0)
    , m_prevFrameIndex(-1)
    , m_interpolationIndex(0)
    , m_interpolationTarget(2)
    , m_frameTime(0.0f)
    , m_gameFPS(0.0f)
    , m_displayFPS(0.0f)
    , m_interpolatedFramesCount(0)
    , m_totalFramesCount(0)
    , m_intermediateTexture(nullptr)
    , m_intermediateRTV(nullptr)
    , m_intermediateSRV(nullptr)
{
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
}

FrameInterpolator::~FrameInterpolator() {
    Reset();
}

void FrameInterpolator::Reset() {
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (m_frames[i].texture) {
            m_frames[i].texture->Release();
            m_frames[i].texture = nullptr;
        }
        if (m_frames[i].rtv) {
            m_frames[i].rtv->Release();
            m_frames[i].rtv = nullptr;
        }
        if (m_frames[i].srv) {
            m_frames[i].srv->Release();
            m_frames[i].srv = nullptr;
        }
        m_frames[i].motionVectors.clear();
    }
    
    if (m_intermediateTexture) {
        m_intermediateTexture->Release();
        m_intermediateTexture = nullptr;
    }
    if (m_intermediateRTV) {
        m_intermediateRTV->Release();
        m_intermediateRTV = nullptr;
    }
    if (m_intermediateSRV) {
        m_intermediateSRV->Release();
        m_intermediateSRV = nullptr;
    }
    
    m_pixelBuffer1.clear();
    m_pixelBuffer2.clear();
    m_outputBuffer.clear();
    m_motionVectors.clear();
    
    m_initialized = false;
    m_prevFrameIndex = -1;
    m_currentFrameIndex = 0;
    m_interpolationIndex = 0;
}

bool FrameInterpolator::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int width, int height) {
    if (m_initialized && m_width == width && m_height == height) {
        return true;
    }
    
    Reset();
    
    m_device = device;
    m_context = context;
    m_width = width;
    m_height = height;
    
    if (!CreateTextures(width, height)) {
        return false;
    }
    
    // Initialize motion vector grid
    m_motionGridWidth = (width + MOTION_BLOCK_SIZE - 1) / MOTION_BLOCK_SIZE;
    m_motionGridHeight = (height + MOTION_BLOCK_SIZE - 1) / MOTION_BLOCK_SIZE;
    m_motionVectors.resize(m_motionGridHeight, std::vector<MotionVector>(m_motionGridWidth));
    
    // Allocate pixel buffers
    size_t bufferSize = width * height * 4; // RGBA8
    m_pixelBuffer1.resize(bufferSize);
    m_pixelBuffer2.resize(bufferSize);
    m_outputBuffer.resize(bufferSize);
    
    m_initialized = true;
    return true;
}

bool FrameInterpolator::CreateTextures(int width, int height) {
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (m_device->CreateTexture2D(&texDesc, nullptr, &m_frames[i].texture) != S_OK) {
            return false;
        }
        m_frames[i].width = width;
        m_frames[i].height = height;
        
        if (m_device->CreateRenderTargetView(m_frames[i].texture, nullptr, &m_frames[i].rtv) != S_OK) {
            return false;
        }
        
        if (m_device->CreateShaderResourceView(m_frames[i].texture, nullptr, &m_frames[i].srv) != S_OK) {
            return false;
        }
    }
    
    // Create intermediate texture for interpolation
    if (m_device->CreateTexture2D(&texDesc, nullptr, &m_intermediateTexture) != S_OK) {
        return false;
    }
    if (m_device->CreateRenderTargetView(m_intermediateTexture, nullptr, &m_intermediateRTV) != S_OK) {
        return false;
    }
    if (m_device->CreateShaderResourceView(m_intermediateTexture, nullptr, &m_intermediateSRV) != S_OK) {
        return false;
    }
    
    return true;
}

void FrameInterpolator::ProcessFrame(ID3D11Texture2D* sourceTexture) {
    if (!m_initialized || !sourceTexture) return;
    
    auto now = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    
    // Update timing
    if (deltaTime > 0.0001f) {
        m_frameTime = deltaTime;
        m_gameFPS = 1.0f / deltaTime;
        m_displayFPS = m_gameFPS * m_config->FrameMultiplier;
    }
    
    // Store previous frame
    m_prevFrameIndex = m_currentFrameIndex;
    m_frames[m_prevFrameIndex].deltaTime = deltaTime;
    m_frames[m_prevFrameIndex].timestamp = now;
    
    // Copy source to current frame
    m_context->CopyResource(m_frames[m_currentFrameIndex].texture, sourceTexture);
    
    // Calculate motion vectors if we have a previous frame
    if (m_prevFrameIndex >= 0 && m_config->UseMotionVectors) {
        CalculateMotionVectors(&m_frames[m_prevFrameIndex], &m_frames[m_currentFrameIndex]);
    }
    
    // Update interpolation target based on config
    m_interpolationTarget = m_config->FrameMultiplier;
    m_interpolationIndex = 0;
    
    // Advance frame index
    m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES;
    m_totalFramesCount++;
}

void FrameInterpolator::CalculateMotionVectors(FrameData* prev, FrameData* curr) {
    if (!prev || !curr || !m_config->UseMotionVectors) return;
    
    EstimateMotion(prev->texture, curr->texture, m_motionVectors);
}

void FrameInterpolator::EstimateMotion(ID3D11Texture2D* prev, ID3D11Texture2D* curr, 
                                        std::vector<std::vector<MotionVector>>& motionVectors) {
    // Create staging textures for CPU access
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_width;
    stagingDesc.Height = m_height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    ID3D11Texture2D* stagingPrev = nullptr;
    ID3D11Texture2D* stagingCurr = nullptr;
    
    if (m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingPrev) != S_OK) return;
    if (m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingCurr) != S_OK) {
        stagingPrev->Release();
        return;
    }
    
    // Copy to staging textures
    m_context->CopyResource(stagingPrev, prev);
    m_context->CopyResource(stagingCurr, curr);
    
    // Map for reading
    D3D11_MAPPED_SUBRESOURCE mappedPrev, mappedCurr;
    if (m_context->Map(stagingPrev, 0, D3D11_MAP_READ, 0, &mappedPrev) != S_OK) {
        stagingPrev->Release();
        stagingCurr->Release();
        return;
    }
    if (m_context->Map(stagingCurr, 0, D3D11_MAP_READ, 0, &mappedCurr) != S_OK) {
        m_context->Unmap(stagingPrev, 0);
        stagingPrev->Release();
        stagingCurr->Release();
        return;
    }
    
    // Simple block matching motion estimation
    const int searchRadius = m_config->InterpolationQuality == 0 ? 4 : 
                              m_config->InterpolationQuality == 1 ? 8 : 16;
    
    for (int by = 0; by < m_motionGridHeight; by++) {
        for (int bx = 0; bx < m_motionGridWidth; bx++) {
            int baseX = bx * MOTION_BLOCK_SIZE;
            int baseY = by * MOTION_BLOCK_SIZE;
            
            float bestMvx = 0, bestMvy = 0;
            int64_t bestSAD = INT64_MAX;
            
            // Search for best matching block
            for (int dy = -searchRadius; dy <= searchRadius; dy += 2) {
                for (int dx = -searchRadius; dx <= searchRadius; dx += 2) {
                    int refX = baseX + dx;
                    int refY = baseY + dy;
                    
                    if (refX < 0 || refX >= m_width - MOTION_BLOCK_SIZE ||
                        refY < 0 || refY >= m_height - MOTION_BLOCK_SIZE) continue;
                    
                    int64_t sad = 0;
                    
                    // Calculate SAD (Sum of Absolute Differences)
                    for (int py = 0; py < MOTION_BLOCK_SIZE; py++) {
                        for (int px = 0; px < MOTION_BLOCK_SIZE; px++) {
                            uint8_t* prevPixel = (uint8_t*)mappedPrev.pData + 
                                                  ((refY + py) * mappedPrev.RowPitch) + 
                                                  ((refX + px) * 4);
                            uint8_t* currPixel = (uint8_t*)mappedCurr.pData + 
                                                  ((baseY + py) * mappedCurr.RowPitch) + 
                                                  ((baseX + px) * 4);
                            
                            sad += abs(prevPixel[0] - currPixel[0]) +
                                   abs(prevPixel[1] - currPixel[1]) +
                                   abs(prevPixel[2] - currPixel[2]);
                        }
                    }
                    
                    if (sad < bestSAD) {
                        bestSAD = sad;
                        bestMvx = (float)dx;
                        bestMvy = (float)dy;
                    }
                }
            }
            
            motionVectors[by][bx].x = bestMvx;
            motionVectors[by][bx].y = bestMvy;
            motionVectors[by][bx].confidence = 1.0f - (float)bestSAD / (MOTION_BLOCK_SIZE * MOTION_BLOCK_SIZE * 255 * 3);
        }
    }
    
    m_context->Unmap(stagingCurr, 0);
    m_context->Unmap(stagingPrev, 0);
    stagingPrev->Release();
    stagingCurr->Release();
}

bool FrameInterpolator::GetInterpolatedFrame(ID3D11Texture2D** outTexture) {
    if (!m_initialized || m_prevFrameIndex < 0) return false;
    
    m_interpolationIndex++;
    
    if (m_interpolationIndex >= m_interpolationTarget) {
        // Return current real frame
        *outTexture = m_frames[m_currentFrameIndex].texture;
        return true;
    }
    
    // Calculate interpolation position (0.0 to 1.0)
    float t = (float)m_interpolationIndex / (float)m_interpolationTarget;
    
    // Apply input lag compensation - shift interpolation towards current frame
    if (m_config->InputLagCompensation > 0 && m_config->PredictInput) {
        float shift = 0.1f * m_config->InputLagCompensation;
        t = std::min(1.0f, t + shift * (1.0f - t));
    }
    
    InterpolateFrame(&m_frames[m_prevFrameIndex], &m_frames[m_currentFrameIndex], t, m_intermediateTexture);
    
    *outTexture = m_intermediateTexture;
    m_interpolatedFramesCount++;
    return true;
}

void FrameInterpolator::InterpolateFrame(FrameData* prev, FrameData* curr, float t, ID3D11Texture2D* outTexture) {
    // Create staging textures
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_width;
    stagingDesc.Height = m_height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    
    ID3D11Texture2D* stagingPrev = nullptr;
    ID3D11Texture2D* stagingCurr = nullptr;
    ID3D11Texture2D* stagingOut = nullptr;
    
    m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingPrev);
    m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingCurr);
    m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingOut);
    
    if (!stagingPrev || !stagingCurr || !stagingOut) {
        if (stagingPrev) stagingPrev->Release();
        if (stagingCurr) stagingCurr->Release();
        if (stagingOut) stagingOut->Release();
        return;
    }
    
    m_context->CopyResource(stagingPrev, prev->texture);
    m_context->CopyResource(stagingCurr, curr->texture);
    
    D3D11_MAPPED_SUBRESOURCE mappedPrev, mappedCurr, mappedOut;
    if (m_context->Map(stagingPrev, 0, D3D11_MAP_READ, 0, &mappedPrev) != S_OK) {
        stagingPrev->Release(); stagingCurr->Release(); stagingOut->Release();
        return;
    }
    if (m_context->Map(stagingCurr, 0, D3D11_MAP_READ, 0, &mappedCurr) != S_OK) {
        m_context->Unmap(stagingPrev, 0);
        stagingPrev->Release(); stagingCurr->Release(); stagingOut->Release();
        return;
    }
    if (m_context->Map(stagingOut, 0, D3D11_MAP_WRITE, 0, &mappedOut) != S_OK) {
        m_context->Unmap(stagingCurr, 0);
        m_context->Unmap(stagingPrev, 0);
        stagingPrev->Release(); stagingCurr->Release(); stagingOut->Release();
        return;
    }
    
    // Interpolate each pixel
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            // Get motion vector for this pixel
            int bx = x / MOTION_BLOCK_SIZE;
            int by = y / MOTION_BLOCK_SIZE;
            bx = std::min(bx, m_motionGridWidth - 1);
            by = std::min(by, m_motionGridHeight - 1);
            
            const MotionVector& mv = m_motionVectors[by][bx];
            
            // Calculate source positions
            float srcX_prev = x + mv.x * t;
            float srcY_prev = y + mv.y * t;
            float srcX_curr = x - mv.x * (1.0f - t);
            float srcY_curr = y - mv.y * (1.0f - t);
            
            // Clamp to bounds
            srcX_prev = std::max(0.0f, std::min((float)(m_width - 1), srcX_prev));
            srcY_prev = std::max(0.0f, std::min((float)(m_height - 1), srcY_prev));
            srcX_curr = std::max(0.0f, std::min((float)(m_width - 1), srcX_curr));
            srcY_curr = std::max(0.0f, std::min((float)(m_height - 1), srcY_curr));
            
            // Bilinear sampling for previous frame
            int x0_prev = (int)srcX_prev, y0_prev = (int)srcY_prev;
            int x1_prev = std::min(x0_prev + 1, m_width - 1);
            int y1_prev = std::min(y0_prev + 1, m_height - 1);
            float fx_prev = srcX_prev - x0_prev;
            float fy_prev = srcY_prev - y0_prev;
            
            uint8_t* p00_prev = (uint8_t*)mappedPrev.pData + (y0_prev * mappedPrev.RowPitch) + (x0_prev * 4);
            uint8_t* p10_prev = (uint8_t*)mappedPrev.pData + (y0_prev * mappedPrev.RowPitch) + (x1_prev * 4);
            uint8_t* p01_prev = (uint8_t*)mappedPrev.pData + (y1_prev * mappedPrev.RowPitch) + (x0_prev * 4);
            uint8_t* p11_prev = (uint8_t*)mappedPrev.pData + (y1_prev * mappedPrev.RowPitch) + (x1_prev * 4);
            
            // Bilinear sampling for current frame
            int x0_curr = (int)srcX_curr, y0_curr = (int)srcY_curr;
            int x1_curr = std::min(x0_curr + 1, m_width - 1);
            int y1_curr = std::min(y0_curr + 1, m_height - 1);
            float fx_curr = srcX_curr - x0_curr;
            float fy_curr = srcY_curr - y0_curr;
            
            uint8_t* p00_curr = (uint8_t*)mappedCurr.pData + (y0_curr * mappedCurr.RowPitch) + (x0_curr * 4);
            uint8_t* p10_curr = (uint8_t*)mappedCurr.pData + (y0_curr * mappedCurr.RowPitch) + (x1_curr * 4);
            uint8_t* p01_curr = (uint8_t*)mappedCurr.pData + (y1_curr * mappedCurr.RowPitch) + (x0_curr * 4);
            uint8_t* p11_curr = (uint8_t*)mappedCurr.pData + (y1_curr * mappedCurr.RowPitch) + (x1_curr * 4);
            
            // Interpolate colors
            uint8_t* outPixel = (uint8_t*)mappedOut.pData + (y * mappedOut.RowPitch) + (x * 4);
            
            for (int c = 0; c < 4; c++) {
                float val_prev = (1 - fx_prev) * (1 - fy_prev) * p00_prev[c] +
                                 fx_prev * (1 - fy_prev) * p10_prev[c] +
                                 (1 - fx_prev) * fy_prev * p01_prev[c] +
                                 fx_prev * fy_prev * p11_prev[c];
                
                float val_curr = (1 - fx_curr) * (1 - fy_curr) * p00_curr[c] +
                                 fx_curr * (1 - fy_curr) * p10_curr[c] +
                                 (1 - fx_curr) * fy_curr * p01_curr[c] +
                                 fx_curr * fy_curr * p11_curr[c];
                
                // Blend based on interpolation position
                float blend = mv.confidence > 0.5f ? t : 0.5f; // Use motion vector confidence
                float val = val_prev * (1.0f - blend) + val_curr * blend;
                outPixel[c] = (uint8_t)std::max(0.0f, std::min(255.0f, val));
            }
        }
    }
    
    m_context->Unmap(stagingOut, 0);
    m_context->Unmap(stagingCurr, 0);
    m_context->Unmap(stagingPrev, 0);
    
    m_context->CopyResource(outTexture, stagingOut);
    
    stagingPrev->Release();
    stagingCurr->Release();
    stagingOut->Release();
}

void FrameInterpolator::UpdateConfig() {
    m_interpolationTarget = m_config->FrameMultiplier;
}

void FrameInterpolator::ApplyMotionVector(int x, int y, const MotionVector& mv, 
                                           int width, int height, int& outX, int& outY) {
    outX = x + (int)mv.x;
    outY = y + (int)mv.y;
    outX = std::max(0, std::min(width - 1, outX));
    outY = std::max(0, std::min(height - 1, outY));
}
