#include "editor_view.h"

#include <algorithm>
#include <cmath>

#include <cstdio>

#include "clipboard.h"
#include "markdown.h"
#include "richtext.h"
#include "theme.h"
#include "util.h"

namespace {

namespace {

// Tabs have no glyph, so code lines are displayed with them expanded.
std::string expandTabs(std::string_view text, std::size_t tabStop = 4) {
    if (text.find('\t') == std::string_view::npos) return std::string(text);
    std::string out;
    out.reserve(text.size() + 4);
    std::size_t column = 0;
    for (char c : text) {
        if (c == '\t') {
            const std::size_t count = tabStop - (column % tabStop);
            out.append(count, ' ');
            column += count;
        } else {
            out += c;
            ++column;
        }
    }
    return out;
}

}  // namespace

sf::Color colorFor(syntax::Token token) {
    const theme::Palette& pal = theme::pal;
    switch (token) {
        case syntax::Token::Keyword: return pal.synKeyword;
        case syntax::Token::Type: return pal.synType;
        case syntax::Token::String: return pal.synString;
        case syntax::Token::Number: return pal.synNumber;
        case syntax::Token::Comment: return pal.synComment;
        case syntax::Token::Function: return pal.synFunction;
        case syntax::Token::Builtin: return pal.synBuiltin;
        case syntax::Token::Constant: return pal.synConstant;
        case syntax::Token::Operator: return pal.synOperator;
        case syntax::Token::Punctuation: return pal.synPunct;
        case syntax::Token::Tag: return pal.synTag;
        case syntax::Token::Attribute: return pal.synAttribute;
        case syntax::Token::Property: return pal.synProperty;
        case syntax::Token::Meta: return pal.synMeta;
        case syntax::Token::Removed: return pal.error;
        default: return pal.synPlain;
    }
}

std::size_t signatureFor(const RunManager* runs, const std::vector<std::string>& keys) {
    std::size_t signature = 0;
    for (const std::string& key : keys) {
        const RunState* state = runs != nullptr ? runs->find(key) : nullptr;
        if (state == nullptr) continue;
        signature = signature * 31u + key.size() + 1u;
        signature += state->output.size() + (state->running ? 7u : 0u) +
                     static_cast<std::size_t>(state->exitCode + 2);
    }
    return signature;
}

}  // namespace

void EditorView::attach(Document* document, RunManager* runs) {
    document_ = document;
    runs_ = runs;
    layoutVersion_ = 0xFFFFFFFFu;
    layoutBounds_ = Rect();
}

void EditorView::setBounds(const Rect& bounds) {
    if (bounds_ == bounds) return;  // relayouts are expensive, only redo them on change
    bounds_ = bounds;
    if (bounds_.width <= 0.f || bounds_.height <= 0.f) return;
    rebuildLayout();
    scroll_ = std::clamp(scroll_, 0.f, maxScroll());
}

bool EditorView::layoutStale() const {
    if (document_ == nullptr) return false;
    if (slots_.size() != static_cast<std::size_t>(document_->lineCount())) return true;
    if (layoutVersion_ != document_->version()) return true;
    if (layoutBounds_ != bounds_) return true;
    return signatureFor(runs_, runKeys()) != runSignature_;
}

float EditorView::maxScroll() const {
    return std::max(0.f, contentHeight_ - bounds_.height);
}

int EditorView::lineAt(float y) const {
    if (slots_.empty()) return 0;
    const float target = y + scroll_;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (target < slots_[i].y + slots_[i].height) return static_cast<int>(i);
    }
    return static_cast<int>(slots_.size()) - 1;
}

float EditorView::lineTextX(int line) const {
    if (slots_.empty()) return bounds_.left + theme::metrics.gutterWidth;
    const int clamped = std::clamp(line, 0, static_cast<int>(slots_.size()) - 1);
    const LineSlot& slot = slots_[static_cast<std::size_t>(clamped)];
    return bounds_.left + theme::metrics.gutterWidth +
           (slot.block >= 0 ? theme::metrics.codePadX : 0.f);
}

float EditorView::columnX(int line, int column) const {
    if (document_ == nullptr || slots_.empty()) return 0.f;
    const int clamped = std::clamp(line, 0, static_cast<int>(slots_.size()) - 1);
    const std::string& text = displayLine(clamped);
    column = std::clamp(column, 0, static_cast<int>(text.size()));
    const LineSlot& slot = slots_[static_cast<std::size_t>(clamped)];
    const sf::Font& font = slot.block >= 0 ? theme::monoFont : theme::uiFont;
    const unsigned int size =
        static_cast<unsigned int>(slot.block >= 0 ? theme::metrics.codeText : theme::metrics.uiText);
    const float prefix =
        draw::textWidth(font, std::string_view(text).substr(0, static_cast<std::size_t>(column)), size);
    return lineTextX(clamped) + prefix;
}

