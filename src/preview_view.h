#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <vector>

#include "doc.h"
#include "draw.h"
#include "highlight.h"
#include "markdown.h"
#include "richtext.h"
#include "runner.h"

// Renders the markdown preview of the current note. Code blocks keep their run
// buttons so a snippet can be launched straight from the preview.
class PreviewView {
public:
    void invalidate() { layoutVersion_ = 0xFFFFFFFFu; }
    void attach(Document* document, RunManager* runs);
    void setBounds(const Rect& bounds);
    const Rect& bounds() const { return bounds_; }

    void update(const MouseState& mouse, float dt);
    void draw(sf::RenderTarget& target);
    void onMousePressed(const sf::Vector2f& position);
    void onMouseDragged(const sf::Vector2f& position);
    void onMouseReleased(const sf::Vector2f& position);
    void onWheel(float delta);

    std::vector<std::string> runKeys() const { return keys_; }
    void resetScroll();

private:
    struct LineItem {
        float y = 0.f;
        float height = 0.f;
    };

    struct LinkRegion {
        float y = 0.f;
        float height = 0.f;
        float x0 = 0.f;
        float x1 = 0.f;
        std::string url;
    };

    struct BlockItem {
        md::BlockType type = md::BlockType::Paragraph;
        int blockIndex = -1;

        // code blocks
        std::string key;
        std::string rawLang;
        std::string displayName;
        std::string code;
        std::vector<std::string> codeRows;
        std::vector<syntax::Span> spans;
        std::vector<LineItem> codeLines;
        std::vector<LineItem> outputLines;
        std::vector<std::string> outputRows;
        std::string errorText;
        std::string statusText;
        sf::Color statusColor;
        bool hasOutput = false;
        Rect bounds;
        Rect headerRect;
        Button runButton;
        Button stopButton;

        // text blocks
        std::vector<LineItem> textLines;
        std::vector<std::vector<richtext::Piece>> pieces;
        std::vector<LinkRegion> links;
        float indent = 0.f;
        unsigned int textSize = 0;
        sf::Color textColor;
        bool bold = false;
        bool underline = false;
        std::string marker;
        sf::Color markerColor;
    };

    void rebuild();
    bool stale() const;
    float maxScroll() const;
    void drawItem(sf::RenderTarget& target, const BlockItem& item, const richtext::Style& style);
    std::string codeOf(const md::Block& block) const;

    Document* document_ = nullptr;
    RunManager* runs_ = nullptr;
    Rect bounds_;
    float scroll_ = 0.f;
    float contentHeight_ = 0.f;
    bool scrollHover_ = false;
    std::vector<BlockItem> items_;
    std::vector<std::string> keys_;
    unsigned layoutVersion_ = 0xFFFFFFFFu;
    Rect layoutBounds_;
    mutable std::size_t runSignature_ = 0;
};
