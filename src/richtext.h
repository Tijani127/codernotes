#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "markdown.h"

namespace richtext {

struct Style {
    sf::Color normal{201, 209, 217, 255};
    sf::Color emphasis{210, 168, 255, 255};
    sf::Color italic{176, 186, 197, 255};
    sf::Color code{152, 195, 121, 255};
    sf::Color link{88, 166, 255, 255};
    sf::Color marker{92, 100, 112, 255};
    sf::Color codeBackground{28, 35, 47, 255};
};

struct Piece {
    std::string text;
    sf::Color color;
    bool bold = false;
    bool code = false;
    std::string link;
};

// Splits raw markdown-ish text into styled runs. Markers stay in the output but
// are dimmed so the editor and preview can share the same code path.
std::vector<Piece> split(std::string_view text, const Style& style, bool dimMarkers = true);
std::vector<Piece> split(std::string_view text, const std::vector<md::InlineSpan>& spans,
                         const Style& style, bool dimMarkers = true);

std::vector<std::string> wrap(std::string_view text, const sf::Font& font, unsigned int size,
                              float maxWidth);

float width(const sf::Font& font, const std::vector<Piece>& pieces, unsigned int size);

float draw(sf::RenderTarget& target, const sf::Font& font, const std::vector<Piece>& pieces,
           unsigned int size, const sf::Vector2f& origin, const Style& style);

}  // namespace richtext
