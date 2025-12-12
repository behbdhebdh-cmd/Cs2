# CS2 visible-only glow overlay (educational)

This repository contains a small Visual Studio 2022 solution that demonstrates how to read Counter-Strike 2 memory externally and draw a legitimate-style visible glow overlay for practice against offline bots. The project focuses on safe, read-only techniques for studying view matrices, entity iteration, and simple overlay rendering.

## Project layout

```
GlowOverlay/
├── GlowOverlay.sln              // Solution for VS2022 x64
├── GlowOverlay.vcxproj          // Win32 app target
└── src/
    ├── Entity.h                 // Entity info and highlight configuration
    ├── Main.cpp                 // Program entry point and main loop
    ├── Math.h                   // View/projection helpers
    ├── Memory.cpp/.h            // OpenProcess + ReadProcessMemory helpers
    ├── Menu.cpp/.h              // ImGui (optional) menu or hotkey fallback
    ├── Offsets.h                // Offsets provided for the current build
    └── Overlay.cpp/.h           // Layered, click-through GDI overlay
```

## Features

- Opens the `cs2.exe` process in read-only mode and resolves the `client.dll` base address via the Toolhelp API.
- Reads the local controller/pawn, view matrix, and entity list using provided offsets.
- Calculates bounding boxes with a basic world-to-screen projection and draws transparent GDI rectangles for visible/detected enemies.
- Includes an optional Dear ImGui menu (guarded by `IMGUI_AVAILABLE`) for toggling "visible only" mode and editing colors. Without ImGui present, F1/F2 hotkeys toggle the primary options.
- Configured for x64 Debug/Release in Visual Studio 2022.

## Building

1. Open `GlowOverlay/GlowOverlay.sln` in Visual Studio 2022 (x64).
2. Ensure the Windows 10 SDK is installed.
3. (Optional) To enable Dear ImGui:
   - Add the official `imgui` sources under `GlowOverlay/third_party/imgui`.
   - Include the Win32/DX11 backends, add them to the project, and define `IMGUI_AVAILABLE` in the project preprocessor definitions.
   - Link against `d3d11.lib` (already listed via `#pragma comment`).
4. Build and run **inside a VM** with CS2 launched in an offline practice session with bots.

## Usage & safety

- The overlay only reads memory; it does **not** write to game memory.
- Offsets in `Offsets.h` are decimal and should be updated for new builds.
- Press **END** to close the overlay. ImGui menu can be toggled with **Insert** when enabled.
- Keep testing to offline/bot matches for educational rendering experiments only.
