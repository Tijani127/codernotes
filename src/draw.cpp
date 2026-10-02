#include "draw.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

#include "theme.h"
#include "util.h"

namespace {

// sf::Vertex is an aggregate in SFML 3, so build the vertices field by field.
inline sf::Vertex makeVertex(const sf::Vector2f& position, const sf::Color& color) {
    sf::Vertex vertex{};
    vertex.position = position;
    vertex.color = color;
    return vertex;
}

inline sf::Vertex makeVertex(const sf::Vector2f& position, const sf::Color& color,
                              const sf::Vector2f& texCoords) {
    sf::Vertex vertex{};
    vertex.position = position;
    vertex.color = color;
    vertex.texCoords = texCoords;
    return vertex;
}

// Writes a quad (two triangles) between two points with a given thickness.
void stripQuad(sf::VertexArray& into, std::size_t index, const sf::Vector2f& from,
               const sf::Vector2f& to, float thickness, const sf::Color& color) {
    sf::Vector2f direction = to - from;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 0.001f) return;
    direction /= length;
    const sf::Vector2f normal{-direction.y * thickness * 0.5f, direction.x * thickness * 0.5f};
    sf::Vertex* quad = &into[index];
    quad[0] = makeVertex(from - normal, color);
    quad[1] = makeVertex(from + normal, color);
    quad[2] = makeVertex(to - normal, color);
    quad[3] = makeVertex(to + normal, color);
}

}  // namespace

namespace draw {
namespace {

sf::VertexArray arcFan(const Rect& bounds, float radius, const sf::Color& color, bool outlineOnly) {
    const float r = std::min({radius, bounds.width * 0.5f, bounds.height * 0.5f});
    const sf::Vector2f corners[4] = {
        {bounds.left + r, bounds.top + r},
        {bounds.left + bounds.width - r, bounds.top + r},
        {bounds.left + bounds.width - r, bounds.top + bounds.height - r},
        {bounds.left + r, bounds.top + bounds.height - r},
    };
    const int segments = 5;
    std::vector<sf::Vector2f> ring;
    for (int corner = 0; corner < 4; ++corner) {
        const float baseAngle =
            corner == 0 ? 180.f : corner == 1 ? 270.f : corner == 2 ? 0.f : 90.f;
        for (int step = 0; step <= segments; ++step) {
            const float angle = (baseAngle + 90.f * step / segments) * 3.14159265f / 180.f;
            ring.push_back({corners[corner].x + std::cos(angle) * r,
                            corners[corner].y + std::sin(angle) * r});
        }
    }

    std::vector<sf::Vector2f> points;
    if (outlineOnly) {
        points = ring;
    } else {
        const sf::Vector2f center(bounds.left + bounds.width * 0.5f,
                                  bounds.top + bounds.height * 0.5f);
        points.push_back(center);
        for (const sf::Vector2f& point : ring) points.push_back(point);
        // A triangle fan stops at the last vertex, so repeat the first one to
        // close the polygon.
        points.push_back(ring.front());
    }

    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        fan[i] = makeVertex(points[i], color);
    }
    return fan;
}

}  // namespace

sf::String utf8(std::string_view text) {
    return sf::String::fromUtf8(text.begin(), text.end());
}

void rect(sf::RenderTarget& target, const Rect& bounds, const sf::Color& color) {
    const float w = std::max(bounds.width, 1.f);
    const float h = std::max(bounds.height, 1.f);
    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0] = makeVertex(sf::Vector2f(bounds.left, bounds.top), color);
    quad[1] = makeVertex(sf::Vector2f(bounds.left + w, bounds.top), color);
    quad[2] = makeVertex(sf::Vector2f(bounds.left, bounds.top + h), color);
    quad[3] = makeVertex(sf::Vector2f(bounds.left + w, bounds.top + h), color);
    target.draw(quad);
}

