#include "CaptureManager.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <vector>

CaptureManager::CaptureManager()
    : m_isInitialized(false),
    m_screenWidth(0),
    m_screenHeight(0) {
}

CaptureManager::~CaptureManager() {
    Shutdown();
}

bool CaptureManager::Initialize() {
    if (m_isInitialized) {
        return true;
    }

    std::wcout << L"[*] Initializing DirectX 11 Device Context...\n";

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP
    };
    UINT numDriverTypes = sizeof(driverTypes) / sizeof(driverTypes[0]);

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    UINT numFeatureLevels = sizeof(featureLevels) / sizeof(featureLevels[0]);

    HRESULT hr = S_OK;
    D3D_FEATURE_LEVEL establishedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    for (UINT i = 0; i < numDriverTypes; ++i) {
        hr = D3D11CreateDevice(
            nullptr, // Primary GPU adapter (RTX 3060 under Ultimate mode)
            driverTypes[i],
            nullptr,
            0,
            featureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &establishedFeatureLevel,
            &m_d3dContext
        );

        if (SUCCEEDED(hr)) {
            break;
        }
    }

    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to create D3D11 Device. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    if (!InitializeDXGI()) {
        Shutdown();
        return false;
    }

    m_isInitialized = true;
    std::wcout << L"[+] Successfully initialized capture pipeline at "
        << m_screenWidth << L"x" << m_screenHeight << L"\n";
    return true;
}

bool CaptureManager::InitializeDXGI() {
    m_deskDupl.Reset();

    HRESULT hr = S_OK;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to query IDXGIDevice. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to get DXGI Adapter. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to enumerate Primary Output. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIOutput1> dxgiOutput1;
    hr = dxgiOutput.As(&dxgiOutput1);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to query IDXGIOutput1. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    hr = dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), &m_deskDupl);
    if (FAILED(hr)) {
        std::wcerr << L"[-] DuplicateOutput failed. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    DXGI_OUTPUT_DESC outputDesc = {};
    hr = dxgiOutput->GetDesc(&outputDesc);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to retrieve output description. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    m_screenWidth = static_cast<UINT>(outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left);
    m_screenHeight = static_cast<UINT>(outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);

    return true;
}

bool CaptureManager::CaptureAndSave(const std::wstring& folderPath, const std::wstring& fileName) {
    if (!Initialize()) {
        return false;
    }

    HRESULT hr = S_OK;
    Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};

    const int maxRetries = 15;
    for (int retry = 0; retry < maxRetries; ++retry) {
        hr = m_deskDupl->AcquireNextFrame(50, &frameInfo, &desktopResource);
        if (SUCCEEDED(hr)) {
            break;
        }

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            Sleep(5);
            continue;
        }

        if (hr == DXGI_ERROR_ACCESS_LOST) {
            std::wcout << L"[*] DXGI Access lost. Reinitializing...\n";
            if (!InitializeDXGI()) {
                return false;
            }
            continue;
        }

        break;
    }

    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to acquire next frame. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> acquiredTexture;
    hr = desktopResource.As(&acquiredTexture);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to query 2D texture. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        m_deskDupl->ReleaseFrame();
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_screenWidth;
    stagingDesc.Height = m_screenHeight;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    hr = m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to create staging texture. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        m_deskDupl->ReleaseFrame();
        return false;
    }

    m_d3dContext->CopyResource(stagingTexture.Get(), acquiredTexture.Get());

    // Release DirectX Frame immediately to unblock GPU queues
    m_deskDupl->ReleaseFrame();

    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    hr = m_d3dContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        std::wcerr << L"[-] Failed to map staging texture. HRESULT: 0x"
            << std::hex << hr << std::dec << L"\n";
        return false;
    }

    // 1. Allocate a contiguous buffer in CPU RAM (removes pitch padding)
    UINT bytesPerRow = m_screenWidth * 4;
    UINT pixelDataSize = m_screenWidth * m_screenHeight * 4;
    auto heapBuffer = std::make_shared<std::vector<BYTE>>(pixelDataSize);

    // 2. Perform a fast CPU memory copy (<0.5 milliseconds)
    BYTE* destPtr = heapBuffer->data();
    const BYTE* srcPtr = static_cast<const BYTE*>(mappedResource.pData);

    for (UINT y = 0; y < m_screenHeight; ++y) {
        std::memcpy(
            destPtr + (static_cast<size_t>(y) * bytesPerRow),
            srcPtr + (static_cast<size_t>(y) * mappedResource.RowPitch),
            bytesPerRow
        );
    }

    // 3. Instantly unmap memory context on the game execution thread
    m_d3dContext->Unmap(stagingTexture.Get(), 0);

    // 4. Detach a background thread to handle the slow disk write (5-30ms) asynchronously
    std::wstring fullPath = folderPath + L"\\" + fileName;
    std::thread([this, fullPath, heapBuffer, width = m_screenWidth, height = m_screenHeight]() {
        SavePackedBmp(fullPath, heapBuffer->data(), width, height);
        }).detach();

    return true;
}

bool CaptureManager::SavePackedBmp(const std::wstring& filePath, const BYTE* buffer, UINT width, UINT height) {
    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};

    UINT pixelDataSize = width * height * 4;

    bfh.bfType = 0x4D42; // ASCII "BM"
    bfh.bfSize = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixelDataSize);
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));

    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = static_cast<LONG>(width);
    bih.biHeight = -static_cast<LONG>(height); // Native top-down mapping
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = pixelDataSize;
    bih.biXPelsPerMeter = 0;
    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    file.write(reinterpret_cast<const char*>(&bih), sizeof(bih));
    file.write(reinterpret_cast<const char*>(buffer), pixelDataSize);

    file.close();
    return true;
}

void CaptureManager::Shutdown() {
    m_deskDupl.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();
    m_isInitialized = false;
    m_screenWidth = 0;
    m_screenHeight = 0;
}