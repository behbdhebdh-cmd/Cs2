#include "Menu.h"

#ifdef IMGUI_AVAILABLE
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

static ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static IDXGISwapChain* g_SwapChain = nullptr;
static ID3D11RenderTargetView* g_Rtv = nullptr;

static bool CreateDeviceAndSwapChain(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = hwnd;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        levels,
        _countof(levels),
        D3D11_SDK_VERSION,
        &desc,
        &g_SwapChain,
        &g_Device,
        &featureLevel,
        &g_Context);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_Device->CreateRenderTargetView(backBuffer, nullptr, &g_Rtv);
    backBuffer->Release();
    return true;
}

bool Menu::Initialize(HWND hwnd) {
    if (!CreateDeviceAndSwapChain(hwnd)) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_Device, g_Context);
    return true;
}

void Menu::Render(MenuState& state, HighlightConfig& config) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (state.showMenu) {
        ImGui::Begin("Visible-only glow", &state.showMenu);
        ImGui::TextWrapped("Educational overlay to mimic legitimate visibility glow. Only draws when enemies are visible/detected.");
        ImGui::Checkbox("Highlight only visible enemies", &config.highlightVisibleOnly);
        ImGui::Checkbox("Highlight allies for debugging", &config.highlightAllies);
        if (ImGui::ColorEdit3("Visible enemy color", config.visibleColorPicker)) {
            config.SyncFromPickers();
        }
        if (ImGui::ColorEdit3("Hidden enemy color", config.hiddenColorPicker)) {
            config.SyncFromPickers();
        }
        if (ImGui::ColorEdit3("Ally color", config.allyColorPicker)) {
            config.SyncFromPickers();
        }
        ImGui::End();
    }

    if (state.showDebug) {
        ImGui::Begin("Debug", &state.showDebug);
        ImGui::Text("Toggle menu with Insert. Close with End.");
        ImGui::End();
    }

    ImGui::EndFrame();
    ImGui::Render();
    const float clearColor[4]{ 0,0,0,0 };
    g_Context->OMSetRenderTargets(1, &g_Rtv, nullptr);
    g_Context->ClearRenderTargetView(g_Rtv, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_SwapChain->Present(1, 0);
}

#else

bool Menu::Initialize(HWND) { return true; }

void Menu::Render(MenuState& state, HighlightConfig& config) {
    if (GetAsyncKeyState(VK_INSERT) & 1) {
        state.showMenu = !state.showMenu;
    }
    if (!state.showMenu) return;

    if (GetAsyncKeyState(VK_F1) & 1) config.highlightVisibleOnly = !config.highlightVisibleOnly;
    if (GetAsyncKeyState(VK_F2) & 1) config.highlightAllies = !config.highlightAllies;
}

#endif