void vGradient(sf::RenderTarget& target, const Rect& bounds, const sf::Color& top,
               const sf::Color& bottom) {
    const auto mix = [](const sf::Color& a, const sf::Color& b, float t) {
        return sf::Color(static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
                         static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
                         static_cast<std::uint8_t>(a.b + (b.b - a.b) * t), a.a);
    };
    const int steps = 8;
    const float h = std::max(bounds.height, 1.f);
    for (int i = 0; i < steps; ++i) {
        const float t0 = static_cast<float>(i) / steps;
        const float t1 = static_cast<float>(i + 1) / steps;
        sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
        const sf::Color c0 = mix(top, bottom, t0);
        const sf::Color c1 = mix(top, bottom, t1);
        quad[0] = makeVertex(sf::Vector2f(bounds.left, bounds.top + h * t0), c0);
        quad[1] = makeVertex(sf::Vector2f(bounds.left + bounds.width, bounds.top + h * t0), c0);
        quad[2] = makeVertex(sf::Vector2f(bounds.left, bounds.top + h * t1), c1);
        quad[3] = makeVertex(sf::Vector2f(bounds.left + bounds.width, bounds.top + h * t1), c1);
        target.draw(quad);
    }
}

void roundedRect(sf::RenderTarget& target, const Rect& bounds, float radius,
                 const sf::Color& color) {
    if (radius <= 0.5f) {
        rect(target, bounds, color);
        return;
    }
    target.draw(arcFan(bounds, radius, color, false));
}

void roundedOutline(sf::RenderTarget& target, const Rect& bounds, float radius,
                    const sf::Color& color, float thickness) {
    const float r = std::min({radius, bounds.width * 0.5f, bounds.height * 0.5f});
    sf::VertexArray fan = arcFan(bounds, r, color, true);
    for (std::size_t i = 0; i + 1 < fan.getVertexCount(); ++i) {
        sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
        quad[0] = fan[i];
        quad[1] = fan[i + 1];
        const sf::Vector2f a = fan[i].position;
        const sf::Vector2f b = fan[i + 1].position;
        sf::Vector2f normal = {b.y - a.y, a.x - b.x};
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y);
        if (length > 0.001f) {
            normal.x = normal.x / length * thickness * 0.5f;
            normal.y = normal.y / length * thickness * 0.5f;
        }
        quad[2] = makeVertex(a - normal, color);
        quad[3] = makeVertex(b - normal, color);
        target.draw(quad);
    }
}

void shadowedPanel(sf::RenderTarget& target, const Rect& bounds, float radius,
                   const sf::Color& fill, const sf::Color& edge) {
    // A soft drop shadow under the panel gives the UI some depth.
    for (int i = 3; i >= 1; --i) {
        const sf::Color shade(0, 0, 0, static_cast<std::uint8_t>(14 / i));
        draw::roundedRect(target,
                          {bounds.left - i, bounds.top + i, bounds.width + 2.f * i, bounds.height},
                          radius + i, shade);
    }
    roundedRect(target, bounds, radius, fill);
    roundedOutline(target, bounds, radius, edge);
}

void circle(sf::RenderTarget& target, const sf::Vector2f& center, float radius,
            const sf::Color& color) {
    if (radius <= 0.f) return;
    constexpr int kSegments = 20;
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, kSegments + 2);
    fan[0] = makeVertex(center, color);
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / kSegments;
        fan[static_cast<std::size_t>(i) + 1] =
            makeVertex({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius},
                       color);
    }
    target.draw(fan);
}

void line(sf::RenderTarget& target, const sf::Vector2f& from, const sf::Vector2f& to,
          float thickness, const sf::Color& color) {
    sf::Vector2f direction = to - from;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 0.001f || thickness <= 0.f) return;
    direction /= length;
    const sf::Vector2f normal{-direction.y * thickness * 0.5f, direction.x * thickness * 0.5f};
    // Round caps keep joins between segments clean.
    circle(target, from, thickness * 0.5f, color);
    circle(target, to, thickness * 0.5f, color);
    polygon(target, {from + normal, from - normal, to - normal, to + normal}, color);
}

void polygon(sf::RenderTarget& target, const std::vector<sf::Vector2f>& points,
             const sf::Color& color) {
    if (points.size() < 3) return;
    sf::VertexArray fan(sf::PrimitiveType::TriangleFan, points.size() + 1);
    for (std::size_t i = 0; i < points.size(); ++i) fan[i] = makeVertex(points[i], color);
    fan[points.size()] = makeVertex(points.front(), color);
    target.draw(fan);
}

