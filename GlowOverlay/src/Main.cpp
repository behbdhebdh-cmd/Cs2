#include <Windows.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "Memory.h"
#include "Offsets.h"
#include "Overlay.h"
#include "Entity.h"
#include "Menu.h"

static constexpr int kMaxPlayers = 64;

struct LoopState {
    Memory memory;
    uintptr_t clientBase{};
    PlayerInfo local{};
};

static bool ReadPlayer(const Memory& memory, uintptr_t pawn, PlayerInfo& out) {
    if (!pawn) return false;

    auto team = memory.Read<int>(pawn + offsets::m_iTeamNum);
    auto health = memory.Read<int>(pawn + offsets::m_iHealth);
    auto scene = memory.Read<uintptr_t>(pawn + offsets::m_pGameSceneNode);
    if (!team || !health || !scene) return false;

    Vec3 origin{};
    memory.ReadBytes(scene.value() + offsets::m_vOldOrigin, &origin, sizeof(origin));

    out.pawn = pawn;
    out.team = team.value();
    out.health = health.value();
    out.origin = origin;
    return out.health > 0;
}

static bool IsVisibleEnemy(const PlayerInfo& local, const PlayerInfo& other, bool highlightVisibleOnly) {
    if (other.team == local.team) return false;
    // Extend visibility logic here: read spotted flags, lastVisibleTime, or glow manager visibility bits.
    return highlightVisibleOnly ? true : true;
}

static void GatherEntities(LoopState& state, std::vector<std::pair<BoundingBox, COLORREF>>& outBoxes, const HighlightConfig& config, float width, float height) {
    outBoxes.clear();
    auto view = state.memory.Read<ViewMatrix>(state.clientBase + offsets::dwViewMatrix);
    if (!view) return;

    auto listBase = state.memory.Read<uintptr_t>(state.clientBase + offsets::dwEntityList);
    if (!listBase) return;

    for (int i = 0; i < kMaxPlayers; ++i) {
        uintptr_t entry = state.memory.Read<uintptr_t>(listBase.value() + (static_cast<uintptr_t>(i) * 0x8)).value_or(0);
        uintptr_t pawn = state.memory.Read<uintptr_t>(entry + 0x10).value_or(0);
        if (!pawn) continue;

        PlayerInfo player;
        if (!ReadPlayer(state.memory, pawn, player)) continue;
        if (player.pawn == state.local.pawn) continue;

        bool sameTeam = player.team == state.local.team;
        if (sameTeam && !config.highlightAllies) continue;

        auto box = MakeBoundingBox(player.origin, view.value(), width, height);
        if (!box) continue;

        bool visible = IsVisibleEnemy(state.local, player, config.highlightVisibleOnly);
        if (sameTeam) {
            outBoxes.emplace_back(*box, config.allyColor);
        } else if (visible) {
            outBoxes.emplace_back(*box, config.visibleColor);
        } else if (!config.highlightVisibleOnly) {
            outBoxes.emplace_back(*box, config.hiddenEnemyColor);
        }
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    LoopState state{};
    Overlay overlay;
    Menu menu;
    MenuState menuState{};
    HighlightConfig config{};

    if (!state.memory.Attach(L"cs2.exe")) {
        MessageBoxW(nullptr, L"cs2.exe not found. Launch CS2 before the overlay.", L"Glow overlay", MB_ICONERROR);
        return 1;
    }

    state.clientBase = state.memory.GetModuleBase(L"client.dll");
    if (!state.clientBase) {
        MessageBoxW(nullptr, L"client.dll not found. Ensure offsets are up to date.", L"Glow overlay", MB_ICONERROR);
        return 1;
    }

    if (!overlay.Create()) {
        MessageBoxW(nullptr, L"Failed to create overlay window.", L"Glow overlay", MB_ICONERROR);
        return 1;
    }

    menu.Initialize(overlay.GetWindow());

    MSG msg{};
    bool running = true;

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        auto localController = state.memory.Read<uintptr_t>(state.clientBase + offsets::dwLocalPlayerController).value_or(0);
        auto localPawn = state.memory.Read<uintptr_t>(state.clientBase + offsets::dwLocalPlayerPawn).value_or(0);
        if (localController) {
            auto pawnFromController = state.memory.Read<uintptr_t>(localController + offsets::m_pCSPlayerPawn).value_or(0);
            if (pawnFromController) localPawn = pawnFromController;
        }

        if (!ReadPlayer(state.memory, localPawn, state.local)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        RECT rect;
        GetClientRect(GetDesktopWindow(), &rect);
        float width = static_cast<float>(rect.right - rect.left);
        float height = static_cast<float>(rect.bottom - rect.top);

        std::vector<std::pair<BoundingBox, COLORREF>> boxes;
        GatherEntities(state, boxes, config, width, height);
        overlay.Update(boxes);

        menu.Render(menuState, config);

        if (GetAsyncKeyState(VK_END) & 1) running = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return 0;
}
