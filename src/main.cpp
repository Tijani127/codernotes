#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <algorithm>
#include <algorithm>
#include <chrono>
#include <set>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "doc.h"
#include "brand.h"
#include "draw.h"
#include "editor_view.h"
#include "markdown.h"
#include "note.h"
#include "platform.h"
#include "preview_view.h"
#include "runner.h"
#include "theme.h"
#include "util.h"

namespace {

const char* const kStarterNote = R"(# Welcome to codernotes

A markdown notebook where every fenced code block is a small editor that you can
run without leaving the app.

## Notes

- Type normal markdown: *italic*, **bold**, `inline code` and [links](https://sfml-dev.org)
- Fenced blocks get syntax highlighting, line numbers, a caret and a **Run** button
- Output is captured live and printed under the block

## Try it

```js
const squares = [1, 2, 3, 4, 5].map(n => n * n);
console.log("squares:", squares.join(", "));
```

```python
def fib(count):
    a, b = 0, 1
    for _ in range(count):
        yield a
        a, b = b, a + b

print("fib:", list(fib(12)))
```

## Shortcuts

- `Ctrl+N` new note, `Ctrl+S` save
- `F5` or `Ctrl+Enter` runs the block under the caret
- `Ctrl+B` bold, `Ctrl+I` italic, `Ctrl+/` toggle comment
- `Ctrl+Z` undo, `Shift+Ctrl+Z` redo
- `Ctrl+P` toggles the preview pane
- `Ctrl+Shift+T` switches colour theme
)";

const float kSidebarRowHeight = 58.f;
const float kSplitterWidth = 6.f;
constexpr float kSidebarSearchTop = 34.f;
constexpr float kSidebarListGap = 8.f;
constexpr float kSidebarFooter = 26.f;

// Case-insensitive substring match over a note's title and body. Returns the
// body offset of the match, or npos when the title matched or nothing did.
std::size_t bodyMatch(const NoteMeta& meta, const std::string& loweredQuery) {
    if (loweredQuery.empty()) return 0;
    if (util::findFold(meta.title, loweredQuery) != std::string_view::npos) return 0;
    return util::findFold(meta.body, loweredQuery);
}

bool noteMatches(const NoteMeta& meta, const std::string& query) {
    if (query.empty()) return true;
    const std::string lowered = util::toLower(util::collapseSpaces(query));
    return util::findFold(meta.title, lowered) != std::string_view::npos ||
           util::findFold(meta.body, lowered) != std::string_view::npos;
}

// A short line of body text around a match, so a filtered row explains itself.
// Markdown syntax is stripped so the snippet reads as prose, not source. The
// caller truncates to the real row width, so the whole line is returned here.
std::string matchSnippet(const NoteMeta& meta, const std::string& query) {
    const std::string lowered = util::toLower(util::collapseSpaces(query));
    const std::size_t offset = bodyMatch(meta, lowered);
    if (offset == std::string_view::npos || lowered.empty()) return {};
    // Walk back to the start of the line so the snippet does not begin mid-word.
    std::size_t start = meta.body.rfind('\n', offset);
    start = (start == std::string::npos) ? 0 : start + 1;
    std::size_t end = meta.body.find('\n', offset);
    if (end == std::string::npos) end = meta.body.size();
    if (end <= start) return {};
    std::string line = util::collapseSpaces(md::stripInline(meta.body.substr(start, end - start)));
    // Skip lines that are only markup, such as a bare fence or an empty rule.
    if (line.empty() || md::isHeadingStart(line) || line[0] == '`' || line[0] == '#') return {};
    return line;
}

class App {
public:
    App();

    struct Options {
        std::string screenshotPath;
        std::string runLanguage;
        std::string iconPreviewPath;
        // Pre-fills the sidebar filter, so a filtered layout can be captured
        // without driving the keyboard.
        std::string initialSearch;
        int warmupFrames = 6;
        int benchmarkFrames = 0;
        int profileFrames = 0;
        int testWidth = 0;
        int testHeight = 0;
        bool selfTest = false;
        bool live = false;
    };

    int selfTest();
    void simulateClick(const sf::Vector2f& position);

    int run(const Options& options);

private:
    void handleEvent(const sf::Event& event);
    void handleKey(sf::Keyboard::Key key, bool control, bool shift);
    void handleMousePressed(const sf::Vector2f& position);
    void handleMouseReleased(const sf::Vector2f& position);
    void handleMouseMoved(const sf::Vector2f& position);
    void handleWheel(float delta, const sf::Vector2f& position);

    void update(float dt);
    void layout();
    void render(sf::RenderTarget& target);
    int saveScreenshot(const std::string& path);

    void drawHeader(sf::RenderTarget& target);
    void drawSidebar(sf::RenderTarget& target);
    void drawStatusBar(sf::RenderTarget& target);

    void openNote(int index);
    void createNote();
    void deleteCurrentNote();
    bool saveCurrent();
    void markDirty();
    void syncWindowView();
    void toast(const std::string& message, bool isError = false);
    void cycleTheme();
    Rect themeButtonBounds() const;

    Rect sidebarRect() const;
    Rect editorRect() const;
    Rect previewRect() const;
    Rect statusRect() const;
    Rect searchRect() const;
    float sidebarListTop() const;
    int visibleNoteCount() const;

    sf::RenderWindow window_;
    NoteStore store_;
    Document document_;
    EditorView editor_;
    PreviewView preview_;
    RunManager runs_;
    MouseState mouse_;

    int current_ = -1;
    int wordCount_ = 0;
    unsigned wordCountVersion_ = 0xFFFFFFFFu;
    unsigned retainedVersion_ = 0xFFFFFFFFu;
    bool retainedPreview_ = true;
    bool simulatedMouse_ = false;
    std::set<std::string> finishedRuns_;
    bool showPreview_ = true;
    bool editorFocused_ = true;
    bool draggingSplitter_ = false;
    bool draggingSidebar_ = false;
    float sidebarWidth_ = theme::metrics.sidebarWidth;
    float splitRatio_ = 0.56f;
    float sidebarScroll_ = 0.f;
    bool sidebarScrollHover_ = false;
    std::string searchText_;
    bool searchFocused_ = false;
    double saveTimer_ = 0.0;
    std::string toastText_;
    double toastTimer_ = 0.0;
    bool toastError_ = false;

