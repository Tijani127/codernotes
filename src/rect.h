#pragma once

#include <SFML/System/Vector2.hpp>

// SFML 3 removed the left/top/width/height accessors from sf::Rect, so the app
// keeps its own tiny rectangle type for layout, hit testing and drawing.
struct Rect {
    float left = 0.f;
    float top = 0.f;
    float width = 0.f;
    float height = 0.f;

    constexpr Rect() = default;
    constexpr Rect(float l, float t, float w, float h) : left(l), top(t), width(w), height(h) {}

    constexpr float right() const { return left + width; }
    constexpr float bottom() const { return top + height; }
    constexpr sf::Vector2f center() const { return {left + width * 0.5f, top + height * 0.5f}; }

    constexpr bool contains(const sf::Vector2f& point) const {
        return point.x >= left && point.x < left + width && point.y >= top && point.y < top + height;
    }

    constexpr bool operator==(const Rect& other) const {
        return left == other.left && top == other.top && width == other.width && height == other.height;
    }
    constexpr bool operator!=(const Rect& other) const { return !(*this == other); }
};
