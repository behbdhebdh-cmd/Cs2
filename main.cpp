#include <windows.h>

// Linker directive to build as a Windows (GUI) subsystem application without a console.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")

namespace {
constexpr UINT_PTR kLoadTimerId = 1;
constexpr UINT kLoadTimerDelayMs = 5000; // 5 seconds

HWND g_statusLabel = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Create a static control to display the loading message.
        g_statusLabel = CreateWindowExW(
            0,
            L"STATIC",
            L"Loading resolution presets...",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            20, 70, 360, 20,
            hwnd,
            nullptr,
            reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance,
            nullptr);

        // Start a timer that will update the text after 5 seconds.
        SetTimer(hwnd, kLoadTimerId, kLoadTimerDelayMs, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == kLoadTimerId && g_statusLabel) {
            SetWindowTextW(g_statusLabel, L"Presets loaded successfully!");
            KillTimer(hwnd, kLoadTimerId);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kLoadTimerId);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}
} // namespace

// Entry point for a GUI-only Win32 application (no console window).
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Ensure the process is DPI aware for crisp rendering on high DPI displays.
    SetProcessDPIAware();

    // Define and register a window class for the main application window.
    const wchar_t CLASS_NAME[] = L"CS2RPLoaderWindowClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"Failed to register window class.", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    // Create the main application window.
    HWND hwnd = CreateWindowExW(
        0,                      // Optional window styles.
        CLASS_NAME,             // Window class name.
        L"CS2 RP Loader v1",    // Window title.
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // Basic window with fixed size.
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200, // Position and size.
        nullptr,                // Parent window.
        nullptr,                // Menu.
        hInstance,              // Instance handle.
        nullptr                 // Additional application data.
    );

    if (!hwnd) {
        MessageBoxW(nullptr, L"Failed to create window.", L"Error", MB_ICONERROR | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Standard message loop for a GUI application.
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