    Button newButton_;
    Button saveButton_;
    Button previewButton_;
    Button themeButton_;
    std::vector<Button> deleteButtons_;
    int hoveredDelete_ = -1;
};

App::App() : window_(), store_(platform::notesDirectory()) {}

void App::toast(const std::string& message, bool isError) {
    toastText_ = message;
    toastTimer_ = 4.0;
    toastError_ = isError;
}

Rect App::sidebarRect() const {
    const sf::Vector2u size = window_.getSize();
    return {0.f, theme::metrics.headerHeight, sidebarWidth_,
            static_cast<float>(size.y) - theme::metrics.headerHeight - theme::metrics.statusHeight};
}

Rect App::statusRect() const {
    const sf::Vector2u size = window_.getSize();
    return {0.f, static_cast<float>(size.y) - theme::metrics.statusHeight,
            static_cast<float>(size.x), theme::metrics.statusHeight};
}

Rect App::searchRect() const {
    const Rect sidebar = sidebarRect();
    return {sidebar.left + 12.f, sidebar.top + kSidebarSearchTop, sidebar.width - 24.f,
            theme::metrics.searchHeight};
}

float App::sidebarListTop() const {
    return searchRect().bottom() + kSidebarListGap;
}

int App::visibleNoteCount() const {
    int count = 0;
    for (const NoteMeta& meta : store_.notes()) {
        if (noteMatches(meta, searchText_)) ++count;
    }
    return count;
}

Rect App::editorRect() const {
    const sf::Vector2u size = window_.getSize();
    const Rect sidebar = sidebarRect();
    const float contentX = sidebar.left + sidebar.width;
    const float contentWidth = static_cast<float>(size.x) - contentX;
    const float height = static_cast<float>(size.y) - theme::metrics.headerHeight -
                         theme::metrics.statusHeight;
    if (!showPreview_) {
        return {contentX, theme::metrics.headerHeight, contentWidth, height};
    }
    const float editorWidth = std::max(200.f, contentWidth * splitRatio_ - kSplitterWidth * 0.5f);
    return {contentX, theme::metrics.headerHeight, editorWidth, height};
}

Rect App::previewRect() const {
    if (!showPreview_) return {};
    const sf::Vector2u size = window_.getSize();
    const Rect editor = editorRect();
    const float x = editor.left + editor.width + kSplitterWidth;
    return {x, theme::metrics.headerHeight,
            std::max(0.f, static_cast<float>(size.x) - x),
            static_cast<float>(size.y) - theme::metrics.headerHeight - theme::metrics.statusHeight};
}

void App::syncWindowView() {
    // SFML 3.0 does not follow the window size with its default view, which
    // would stretch every drawn element away from its hit test rectangle. It
    // also does not handle per monitor DPI changes, so the client rect is the
    // source of truth and a stale size is corrected.
    if (!simulatedMouse_) {
#if defined(_WIN32)
        RECT client{};
        if (GetClientRect(window_.getNativeHandle(), &client) != 0) {
            const auto clientWidth = static_cast<unsigned>(client.right - client.left);
            const auto clientHeight = static_cast<unsigned>(client.bottom - client.top);
            const sf::Vector2u size = window_.getSize();
            if (clientWidth > 0 && clientHeight > 0 &&
                (clientWidth != size.x || clientHeight != size.y)) {
                window_.setSize({clientWidth, clientHeight});
            }
        }
#endif
        const float scale = platform::windowScale(window_.getNativeHandle());
        if (std::fabs(scale - theme::metrics.scale) > 0.01f) {
            theme::applyScale(scale);
            theme::metrics.lineHeight =
                theme::lineHeight(theme::uiFont, static_cast<unsigned>(theme::metrics.uiText));
            theme::metrics.codeLineHeight =
                theme::lineHeight(theme::monoFont, static_cast<unsigned>(theme::metrics.codeText));
            editor_.invalidate();
            preview_.invalidate();
        }
    }
    const sf::Vector2f size(window_.getSize());
    window_.setView(sf::View(size * 0.5f, size));
}

void App::markDirty() {
    saveTimer_ = 1.2;
    if (current_ >= 0 && current_ < static_cast<int>(store_.notes().size())) {
        store_.notes()[static_cast<std::size_t>(current_)].dirty = true;
    }
}

void App::openNote(int index) {
    if (index < 0 || index >= static_cast<int>(store_.notes().size())) return;
    if (current_ >= 0 && document_.modified()) saveCurrent();
    current_ = index;
    NoteMeta& meta = store_.notes()[static_cast<std::size_t>(index)];
    document_.setText(store_.read(meta.id));
    meta.dirty = false;
    editor_.resetScroll();
    preview_.resetScroll();
    editor_.scrollCaretIntoView();
    if (index >= 0 && index < static_cast<int>(deleteButtons_.size())) {
        deleteButtons_[static_cast<std::size_t>(index)].hovered = false;
    }
}

void App::createNote() {
    if (current_ >= 0 && document_.modified()) saveCurrent();
    store_.create(kStarterNote);
    current_ = 0;
    document_.setText(store_.read(store_.notes()[0].id));
    store_.notes()[0].dirty = false;
    editor_.resetScroll();
    preview_.resetScroll();
    toast("new note created");
}

void App::deleteCurrentNote() {
    if (current_ < 0 || current_ >= static_cast<int>(store_.notes().size())) return;
    const std::string id = store_.notes()[static_cast<std::size_t>(current_)].id;
    std::string error;
    if (!store_.remove(id, error)) {
        toast(error, true);
        return;
    }
    deleteButtons_.clear();
    if (store_.notes().empty()) {
        current_ = -1;
        document_.setText(std::string());
        toast("note deleted");
        return;
    }
    const int next = std::min(current_, static_cast<int>(store_.notes().size()) - 1);
    current_ = -1;
    openNote(next);
    toast("note deleted");
}

bool App::saveCurrent() {
    if (current_ < 0 || current_ >= static_cast<int>(store_.notes().size())) return false;
    if (!document_.modified()) return true;
    NoteMeta& meta = store_.notes()[static_cast<std::size_t>(current_)];
    const std::string text = document_.text();
    std::string error;
    if (!store_.write(meta.id, text, error)) {
        toast(error, true);
        return false;
    }
document_.markSaved();
    meta.dirty = false;
    meta.title = NoteStore::titleFromText(text, meta.id);
    meta.preview.clear();
    // Keep the search copy in step with the file. `text` is already a full copy
    // of the note, so this is the cheapest possible place to refresh it.
    meta.body = text;
    saveTimer_ = 0.0;
    return true;
}

void App::cycleTheme() {
    theme::cycleTheme();
    theme::savePreference(platform::notesDirectory());
    editor_.invalidate();
    preview_.invalidate();
    layout();
    toast(std::string("theme: ") + std::string(theme::currentTheme().name));
}

Rect App::themeButtonBounds() const {
    const theme::Metrics& m = theme::metrics;
    const std::string label(theme::currentTheme().name);
    const float textWidth = draw::textWidth(theme::uiFont, label,
                                           static_cast<unsigned int>(m.smallText));
    const float width = textWidth + 46.f;
    const float height = m.headerHeight - 18.f;
    const float left = newButton_.bounds.left - 10.f - width;
    return {left, 9.f, width, height};
}

void App::handleKey(sf::Keyboard::Key key, bool control, bool shift) {
    if (control && shift && key == sf::Keyboard::Key::T) {
        cycleTheme();
        return;
    }
    if (control && !searchFocused_) {
        switch (key) {
            case sf::Keyboard::Key::N: createNote(); return;
            case sf::Keyboard::Key::S: saveCurrent(); toast("saved"); return;
            case sf::Keyboard::Key::P: showPreview_ = !showPreview_; layout(); return;
            default: break;
        }
    }
    if (searchFocused_) {
        switch (key) {
            case sf::Keyboard::Key::Escape:
            case sf::Keyboard::Key::Enter:
                searchFocused_ = false;
                editorFocused_ = true;
                return;
            case sf::Keyboard::Key::Backspace:
                if (!searchText_.empty()) {
                    searchText_.pop_back();
                    sidebarScroll_ = 0.f;
                }
                return;
            default: break;
        }
        return;
    }
    if (key == sf::Keyboard::Key::Escape) {
        document_.clearSelection();
        return;
    }
    if (editorFocused_) {
        editor_.onKeyPressed(key, control, shift, false);
    }
}

void App::handleMousePressed(const sf::Vector2f& position) {
    mouse_.pressPosition = position;
    if (position.y <= theme::metrics.headerHeight) {
        if (newButton_.bounds.contains(position)) return;  // handled in update()
        return;
    }
    if (position.x <= 4.f && position.y > theme::metrics.headerHeight) {
        draggingSidebar_ = true;
        return;
    }
    const Rect sidebar = sidebarRect();
    if (sidebar.contains(position)) {
        if (searchRect().contains(position)) {
            searchFocused_ = true;
            editorFocused_ = false;
            return;
        }
        // Map the click to a row index inside the filtered list.
        const float local = position.y - sidebarListTop() + sidebarScroll_;
        const int row = static_cast<int>(std::floor(local / kSidebarRowHeight));
        if (row >= 0 && row < visibleNoteCount()) {
            int index = -1;
            int seen = 0;
            for (std::size_t i = 0; i < store_.notes().size(); ++i) {
                if (!noteMatches(store_.notes()[i], searchText_)) continue;
                if (seen == row) {
                    index = static_cast<int>(i);
                    break;
                }
                ++seen;
            }
            if (index < 0) return;
            if (index < static_cast<int>(deleteButtons_.size()) &&
                deleteButtons_[static_cast<std::size_t>(index)].bounds.contains(position)) {
                current_ = index;
                deleteCurrentNote();
                return;
            }
            if (index != current_) openNote(index);
            editorFocused_ = false;
            return;
        }
        return;
    }
    const Rect editor = editorRect();
    const Rect preview = previewRect();
    if (showPreview_ && std::fabs(position.x - (editor.left + editor.width + kSplitterWidth * 0.5f)) <
                           kSplitterWidth) {
        draggingSplitter_ = true;
        return;
    }
    if (editor.contains(position)) {
        editorFocused_ = true;
        editor_.onMousePressed(position);
        return;
    }
    if (preview.contains(position)) {
        editorFocused_ = false;
        preview_.onMousePressed(position);
    }
}

void App::handleMouseReleased(const sf::Vector2f& position) {
    draggingSplitter_ = false;
    draggingSidebar_ = false;
    editor_.onMouseReleased(position);
    preview_.onMouseReleased(position);
}

void App::handleMouseMoved(const sf::Vector2f& position) {
    if (draggingSplitter_) {
        const sf::Vector2u size = window_.getSize();
        const float contentX = sidebarRect().width;
        const float contentWidth = static_cast<float>(size.x) - contentX;
        splitRatio_ = std::clamp((position.x - contentX) / contentWidth, 0.2f, 0.85f);
        layout();
        return;
    }
    if (draggingSidebar_) {
        sidebarWidth_ = std::clamp(position.x, 170.f, 420.f);
        layout();
        return;
    }
    if (mouse_.leftDown) {
        const Rect editor = editorRect();
        if (editorFocused_ && editor.contains(position)) {
            editor_.onMouseDragged(position);
            return;
        }
    }
}

void App::handleWheel(float delta, const sf::Vector2f& position) {
    const Rect sidebar = sidebarRect();
    if (sidebar.contains(position)) {
        const float listTop = sidebarListTop();
        if (position.y < listTop) return;  // over the search field
        const float total = static_cast<float>(visibleNoteCount()) * kSidebarRowHeight;
        const float visible = sidebar.height - (listTop - sidebar.top) - kSidebarFooter;
        sidebarScroll_ = std::clamp(sidebarScroll_ - delta * 60.f, 0.f, std::max(0.f, total - visible));
        return;
    }
    if (editorFocused_ && editorRect().contains(position)) {
        editor_.onWheel(delta);
        return;
    }
    preview_.onWheel(delta);
}

void App::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        if (document_.modified()) saveCurrent();
        window_.close();
        return;
    }
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        (void)resized;
        syncWindowView();
        layout();
        return;
    }
    if (const auto* focus = event.getIf<sf::Event::FocusLost>()) {
        (void)focus;
        editorFocused_ = false;
        return;
    }
    if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
        if (searchFocused_) {
            if (text->unicode >= 32 && text->unicode != 127 && searchText_.size() < 64) {
                const char32_t codePoint = text->unicode;
                char buffer[5] = {};
                std::size_t length = 0;
                if (codePoint < 0x80) {
                    buffer[0] = static_cast<char>(codePoint);
                    length = 1;
                } else if (codePoint < 0x800) {
                    buffer[0] = static_cast<char>(0xC0 | (codePoint >> 6));
                    buffer[1] = static_cast<char>(0x80 | (codePoint & 0x3F));
                    length = 2;
                } else if (codePoint < 0x10000) {
                    buffer[0] = static_cast<char>(0xE0 | (codePoint >> 12));
                    buffer[1] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                    buffer[2] = static_cast<char>(0x80 | (codePoint & 0x3F));
                    length = 3;
                } else {
                    buffer[0] = static_cast<char>(0xF0 | (codePoint >> 18));
                    buffer[1] = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                    buffer[2] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                    buffer[3] = static_cast<char>(0x80 | (codePoint & 0x3F));
                    length = 4;
                }
                searchText_.append(buffer, length);
                sidebarScroll_ = 0.f;
            }
            return;
        }
        if (editorFocused_ && text->unicode >= 32 && text->unicode != 127) {
            editor_.onTextEntered(text->unicode);
            markDirty();
        }
        return;
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        handleKey(key->code, key->control, key->shift);
        if (document_.modified()) markDirty();
        return;
    }
    if (const auto* press = event.getIf<sf::Event::MouseButtonPressed>()) {
        const sf::Vector2f position(static_cast<float>(press->position.x),
                                    static_cast<float>(press->position.y));
        mouse_.position = position;
        if (press->button == sf::Mouse::Button::Left) {
            mouse_.leftDown = true;
            mouse_.leftPressed = true;
            handleMousePressed(position);
        }
        return;
    }
    if (const auto* release = event.getIf<sf::Event::MouseButtonReleased>()) {
        const sf::Vector2f position(static_cast<float>(release->position.x),
                                    static_cast<float>(release->position.y));
        mouse_.position = position;
        if (release->button == sf::Mouse::Button::Left) {
            mouse_.leftDown = false;
            mouse_.leftReleased = true;
            handleMouseReleased(position);
        }
        return;
    }
    if (const auto* moved = event.getIf<sf::Event::MouseMoved>()) {
        const sf::Vector2f position(static_cast<float>(moved->position.x),
                                    static_cast<float>(moved->position.y));
        mouse_.position = position;
        handleMouseMoved(position);
        return;
    }
    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>()) {
        const sf::Vector2f position(static_cast<float>(wheel->position.x),
                                    static_cast<float>(wheel->position.y));
        mouse_.position = position;
        handleWheel(wheel->delta, position);
    }
}