void vline(sf::RenderTarget& target, float x, float y0, float y1, const sf::Color& color) {
    rect(target, {x, y0, 1.f, std::max(y1 - y0, 1.f)}, color);
}

void hline(sf::RenderTarget& target, float y, float x0, float x1, const sf::Color& color) {
    rect(target, {x0, y, std::max(x1 - x0, 1.f), 1.f}, color);
}

namespace {

// One batch of glyph quads per font page. SFML keeps every glyph of a size on a
// single texture page (regular and bold included), so text of the same size can
// be submitted with a single draw call no matter how many colours it uses.
struct GlyphBatch {
    static constexpr std::size_t kVerticesPerGlyph = 6;

    sf::RenderTarget* target = nullptr;
    const sf::Font* font = nullptr;
    unsigned int size = 0;
    std::size_t used = 0;
    Rect clip;
    bool hasClip = false;
    sf::VertexArray vertices{sf::PrimitiveType::Triangles};

    void ensureRoom(std::size_t glyphs) {
        const std::size_t needed = glyphs * kVerticesPerGlyph;
        if (vertices.getVertexCount() < needed) {
            vertices.resize(needed * 2);
        }
    }

    void flush() {
        if (target != nullptr && font != nullptr && used > 0) {
            sf::VertexArray drawable(sf::PrimitiveType::Triangles, used);
            for (std::size_t i = 0; i < used; ++i) drawable[i] = vertices[i];
            // The default (normalised) coordinate type is used on purpose: it is
            // the same projection the shapes are drawn with, so batched glyphs
            // and geometry always line up, including inside scrolled panes.
            target->draw(drawable, sf::RenderStates(&font->getTexture(size)));
        }
        used = 0;
        font = nullptr;
    }
};

GlyphBatch& batch() {
    static GlyphBatch instance;
    return instance;
}

// Decodes one UTF-8 code point in place, no allocation.
std::size_t decodeAt(std::string_view text, std::size_t index, char32_t& codePoint) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    char32_t value = 0xFFFD;
    std::size_t length = 1;
    if (lead < 0x80) {
        value = lead;
    } else if ((lead & 0xE0) == 0xC0 && index + 1 < text.size()) {
        value = lead & 0x1Fu;
        length = 2;
    } else if ((lead & 0xF0) == 0xE0 && index + 2 < text.size()) {
        value = lead & 0x0Fu;
        length = 3;
    } else if ((lead & 0xF8) == 0xF0 && index + 3 < text.size()) {
        value = lead & 0x07u;
        length = 4;
    }
    if (index + length > text.size()) {
        value = 0xFFFD;
        length = 1;
    } else {
        for (std::size_t k = 1; k < length; ++k) {
            const unsigned char next = static_cast<unsigned char>(text[index + k]);
            if ((next & 0xC0) != 0x80) {
                value = 0xFFFD;
                length = 1;
                break;
            }
            value = (value << 6) | (next & 0x3Fu);
        }
    }
    codePoint = value;
    return length;
}

}  // namespace

void beginText(sf::RenderTarget& target) {
    batch().flush();
    batch().target = &target;
}

void endText() {
    batch().flush();
    batch().target = nullptr;
}

void beginClip(sf::RenderTarget& target, const Rect& clip) {
    batch().flush();
    batch().target = &target;
    batch().clip = clip;
}

void endClip() {
    batch().flush();
    batch().clip = Rect();
    batch().hasClip = false;
}