Position EditorView::positionAt(const sf::Vector2f& point) const {
    if (document_ == nullptr || slots_.empty()) return Position{0, 0};
    const int line = lineAt(point.y);
    const std::string& text = displayLine(line);
    const LineSlot& slot = slots_[static_cast<std::size_t>(line)];
    const sf::Font& font = slot.block >= 0 ? theme::monoFont : theme::uiFont;
    const unsigned int size =
        static_cast<unsigned int>(slot.block >= 0 ? theme::metrics.codeText : theme::metrics.uiText);
    const float originX = lineTextX(line);
    const std::vector<util::Utf8Char> chars = util::decodeUtf8(text);
    int bestColumn = 0;
    float bestDistance = 1e9f;
    float cursorX = 0.f;
    int column = 0;
    for (std::size_t i = 0; i <= chars.size(); ++i) {
        const float distance = std::fabs(cursorX - (point.x - originX));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestColumn = column;
        }
        if (i == chars.size()) break;
        const float advance =
            font.hasGlyph(chars[i].codePoint) ? font.getGlyph(chars[i].codePoint, size, false).advance : 0.f;
        cursorX += advance;
        column += static_cast<int>(chars[i].bytes);
    }
    return Position{line, bestColumn};
}

EditContext EditorView::contextFor(int line) const {
    EditContext context;
    for (const BlockLayout& block : blocks_) {
        if (line == block.openLine || line == block.closeLine) {
            context.onFenceLine = true;
            return context;
        }
        if (line > block.openLine && line < block.closeLine) {
            context.inCodeBlock = true;
            return context;
        }
    }
    return context;
}

const EditorView::BlockLayout* EditorView::blockAtLine(int line) const {
    if (line < 0 || line >= static_cast<int>(slots_.size())) return nullptr;
    const int index = slots_[static_cast<std::size_t>(line)].block;
    if (index < 0 || index >= static_cast<int>(blocks_.size())) return nullptr;
    return &blocks_[static_cast<std::size_t>(index)];
}

std::string EditorView::codeOf(const BlockLayout& block) const {
    if (document_ == nullptr) return {};
    const int last = block.closeLine >= 0 ? block.closeLine : document_->lineCount();
    std::string code;
    for (int line = block.openLine + 1; line < last; ++line) {
        if (line > block.openLine + 1) code += '\n';
        code += document_->line(line);
    }
    return code;
}


std::string EditorView::languageAtCaret() const {
    const int index = caretBlockIndex();
    if (index < 0 || index >= static_cast<int>(blocks_.size())) return {};
    return blocks_[static_cast<std::size_t>(index)].displayName;
}

bool EditorView::hasOutputFor(const std::string& key) const {
    for (const BlockLayout& block : blocks_) {
        if (block.key == key) return block.hasOutput;
    }
    return false;
}

Rect EditorView::runButtonBounds(int blockIndex) const {
    if (blockIndex < 0 || blockIndex >= static_cast<int>(blocks_.size())) return {};
    const BlockLayout& block = blocks_[static_cast<std::size_t>(blockIndex)];
    const RunState* state = runs_ != nullptr ? runs_->find(block.key) : nullptr;
    const bool running = state != nullptr && state->running;
    return running ? block.stopButton.bounds : block.runButton.bounds;
}

void EditorView::revealOutput(const std::string& key) {
    for (const BlockLayout& block : blocks_) {
        if (block.key != key) continue;
        const float top = bounds_.top + theme::metrics.pad;
        const float bottom = bounds_.top + bounds_.height - theme::metrics.pad;
        if (block.top - scroll_ >= top && block.bottom - scroll_ <= bottom) return;
        if (block.bottom - scroll_ > bottom) {
            scroll_ = std::min(maxScroll(), block.bottom - bottom + theme::metrics.blockGap);
        } else {
            scroll_ = std::max(0.f, block.top - top - theme::metrics.pad);
        }
        scroll_ = std::clamp(scroll_, 0.f, maxScroll());
        return;
    }
}

int EditorView::caretBlockIndex() const {
    if (document_ == nullptr || slots_.empty()) return -1;
    const int line = document_->caret().line;
    if (line < 0 || line >= static_cast<int>(slots_.size())) return -1;
    return slots_[static_cast<std::size_t>(line)].block;
}

bool EditorView::runBlockUnderCaret() {
    const int index = caretBlockIndex();
    if (index < 0 || index >= static_cast<int>(blocks_.size()) || runs_ == nullptr) return false;
    const BlockLayout& block = blocks_[static_cast<std::size_t>(index)];
    runs_->start(block.key, codeOf(block), block.rawLang);
    return true;
}

bool EditorView::mouseOverHeader(const sf::Vector2f& point) const {
    for (const BlockLayout& block : blocks_) {
        if (block.headerRect.contains(point)) return true;
    }
    return false;
}

