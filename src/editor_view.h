#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <string>
#include <vector>

#include "doc.h"
#include "draw.h"
#include "highlight.h"
#include "richtext.h"
#include "runner.h"

// Renders a markdown note as a block editor: plain markdown lines stay plain,
// and every fenced code block turns into a small code editor with syntax
// highlighting, a run button and inline program output.
class EditorView {
public:
    void invalidate() { layoutVersion_ = 0xFFFFFFFFu; }
    void attach(Document* document, RunManager* runs);
    void setBounds(const Rect& bounds);
    const Rect& bounds() const { return bounds_; }

    void update(const MouseState& mouse, float dt, bool focused);
    void draw(sf::RenderTarget& target, bool focused);

    void onTextEntered(char32_t codePoint);
    void onKeyPressed(sf::Keyboard::Key key, bool control, bool shift, bool alt);
    void onMousePressed(const sf::Vector2f& position);
    void onMouseDragged(const sf::Vector2f& position);
    void onMouseReleased(const sf::Vector2f& position);
    void onWheel(float delta);

    void scrollCaretIntoView();
    void resetScroll();

    const std::vector<std::string>& runKeys() const { return keys_; }
    std::string languageAtCaret() const;
    bool hasOutputFor(const std::string& key) const;
    Rect runButtonBounds(int blockIndex) const;
    void revealOutput(const std::string& key);
    bool runBlockUnderCaret();
    int caretBlockIndex() const;
    bool textSelected() const;

private:
    struct LineSlot {
        float y = 0.f;
        float height = 0.f;
        int block = -1;
        bool fenceOpen = false;
        bool fenceClose = false;
    };

    struct BlockLayout {
        int openLine = 0;
        int closeLine = -1;
        std::string rawLang;
        std::string lang;
        std::string displayName;
        std::string key;
        float top = 0.f;
        float bottom = 0.f;
        float bodyTop = 0.f;
        float bodyBottom = 0.f;
        float outputTop = 0.f;
        float outputHeight = 0.f;
        Rect headerRect;
        Rect bodyRect;
        Rect outputRect;
        Button runButton;
        Button stopButton;
        std::vector<std::string> outputLines;
        // Body lines with tabs expanded for display; the document keeps the tabs.
        std::vector<std::string> displayLines;
        std::vector<std::vector<syntax::Span>> lineSpans;
        bool hasOutput = false;
    };

    void rebuildLayout();
    void rebuildLineStyles();
    const std::vector<richtext::Piece>& lineStyle(int line);
    // Text as drawn: code lines get their tabs expanded, the document is untouched.
    const std::string& displayLine(int line) const;
    bool layoutStale() const;
    float maxScroll() const;
    int lineAt(float y) const;
    Position positionAt(const sf::Vector2f& point) const;
    float lineTextX(int line) const;
    float columnX(int line, int column) const;
    EditContext contextFor(int line) const;
    const BlockLayout* blockAtLine(int line) const;
    std::string codeOf(const BlockLayout& block) const;
    bool mouseOverHeader(const sf::Vector2f& point) const;

    Document* document_ = nullptr;
    RunManager* runs_ = nullptr;
    Rect bounds_;
    float scroll_ = 0.f;

    std::vector<LineSlot> slots_;
    std::vector<BlockLayout> blocks_;
    std::vector<std::string> keys_;
    float contentHeight_ = 0.f;
    bool scrollHover_ = false;

    unsigned layoutVersion_ = 0;
    Rect layoutBounds_;
    mutable std::size_t runSignature_ = 0;
    // Markdown styling of each source line, rebuilt only when the text changes.
    unsigned styleVersion_ = 0xFFFFFFFFu;
    std::vector<std::vector<richtext::Piece>> lineStyles_;
    bool dragging_ = false;
    float time_ = 0.f;
};
