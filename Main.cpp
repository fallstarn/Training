#include "CaptureManager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

/**
 * @brief Utility function to generate a PascalCase timestamped filename with millisecond resolution.
 *        Made static to limit scope to Main.cpp and resolve VCR003 warnings.
 */
static std::wstring GeneratePascalCaseFileName() {
    auto now = std::chrono::system_clock::now();
    auto timeType = std::chrono::system_clock::to_time_t(now);

    std::tm localTime = {};
    if (localtime_s(&localTime, &timeType) != 0) {
        return L"CaptureGeneric.bmp";
    }

    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

    std::wstringstream wss;
    wss << L"Capture"
        << std::setfill(L'0')
        << std::setw(4) << (localTime.tm_year + 1900)
        << std::setw(2) << (localTime.tm_mon + 1)
        << std::setw(2) << localTime.tm_mday
        << L"_"
        << std::setw(2) << localTime.tm_hour
        << std::setw(2) << localTime.tm_min
        << std::setw(2) << localTime.tm_sec
        << L"_"
        << std::setw(3) << milliseconds
        << L".bmp";

    return wss.str();
}

int main() {
    std::wcout << L"==================================================\n";
    std::wcout << L"       D3D11/DXGI Desktop Capture Engine          \n";
    std::wcout << L"==================================================\n\n";

    fs::path datasetPath = fs::current_path() / "Dataset";
    if (!fs::exists(datasetPath)) {
        std::wcout << L"[*] Target directory 'Dataset' not found. Creating...\n";
        try {
            if (fs::create_directories(datasetPath)) {
                std::wcout << L"[+] Created directory: " << datasetPath.wstring() << L"\n";
            }
            else {
                std::wcerr << L"[-] Failed to create directory: " << datasetPath.wstring() << L"\n";
                return 1;
            }
        }
        catch (const fs::filesystem_error& ex) {
            std::wcerr << L"[-] Filesystem Exception: " << ex.what() << L"\n";
            return 1;
        }
    }
    else {
        std::wcout << L"[+] Saving frames to existing directory: " << datasetPath.wstring() << L"\n";
    }

    CaptureManager captureManager;
    if (!captureManager.Initialize()) {
        std::wcerr << L"[-] Critical error: Failed to initialize Capture Pipeline. Exiting.\n";
        return 1;
    }

    std::wcout << L"\n[+] Capture Engine ready!\n";
    std::wcout << L"--> Press [G] to capture lossless screenshot.\n";
    std::wcout << L"--> Press [ESC] to exit gracefully.\n\n";

    bool gKeyWasPressed = false;
    UINT captureCounter = 0;

    while (true) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            std::wcout << L"[*] Escape detected. Shutting down system...\n";
            break;
        }

        bool gIsPressedNow = (GetAsyncKeyState('G') & 0x8000) != 0;

        if (gIsPressedNow && !gKeyWasPressed) {
            std::wstring fileName = GeneratePascalCaseFileName();

            std::wcout << L"[*] Capturing frame... ";

            if (captureManager.CaptureAndSave(datasetPath.wstring(), fileName)) {
                captureCounter++;
                std::wcout << L"[+] Success! [Saved: " << fileName
                    << L" | Total Captured: " << captureCounter << L"]\n";

                // Audio feedback: Play a quick, sharp beep (1500 Hz for 50 milliseconds)
                // This lets you know the snapshot occurred without interfering with gameplay audio.
                Beep(1500, 50);
            }
            else {
                std::wcerr << L"[-] Capture failed.\n";
                // Low error beep indicating failure
                Beep(400, 150);
            }
        }

        gKeyWasPressed = gIsPressedNow;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    captureManager.Shutdown();
    std::wcout << L"[+] Safe shutdown complete. Process finalized.\n";

    return 0;
}