void EditorView::resetScroll() {
    scroll_ = 0.f;
}

void EditorView::onWheel(float delta) {
    scroll_ = std::clamp(scroll_ - delta * theme::metrics.lineHeight * 3.f, 0.f, maxScroll());
}

void EditorView::scrollCaretIntoView() {
    if (document_ == nullptr || slots_.empty()) return;
    const Position caret = document_->caret();
    if (caret.line < 0 || caret.line >= static_cast<int>(slots_.size())) return;
    const LineSlot& slot = slots_[static_cast<std::size_t>(caret.line)];
    const float top = bounds_.top + theme::metrics.pad;
    const float bottom = bounds_.top + bounds_.height - theme::metrics.pad;
    const float caretTop = slot.y - scroll_;
    if (caretTop < top) {
        scroll_ = std::max(0.f, slot.y - top);
    } else if (caretTop + slot.height > bottom) {
        scroll_ = std::min(maxScroll(), slot.y + slot.height - bottom);
    }
    scroll_ = std::clamp(scroll_, 0.f, maxScroll());
}

void EditorView::rebuildLineStyles() {
    if (document_ == nullptr) return;
    lineStyles_.assign(static_cast<std::size_t>(document_->lineCount()), {});
    richtext::Style style;
    style.normal = theme::pal.text;
    style.emphasis = theme::pal.magenta;
    style.italic = theme::pal.textDim;
    style.code = theme::pal.synString;
    style.link = theme::pal.accent;
    style.marker = theme::pal.textFaint;
    style.codeBackground = sf::Color(0, 0, 0, 0);
    for (std::size_t i = 0; i < lineStyles_.size(); ++i) {
        lineStyles_[i] = richtext::split(document_->line(static_cast<int>(i)), style);
    }
    styleVersion_ = document_->version();
}

const std::vector<richtext::Piece>& EditorView::lineStyle(int line) {
    static const std::vector<richtext::Piece> empty;
    if (document_ == nullptr) return empty;
    // Rebuild first: the cache starts out empty, so checking the bounds before
    // filling it would silently drop every line.
    if (styleVersion_ != document_->version() ||
        lineStyles_.size() != static_cast<std::size_t>(document_->lineCount())) {
        rebuildLineStyles();
    }
    if (line < 0 || line >= static_cast<int>(lineStyles_.size())) return empty;
    return lineStyles_[static_cast<std::size_t>(line)];
}

const std::string& EditorView::displayLine(int line) const {
    const BlockLayout* block = blockAtLine(line);
    if (block != nullptr && line > block->openLine) {
        const std::size_t index = static_cast<std::size_t>(line - block->openLine - 1);
        if (index < block->displayLines.size()) return block->displayLines[index];
    }
    static std::string plain;
    plain = document_ != nullptr ? document_->line(line) : std::string();
    return plain;
}

