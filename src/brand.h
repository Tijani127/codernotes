#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/WindowBase.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

#include "rect.h"

// The codernotes mark: a rounded badge holding a note page with a folded
// corner, three text lines and a forward caret. It is described once as a list
// of convex polygons so the vector version drawn in the header and the
// rasterised version handed to the window manager are the same artwork.
namespace brand {

// Brand colours. Deliberately fixed rather than theme derived: the mark has to
// read on a light taskbar and a dark one, and it doubles as the app identity.
inline const sf::Color kBadge{31, 111, 235, 255};
inline const sf::Color kBadgeDeep{20, 82, 189, 255};
inline const sf::Color kPaper{248, 250, 253, 255};
inline const sf::Color kLine{110, 132, 162, 255};

// Detail levels. Small sizes drop the parts that turn to mud.
enum Detail {
    kSimple = 0,   // badge, page, caret
    kMedium = 1,   // adds the folded corner and two text lines
    kFull = 2,     // adds the third text line
};

int detailForSize(float size);

// Draws the mark centred in `box` (a square region).
void draw(sf::RenderTarget& target, const Rect& box, int detail = kFull);

// Renders the mark at `size` x `size` and hands it to the window manager.
// Returns false when no OpenGL context is available.
bool applyToWindow(sf::WindowBase& window);

// Exposed for the self test: renders the mark offscreen at `size` and returns
// tightly packed RGBA bytes, or an empty vector on failure.
std::vector<std::uint8_t> renderPixels(unsigned int size, int detail);

// Diagnostic: writes a contact sheet of the mark at every size the window
// manager asks for, plus the vector version used in the header.
bool writePreview(const std::filesystem::path& path);

}  // namespace brand
