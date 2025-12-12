#pragma once
#include <Windows.h>
#include <vector>
#include "Math.h"
#include "Entity.h"

class Overlay {
public:
    Overlay();
    ~Overlay();

    bool Create();
    void Update(const std::vector<std::pair<BoundingBox, COLORREF>>& boxes);
    HWND GetWindow() const { return window_; }
    void SetHighlightConfig(const HighlightConfig& cfg) { config_ = cfg; }
    const HighlightConfig& GetHighlightConfig() const { return config_; }

private:
    HWND window_{nullptr};
    HDC memoryDc_{nullptr};
    HBITMAP memoryBitmap_{nullptr};
    SIZE windowSize_{1280, 720};
    HighlightConfig config_{};

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void InitializeSurface();
    void Present(const std::vector<std::pair<BoundingBox, COLORREF>>& boxes);
};