void EditorView::rebuildLayout() {
    if (document_ == nullptr) return;
    const theme::Metrics& m = theme::metrics;
    const std::vector<std::string>& lines = document_->lines();
    const std::vector<md::Fence> fences = md::scanFences(lines);

    slots_.assign(lines.size(), LineSlot{});
    blocks_.clear();
    keys_.clear();

    float y = bounds_.top + m.pad;
    std::size_t fenceIndex = 0;
    int openBlock = -1;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const int lineIndex = static_cast<int>(i);
        const md::Fence* fence = fenceIndex < fences.size() && fences[fenceIndex].openLine == lineIndex
                                     ? &fences[fenceIndex]
                                     : nullptr;
        if (fence != nullptr) {
            BlockLayout block;
            block.openLine = fence->openLine;
            block.closeLine = fence->closeLine;
            block.rawLang = fence->info;
            block.lang = syntax::normalizeLanguage(fence->info);
            block.displayName = syntax::displayName(fence->info);
            block.key = "editor:" + std::to_string(fence->openLine) + ":" + block.lang;
            keys_.push_back(block.key);
            block.top = y;
            slots_[i] = LineSlot{y, m.codeHeaderHeight, static_cast<int>(blocks_.size()), true, false};
            y += m.codeHeaderHeight;
            block.bodyTop = y;
            openBlock = static_cast<int>(blocks_.size());
            blocks_.push_back(std::move(block));
            ++fenceIndex;
            continue;
        }
        if (openBlock >= 0) {
            BlockLayout& block = blocks_[static_cast<std::size_t>(openBlock)];
            if (block.closeLine == lineIndex) {
                slots_[i] = LineSlot{y, m.codePadY * 2.f, openBlock, false, true};
                block.bodyBottom = y;
                y += m.codePadY * 2.f;
                block.bottom = y + m.codePadY + m.blockGap;
                y += m.codePadY + m.blockGap;
                openBlock = -1;
                continue;
            }
            slots_[i] = LineSlot{y, m.codeLineHeight, openBlock, false, false};
            y += m.codeLineHeight;
            continue;
        }
        slots_[i] = LineSlot{y, m.lineHeight, -1, false, false};
        y += m.lineHeight;
    }
    if (openBlock >= 0) {
        BlockLayout& block = blocks_[static_cast<std::size_t>(openBlock)];
        block.bodyBottom = y;
        block.bottom = y + m.codePadY + m.blockGap;
    }

    const float left = bounds_.left + m.gutterWidth;
    const float right = bounds_.left + bounds_.width - 10.f;
    for (BlockLayout& block : blocks_) {
        const int first = block.openLine + 1;
        const int last = block.closeLine >= 0 ? block.closeLine : static_cast<int>(lines.size());
        // Highlight the text as it is displayed (tabs expanded) so the coloured
        // spans line up with the glyphs that get drawn.
        block.displayLines.clear();
        std::string code;
        for (int line = first; line < last; ++line) {
            if (line > first) code += '\n';
            block.displayLines.push_back(
                expandTabs(lines[static_cast<std::size_t>(line)]));
            code += block.displayLines.back();
        }
        const std::vector<syntax::Span> spans = syntax::highlight(code, block.lang);
        block.lineSpans.assign(block.displayLines.size(), {});
        std::size_t offset = 0;
        for (int line = first; line < last; ++line) {
            const std::string& text = block.displayLines[static_cast<std::size_t>(line - first)];
            std::vector<syntax::Span> lineSpans;
            for (const syntax::Span& span : spans) {
                if (span.start + span.length <= offset) continue;
                if (span.start >= offset + text.size()) break;
                const std::size_t begin = span.start > offset ? span.start - offset : 0;
                const std::size_t end =
                    std::min(span.start + span.length, offset + text.size()) - offset;
                if (end > begin) lineSpans.push_back({begin, end - begin, span.token});
            }
            block.lineSpans[static_cast<std::size_t>(line - first)] = std::move(lineSpans);
            offset += text.size() + 1;
        }

        block.headerRect = {left, block.top, right - left, m.codeHeaderHeight};
        block.bodyRect = {left, block.bodyTop, right - left, std::max(0.f, block.bodyBottom - block.bodyTop)};
        block.runButton.bounds = {right - 92.f, block.top + 3.f, 86.f, m.codeHeaderHeight - 6.f};
        block.runButton.label = "Run";
        block.runButton.iconName = "play";
        block.runButton.enabled = true;
        block.stopButton.bounds = block.runButton.bounds;
        block.stopButton.label = "Stop";
        block.stopButton.iconName = "stop";
        block.stopButton.enabled = false;

        const RunState* state = runs_ != nullptr ? runs_->find(block.key) : nullptr;
        if (state != nullptr && (!state->output.empty() || state->running || !state->error.empty())) {
            block.hasOutput = true;
            std::string text = state->output;
            if (state->running && !text.empty() && text.back() != '\n') text += '\n';
            block.outputLines = util::split(text, '\n');
            const std::size_t limit = 14;
            if (block.outputLines.size() > limit) {
                // Keep the tail, but say how much was dropped so long output
                // never looks like output that failed to appear.
                const std::size_t dropped = block.outputLines.size() - limit + 1;
                block.outputLines.erase(block.outputLines.begin(),
                                        block.outputLines.begin() + static_cast<long>(dropped));
                block.outputLines.insert(block.outputLines.begin(),
                                         "... " + std::to_string(dropped) + " earlier lines");
            }
            block.outputHeight = m.codePadY + static_cast<float>(block.outputLines.size() + 1) * m.codeLineHeight;
            block.outputTop = block.bottom - m.blockGap;
            block.outputRect = {left, block.outputTop, right - left, block.outputHeight};
            block.bottom = block.outputTop + block.outputHeight + m.codePadY;
        }
    }

    float maximum = bounds_.top + m.pad;
    for (const LineSlot& slot : slots_) maximum = std::max(maximum, slot.y + slot.height);
    if (!blocks_.empty()) maximum = std::max(maximum, blocks_.back().bottom + m.pad);
    contentHeight_ = maximum;
    layoutVersion_ = document_->version();
    layoutBounds_ = bounds_;
    runSignature_ = signatureFor(runs_, runKeys());
}

bool EditorView::textSelected() const {
    return document_ != nullptr && document_->caret() != document_->selectionStart();
}

