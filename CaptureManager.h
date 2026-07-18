#pragma once

#ifndef CAPTURE_MANAGER_H
#define CAPTURE_MANAGER_H

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>

// Link required DirectX libraries automatically
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    // Deleted copy constructor and assignment operator to prevent resource duplication issues
    CaptureManager(const CaptureManager&) = delete;
    CaptureManager& operator=(const CaptureManager&) = delete;

    /**
     * @brief Initializes the D3D11 Device and the DXGI Desktop Duplication API.
     * @return True if initialization succeeded, false otherwise.
     */
    bool Initialize();

    /**
     * @brief Captures the current desktop frame asynchronously to prevent stutters.
     * @param folderPath The directory path where the image will be saved (e.g., L"Dataset").
     * @param fileName The name of the file (e.g., L"Capture20260718_183000_123.bmp").
     * @return True if the frame was successfully copied and queued for saving, false otherwise.
     */
    bool CaptureAndSave(const std::wstring& folderPath, const std::wstring& fileName);

    /**
     * @brief Releases all allocated DirectX and DXGI resources.
     */
    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_deskDupl;

    bool m_isInitialized;
    UINT m_screenWidth;
    UINT m_screenHeight;

    /**
     * @brief Internal helper to set up the DXGI duplication interface.
     */
    bool InitializeDXGI();

    /**
     * @brief Internal helper to write contiguous BGRA buffer data to a lossless Windows Bitmap (BMP).
     *        This is optimized to execute in a single disk I/O operation on a background thread.
     */
    bool SavePackedBmp(const std::wstring& filePath, const BYTE* buffer, UINT width, UINT height);
};

#endif // CAPTURE_MANAGER_H