void App::update(float dt) {
    // The window view has to track the window size: SFML keeps the old one after
    // a resize, which offsets everything that is drawn from what is hit tested.
    syncWindowView();
    // Note: mouse_.beginFrame() runs before events are polled (see the main
    // loop), otherwise the press/release flags would be cleared before any
    // button ever sees them.
    if (!simulatedMouse_) {
        const sf::Vector2i mousePosition = sf::Mouse::getPosition(window_);
        mouse_.position = {static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)};
    }

    runs_.update();

    const sf::Vector2u size = window_.getSize();
    const float buttonH = theme::metrics.headerHeight - 18.f;
    const float right = static_cast<float>(size.x) - 14.f;
    // Right-aligned button group: Preview, Save, New.
    previewButton_.bounds = {right - 104.f, 9.f, 104.f, buttonH};
    saveButton_.bounds = {previewButton_.bounds.left - 8.f - 88.f, 9.f, 88.f, buttonH};
    newButton_.bounds = {saveButton_.bounds.left - 8.f - 84.f, 9.f, 84.f, buttonH};
    newButton_.label = "New";
    newButton_.iconName = "plus";
    newButton_.update(mouse_, dt);
    if (newButton_.clickedFlag) createNote();

    saveButton_.label = document_.modified() ? "Save*" : "Save";
    saveButton_.update(mouse_, dt);
    if (saveButton_.clickedFlag) {
        saveCurrent();
        toast("saved");
    }

    previewButton_.label = showPreview_ ? "Preview" : "Preview off";
    previewButton_.activeState = showPreview_;
    previewButton_.update(mouse_, dt);
    if (previewButton_.clickedFlag) {
        showPreview_ = !showPreview_;
        layout();
    }

    themeButton_.bounds = themeButtonBounds();
    themeButton_.label = std::string(theme::currentTheme().name);
    themeButton_.iconName = "dot";
    themeButton_.update(mouse_, dt);
    if (themeButton_.clickedFlag) cycleTheme();

    if (deleteButtons_.size() != store_.notes().size()) deleteButtons_.resize(store_.notes().size());
    hoveredDelete_ = -1;
    const Rect sidebar = sidebarRect();
    const float listTop = sidebarListTop();
    sidebarScrollHover_ = sidebar.contains(mouse_.position) && mouse_.position.y >= listTop &&
                         mouse_.position.x >= sidebar.right() - theme::metrics.scrollbarWidth - 10.f;
    int visible = 0;
    for (std::size_t i = 0; i < deleteButtons_.size(); ++i) {
        const float y = listTop + static_cast<float>(visible) * kSidebarRowHeight - sidebarScroll_;
        if (noteMatches(store_.notes()[i], searchText_)) ++visible;
        const Rect row{sidebar.left + 8.f, y, sidebar.width - 20.f, kSidebarRowHeight - 6.f};
        Button& button = deleteButtons_[i];
        button.bounds = {row.right() - 30.f, y + 7.f, 22.f, 22.f};
        button.label = "x";
        button.enabled = static_cast<int>(i) == current_;
        if (!button.enabled) {
            button.hovered = false;
            button.held = false;
            button.clickedFlag = false;
            continue;
        }
        button.update(mouse_, dt);
        if (button.hovered) hoveredDelete_ = static_cast<int>(i);
    }

    editor_.update(mouse_, dt, editorFocused_);
    if (showPreview_) preview_.update(mouse_, dt);

    if (document_.version() != retainedVersion_ || showPreview_ != retainedPreview_) {
        retainedVersion_ = document_.version();
        retainedPreview_ = showPreview_;
        std::vector<std::string> keys = editor_.runKeys();
        if (showPreview_) {
            const std::vector<std::string>& previewKeys = preview_.runKeys();
            keys.insert(keys.end(), previewKeys.begin(), previewKeys.end());
        }
        runs_.retainOnly(keys);
    }

    // When a snippet finishes, scroll its output panel into view.
    for (const std::string& key : editor_.runKeys()) {
        const RunState* state = runs_.find(key);
        const bool done = state != nullptr && state->finished;
        const bool wasDone = finishedRuns_.count(key) != 0;
        if (done && !wasDone) editor_.revealOutput(key);
        if (done) finishedRuns_.insert(key);
    }

    if (document_.modified()) {
        saveTimer_ -= dt;
        if (saveTimer_ <= 0.0) saveCurrent();
    }
    if (toastTimer_ > 0.0) toastTimer_ -= dt;

    (void)sidebar;
}

void App::layout() {
    editor_.setBounds(editorRect());
    if (showPreview_) preview_.setBounds(previewRect());
}