void EditorView::update(const MouseState& mouse, float dt, bool focused) {
    (void)focused;
    time_ += dt;
    if (layoutStale()) rebuildLayout();
    if (document_ == nullptr) return;

    // The scrollbar reacts to hover anywhere in the right gutter of the pane.
    if (bounds_.contains(mouse.position)) {
        const float railLeft = bounds_.right() - theme::metrics.scrollbarWidth - 10.f;
        scrollHover_ = mouse.position.x >= railLeft;
    } else {
        scrollHover_ = false;
    }

    for (BlockLayout& block : blocks_) {
        const RunState* state = runs_ != nullptr ? runs_->find(block.key) : nullptr;
        const bool isRunning = state != nullptr && state->running;
        block.runButton.enabled = !isRunning;
        block.stopButton.enabled = isRunning;
        block.runButton.update(mouse, dt);
        block.stopButton.update(mouse, dt);
        if (block.runButton.clickedFlag && runs_ != nullptr) {
            runs_->start(block.key, codeOf(block), block.rawLang);
        }
        if (block.stopButton.clickedFlag && runs_ != nullptr) {
            runs_->stop(block.key);
        }
    }
}

void EditorView::onTextEntered(char32_t codePoint) {
    if (document_ == nullptr) return;
    if (codePoint == U'\r' || codePoint == U'\n') return;
    const EditContext context = contextFor(document_->caret().line);
    std::string text;
    util::appendUtf8(text, codePoint);
    document_->insertText(text, context);

    // Typing a fence on an empty line creates the matching closing fence.
    const int caretLine = document_->caret().line;
    const std::string trimmed = util::trim(document_->line(caretLine));
    if (trimmed == "```" || trimmed == "~~~") {
        const std::vector<md::Fence> fences = md::scanFences(document_->lines());
        bool partOfPair = false;
        for (const md::Fence& fence : fences) {
            if (fence.openLine == caretLine || fence.closeLine == caretLine) partOfPair = true;
        }
        if (!partOfPair) {
            document_->insertText("\n" + trimmed, EditContext{});
            document_->setCaret(Position{caretLine + 1, 0});
        }
    }
    scrollCaretIntoView();
}

void EditorView::onKeyPressed(sf::Keyboard::Key key, bool control, bool shift, bool alt) {
    if (document_ == nullptr) return;
    (void)alt;
    const int line = document_->caret().line;
    const EditContext context = contextFor(line);

    if (control) {
        switch (key) {
            case sf::Keyboard::Key::A: document_->selectAll(); return;
            case sf::Keyboard::Key::Z:
                if (shift) document_->redo();
                else document_->undo();
                scrollCaretIntoView();
                return;
            case sf::Keyboard::Key::Y: document_->redo(); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::C: {
                const BlockLayout* block = blockAtLine(line);
                clipboard::set(textSelected() ? document_->selectedText()
                                              : (block != nullptr ? codeOf(*block) : document_->line(line)));
                return;
            }
            case sf::Keyboard::Key::X:
                if (textSelected()) {
                    clipboard::set(document_->selectedText());
                    document_->removeForward(context);
                    scrollCaretIntoView();
                }
                return;
            case sf::Keyboard::Key::V:
                document_->insertText(clipboard::get(), context);
                scrollCaretIntoView();
                return;
            case sf::Keyboard::Key::D: {
                const std::string current = document_->line(line);
                document_->insertText(current, context);
                document_->insertNewline(context);
                scrollCaretIntoView();
                return;
            }
            case sf::Keyboard::Key::Slash: document_->toggleLineComment(context); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::B: document_->wrapSelection("**", "**"); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::I: document_->wrapSelection("*", "*"); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::Home: document_->moveToDocument(true, shift); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::End: document_->moveToDocument(false, shift); scrollCaretIntoView(); return;
            case sf::Keyboard::Key::Enter: runBlockUnderCaret(); return;
            default: return;
        }
    }

    if (key == sf::Keyboard::Key::F5) {
        runBlockUnderCaret();
        return;
    }

    switch (key) {
        case sf::Keyboard::Key::Left:
            if (shift) document_->moveWord(-1, true);
            else document_->moveCaret(0, -1, false);
            break;
        case sf::Keyboard::Key::Right:
            if (shift) document_->moveWord(1, true);
            else document_->moveCaret(0, 1, false);
            break;
        case sf::Keyboard::Key::Up: document_->moveCaret(-1, 0, shift); break;
        case sf::Keyboard::Key::Down: document_->moveCaret(1, 0, shift); break;
        case sf::Keyboard::Key::Home: document_->moveToLineEdge(true, shift); break;
        case sf::Keyboard::Key::End: document_->moveToLineEdge(false, shift); break;
        case sf::Keyboard::Key::PageUp: document_->moveCaret(-8, 0, shift); break;
        case sf::Keyboard::Key::PageDown: document_->moveCaret(8, 0, shift); break;
        case sf::Keyboard::Key::Backspace: document_->removeBackward(context); break;
        case sf::Keyboard::Key::Delete: document_->removeForward(context); break;
        case sf::Keyboard::Key::Enter: document_->insertNewline(context); break;
        case sf::Keyboard::Key::Tab:
            if (shift) document_->indentSelection(context, false);
            else document_->insertIndent(context);
            break;
        case sf::Keyboard::Key::Escape: document_->clearSelection(); break;
        default: return;
    }
    scrollCaretIntoView();
}

