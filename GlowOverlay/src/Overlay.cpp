#include "Overlay.h"
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")

Overlay::Overlay() = default;
Overlay::~Overlay() {
    if (memoryBitmap_) DeleteObject(memoryBitmap_);
    if (memoryDc_) DeleteDC(memoryDc_);
    if (window_) DestroyWindow(window_);
}

bool Overlay::Create() {
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"CS2GlowOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT desktop;
    GetWindowRect(GetDesktopWindow(), &desktop);
    windowSize_.cx = desktop.right;
    windowSize_.cy = desktop.bottom;

    window_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"Glow overlay",
        WS_POPUP,
        0,
        0,
        windowSize_.cx,
        windowSize_.cy,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!window_) {
        return false;
    }

    SetLayeredWindowAttributes(window_, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margins{ -1 };
    DwmExtendFrameIntoClientArea(window_, &margins);
    ShowWindow(window_, SW_SHOW);

    InitializeSurface();
    return true;
}

void Overlay::InitializeSurface() {
    HDC screenDc = GetDC(nullptr);
    memoryDc_ = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = windowSize_.cx;
    bmi.bmiHeader.biHeight = -windowSize_.cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits{};
    memoryBitmap_ = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(memoryDc_, memoryBitmap_);
    ReleaseDC(nullptr, screenDc);
}

void Overlay::Update(const std::vector<std::pair<BoundingBox, COLORREF>>& boxes) {
    if (!window_) return;
    Present(boxes);
}

void Overlay::Present(const std::vector<std::pair<BoundingBox, COLORREF>>& boxes) {
    RECT rect{ 0,0, windowSize_.cx, windowSize_.cy };
    HBRUSH clearBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memoryDc_, &rect, clearBrush);
    DeleteObject(clearBrush);

    SetBkMode(memoryDc_, TRANSPARENT);

    for (const auto& [box, color] : boxes) {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        HGDIOBJ oldPen = SelectObject(memoryDc_, pen);
        Rectangle(memoryDc_, static_cast<int>(box.topLeft.x), static_cast<int>(box.topLeft.y), static_cast<int>(box.bottomRight.x), static_cast<int>(box.bottomRight.y));
        SelectObject(memoryDc_, oldPen);
        DeleteObject(pen);
    }

    HDC windowDc = GetDC(window_);
    BitBlt(windowDc, 0, 0, windowSize_.cx, windowSize_.cy, memoryDc_, 0, 0, SRCCOPY);
    ReleaseDC(window_, windowDc);
}

LRESULT CALLBACK Overlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
