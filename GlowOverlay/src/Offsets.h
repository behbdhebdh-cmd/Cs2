#pragma once

namespace offsets {
    // Static offsets for cs2.exe current build (decimal values supplied by user).
    constexpr long dwEntityList = 30489832;
    constexpr long dwLocalPlayerPawn = 29290280;
    constexpr long dwViewMatrix = 31663056;
    constexpr long dwGlowManager = 31646392;
    constexpr long dwLocalPlayerController = 31579160;

    // Common netvar-style offsets (example values, validate for the specific build).
    constexpr long m_iTeamNum = 0x3EB;
    constexpr long m_iHealth = 0x34C;
    constexpr long m_pGameSceneNode = 0x330;
    constexpr long m_vOldOrigin = 0xC8; // inside game scene node
    constexpr long m_pCSPlayerPawn = 0x5E0; // from controller -> pawn
    constexpr long m_pClippingWeapon = 0x1300; // placeholder for weapon visibility tweaks
    constexpr long m_flDetectedByEnemySensorTime = 0x1410; // example visibility helper
} // namespace offsets
