#include <windows.h>
#include <wininet.h>

#include <array>
#include <cstdlib>
#include <string>

#pragma comment(lib, "wininet.lib")

// Linker directive to build as a Windows (GUI) subsystem application without a console.
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup")

namespace {
constexpr UINT_PTR kLoadTimerId = 1;
constexpr UINT kLoadTimerDelayMs = 5000; // 5 seconds

HWND g_statusLabel = nullptr;
std::wstring g_publicIp;
std::wstring g_username;
std::wstring g_computerName;

std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int required = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return L"";
    }

    std::wstring wide(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &wide[0], required);

    // Remove the trailing null terminator added by MultiByteToWideChar.
    if (!wide.empty() && wide.back() == L'\0') {
        wide.pop_back();
    }
    return wide;
}

std::wstring FetchUsername() {
    const char* name = std::getenv("USERNAME");
    return name ? Widen(name) : L"";
}

std::wstring FetchComputerName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    if (GetComputerNameW(buffer, &size)) {
        return std::wstring(buffer, size);
    }
    return L"";
}

std::wstring FetchPublicIp() {
    HINTERNET hInternet = InternetOpenW(L"CS2RPLoader/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        return L"";
    }

    HINTERNET hRequest = InternetOpenUrlW(
        hInternet,
        L"https://api.ipify.org",
        nullptr,
        0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI | INTERNET_FLAG_SECURE,
        0);

    if (!hRequest) {
        InternetCloseHandle(hInternet);
        return L"";
    }

    std::string response;
    char buffer[128];
    DWORD bytesRead = 0;

    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hInternet);

    return Widen(response);
}

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
            g_username = FetchUsername();
            g_computerName = FetchComputerName();
            g_publicIp = FetchPublicIp();

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