void EditorView::onMousePressed(const sf::Vector2f& position) {
    if (document_ == nullptr || slots_.empty()) return;
    if (!bounds_.contains(position)) return;
    if (mouseOverHeader(position)) {
        dragging_ = false;
        return;
    }
    const bool extend = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
                        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    document_->setCaret(positionAt(position), extend);
    dragging_ = true;
    scrollCaretIntoView();
}

void EditorView::onMouseDragged(const sf::Vector2f& position) {
    if (!dragging_ || document_ == nullptr) return;
    document_->setCaret(positionAt(position), true);
    scrollCaretIntoView();
}

void EditorView::onMouseReleased(const sf::Vector2f& position) {
    (void)position;
    dragging_ = false;
}

void EditorView::draw(sf::RenderTarget& target, bool focused) {
    if (document_ == nullptr || slots_.empty()) return;
    if (layoutStale()) rebuildLayout();

    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;

    draw::rect(target, bounds_, pal.editorBg);
    // A slightly different tone behind the line number column separates the
    // gutter from the text area without needing a hard border.
    draw::rect(target, {bounds_.left, bounds_.top, m.gutterWidth, bounds_.height}, pal.gutterBg);
    draw::vline(target, bounds_.left + m.gutterWidth, bounds_.top, bounds_.bottom(), pal.borderSoft);
    // Scrolling by moving the view keeps a 1:1 mapping: the view always covers
    // the whole framebuffer, only its centre shifts. Pending glyphs belong to
    // the old view, so they are submitted first.
    draw::endText();
    const sf::View previous = target.getView();
    const sf::Vector2f frame = previous.getSize();
    target.setView(
        sf::View(sf::Vector2f(frame.x * 0.5f, frame.y * 0.5f + scroll_), frame));
    // Glyphs are clipped to the pane, otherwise long lines bleed into the
    // neighbouring panel and the text there is hidden by it.
    draw::beginClip(target, bounds_);

    const int firstVisible = std::max(0, lineAt(bounds_.top + 1.f));
    const int lastVisible =
        std::min(static_cast<int>(slots_.size()) - 1, lineAt(bounds_.top + bounds_.height - 1.f));

    for (const BlockLayout& block : blocks_) {
        if (block.bottom - scroll_ < bounds_.top - 60.f ||
            block.top - scroll_ > bounds_.top + bounds_.height + 60.f) {
            continue;
        }
        const bool active =
            document_->caret().line >= block.openLine && document_->caret().line <= block.closeLine;
        const Rect outer = {block.headerRect.left, block.top, block.headerRect.width,
                            block.bottom - block.top};
        draw::shadowedPanel(target, outer, m.radius, active ? pal.codeBlockActive : pal.codeBlockBg,
                            active ? pal.borderStrong : pal.border);
        draw::vGradient(target, {outer.left + 1.f, block.top + 1.f, outer.width - 2.f, m.codeHeaderHeight},
                        pal.codeBlockHeader, active ? pal.hover : pal.codeBlockBg);
        draw::hline(target, block.top + m.codeHeaderHeight, outer.left + 1.f, outer.right() - 1.f,
                    pal.border);
        if (block.hasOutput) {
            draw::hline(target, block.outputTop, outer.left + 1.f, outer.right() - 1.f, pal.border);
            draw::rect(target, {outer.left + 1.f, block.outputTop, 2.f, block.outputHeight}, pal.accent);
        }

        const float chipX = block.headerRect.left + 12.f;
        const float chipY = block.headerRect.top + m.codeHeaderHeight * 0.5f;
        const std::string chipText = block.rawLang.empty() ? "code" : block.rawLang;
        const unsigned int chipSize = static_cast<unsigned int>(m.smallText - 1.f);
        const float chipW = draw::textWidth(theme::uiFont, chipText, chipSize) + 30.f;
        draw::roundedRect(target, {chipX, chipY - 10.f, chipW, 20.f}, 10.f, pal.accentSoft);
        draw::icon(target, "dot", {chipX + 12.f, chipY}, 11.f, pal.accent);
        draw::text(target, theme::uiFont, chipText, chipSize, pal.accent, {chipX + 20.f, chipY - 7.f}, true);

        const RunState* state = runs_ != nullptr ? runs_->find(block.key) : nullptr;
        if (state != nullptr) {
            std::string status;
            sf::Color statusColor = pal.textFaint;
            if (state->running) {
                status = "running " + ::formatSeconds(state->seconds);
                statusColor = pal.warning;
            } else if (state->exitCode == 0) {
                status = "exit 0  " + ::formatSeconds(state->seconds);
                statusColor = pal.success;
            } else {
                status = "exit " + std::to_string(state->exitCode);
                statusColor = pal.error;
            }
            const unsigned int statusSize = static_cast<unsigned int>(m.tinyText);
            const float statusWidth = draw::textWidth(theme::uiFont, status, statusSize);
            const Rect pill{block.runButton.bounds.left - 30.f - statusWidth, chipY - 9.f,
                            statusWidth + 18.f, 18.f};
            draw::roundedRect(target, pill, 9.f, pal.pill);
            draw::icon(target, "dot", {pill.left + 10.f, chipY}, 9.f, statusColor);
            draw::text(target, theme::uiFont, status, statusSize, statusColor, {pill.left + 18.f, chipY - 6.f});
        }
        if (state == nullptr || !state->running) {
            block.runButton.draw(target, theme::uiFont, 5.f, pal.codeBlockHeader, pal.accent,
                                 pal.accent, pal.accent, pal.onAccent);

        } else {
            block.stopButton.draw(target, theme::uiFont, 5.f, pal.error, pal.error, pal.error,
                                  pal.onAccent, pal.onAccent);

        }
    }

    const unsigned int uiSize = static_cast<unsigned int>(m.uiText);
    const unsigned int codeSize = static_cast<unsigned int>(m.codeText);
    const unsigned int gutterSize = static_cast<unsigned int>(m.smallText - 1.f);
    const Position caret = document_->caret();
    const Position selStart = document_->selectionStart();
    const Position selEnd = document_->selectionEnd();
    const bool hasSelection = caret != selStart;

    richtext::Style style;
    style.normal = pal.text;
    style.emphasis = pal.magenta;
    style.italic = pal.textDim;
    style.code = pal.synString;
    style.link = pal.accent;
    style.marker = pal.textFaint;
    style.codeBackground = sf::Color(0, 0, 0, 0);

    for (int index = firstVisible; index <= lastVisible; ++index) {
        const LineSlot& slot = slots_[static_cast<std::size_t>(index)];
        const std::string& lineText = displayLine(index);
        const float lineTop = slot.y;
        const float slotHeight = slot.height;

        if (index == caret.line) {
            draw::rect(target, {bounds_.left + 2.f, lineTop, bounds_.width - 4.f, slotHeight},
                       pal.caretLine);
        }

        if (hasSelection) {
            const int from = std::min(index, selStart.line);
            const int to = std::max(index, selEnd.line);
            if (from <= to) {
                const int startColumn = index <= selStart.line ? selStart.column : 0;
                const int endColumn =
                    index >= selEnd.line ? selEnd.column : static_cast<int>(lineText.size());
                const float x0 = columnX(index, startColumn);
                const float x1 = columnX(index, endColumn);
                draw::rect(target, {x0, lineTop, std::max(1.f, x1 - x0), slotHeight},
                           focused ? pal.selection : pal.selectionInactive);
            }
        }

        const std::string number = std::to_string(index + 1);
        draw::text(target, theme::uiFont, number, gutterSize, pal.textFaint,
                   {bounds_.left + m.gutterWidth - 12.f - draw::textWidth(theme::uiFont, number, gutterSize),
                    lineTop + 4.f});

        const float textSize = static_cast<float>(slot.block >= 0 ? codeSize : uiSize);
        const float baseline = lineTop + (slotHeight - textSize) * 0.5f - 1.f;

        if (slot.fenceOpen) {
            // The raw fence text only shows while the caret is on that line,
            // otherwise the header chip represents it.
            if (caret.line == index) {
                draw::text(target, theme::monoFont, lineText, codeSize, pal.synMeta,
                           {lineTextX(index), baseline});
            }
            if (caret.line == index && focused) {
                draw::rect(target, {columnX(index, caret.column), lineTop + 3.f, 2.f, slotHeight - 6.f},
                           pal.caret);
            }
            continue;
        }
        if (slot.fenceClose) {
            if (caret.line == index && focused) {
                draw::rect(target, {columnX(index, caret.column), lineTop + 2.f, 2.f, slotHeight - 4.f},
                           pal.caret);
            }
            continue;
        }

        if (slot.block >= 0) {
            const BlockLayout& block = blocks_[static_cast<std::size_t>(slot.block)];
            const float x = lineTextX(index);
            // Indent guides: one faint rule per 4 spaces of leading whitespace.
            const std::string_view codeView(lineText);
            const std::size_t codeIndent = codeView.find_first_not_of(" ");
            if (m.guideStep > 0.f && codeIndent != std::string_view::npos && codeIndent >= 4) {
                const int guides = static_cast<int>(codeIndent / 4);
                for (int g = 1; g <= guides; ++g) {
                    draw::rect(target,
                               {x + m.guideStep * 4.f * g - m.guideStep * 0.5f, lineTop + 2.f, 1.f,
                                std::max(0.f, slotHeight - 5.f)},
                               pal.guide);
                }
            }
            draw::text(target, theme::monoFont, lineText, codeSize, pal.synPlain, {x, baseline});
            const int bodyIndex = index - block.openLine - 1;
            if (bodyIndex >= 0 && bodyIndex < static_cast<int>(block.lineSpans.size())) {
                for (const syntax::Span& span : block.lineSpans[static_cast<std::size_t>(bodyIndex)]) {
                    if (span.start >= lineText.size()) break;
                    const std::string piece = lineText.substr(span.start, span.length);
                    const float offset = draw::textWidth(
                        theme::monoFont, std::string_view(lineText).substr(0, span.start), codeSize);
                    draw::text(target, theme::monoFont, piece, codeSize, colorFor(span.token),
                               {x + offset, baseline});
                }
            }
            if (caret.line == index && focused) {
                draw::rect(target, {columnX(index, caret.column), lineTop + 2.f, 2.f, slotHeight - 4.f},
                           pal.caret);
            }
            continue;
        }

        // Markdown source line. Indentation is kept everywhere: it is part of
        // what the user typed, so only the marker glyphs get a colour.
        const std::string_view view(lineText);
        const std::size_t indent = view.find_first_not_of(" \t");
        const std::string_view body = indent == std::string_view::npos ? std::string_view()
                                                                      : view.substr(indent);
        const float x = lineTextX(index);
        if (!body.empty() && body[0] == '#' && md::isHeadingStart(std::string(body))) {
            const std::size_t space = body.find(' ');
            draw::text(target, theme::uiFont, body.substr(0, space == std::string_view::npos
                                                                ? body.size()
                                                                : space + 1),
                       uiSize, pal.accent, {x, baseline}, true);
            if (space != std::string_view::npos) {
                draw::text(target, theme::uiFont, body.substr(space + 1), uiSize, pal.heading,
                           {x + draw::textWidth(theme::uiFont,
                                                std::string(body.substr(0, space + 1)), uiSize, true),
                            baseline},
                           true);
            }
        } else if (body.size() > 1 && (body[0] == '-' || body[0] == '*' || body[0] == '+') &&
                   body[1] == ' ') {
            draw::text(target, theme::uiFont, body.substr(0, 2), uiSize, pal.accent, {x, baseline});
            richtext::draw(target, theme::uiFont, richtext::split(std::string(body.substr(2)), style),
                           uiSize, {x + draw::textWidth(theme::uiFont, std::string(body.substr(0, 2)), uiSize), baseline},
                           style);
        } else if (!body.empty() && body[0] == '>') {
            draw::text(target, theme::uiFont, ">", uiSize, pal.warning, {x, baseline});
            draw::text(target, theme::uiFont, body.substr(1), uiSize, pal.synComment,
                       {x + draw::textWidth(theme::uiFont, ">", uiSize), baseline});
        } else if (!body.empty() && (body[0] == '-' || body[0] == '*' || body[0] == '_') &&
                   body.find_first_not_of(std::string(1, body[0])) == std::string_view::npos) {
            draw::text(target, theme::uiFont, body, uiSize, pal.synComment, {x, baseline});
        } else {
            richtext::draw(target, theme::uiFont, lineStyle(index), uiSize, {x, baseline}, style);
        }

        if (caret.line == index && focused) {
            draw::rect(target, {columnX(index, caret.column), lineTop + 2.f, 2.f, slotHeight - 4.f}, pal.caret);
        }
    }

    for (const BlockLayout& block : blocks_) {
        if (!block.hasOutput) continue;
        if (block.outputRect.top - scroll_ > bounds_.top + bounds_.height ||
            block.outputRect.top + block.outputRect.height - scroll_ < bounds_.top) {
            continue;
        }
        const RunState* state = runs_ != nullptr ? runs_->find(block.key) : nullptr;
        if (state == nullptr) continue;
        const unsigned int size = static_cast<unsigned int>(m.codeText - 2.f);
        float y = block.outputRect.top + m.codePadY * 0.4f;
        for (const std::string& outputLine : block.outputLines) {
            if (y + m.codeLineHeight > block.outputRect.top + block.outputRect.height - m.codePadY) break;
            draw::text(target, theme::monoFont, outputLine, size, pal.synPlain,
                       {block.outputRect.left + 12.f, y});
            y += m.codeLineHeight;
        }
        if (!state->error.empty() && y + m.codeLineHeight <= block.outputRect.top + block.outputRect.height) {
            draw::text(target, theme::monoFont, state->error, size, pal.error,
                       {block.outputRect.left + 12.f, y});
        }
    }

    draw::endClip();
    draw::scrollbar(target,
                    {bounds_.left + 4.f, bounds_.top + 4.f, bounds_.width - 8.f, bounds_.height - 8.f},
                    scroll_, contentHeight_, bounds_.height, scrollHover_, false);
    target.setView(previous);
}
