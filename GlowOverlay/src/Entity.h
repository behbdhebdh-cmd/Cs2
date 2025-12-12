#pragma once
#include <cstdint>
#include <Windows.h>
#include "Math.h"

struct PlayerInfo {
    uintptr_t pawn{};
    int team{};
    int health{};
    Vec3 origin{};
};

struct HighlightConfig {
    bool highlightVisibleOnly{true};
    bool highlightAllies{false};
    COLORREF visibleColor{RGB(255, 200, 0)};
    COLORREF allyColor{RGB(0, 200, 255)};
    COLORREF hiddenEnemyColor{RGB(255, 0, 0)};
    float visibleColorPicker[3]{1.0f, 0.78f, 0.0f};
    float allyColorPicker[3]{0.0f, 0.78f, 1.0f};
    float hiddenColorPicker[3]{1.0f, 0.0f, 0.0f};

    void SyncFromPickers() {
        visibleColor = RGB(static_cast<BYTE>(visibleColorPicker[0] * 255.0f),
                           static_cast<BYTE>(visibleColorPicker[1] * 255.0f),
                           static_cast<BYTE>(visibleColorPicker[2] * 255.0f));
        allyColor = RGB(static_cast<BYTE>(allyColorPicker[0] * 255.0f),
                        static_cast<BYTE>(allyColorPicker[1] * 255.0f),
                        static_cast<BYTE>(allyColorPicker[2] * 255.0f));
        hiddenEnemyColor = RGB(static_cast<BYTE>(hiddenColorPicker[0] * 255.0f),
                               static_cast<BYTE>(hiddenColorPicker[1] * 255.0f),
                               static_cast<BYTE>(hiddenColorPicker[2] * 255.0f));
    }
};