float textRun(const sf::Font& font, std::string_view value, unsigned int size,
              const sf::Color& color, const sf::Vector2f& position, bool bold) {
    if (value.empty()) return 0.f;
    GlyphBatch& state = batch();
    if (state.font != &font || state.size != size) {
        state.flush();
        state.font = &font;
        state.size = size;
    }
    if (state.target == nullptr) return 0.f;  // text outside a frame is dropped

    // Everything on one baseline: cull it in a single test.
    const float baseline = position.y + static_cast<float>(size);
    if (state.hasClip) {
        const Rect clip = state.clip;
        if (baseline < clip.top || baseline > clip.bottom()) return 0.f;
        if (position.x > clip.right()) return 0.f;
    }

    float x = position.x;
    // In SFML 3 a text position is the top of the line, the baseline sits one
    // character size lower (glyph bounds are relative to the baseline).
    const float xLimit = state.hasClip ? state.clip.right() : 0.f;
    const bool clipped = state.hasClip;
    const std::size_t base = state.used;
    state.ensureRoom(base + value.size());
    std::size_t index = 0;
    while (index < value.size()) {
        char32_t codePoint = 0;
        index += decodeAt(value, index, codePoint);
        const sf::Glyph& glyph = font.getGlyph(codePoint, size, bold);
        const float penX = x;
        x += glyph.advance;
        if (glyph.bounds.size.x <= 0.f || glyph.bounds.size.y <= 0.f) continue;
        if (clipped && penX > xLimit) break;  // past the right edge of the pane
        const sf::Vector2f topLeft(penX + glyph.bounds.position.x,
                                   baseline + glyph.bounds.position.y);
        const float right = topLeft.x + glyph.bounds.size.x;
        const float bottom = topLeft.y + glyph.bounds.size.y;
        const sf::Vector2f bottomLeft(topLeft.x, bottom);
        const sf::Vector2f topRight(right, topLeft.y);
        const sf::Vector2f bottomRight(right, bottom);
        const float texLeft = static_cast<float>(glyph.textureRect.position.x);
        const float texTop = static_cast<float>(glyph.textureRect.position.y);
        const float texRight = texLeft + static_cast<float>(glyph.textureRect.size.x);
        const float texBottom = texTop + static_cast<float>(glyph.textureRect.size.y);
        const sf::Vector2f uvTopLeft(texLeft, texTop);
        const sf::Vector2f uvTopRight(texRight, texTop);
        const sf::Vector2f uvBottomLeft(texLeft, texBottom);
        const sf::Vector2f uvBottomRight(texRight, texBottom);

        sf::Vertex* quad = &state.vertices[state.used];
        quad[0] = makeVertex(topLeft, color, uvTopLeft);
        quad[1] = makeVertex(topRight, color, uvTopRight);
        quad[2] = makeVertex(bottomLeft, color, uvBottomLeft);
        quad[3] = makeVertex(bottomLeft, color, uvBottomLeft);
        quad[4] = makeVertex(topRight, color, uvTopRight);
        quad[5] = makeVertex(bottomRight, color, uvBottomRight);
        state.used += 6;
    }
    return x - position.x;
}

float textWidth(const sf::Font& font, std::string_view text, unsigned int size, bool bold) {
    if (text.empty()) return 0.f;
    float width = 0.f;
    std::size_t index = 0;
    while (index < text.size()) {
        char32_t codePoint = 0;
        index += decodeAt(text, index, codePoint);
        width += font.getGlyph(codePoint, size, bold).advance;
    }
    return width;
}

void text(sf::RenderTarget& target, const sf::Font& font, std::string_view value, unsigned int size,
          const sf::Color& color, const sf::Vector2f& position, bool bold) {
    if (value.empty()) return;
    if (batch().target != &target) beginText(target);
    textRun(font, value, size, color, position, bold);
}

std::string ellipsize(const sf::Font& font, std::string_view value, unsigned int size,
                      float maxWidth) {
    if (maxWidth <= 0.f) return {};
    if (textWidth(font, value, size) <= maxWidth) return std::string(value);
    const std::string dots = "...";
    const float dotsWidth = textWidth(font, dots, size);
    std::vector<util::Utf8Char> chars = util::decodeUtf8(value);
    std::string out;
    float width = 0.f;
    for (std::size_t i = 0; i < chars.size(); ++i) {
        const float advance = font.hasGlyph(chars[i].codePoint)
                                  ? font.getGlyph(chars[i].codePoint, size, false).advance
                                  : 0.f;
        if (width + advance + dotsWidth > maxWidth) break;
        util::appendUtf8(out, chars[i].codePoint);
        width += advance;
    }
    return out + dots;
}

float textEllipsized(sf::RenderTarget& target, const sf::Font& font, std::string_view value,
                     unsigned int size, const sf::Color& color, const Rect& bounds,
                     bool bold) {
    const std::string fitted = ellipsize(font, value, size, bounds.width);
    text(target, font, fitted, size, color, sf::Vector2f(bounds.left, bounds.top), bold);
    return textWidth(font, fitted, size, bold);
}

