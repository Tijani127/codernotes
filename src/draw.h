#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/String.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "rect.h"

namespace draw {

sf::String utf8(std::string_view text);

// All text goes through a glyph batch: every glyph is written into one vertex
// array per font page and flushed with a single draw call, which keeps the
// immediate mode UI fast enough to hold a steady frame rate.
void beginText(sf::RenderTarget& target);
void endText();
// Restricts following glyphs to a rectangle given in the current view's
// coordinates. Without it long lines would spill into the neighbouring pane.
void beginClip(sf::RenderTarget& target, const Rect& clip);
void endClip();
float textRun(const sf::Font& font, std::string_view value, unsigned int size,
              const sf::Color& color, const sf::Vector2f& position, bool bold);

void rect(sf::RenderTarget& target, const Rect& bounds, const sf::Color& color);
void vGradient(sf::RenderTarget& target, const Rect& bounds, const sf::Color& top,
               const sf::Color& bottom);
void roundedRect(sf::RenderTarget& target, const Rect& bounds, float radius,
                 const sf::Color& color);
void roundedRectV(sf::RenderTarget& target, const Rect& bounds, float radius,
                  const sf::Color& top, const sf::Color& bottom);
void roundedOutline(sf::RenderTarget& target, const Rect& bounds, float radius,
                    const sf::Color& color, float thickness = 1.f);
void circle(sf::RenderTarget& target, const sf::Vector2f& center, float radius,
            const sf::Color& color);
void line(sf::RenderTarget& target, const sf::Vector2f& from, const sf::Vector2f& to,
          float thickness, const sf::Color& color);
void polygon(sf::RenderTarget& target, const std::vector<sf::Vector2f>& points,
             const sf::Color& color);
void shadowedPanel(sf::RenderTarget& target, const Rect& bounds, float radius,
                   const sf::Color& fill, const sf::Color& edge);
void vline(sf::RenderTarget& target, float x, float y0, float y1, const sf::Color& color);
void hline(sf::RenderTarget& target, float y, float x0, float x1, const sf::Color& color);

// Small glyph icons: "play", "stop", "plus", "close", "search", "chevron", "note".
void icon(sf::RenderTarget& target, const char* name, const sf::Vector2f& center, float size,
          const sf::Color& color);

// Slim scrollbar, drawn only when the content does not fit.
void scrollbar(sf::RenderTarget& target, const Rect& track, float offset, float content,
               float view, bool hovered, bool active);

float textWidth(const sf::Font& font, std::string_view text, unsigned int size, bool bold = false);
void text(sf::RenderTarget& target, const sf::Font& font, std::string_view value, unsigned int size,
          const sf::Color& color, const sf::Vector2f& position, bool bold = false);
float textEllipsized(sf::RenderTarget& target, const sf::Font& font, std::string_view value,
                     unsigned int size, const sf::Color& color, const Rect& bounds,
                     bool bold = false);
std::string ellipsize(const sf::Font& font, std::string_view value, unsigned int size,
                      float maxWidth);

}  // namespace draw

struct MouseState {
    sf::Vector2f position;
    sf::Vector2f pressPosition;
    bool leftDown = false;
    bool leftPressed = false;
    bool leftReleased = false;
    bool rightPressed = false;
    bool doubleClicked = false;

    void beginFrame() {
        leftPressed = false;
        leftReleased = false;
        rightPressed = false;
        doubleClicked = false;
    }
};

struct Button {
    Rect bounds;
    std::string label;
    std::string iconName;  // optional leading glyph, see draw::icon
    bool enabled = true;
    bool hovered = false;
    bool held = false;
    bool clickedFlag = false;
    bool activeState = false;
    bool accent = false;
    unsigned int fontSize = 0;

    void update(const MouseState& mouse, float dt = 0.f) {
        (void)dt;
        clickedFlag = false;
        hovered = enabled && bounds.contains(mouse.position);
        if (!enabled) {
            held = false;
            return;
        }
        if (mouse.leftPressed && hovered) held = true;
        if (mouse.leftReleased) {
            if (held && hovered) clickedFlag = true;
            held = false;
        }
        if (!mouse.leftDown) held = false;
    }

    void draw(sf::RenderTarget& target, const sf::Font& font, float radius,
              const sf::Color& normalFill, const sf::Color& hoverFill, const sf::Color& activeFill,
              const sf::Color& normalText, const sf::Color& activeText) const;
};

