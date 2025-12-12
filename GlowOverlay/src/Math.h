#pragma once
#include <Windows.h>
#include <cmath>
#include <optional>

struct Vec3 {
    float x{};
    float y{};
    float z{};
};

struct Vec2 {
    float x{};
    float y{};
};

struct ViewMatrix {
    float m[4][4]{};
};

inline std::optional<Vec2> WorldToScreen(const Vec3& position, const ViewMatrix& view, float width, float height) {
    float clipX = position.x * view.m[0][0] + position.y * view.m[0][1] + position.z * view.m[0][2] + view.m[0][3];
    float clipY = position.x * view.m[1][0] + position.y * view.m[1][1] + position.z * view.m[1][2] + view.m[1][3];
    float clipW = position.x * view.m[3][0] + position.y * view.m[3][1] + position.z * view.m[3][2] + view.m[3][3];

    if (clipW < 0.01f) {
        return std::nullopt;
    }

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    Vec2 screen;
    screen.x = (width * 0.5f * ndcX) + (ndcX + width * 0.5f);
    screen.y = (height * 0.5f * -ndcY) + (ndcY + height * 0.5f);
    return screen;
}

inline bool IsOnScreen(const Vec2& point, float width, float height) {
    return point.x >= 0 && point.x <= width && point.y >= 0 && point.y <= height;
}

struct BoundingBox {
    Vec2 topLeft{};
    Vec2 bottomRight{};
};

inline std::optional<BoundingBox> MakeBoundingBox(const Vec3& origin, const ViewMatrix& view, float width, float height) {
    Vec3 head = {origin.x, origin.y, origin.z + 72.0f};

    auto headScreen = WorldToScreen(head, view, width, height);
    auto feetScreen = WorldToScreen(origin, view, width, height);
    if (!headScreen || !feetScreen) {
        return std::nullopt;
    }

    float boxHeight = std::abs(feetScreen->y - headScreen->y);
    float boxWidth = boxHeight * 0.4f;

    BoundingBox box{};
    box.topLeft = {headScreen->x - boxWidth * 0.5f, headScreen->y};
    box.bottomRight = {headScreen->x + boxWidth * 0.5f, feetScreen->y};

    if (!IsOnScreen(box.topLeft, width, height) && !IsOnScreen(box.bottomRight, width, height)) {
        return std::nullopt;
    }

    return box;
}