void icon(sf::RenderTarget& target, const char* name, const sf::Vector2f& center, float size,
          const sf::Color& color) {
    const std::string_view id(name);
    const float cx = center.x;
    const float cy = center.y;
    if (id == "play") {
        const sf::Vector2f triangle[3] = {{cx - size * 0.30f, cy - size * 0.38f},
                                          {cx + size * 0.36f, cy},
                                          {cx - size * 0.30f, cy + size * 0.38f}};
        sf::VertexArray fan(sf::PrimitiveType::TriangleFan, 3);
        for (int i = 0; i < 3; ++i) fan[i] = makeVertex(triangle[i], color);
        target.draw(fan);
    } else if (id == "stop") {
        const float h = std::max(2.f, size * 0.30f);
        draw::rect(target, {cx - h, cy - h, h * 2.f, h * 2.f}, color);
    } else if (id == "plus") {
        const float t = std::max(1.f, size * 0.11f);
        const float arm = size * 0.30f;
        draw::rect(target, {cx - arm, cy - t * 0.5f, arm * 2.f, t}, color);
        draw::rect(target, {cx - t * 0.5f, cy - arm, t, arm * 2.f}, color);
    } else if (id == "close") {
        const float r = std::max(2.f, size * 0.22f);
        const float t = std::max(1.f, size * 0.10f);
        const sf::Vector2f normal{-t, t};
        sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, 4);
        const sf::Vector2f a{cx - r, cy - r};
        const sf::Vector2f b{cx + r, cy + r};
        strip[0] = makeVertex(a + normal, color);
        strip[1] = makeVertex(b + normal, color);
        strip[2] = makeVertex(a - normal, color);
        strip[3] = makeVertex(b - normal, color);
        target.draw(strip);
        const sf::Vector2f c{cx - r, cy + r};
        const sf::Vector2f d{cx + r, cy - r};
        sf::VertexArray strip2(sf::PrimitiveType::TriangleStrip, 4);
        strip2[0] = makeVertex(c + normal, color);
        strip2[1] = makeVertex(d + normal, color);
        strip2[2] = makeVertex(c - normal, color);
        strip2[3] = makeVertex(d - normal, color);
        target.draw(strip2);
    } else if (id == "search") {
        const float r = std::max(2.f, size * 0.26f);
        const float t = std::max(1.f, size * 0.10f);
        sf::VertexArray ring(sf::PrimitiveType::TriangleStrip, 4 * 9);
        for (int i = 0; i < 9; ++i) {
            const float a0 = 3.14159f * 1.25f + 6.2832f * i / 9.f;
            const float a1 = 3.14159f * 1.25f + 6.2832f * (i + 1) / 9.f;
            stripQuad(ring, static_cast<std::size_t>(i) * 4,
                      {cx - size * 0.08f + std::cos(a0) * r, cy - size * 0.08f + std::sin(a0) * r},
                      {cx - size * 0.08f + std::cos(a1) * r, cy - size * 0.08f + std::sin(a1) * r}, t,
                      color);
        }
        target.draw(ring);
        draw::rect(target, {cx + r * 0.55f, cy + r * 0.55f, r * 0.85f, t}, color);
    } else if (id == "chevron") {
        const float r = std::max(2.f, size * 0.18f);
        const float t = std::max(1.f, size * 0.10f);
        sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, 4);
        const sf::Vector2f a{cx - r, cy - r};
        const sf::Vector2f b{cx + r * 0.4f, cy};
        strip[0] = makeVertex(a - sf::Vector2f(0.f, t * 0.5f), color);
        strip[1] = makeVertex(b - sf::Vector2f(0.f, t * 0.5f), color);
        strip[2] = makeVertex(a + sf::Vector2f(0.f, t * 0.5f), color);
        strip[3] = makeVertex(b + sf::Vector2f(0.f, t * 0.5f), color);
        target.draw(strip);
    } else if (id == "note") {
        const float w = size * 0.42f;
        const float h = size * 0.54f;
        const float fold = size * 0.18f;
        // Page outline with a cut corner, drawn as a polygon fan.
        sf::VertexArray page(sf::PrimitiveType::TriangleFan, 6);
        page[0] = makeVertex({cx - w, cy - h}, color);
        page[1] = makeVertex({cx + w - fold, cy - h}, color);
        page[2] = makeVertex({cx + w, cy - h + fold}, color);
        page[3] = makeVertex({cx + w, cy + h}, color);
        page[4] = makeVertex({cx - w, cy + h}, color);
        page[5] = makeVertex({cx - w, cy - h}, color);
        target.draw(page);
        // Text lines, punched out in the background colour.
        const sf::Color ink = theme::pal.codeBlockBg;
        const float lineW = w * 1.5f;
        for (int i = 0; i < 3; ++i) {
            const float y = cy - h * 0.28f + static_cast<float>(i) * size * 0.19f;
            const float inset = i == 2 ? lineW * 0.45f : 0.f;
            draw::rect(target, {cx - w + size * 0.12f, y, lineW - size * 0.24f - inset, 1.2f}, ink);
        }
        draw::rect(target, {cx + w - fold - fold * 0.8f, cy - h, fold, fold}, ink);
    } else if (id == "dot") {
        const float r = std::max(1.5f, size * 0.22f);
        sf::VertexArray circle(sf::PrimitiveType::TriangleFan, 12);
        for (int i = 0; i < 12; ++i) {
            const float angle = 6.2832f * i / 11.f;
            circle[static_cast<std::size_t>(i)] =
                makeVertex({cx + std::cos(angle) * r, cy + std::sin(angle) * r}, color);
        }
        target.draw(circle);
    }
}

