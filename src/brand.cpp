#include "brand.h"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include "draw.h"

namespace brand {
namespace {

// Normalised geometry of the mark, in a 0..1 square with y pointing down.
constexpr float kBadgeRadius = 0.235f;
constexpr float kPageLeft = 0.205f;
constexpr float kPageTop = 0.145f;
constexpr float kPageWidth = 0.375f;
constexpr float kPageHeight = 0.710f;
constexpr float kPageRadius = 0.050f;
constexpr float kFoldSize = 0.120f;
constexpr float kLineHeight = 0.052f;
constexpr float kCaretThickness = 0.088f;

struct Piece {
    std::vector<sf::Vector2f> points;
    sf::Color color;
};

// Rounded rectangle as a triangle fan, so one description can be filled by the
// GPU on screen and by the downsampler for the window icon.
std::vector<sf::Vector2f> roundedBox(const Rect& bounds, float radius, int segments = 6) {
    const float r = std::min({radius, bounds.width * 0.5f, bounds.height * 0.5f});
    const std::array<sf::Vector2f, 4> corners = {
        sf::Vector2f{bounds.left + r, bounds.top + r},
        sf::Vector2f{bounds.right() - r, bounds.top + r},
        sf::Vector2f{bounds.right() - r, bounds.bottom() - r},
        sf::Vector2f{bounds.left + r, bounds.bottom() - r},
    };
    std::vector<sf::Vector2f> ring;
    ring.reserve(4 * static_cast<std::size_t>(segments + 1));
    for (int corner = 0; corner < 4; ++corner) {
        const float baseAngle = corner == 0 ? 180.f : corner == 1 ? 270.f : corner == 2 ? 0.f : 90.f;
        for (int step = 0; step <= segments; ++step) {
            const float angle = (baseAngle + 90.f * static_cast<float>(step) / segments) * 3.14159265f / 180.f;
            ring.push_back({corners[static_cast<std::size_t>(corner)].x + std::cos(angle) * r,
                            corners[static_cast<std::size_t>(corner)].y + std::sin(angle) * r});
        }
    }
    return ring;
}

std::vector<sf::Vector2f> disc(const sf::Vector2f& center, float radius, int segments = 16) {
    std::vector<sf::Vector2f> points;
    points.reserve(static_cast<std::size_t>(segments));
    for (int i = 0; i < segments; ++i) {
        const float angle = 6.2831853f * static_cast<float>(i) / segments;
        points.push_back({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius});
    }
    return points;
}

// A thick segment as a quad; the caller adds discs for the caps.
std::vector<sf::Vector2f> thickSegment(const sf::Vector2f& from, const sf::Vector2f& to,
                                       float thickness) {
    sf::Vector2f direction = to - from;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length < 0.001f) return {};
    direction /= length;
    const sf::Vector2f normal{-direction.y * thickness * 0.5f, direction.x * thickness * 0.5f};
    return {from + normal, from - normal, to - normal, to + normal};
}

// Builds every filled piece of the mark inside `box`.
std::vector<Piece> buildPieces(const Rect& box, int detail) {
    const float u = std::min(box.width, box.height);
    const auto at = [box, u](float x, float y) {
        return sf::Vector2f{box.left + x * u, box.top + y * u};
    };
    std::vector<Piece> pieces;

    // Badge with a slightly deeper lower edge for a hint of depth.
    pieces.push_back({roundedBox(box, kBadgeRadius * u), kBadge});
    if (detail >= kMedium) {
        const Rect lower{box.left, box.top + u * 0.72f, u, u * 0.28f};
        std::vector<sf::Vector2f> lip = roundedBox(box, kBadgeRadius * u);
        std::vector<sf::Vector2f> band;
        for (const sf::Vector2f& point : lip) {
            if (point.y >= lower.top) band.push_back(point);
        }
        if (band.size() >= 3) pieces.push_back({std::move(band), kBadgeDeep});
    }

    // The note page.
    const Rect page{box.left + kPageLeft * u, box.top + kPageTop * u, kPageWidth * u, kPageHeight * u};
    pieces.push_back({roundedBox(page, kPageRadius * u), kPaper});

    if (detail >= kMedium) {
        // Folded corner: the badge showing through the clipped page corner.
        pieces.push_back({{at(kPageLeft + kPageWidth - kFoldSize, kPageTop),
                           at(kPageLeft + kPageWidth, kPageTop),
                           at(kPageLeft + kPageWidth, kPageTop + kFoldSize)},
                          kBadge});
        // Text lines.
        const int lineCount = detail >= kFull ? 3 : 2;
        const float widths[3] = {0.235f, 0.160f, 0.205f};
        for (int i = 0; i < lineCount; ++i) {
            const float y = kPageTop + 0.230f + static_cast<float>(i) * 0.150f;
            const Rect line{box.left + (kPageLeft + 0.070f) * u, box.top + y * u, widths[i] * u,
                            kLineHeight * u};
            pieces.push_back({roundedBox(line, kLineHeight * u * 0.5f, 4), kLine});
        }
    }

    // Forward caret, sitting clear of the page on the badge.
    const sf::Vector2f top = at(0.660f, 0.375f);
    const sf::Vector2f tip = at(0.818f, 0.500f);
    const sf::Vector2f bottom = at(0.660f, 0.625f);
    const std::vector<sf::Vector2f> upper = thickSegment(top, tip, kCaretThickness * u);
    const std::vector<sf::Vector2f> lower = thickSegment(tip, bottom, kCaretThickness * u);
    if (upper.size() == 4) pieces.push_back({upper, kPaper});
    if (lower.size() == 4) pieces.push_back({lower, kPaper});
    pieces.push_back({disc(tip, kCaretThickness * u * 0.5f), kPaper});
    pieces.push_back({disc(top, kCaretThickness * u * 0.5f), kPaper});
    pieces.push_back({disc(bottom, kCaretThickness * u * 0.5f), kPaper});
    return pieces;
}

// Software rasteriser for the same polygon list. Reading pixels back out of a
// framebuffer is unreliable across drivers, and the window icon has to be
// exactly the artwork on screen, so the icon is filled here instead.
struct Canvas {
    unsigned int size = 0;  // side length in pixels
    std::vector<float> rgba;  // straight alpha, 4 floats per pixel