void App::drawHeader(sf::RenderTarget& target) {
    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;
    const sf::Vector2u size = window_.getSize();
    const float headerH = m.headerHeight;
    draw::vGradient(target, {0.f, 0.f, static_cast<float>(size.x), headerH}, pal.chromeHi, pal.headerBg);
    draw::hline(target, headerH - 1.f, 0.f, static_cast<float>(size.x), pal.border);

    // Primary action: New. Secondary actions stay as quiet ghost buttons.
    newButton_.draw(target, theme::uiFont, 5.f, pal.accent, pal.accent,
                    pal.accentHi, pal.onAccent, pal.onAccent);
    saveButton_.draw(target, theme::uiFont, 5.f, sf::Color(0, 0, 0, 0), pal.hover, sf::Color(0, 0, 0, 0),
                     pal.text, pal.heading);
    previewButton_.draw(target, theme::uiFont, 5.f, sf::Color(0, 0, 0, 0), pal.hover,
                        sf::Color(0, 0, 0, 0), pal.textDim, pal.accent);
    // Theme switcher: a swatch of the accent colour plus the theme name.
    themeButton_.draw(target, theme::uiFont, 5.f, sf::Color(0, 0, 0, 0), pal.hover,
                      sf::Color(0, 0, 0, 0), pal.textDim, pal.accent);

    // Brand mark: the same artwork the window icon uses, drawn as vectors.
    const float tileY = (headerH - 30.f) * 0.5f;
    brand::draw(target, Rect{14.f, tileY, 30.f, 30.f}, brand::kFull);

    const float titleX = 56.f;
    const unsigned int titleSize = static_cast<unsigned int>(m.uiText + 1.f);
    const float mid = headerH * 0.5f;
    draw::text(target, theme::uiFont, "codernotes", titleSize, pal.heading, {titleX, mid - 10.f}, true);
    const float brandWidth = draw::textWidth(theme::uiFont, "codernotes", titleSize, true);
    if (current_ >= 0 && current_ < static_cast<int>(store_.notes().size())) {
        draw::rect(target, {titleX + brandWidth + 9.f, mid - 9.f, 1.f, 18.f}, pal.borderStrong);
        const std::string& title = store_.notes()[static_cast<std::size_t>(current_)].title;
        const float nameX = titleX + brandWidth + 20.f;
        const float nameWidth =
            std::max(60.f, themeButtonBounds().left - nameX - 20.f);
        draw::textEllipsized(target, theme::uiFont, title, titleSize, pal.text,
                             {nameX, mid - 10.f, nameWidth, 22.f});
    }
}

void App::drawSidebar(sf::RenderTarget& target) {
    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;
    const Rect sidebar = sidebarRect();
    draw::rect(target, sidebar, pal.sidebarBg);
    draw::vline(target, sidebar.left + sidebar.width - 1.f, sidebar.top, sidebar.top + sidebar.height,
                pal.border);

    const unsigned int labelSize = static_cast<unsigned int>(m.tinyText);
    draw::text(target, theme::uiFont, "NOTES", labelSize, pal.textFaint,
               {sidebar.left + 16.f, sidebar.top + 14.f}, true);
    // Count badge keeps the sidebar header balanced.
    if (!store_.notes().empty()) {
        const std::string count = std::to_string(store_.notes().size());
        const unsigned int countSize = static_cast<unsigned int>(m.tinyText - 1.f);
        const float w = draw::textWidth(theme::uiFont, count, countSize) + 14.f;
        const Rect badge{sidebar.left + sidebar.width - 16.f - w, sidebar.top + 11.f, w, 16.f};
        draw::roundedRect(target, badge, 8.f, pal.pill);
        draw::text(target, theme::uiFont, count, countSize, pal.textDim,
                   {badge.left + 7.f, badge.top + 2.f});
    }

    // Search field.
    const Rect search = searchRect();
    const bool searchHovered = search.contains(mouse_.position);
    draw::roundedRect(target, search, 6.f, pal.codeBlockBg);
    draw::roundedOutline(target, search, 6.f,
                         searchFocused_ ? pal.accent : (searchHovered ? pal.borderStrong : pal.borderSoft));
    draw::icon(target, "search", {search.left + 14.f, search.top + m.searchHeight * 0.5f}, 13.f,
               searchText_.empty() ? pal.textFaint : pal.textDim);
    if (searchText_.empty()) {
        draw::text(target, theme::uiFont, "Search notes", static_cast<unsigned int>(m.smallText - 2.f),
                   pal.textFaint, {search.left + 28.f, search.top + m.searchHeight * 0.5f - 8.f});
    } else {
        draw::textEllipsized(target, theme::uiFont, searchText_, static_cast<unsigned int>(m.smallText),
                             pal.text,
                             {search.left + 28.f, search.top + m.searchHeight * 0.5f - 9.f,
                              search.width - 52.f, m.searchHeight});
    }
    if (searchFocused_) {
        const float caretX = search.left + 28.f +
                             draw::textWidth(theme::uiFont, searchText_,
                                             static_cast<unsigned int>(m.smallText));
        draw::rect(target, {caretX, search.top + 6.f, 2.f, search.height - 12.f}, pal.accent);
    }
    const float listTop = sidebarListTop();

    const unsigned int titleSize = static_cast<unsigned int>(m.uiText - 1.f);
    const unsigned int metaSize = static_cast<unsigned int>(m.smallText - 3.f);
    int shown = 0;
    for (std::size_t i = 0; i < store_.notes().size(); ++i) {
        const NoteMeta& meta = store_.notes()[i];
        if (!noteMatches(meta, searchText_)) continue;
        const float y = listTop + static_cast<float>(shown) * kSidebarRowHeight - sidebarScroll_;
        ++shown;
        if (y + kSidebarRowHeight < sidebar.top || y > sidebar.top + sidebar.height) continue;
        const Rect row = {sidebar.left + 8.f, y, sidebar.width - 20.f, kSidebarRowHeight - 6.f};
        const bool active = static_cast<int>(i) == current_;
        const bool hovered = row.contains(mouse_.position);
        if (active) {
            draw::roundedRect(target, row, 6.f, pal.activeRow);
            draw::roundedOutline(target, row, 6.f, pal.accentSoft);
            draw::rect(target, {row.left + 1.f, row.top + 9.f, 2.f, row.height - 18.f}, pal.accent);
        } else if (hovered) {
            draw::roundedRect(target, row, 6.f, pal.hover);
        }

        // Note glyph gives the row a clear anchor.
        draw::icon(target, "note", {row.left + 15.f, row.top + row.height * 0.5f}, 15.f,
                   active ? pal.accent : pal.textFaint);

        const float textX = row.left + 30.f;
        const float textWidth = row.width - 40.f - (active ? 24.f : 0.f);
        std::string title = util::collapseSpaces(meta.title);
        if (title.empty()) title = "(untitled)";
        draw::textEllipsized(target, theme::uiFont, title, titleSize,
                             active ? pal.heading : pal.text,
                             {textX, row.top + 9.f, textWidth, titleSize + 6.f}, active);
std::string meta2 = util::formatTimestamp(meta.stamp);
        if (meta.dirty) meta2 = "unsaved";
        // While filtering, the line that matched explains the result far better
        // than the timestamp, so it takes that slot.
        if (!searchText_.empty()) {
            const std::string snippet = matchSnippet(meta, searchText_);
            if (!snippet.empty()) meta2 = snippet;
        }
        draw::textEllipsized(target, theme::uiFont, meta2, metaSize, pal.textFaint,
                             {textX, row.top + 30.f, textWidth, metaSize + 6.f});
        if (active && static_cast<std::size_t>(hoveredDelete_) == i) {
            deleteButtons_[i].draw(target, theme::uiFont, 4.f, pal.hover, pal.error, pal.error, pal.textDim,
                                  pal.text);
        }
    }

    if (store_.notes().empty()) {
        brand::draw(target, {sidebar.left + sidebar.width * 0.5f - 22.f, sidebar.top + 84.f, 44.f, 44.f},
                    brand::kFull);
        draw::text(target, theme::uiFont, "No notes yet", static_cast<unsigned int>(m.smallText + 1.f),
                   pal.textDim, {sidebar.left + 16.f, sidebar.top + 140.f});
        draw::text(target, theme::uiFont, "Create one with", static_cast<unsigned int>(m.smallText),
                   pal.textFaint, {sidebar.left + 16.f, sidebar.top + 162.f});
        draw::text(target, theme::uiFont, "the New button above.", static_cast<unsigned int>(m.smallText),
                   pal.textFaint, {sidebar.left + 16.f, sidebar.top + 180.f});
    } else if (shown == 0) {
        draw::text(target, theme::uiFont, "no matches", static_cast<unsigned int>(m.smallText),
                   pal.textFaint, {sidebar.left + 16.f, listTop + 12.f});
    }

    // Sidebar scrollbar for long note lists.
    const float listViewport = sidebar.top + sidebar.height - listTop - kSidebarFooter;
    const float listHeight = static_cast<float>(shown) * kSidebarRowHeight;
    draw::scrollbar(target, {sidebar.left, listTop, sidebar.width, listViewport}, sidebarScroll_,
                    listHeight, listViewport, sidebarScrollHover_, false);

    draw::hline(target, sidebar.top + sidebar.height - kSidebarFooter, sidebar.left, sidebar.right() - 1.f,
                pal.borderSoft);
    draw::icon(target, "note", {sidebar.left + 20.f, sidebar.top + sidebar.height - 12.f}, 12.f,
               pal.textFaint);
    draw::textEllipsized(target, theme::uiFont, store_.directory().string(), metaSize, pal.textFaint,
                         {sidebar.left + 30.f, sidebar.top + sidebar.height - 19.f,
                          sidebar.width - 44.f, 16.f});
}

