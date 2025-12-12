#pragma once
#include <Windows.h>
#include "Entity.h"

struct MenuState {
    bool showMenu{true};
    bool showDebug{false};
};

class Menu {
public:
    bool Initialize(HWND hwnd);
    void Render(MenuState& state, HighlightConfig& config);
};