    explicit Canvas(unsigned int side)
        : size(side), rgba(static_cast<std::size_t>(side) * side * 4u, 0.f) {}
};

bool insideConvex(const std::vector<sf::Vector2f>& points, float x, float y) {
    bool positive = false;
    bool negative = false;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const sf::Vector2f& a = points[i];
        const sf::Vector2f& b = points[(i + 1) % points.size()];
        const float cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
        if (cross > 0.f) {
            positive = true;
        } else if (cross < 0.f) {
            negative = true;
        }
        if (positive && negative) return false;
    }
    return true;
}

void fillPolygon(Canvas& canvas, const std::vector<sf::Vector2f>& points, const sf::Color& color) {
    if (points.size() < 3 || canvas.size == 0) return;
    float minX = points[0].x;
    float maxX = points[0].x;
    float minY = points[0].y;
    float maxY = points[0].y;
    for (const sf::Vector2f& point : points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    const int limit = static_cast<int>(canvas.size);
    const int x0 = std::clamp(static_cast<int>(std::floor(minX)), 0, limit);
    const int x1 = std::clamp(static_cast<int>(std::ceil(maxX)), 0, limit);
    const int y0 = std::clamp(static_cast<int>(std::floor(minY)), 0, limit);
    const int y1 = std::clamp(static_cast<int>(std::ceil(maxY)), 0, limit);
    constexpr int kSamples = 4;  // 4x4 supersampling
    const float step = 1.f / kSamples;
    const float sourceR = static_cast<float>(color.r) / 255.f;
    const float sourceG = static_cast<float>(color.g) / 255.f;
    const float sourceB = static_cast<float>(color.b) / 255.f;

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            int hits = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    if (insideConvex(points, x + (static_cast<float>(sx) + 0.5f) * step,
                                     y + (static_cast<float>(sy) + 0.5f) * step)) {
                        ++hits;
                    }
                }
            }
            if (hits == 0) continue;
            const float sourceA = static_cast<float>(hits) / (kSamples * kSamples);
            float* pixel = &canvas.rgba[(static_cast<std::size_t>(y) * canvas.size + x) * 4u];
            const float destA = pixel[3];
            const float outA = sourceA + destA * (1.f - sourceA);
            if (outA <= 0.f) {
                pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0.f;
                continue;
            }
            const float keep = destA * (1.f - sourceA);
            pixel[0] = (sourceR * sourceA + pixel[0] * keep) / outA;
            pixel[1] = (sourceG * sourceA + pixel[1] * keep) / outA;
            pixel[2] = (sourceB * sourceA + pixel[2] * keep) / outA;
            pixel[3] = outA;
        }
    }
}

}  // namespace

int detailForSize(float size) {
    if (size >= 48.f) return kFull;
    if (size >= 24.f) return kMedium;
    return kSimple;
}

void draw(sf::RenderTarget& target, const Rect& box, int detail) {
    if (box.width <= 0.f || box.height <= 0.f) return;
    // The pieces are plain geometry, so the glyph batch has to land first.
    draw::endText();
    for (const Piece& piece : buildPieces(box, detail)) {
        draw::polygon(target, piece.points, piece.color);
    }
}