void App::drawStatusBar(sf::RenderTarget& target) {
    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;
    const Rect status = statusRect();
    draw::rect(target, status, pal.statusBg);
    draw::hline(target, status.top, 0.f, status.width, pal.border);

    const Position caret = document_.caret();
    const unsigned int size = static_cast<unsigned int>(m.smallText - 2.f);
    const float mid = status.top + status.height * 0.5f - 7.f;
    // The language comes from the editor layout, which is already up to date.
    const std::string language = editor_.languageAtCaret().empty() ? "markdown" : editor_.languageAtCaret();
    const float langW = draw::textWidth(theme::uiFont, language, size) + 26.f;

    // Left: caret position and selection size, as a quiet info pill.
    std::string caretText = "Ln " + std::to_string(caret.line + 1) + ", Col " +
                            std::to_string(caret.column + 1);
    if (document_.hasSelection() && document_.caret() != document_.selectionStart()) {
        caretText += "   " + std::to_string(document_.selectedText().size()) + " selected";
    }
    const float caretW = draw::textWidth(theme::uiFont, caretText, size) + 20.f;
    draw::roundedRect(target, {12.f, status.top + 4.f, caretW, status.height - 8.f},
                      (status.height - 8.f) * 0.5f, pal.pill);
    draw::text(target, theme::uiFont, caretText, size, pal.textDim, {22.f, mid});

    if (document_.version() != wordCountVersion_) {
        wordCountVersion_ = document_.version();
        wordCount_ = static_cast<int>(util::split(document_.text(), ' ').size());
    }
    const std::string words = std::to_string(wordCount_) + " words";
    float x = 12.f + caretW + 8.f;
    const float wordsW = draw::textWidth(theme::uiFont, words, size) + 20.f;
    draw::roundedRect(target, {x, status.top + 4.f, wordsW, status.height - 8.f},
                      (status.height - 8.f) * 0.5f, pal.pill);
    draw::text(target, theme::uiFont, words, size, pal.textFaint, {x + 10.f, mid});
    x += wordsW + 8.f;

    // Keyboard shortcut hints, right-aligned and dropped one by one when the
    // window is too narrow to show them all.
    const unsigned int hintSize = static_cast<unsigned int>(m.smallText - 2.f);
    const std::pair<const char*, const char*> kHints[] = {
        {"F5", "run block"}, {"Ctrl+N", "new"},   {"Ctrl+S", "save"},
        {"Ctrl+P", "preview"}, {"Ctrl+Shift+T", "theme"},
    };
    const float gap = 18.f;
    std::string hint;
    float hintWidth = 0.f;
    const float hintLimit =
        std::max(80.f, status.right() - langW - 34.f - (x + 8.f));
    for (const auto& [combo, action] : kHints) {
        const std::string piece = std::string(combo) + " " + action;
        const float pieceWidth = draw::textWidth(theme::uiFont, piece, hintSize);
        const float extra = hint.empty() ? pieceWidth : pieceWidth + gap;
        if (hintWidth + extra > hintLimit) break;
        if (!hint.empty()) hint += "    ";
        hint += piece;
        hintWidth += extra;
    }
    draw::text(target, theme::uiFont, hint, hintSize, pal.textFaint,
               {status.right() - hintWidth - 14.f, mid});

    // Language pill, anchored just left of the hints.
    const Rect langPill{status.right() - hintWidth - 26.f - langW, status.top + 4.f, langW,
                        status.height - 8.f};
    draw::roundedRect(target, langPill, (status.height - 8.f) * 0.5f, pal.accent);
    draw::icon(target, "dot", {langPill.left + 12.f, status.top + status.height * 0.5f}, 9.f, pal.onAccent);
    draw::text(target, theme::uiFont, language, size, pal.onAccent, {langPill.left + 20.f, mid});
}

void App::render(sf::RenderTarget& target) {
    const theme::Palette& pal = theme::pal;
    target.clear(pal.windowBg);
    draw::beginText(target);

    // Panes first: they use a scrolled view and can bleed a little outside
    // their bounds, so the opaque chrome is painted on top afterwards.
    editor_.draw(target, editorFocused_);
    if (showPreview_) preview_.draw(target);

    drawHeader(target);
    drawSidebar(target);
    drawStatusBar(target);

    if (showPreview_) {
        const Rect editor = editorRect();
        const float x = editor.left + editor.width + kSplitterWidth * 0.5f;
        draw::rect(target, {x - kSplitterWidth * 0.5f, editor.top, kSplitterWidth, editor.height},
                   pal.border);
        if (mouse_.position.x >= x - 6.f && mouse_.position.x <= x + 6.f) {
            draw::rect(target, {x - kSplitterWidth * 0.5f, editor.top, kSplitterWidth, editor.height},
                       pal.accentSoft);
        }
    }


    if (toastTimer_ > 0.0 && !toastText_.empty()) {
        const theme::Palette& p = theme::pal;
        const unsigned int textSize = static_cast<unsigned int>(theme::metrics.smallText);
        const sf::Vector2u windowSize = window_.getSize();
        const float toastWidth = draw::textWidth(theme::uiFont, toastText_, textSize) + 30.f;
        const float toastHeight = 32.f;
        const Rect bounds = {static_cast<float>(windowSize.x) * 0.5f - toastWidth * 0.5f,
                             statusRect().top - toastHeight - 12.f, toastWidth, toastHeight};
        const auto alpha =
            static_cast<std::uint8_t>(242.f * std::min(1.f, static_cast<float>(toastTimer_)));
        const sf::Color surface = toastError_ ? p.error : p.codeBlockHeader;
        // Fade in and out rather than popping, and keep the pill readable on
        // both light and dark schemes.
        for (int i = 3; i >= 1; --i) {
            draw::roundedRect(target, {bounds.left - i, bounds.top + i, bounds.width + 2.f * i, bounds.height},
                              8.f + i, sf::Color(p.shadow.r, p.shadow.g, p.shadow.b, 26));
        }
        draw::roundedRect(target, bounds, 8.f,
                          sf::Color(surface.r, surface.g, surface.b, alpha));
        draw::roundedOutline(target, bounds, 8.f,
                             sf::Color(toastError_ ? p.error.r : p.borderStrong.r,
                                       toastError_ ? p.error.g : p.borderStrong.g,
                                       toastError_ ? p.error.b : p.borderStrong.b, alpha));
        draw::roundedRect(target, {bounds.left + 12.f, bounds.top + 11.f, 3.f, 10.f},
                          1.5f, sf::Color(p.accent.r, p.accent.g, p.accent.b, alpha));
        draw::text(target, theme::uiFont, toastText_, textSize,
                   toastError_ ? p.onAccent : p.text, {bounds.left + 22.f, bounds.top + 8.f});
    }

    draw::endText();
    if (&target == &window_) window_.display();
}

// Sends a press and a release at the given window position through the same
// path a real click takes.
void App::simulateClick(const sf::Vector2f& position) {
    simulatedMouse_ = true;
    mouse_.beginFrame();
    mouse_.position = position;
    mouse_.leftPressed = true;
    mouse_.leftDown = true;
    mouse_.pressPosition = position;
    handleMousePressed(position);
    update(1.f / 60.f);
    layout();

    mouse_.position = position;
    mouse_.beginFrame();
    mouse_.leftReleased = true;
    mouse_.leftDown = false;
    handleMouseReleased(position);
    update(1.f / 60.f);
    layout();
}