void scrollbar(sf::RenderTarget& target, const Rect& track, float offset, float content, float view,
               bool hovered, bool active) {
    if (content <= view || view <= 0.f) return;
    const theme::Palette& pal = theme::pal;
    const float width = std::min(theme::metrics.scrollbarWidth, track.width);
    const Rect rail{track.left + track.width - width, track.top, width, track.height};
    const float ratio = std::clamp(view / content, 0.08f, 1.f);
    const float thumbHeight = std::max(28.f, rail.height * ratio);
    const float travel = rail.height - thumbHeight;
    const float position = std::clamp(offset / std::max(1.f, content - view), 0.f, 1.f);
    const Rect thumb{rail.left + 2.f, rail.top + travel * position + 2.f, width - 4.f, thumbHeight - 4.f};
    draw::roundedRect(target, rail, width * 0.5f, pal.scrollTrack);
    draw::roundedRect(target, thumb, (width - 4.f) * 0.5f,
                      active ? pal.scrollThumbHover : (hovered ? pal.scrollThumbHover : pal.scrollThumb));
}

}  // namespace draw

void Button::draw(sf::RenderTarget& target, const sf::Font& font, float radius,
                  const sf::Color& normalFill, const sf::Color& hoverFill, const sf::Color& activeFill,
                  const sf::Color& normalText, const sf::Color& activeText) const {
    if (bounds.width <= 0.f || bounds.height <= 0.f) return;
    sf::Color fill = activeState ? activeFill : normalFill;
    if (hovered) fill = activeState ? activeFill : hoverFill;
    if (held) fill = sf::Color(std::min(255, fill.r + 12), std::min(255, fill.g + 12),
                              std::min(255, fill.b + 12), fill.a);
    const sf::Color textColor = !enabled ? theme::pal.textFaint
                                         : (activeState || accent ? activeText : normalText);
    draw::roundedRect(target, bounds, radius, fill);
    const unsigned int size = fontSize != 0 ? fontSize
                                            : static_cast<unsigned int>(theme::metrics.smallText);
    const float width = draw::textWidth(font, label, size);
    const float iconSize = iconName.empty() ? 0.f : size * 0.85f;
    const float gap = iconName.empty() ? 0.f : 6.f;
    const float group = iconSize + gap + width;
    float x = bounds.left + (bounds.width - group) * 0.5f;
    const float y = bounds.top + (bounds.height - size) * 0.5f - 1.f;
    if (!iconName.empty()) {
        draw::icon(target, iconName.c_str(), {x + iconSize * 0.5f, bounds.top + bounds.height * 0.5f},
                   iconSize, textColor);
        x += iconSize + gap;
    }
    draw::text(target, font, label, size, textColor, {x, y});
}