std::vector<std::uint8_t> renderPixels(unsigned int size, int detail) {
    if (size == 0) return {};
    // Fill one large master and downsample, so every size the window manager
    // asks for is a proper area average of the same artwork.
    constexpr unsigned int kMaster = 256;
    const unsigned int master = std::max(size, kMaster);
    Canvas canvas(master);
    for (const Piece& piece :
         buildPieces(Rect{0.f, 0.f, static_cast<float>(master), static_cast<float>(master)}, detail)) {
        fillPolygon(canvas, piece.points, piece.color);
    }

    sf::Image image(sf::Vector2u{master, master});
    for (unsigned int y = 0; y < master; ++y) {
        for (unsigned int x = 0; x < master; ++x) {
            const float* pixel = &canvas.rgba[(static_cast<std::size_t>(y) * master + x) * 4u];
            const auto to8 = [](float value) {
                return static_cast<std::uint8_t>(std::clamp(value * 255.f + 0.5f, 0.f, 255.f));
            };
            image.setPixel(sf::Vector2u{x, y}, sf::Color(to8(pixel[0]), to8(pixel[1]), to8(pixel[2]),
                                                        to8(pixel[3])));
        }
    }
    while (image.getSize().x > size) {
        image.resize(sf::Vector2u{std::max(1u, image.getSize().x / 2u),
                                  std::max(1u, image.getSize().y / 2u)});
    }
    if (image.getSize().x != size) image.resize(sf::Vector2u{size, size});
    const std::uint8_t* pixels = image.getPixelsPtr();
    if (pixels == nullptr) return {};
    return std::vector<std::uint8_t>(pixels, pixels + static_cast<std::size_t>(size) * size * 4u);
}

bool applyToWindow(sf::WindowBase& window) {
    // Windows and most desktops pick the closest match from these sizes.
    static constexpr std::array<unsigned int, 8> kSizes{16, 20, 24, 32, 48, 64, 128, 256};
    bool any = false;
    for (const unsigned int size : kSizes) {
        const std::vector<std::uint8_t> pixels =
            renderPixels(size, detailForSize(static_cast<float>(size)));
        if (pixels.empty()) continue;
        window.setIcon(sf::Vector2u{size, size}, pixels.data());
        any = true;
    }
    return any;
}

bool writePreview(const std::filesystem::path& path) {
    static constexpr std::array<unsigned int, 7> kSizes{16, 20, 24, 32, 48, 64, 128};
    constexpr unsigned int kGap = 14;
    constexpr unsigned int kHeight = 128;
    // The first tile is the vector mark at header size, the rest are the
    // downsampled icons.
    const unsigned int vectorSize = 28;
    unsigned int width = vectorSize + kGap;
    for (const unsigned int size : kSizes) width += size + kGap;

    sf::Image sheet(sf::Vector2u{width, kHeight}, sf::Color(18, 20, 24, 255));
    const auto blit = [&sheet](const std::vector<std::uint8_t>& pixels, unsigned int size,
                               unsigned int x, unsigned int y) {
        for (unsigned int row = 0; row < size; ++row) {
            for (unsigned int column = 0; column < size; ++column) {
                const std::size_t source = (static_cast<std::size_t>(row) * size + column) * 4u;
                const sf::Color rgba(pixels[source], pixels[source + 1], pixels[source + 2],
                                     pixels[source + 3]);
                if (rgba.a == 0) continue;
                // Source-over compositing onto the sheet background.
                const float alpha = static_cast<float>(rgba.a) / 255.f;
                const sf::Color& dst = sheet.getPixel(sf::Vector2u{x + column, y + row});
                const auto blend = [alpha](int source8, int dest8) {
                    return static_cast<std::uint8_t>(std::clamp(
                        static_cast<int>(source8 * alpha + dest8 * (1.f - alpha) + 0.5f), 0, 255));
                };
                sheet.setPixel(sf::Vector2u{x + column, y + row},
                               sf::Color(blend(rgba.r, dst.r), blend(rgba.g, dst.g),
                                         blend(rgba.b, dst.b), 255));
            }
        }
    };

    unsigned int x = kGap;
    const std::vector<std::uint8_t> vectorPixels = renderPixels(vectorSize, kFull);
    if (vectorPixels.empty()) return false;
    blit(vectorPixels, vectorSize, x, kHeight - vectorSize);
    x += vectorSize + kGap;
    for (const unsigned int size : kSizes) {
        const std::vector<std::uint8_t> pixels = renderPixels(size, detailForSize(static_cast<float>(size)));
        if (pixels.empty()) return false;
        blit(pixels, size, x, kHeight - size);
        x += size + kGap;
    }
    return sheet.saveToFile(path);
}

}  // namespace brand