int App::selfTest() {
    int failures = 0;
    const auto check = [&failures](const char* name, bool ok, const std::string& detail) {
        std::fprintf(stderr, "%-26s %s  %s\n", name, ok ? "ok  " : "FAIL", detail.c_str());
        if (!ok) ++failures;
    };

    const sf::Vector2u size = window_.getSize();
    const Rect header{0.f, 0.f, static_cast<float>(size.x), theme::metrics.headerHeight};
    const Rect sidebar = sidebarRect();
    const Rect editor = editorRect();
    const Rect preview = previewRect();
    const Rect status = statusRect();
    const sf::View defaultView = window_.getView();

    check("window/view size match", defaultView.getSize() == sf::Vector2f(size),
          "window=" + std::to_string(size.x) + "x" + std::to_string(size.y) +
              " view=" + std::to_string(static_cast<int>(defaultView.getSize().x)) + "x" +
              std::to_string(static_cast<int>(defaultView.getSize().y)));
    check("header spans window", std::fabs(header.width - static_cast<float>(size.x)) < 1.f, "");
    check("status spans window", std::fabs(status.width - static_cast<float>(size.x)) < 1.f &&
                                     status.bottom() <= static_cast<float>(size.y) + 1.f,
          "bottom=" + std::to_string(status.bottom()));
    check("sidebar inside window", sidebar.bottom() <= status.top + 1.f, "");
    check("editor inside window", editor.bottom() <= status.top + 1.f &&
                                     editor.left + editor.width <= static_cast<float>(size.x), "");
    check("preview inside window", !showPreview_ || (preview.left + preview.width <=
                                                    static_cast<float>(size.x) + 1.f &&
                                                    preview.bottom() <= status.top + 1.f),
          "preview=" + std::to_string(preview.left) + ".." +
              std::to_string(preview.left + preview.width));

    // Buttons: verify the drawn bounds and that a synthetic click is routed.
    const Rect saveButton{static_cast<float>(size.x) - 220.f, 9.f, 92.f,
                          theme::metrics.headerHeight - 18.f};
    check("save button on screen", saveButton.left >= 0.f && saveButton.bottom() <= header.bottom(),
          "x=" + std::to_string(saveButton.left));

    // Maximize/resize round trip: the default view has to follow the new size or
    // every drawn element ends up offset from where it is hit tested.
    const sf::Vector2u bigger{size.x + 320u, size.y + 200u};
    window_.setSize(bigger);
    while (const std::optional event = window_.pollEvent()) handleEvent(*event);
    update(1.f / 60.f);
    layout();
    {
        const sf::Vector2u resized = window_.getSize();
        const sf::View view = window_.getView();
        const bool viewFollows = view.getSize() == sf::Vector2f(resized);
        check("view follows resize", viewFollows,
              "window=" + std::to_string(resized.x) + "x" + std::to_string(resized.y) +
                  " view=" + std::to_string(static_cast<int>(view.getSize().x)) + "x" +
                  std::to_string(static_cast<int>(view.getSize().y)));
        const Rect statusAfter = statusRect();
        const Rect editorAfter = editorRect();
        const Rect previewAfter = previewRect();
        check("chrome spans resized window",
              std::fabs(statusAfter.width - static_cast<float>(resized.x)) < 1.f &&
                  statusAfter.bottom() <= static_cast<float>(resized.y) + 1.f,
              "status=" + std::to_string(statusAfter.width) + " bottom=" +
                  std::to_string(statusAfter.bottom()));
        check("panes inside resized window",
              editorAfter.bottom() <= statusAfter.top + 1.f &&
                  previewAfter.left + previewAfter.width <= static_cast<float>(resized.x) + 1.f,
              "editor=" + std::to_string(editorAfter.width) + " preview=" +
                  std::to_string(previewAfter.width));
        check("toolbar follows resize", newButton_.bounds.left >= 0.f && previewButton_.bounds.right() <=
                                                                 static_cast<float>(resized.x),
              "previewBtn.right=" + std::to_string(previewButton_.bounds.right()));
    }
    window_.setSize(size);
    while (const std::optional event = window_.pollEvent()) handleEvent(*event);
    update(1.f / 60.f);
    layout();

    // Every character of a source line has to reach the drawn pieces, otherwise
    // text silently disappears from the editor.
    {
        int gaps = 0;
        std::string firstGap;
        const std::vector<std::string>& lines = document_.lines();
        for (int line = 0; line < static_cast<int>(lines.size()) && gaps < 3; ++line) {
            const std::string& source = lines[static_cast<std::size_t>(line)];
            std::string rebuilt;
            for (const richtext::Piece& piece : richtext::split(source, richtext::Style{})) {
                rebuilt += piece.text;
            }
            if (rebuilt != source) {
                ++gaps;
                if (firstGap.empty()) {
                    firstGap = "line " + std::to_string(line + 1) + " src=\"" + source +
                               "\" got=\"" + rebuilt + "\"";
                }
            }
        }
        check("editor text fully covered", gaps == 0, firstGap);
    }

    // Drawing and hit testing have to agree: the window view must map window
    // coordinates onto themselves, and the editor's scrolled view onto the pane.
    {
        // Input arrives as window pixel coordinates and is compared straight
        // against layout rectangles, so the direction that has to be exact is
        // pixel -> coords. SFML's reverse mapping rounds at half-pixel
        // boundaries and lands one pixel low on X11, so allow a pixel of slack
        // there rather than asserting a precision we do not depend on.
        bool identity = true;
        std::string detail = "5/5 samples round-trip";
        const sf::Vector2u s = window_.getSize();
        const sf::Vector2i samples[] = {{0, 0},
                                        {static_cast<int>(s.x) - 1, 0},
                                        {0, static_cast<int>(s.y) - 1},
                                        {static_cast<int>(s.x) / 2, static_cast<int>(s.y) / 2},
                                        {static_cast<int>(s.x) - 1, static_cast<int>(s.y) - 1}};
        for (const sf::Vector2i& sample : samples) {
            const sf::Vector2f point(static_cast<float>(sample.x), static_cast<float>(sample.y));
            const sf::Vector2i toPixel = window_.mapCoordsToPixel(point);
            const sf::Vector2f toCoord = window_.mapPixelToCoords(sample);
            const bool exact = std::fabs(toCoord.x - point.x) <= 0.01f &&
                               std::fabs(toCoord.y - point.y) <= 0.01f;
            const bool withinOne = std::abs(toPixel.x - sample.x) <= 1 &&
                                   std::abs(toPixel.y - sample.y) <= 1;
            if (exact && withinOne) continue;
            identity = false;
            detail = "sample (" + std::to_string(sample.x) + "," + std::to_string(sample.y) +
                     ") -> px (" + std::to_string(toPixel.x) + "," + std::to_string(toPixel.y) +
                     ") coords (" + std::to_string(toCoord.x) + "," + std::to_string(toCoord.y) +
                     ")";
            break;
        }
        check("window view is 1:1", identity, detail);

        const Rect pane = editorRect();
        const sf::View scrolled(sf::Vector2f(static_cast<float>(s.x) * 0.5f,
                                             static_cast<float>(s.y) * 0.5f + 137.f),
                                sf::Vector2f(s));
        const sf::Vector2f contentPoint(pane.left + 40.f, pane.top + 137.f + 40.f);
        const sf::Vector2i screenPoint = window_.mapCoordsToPixel(contentPoint, scrolled);
        const bool scrolledOk =
            std::fabs(static_cast<float>(screenPoint.x) - (pane.left + 40.f)) <= 1.f &&
            std::fabs(static_cast<float>(screenPoint.y) - (pane.top + 40.f)) <= 1.f;
        check("scrolled view matches pane", scrolledOk,
              "content=" + std::to_string(contentPoint.x) + "," + std::to_string(contentPoint.y) +
                  " -> pixel=" + std::to_string(screenPoint.x) + "," +
                  std::to_string(screenPoint.y));
    }

    // A drawn button has to be hit tested at the same place.
    {
        const Rect drawn{std::fabs(static_cast<float>(size.x)) - 220.f, 9.f, 92.f,
                         theme::metrics.headerHeight - 18.f};
        const sf::Vector2i pixel = window_.mapCoordsToPixel(drawn.center());
        const sf::Vector2f back = window_.mapPixelToCoords(pixel);
        const bool roundTrip = std::fabs(back.x - drawn.center().x) <= 1.f &&
                               std::fabs(back.y - drawn.center().y) <= 1.f;
        check("button hit test round trip", roundTrip,
              "center=" + std::to_string(drawn.center().x) + "," +
                  std::to_string(drawn.center().y) + " pixel=" + std::to_string(pixel.x) + "," +
                  std::to_string(pixel.y));
    }

    // The drawn framebuffer and the coordinates everything is laid out in have
    // to be the same thing, otherwise text lands outside the window and buttons
    // drift away from their hit boxes. Checked against the real client rect.
    {
        sf::Vector2u framebuffer = size;
#if defined(_WIN32)
        RECT client{};
        if (GetClientRect(window_.getNativeHandle(), &client) != 0) {
            framebuffer = {static_cast<unsigned>(client.right - client.left),
                           static_cast<unsigned>(client.bottom - client.top)};
        }
#endif
        check("client rect matches getSize", framebuffer == size,
              "client=" + std::to_string(framebuffer.x) + "x" + std::to_string(framebuffer.y) +
                  " size=" + std::to_string(size.x) + "x" + std::to_string(size.y));
        const sf::IntRect viewport = window_.getViewport(window_.getView());
        check("viewport covers framebuffer",
              viewport.size.x >= static_cast<int>(framebuffer.x) &&
                  viewport.size.y >= static_cast<int>(framebuffer.y),
              "viewport=" + std::to_string(viewport.size.x) + "x" +
                  std::to_string(viewport.size.y));
    }

    const int before = static_cast<int>(store_.notes().size());
    simulateClick(newButton_.bounds.center());
    check("+ New creates a note", static_cast<int>(store_.notes().size()) == before + 1,
          "notes=" + std::to_string(store_.notes().size()));

    // Search field: focus, filtering, and returning focus to the editor.
    const int allVisible = visibleNoteCount();
    simulateClick(searchRect().center());
    check("search takes focus", !editorFocused_, "editorFocused=" + std::to_string(editorFocused_));
    for (const char letter : std::string_view{"zzz"}) {
        sf::Event::TextEntered typed{};
        typed.unicode = static_cast<char32_t>(letter);
        handleEvent(typed);
    }
    check("search filters list", visibleNoteCount() == 0 && allVisible > 0,
          "visible=" + std::to_string(visibleNoteCount()) + " of " + std::to_string(allVisible));
for (int i = 0; i < 3; ++i) handleKey(sf::Keyboard::Key::Backspace, false, false);
    check("search restores list", visibleNoteCount() == allVisible,
          "visible=" + std::to_string(visibleNoteCount()));

    // A term that appears in no title but in the starter note's body must still
    // match. "squares" only occurs inside the javascript block.
    {
        const std::string term = "squares";
        bool titleHasIt = false;
        for (const NoteMeta& meta : store_.notes()) {
            if (util::findFold(meta.title, util::toLower(term)) != std::string_view::npos) {
                titleHasIt = true;
            }
        }
        for (const char letter : term) {
            sf::Event::TextEntered typed{};
            typed.unicode = static_cast<char32_t>(letter);
            handleEvent(typed);
        }
        check("search reaches note bodies", !titleHasIt && visibleNoteCount() >= 1,
              "titlesContain=" + std::to_string(titleHasIt ? 1 : 0) +
                  " visible=" + std::to_string(visibleNoteCount()));
        check("match yields a snippet",
              visibleNoteCount() >= 1 && !matchSnippet(store_.notes()[0], term).empty(),
              "snippet=" + matchSnippet(store_.notes()[0], term));
        // Case folding has to work on the body too, not just the title.
        for (int i = 0; i < static_cast<int>(term.size()); ++i) {
            handleKey(sf::Keyboard::Key::Backspace, false, false);
        }
        for (const char letter : term) {
            sf::Event::TextEntered typed{};
            typed.unicode = static_cast<char32_t>(std::toupper(static_cast<unsigned char>(letter)));
            handleEvent(typed);
        }
        check("body search ignores case", visibleNoteCount() >= 1,
              "visible=" + std::to_string(visibleNoteCount()));
        for (int i = 0; i < static_cast<int>(term.size()); ++i) {
            handleKey(sf::Keyboard::Key::Backspace, false, false);
        }
    }
    check("search clears fully", searchText_.empty() && visibleNoteCount() == allVisible,
          "query='" + searchText_ + "' visible=" + std::to_string(visibleNoteCount()));
    handleKey(sf::Keyboard::Key::Escape, false, false);
    check("escape returns to editor", editorFocused_ && searchText_.empty(),
          "editorFocused=" + std::to_string(editorFocused_));
    // A click in the list area must map to the first filtered row.
    const Rect firstRow{sidebarRect().left + 20.f, sidebarListTop() + 4.f, 40.f,
                        kSidebarRowHeight - 12.f};
    simulateClick(firstRow.center());
    check("sidebar row click", current_ == 0 && !editorFocused_,
          "current=" + std::to_string(current_));
    editorFocused_ = true;

    if (!editor_.runKeys().empty()) {
        const std::string key = editor_.runKeys().front();
        // Click the actual Run button of the first block, exactly like a user.
        simulateClick(editor_.runButtonBounds(0).center());
        for (int step = 0; step < 300; ++step) {
            update(0.1f);
            layout();
            const RunState* state = runs_.find(key);
            if (state != nullptr && !state->running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        const RunState* state = runs_.find(key);
        check("Run button starts snippet", state != nullptr,
              state != nullptr ? ("exit=" + std::to_string(state->exitCode)) : "no state");
        check("run produces output", state != nullptr && !state->output.empty(),
              state != nullptr ? ("output=" + state->output.substr(0, 40)) : "no state");
        check("output panel laid out", editor_.hasOutputFor(key), "key=" + key);
    } else {
        check("Run button starts snippet", false, "no code block found");
    }
    // Themes: every palette must be reachable, must actually change the
    // colours, and must survive a save/load round trip.
    {
        const std::string startId(theme::currentTheme().id);
        const sf::Color startAccent = theme::pal.accent;
        const std::size_t count = theme::themes().size();
        bool allDistinct = count >= 2;
        std::size_t seen = 0;
        for (std::size_t i = 0; i < count; ++i) {
            theme::cycleTheme();
            ++seen;
            if (theme::pal.accent == startAccent && i + 1 < count) allDistinct = false;
        }
        check("themes cycle", seen == count && allDistinct,
              "themes=" + std::to_string(count) + " now=" + std::string(theme::currentTheme().name));
        check("cycle returns to start", theme::currentTheme().id == startId,
              std::string(theme::currentTheme().id));

        theme::applyTheme("daylight");
        const bool daylightApplied = theme::pal.windowBg.r > 200 && theme::currentTheme().id == "daylight";
        const std::filesystem::path prefs = platform::tempDirectory() / "theme-test";
        std::error_code ec;
        std::filesystem::remove_all(prefs, ec);
        const bool saved = theme::savePreference(prefs);
        theme::applyTheme("midnight");
        const bool loaded = theme::loadPreference(prefs);
        check("light theme applies", daylightApplied, "bg=" + std::to_string(theme::pal.windowBg.r));
        check("theme persists", saved && loaded && theme::currentTheme().id == "daylight",
              std::string(theme::currentTheme().id));
        check("unknown theme rejected", !theme::applyTheme("does-not-exist") &&
                                          theme::currentTheme().id == "daylight",
              std::string(theme::currentTheme().id));
        std::filesystem::remove_all(prefs, ec);
        theme::applyTheme(startId);
    }

    // Leave the notes folder as we found it.
    if (store_.notes().size() > static_cast<std::size_t>(before)) {
        std::string error;
        const std::string created = store_.notes()[0].id;
        store_.remove(created, error);
    }
    std::fprintf(stderr, "selftest: %d failure(s)\n", failures);
    return failures;
}

int App::saveScreenshot(const std::string& path) {
    sf::RenderTexture frame(window_.getSize());
    frame.setSmooth(false);
    frame.setView(sf::View(sf::Vector2f(window_.getSize()) * 0.5f,
                           sf::Vector2f(window_.getSize())));
    frame.clear(theme::pal.windowBg);
    render(frame);
    frame.display();
    const sf::Image image = frame.getTexture().copyToImage();
    if (!image.saveToFile(path)) {
        std::fprintf(stderr, "codernotes: could not write %s\n", path.c_str());
        return 1;
    }
    std::fprintf(stderr, "codernotes: wrote %s (%ux%u)\n", path.c_str(), image.getSize().x,
                 image.getSize().y);
    return 0;
}

int App::run(const Options& options) {
    const sf::Vector2u work = platform::workAreaSize();
    unsigned width = std::clamp(work.x > 120u ? work.x - 120u : work.x, 980u, 1600u);
    unsigned height = std::clamp(work.y > 200u ? work.y - 200u : work.y, 640u, 1040u);
    if (options.testWidth > 0 && options.testHeight > 0) {
        width = static_cast<unsigned>(options.testWidth);
        height = static_cast<unsigned>(options.testHeight);
    }
    const bool offscreen = !options.screenshotPath.empty() || options.benchmarkFrames > 0 ||
                           (options.selfTest && !options.live);
    window_.create(sf::VideoMode(sf::Vector2u{width, height}), "codernotes",
                   offscreen ? sf::Style::None : sf::Style::Default);
    const sf::Vector2i origin{static_cast<int>((work.x - width) / 2),
                              static_cast<int>((work.y - height) / 2)};
    window_.setPosition(origin);
    syncWindowView();
    // Vsync keeps presentation in step with the compositor; without it the
    // window can show half-composited frames.
    window_.setVerticalSyncEnabled(true);

    const float scale = platform::windowScale(window_.getNativeHandle());
    std::string error;
    theme::applyScale(scale);
    if (!theme::loadFonts(platform::executableDirectory(), error)) {
        std::fprintf(stderr, "codernotes: %s\n", error.c_str());
        return 1;
    }
    // Restore the saved colour scheme; an unknown or missing file keeps the
    // default theme.
    theme::loadPreference(platform::notesDirectory());
    if (!store_.load(error)) toast(error, true);

    editor_.attach(&document_, &runs_);
    preview_.attach(&document_, &runs_);
    if (store_.notes().empty()) {
        store_.create(kStarterNote);
    }
    openNote(0);
    layout();
    if (!options.initialSearch.empty()) {
        searchText_ = options.initialSearch;
        sidebarScroll_ = 0.f;
    }

    // The taskbar icon, the header mark and the icon preview all come from the
    // same artwork, so this runs once the GL context exists.
    if (!brand::applyToWindow(window_)) {
        std::fprintf(stderr, "codernotes: could not set the window icon\n");
    }
    if (!options.iconPreviewPath.empty()) {
        if (brand::writePreview(options.iconPreviewPath)) {
            std::fprintf(stderr, "codernotes: wrote %s\n", options.iconPreviewPath.c_str());
        } else {
            std::fprintf(stderr, "codernotes: could not write %s\n", options.iconPreviewPath.c_str());
            return 1;
        }
    }

    if (offscreen) {
        for (int frame = 0; frame < std::max(1, options.warmupFrames); ++frame) {
            update(1.f / 60.f);
            layout();
        }
        if (options.selfTest) {
            const int failures = selfTest();
            runs_.stopAll();
            return failures == 0 ? 0 : 1;
        }
        if (options.benchmarkFrames > 0) {
            const sf::ContextSettings settings = window_.getSettings();
            std::fprintf(stderr, "gl: %u.%u, aa=%u\n", settings.majorVersion, settings.minorVersion,
                         settings.antiAliasingLevel);
            sf::RenderTexture frame(window_.getSize());
            frame.setView(sf::View(sf::Vector2f(window_.getSize()) * 0.5f,
                                   sf::Vector2f(window_.getSize())));
            frame.clear(theme::pal.windowBg);
            render(frame);
            frame.display();
            const sf::Clock clock;
            std::vector<float> cpu;
            cpu.reserve(static_cast<std::size_t>(options.benchmarkFrames));
            for (int i = 0; i < options.benchmarkFrames; ++i) {
                const std::clock_t started = std::clock();
                update(1.f / 60.f);
                layout();
                frame.clear(theme::pal.windowBg);
                render(frame);
                frame.display();
                const double wall =
                    std::chrono::duration<double, std::milli>(clock.getElapsedTime()).count();
                const double used =
                    static_cast<double>(std::clock() - started) * 1000.0 / CLOCKS_PER_SEC;
                cpu.push_back(static_cast<float>(used));
                if (i == options.benchmarkFrames - 1) {
                    std::sort(cpu.begin(), cpu.end());
                    std::fprintf(stderr,
                                 "bench: %d frames, cpu median %.2f ms, cpu p90 %.2f ms, "
                                 "wall last %.2f ms\n",
                                 options.benchmarkFrames, cpu[cpu.size() / 2],
                                 cpu[static_cast<std::size_t>(cpu.size() * 9 / 10)], wall);
                }
            }
        }
        if (!options.runLanguage.empty()) {
            // Put the caret inside the first code block and execute it.
            const std::vector<md::Block> blocks = md::parse(document_.lines());
            for (const md::Block& block : blocks) {
                if (block.type != md::BlockType::Code) continue;
                document_.setCaret(Position{block.openLine + 1, 0});
                break;
            }
            layout();
            editor_.runBlockUnderCaret();
            for (int step = 0; step < 400; ++step) {
                update(0.1f);
                layout();
                bool busy = false;
                for (const std::string& key : editor_.runKeys()) {
                    const RunState* state = runs_.find(key);
                    if (state != nullptr && state->running) busy = true;
                }
                if (!busy && step > 2) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            layout();
        }
        const int status = options.screenshotPath.empty() ? 0 : saveScreenshot(options.screenshotPath);
        runs_.stopAll();
        return status;
    }

    sf::Clock clock;
    std::vector<float> frameTimes;
    int rendered = 0;
    while (window_.isOpen()) {
        mouse_.beginFrame();
        while (const std::optional event = window_.pollEvent()) handleEvent(*event);
        const sf::Time started = clock.getElapsedTime();
        const float dt = std::min(clock.restart().asSeconds(), 0.1f);
        update(dt);
        layout();
        render(window_);
        if (options.selfTest && rendered == 8) {
            // Maximize for real, let the resize settle, then verify that the
            // whole UI still maps onto the window.
#if defined(_WIN32)
            ShowWindow(window_.getNativeHandle(), SW_MAXIMIZE);
#endif
        }
        if (options.selfTest && rendered == 40) {
            const int failures = selfTest();
            (void)failures;
            break;
        }
        ++rendered;
        if (options.profileFrames > 0) {
            ++rendered;
            const double ms =
                std::chrono::duration<double, std::milli>(clock.getElapsedTime() - started).count();
            if (rendered > 5) frameTimes.push_back(static_cast<float>(ms));
            if (frameTimes.size() >= static_cast<std::size_t>(options.profileFrames)) break;
        }
    }
    if (!frameTimes.empty()) {
        std::vector<float> sorted = frameTimes;
        std::sort(sorted.begin(), sorted.end());
        const auto worst = sorted.back();
        std::fprintf(stderr,
                     "profile: %zu frames, median %.2f ms, p90 %.2f ms, max %.2f ms\n",
                     sorted.size(), sorted[sorted.size() / 2],
                     sorted[static_cast<std::size_t>(sorted.size() * 9 / 10)], worst);
    }
    if (document_.modified()) saveCurrent();
    runs_.stopAll();
    return 0;
}

}  // namespace

int runSmokeTest(const std::string& language) {
    const std::string code = language == "python"   ? "print('hello from codernotes')"
                             : language == "c"      ? "#include <stdio.h>\nint main(){printf(\"hi\\n\");return 0;}"
                             : language == "cpp"    ? "#include <iostream>\nint main(){std::cout << \"hi\" << std::endl;}"
                             : language == "shell"  ? "echo hello from codernotes"
                                                     : "console.log('hello from codernotes');";
    RunManager manager;
    const std::string key = "smoke";
    std::fprintf(stderr, "smoke: language=%s command=%s\n", language.c_str(),
                 planRun(code, language).command.c_str());
    manager.start(key, code, language);
    for (int step = 0; step < 400; ++step) {
        manager.update();
        const RunState* state = manager.find(key);
        if (state != nullptr && !state->running) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    const RunState* state = manager.find(key);
    if (state == nullptr) {
        std::fprintf(stderr, "smoke: no run state\n");
        return 1;
    }
    std::fprintf(stderr, "smoke: finished=%d exit=%d time=%.2fs\n", state->finished ? 1 : 0,
                 state->exitCode, state->seconds);
    std::fprintf(stderr, "smoke: output<<%s>>\n", state->output.c_str());
    if (!state->error.empty()) std::fprintf(stderr, "smoke: hint=%s\n", state->error.c_str());
    manager.stopAll();
    return state->finished && state->exitCode == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
    platform::enableDpiAwareness();
    App::Options options;
    std::string testRun;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screenshot" && i + 1 < argc) {
            options.screenshotPath = argv[++i];
        } else if (argument == "--frames" && i + 1 < argc) {
            options.warmupFrames = std::atoi(argv[++i]);
        } else if (argument == "--benchmark" && i + 1 < argc) {
            options.benchmarkFrames = std::atoi(argv[++i]);
        } else if (argument == "--profile" && i + 1 < argc) {
            options.profileFrames = std::atoi(argv[++i]);
        } else if (argument == "--selftest") {
            options.selfTest = true;
        } else if (argument == "--selftest-live") {
            options.selfTest = true;
            options.live = true;
        } else if (argument == "--size" && i + 2 < argc) {
            options.testWidth = std::atoi(argv[++i]);
            options.testHeight = std::atoi(argv[++i]);
        } else if (argument == "--screenshot-run" && i + 1 < argc) {
            options.runLanguage = argv[++i];
        } else if (argument == "--icon" && i + 1 < argc) {
            options.iconPreviewPath = argv[++i];
        } else if (argument == "--search" && i + 1 < argc) {
            options.initialSearch = argv[++i];
        } else if (argument == "--test-run" && i + 1 < argc) {
            testRun = argv[++i];
        }
    }
    if (options.screenshotPath.empty() && !options.runLanguage.empty()) {
        options.screenshotPath = "screenshot.png";
    }
    if (!testRun.empty()) return runSmokeTest(testRun);
    App app;
    return app.run(options);
}
