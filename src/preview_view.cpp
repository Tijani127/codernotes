#include "preview_view.h"

#include <algorithm>

#include "platform.h"
#include "theme.h"
#include "util.h"

namespace {

// Tabs have no glyph, so they are expanded for display.
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
        if (state->running) {
            signature += static_cast<std::size_t>(state->seconds * 10.0);
        }
    }
    return signature;
}

unsigned int headingSize(int level, float base) {
    switch (level) {
        case 1: return static_cast<unsigned int>(base + 7.f);
        case 2: return static_cast<unsigned int>(base + 4.f);
        case 3: return static_cast<unsigned int>(base + 1.f);
        default: return static_cast<unsigned int>(base);
    }
}

richtext::Style previewStyle() {
    const theme::Palette& pal = theme::pal;
    richtext::Style style;
    style.normal = pal.text;
    style.emphasis = pal.heading;
    style.italic = pal.textDim;
    style.code = pal.synString;
    style.link = pal.accent;
    style.marker = pal.textFaint;
    style.codeBackground = pal.codeChipBg;
    return style;
}

}  // namespace

void PreviewView::attach(Document* document, RunManager* runs) {
    document_ = document;
    runs_ = runs;
    layoutVersion_ = 0xFFFFFFFFu;
}

void PreviewView::setBounds(const Rect& bounds) {
    if (bounds_ == bounds) return;  // rebuilding re-parses and re-highlights everything
    bounds_ = bounds;
    if (bounds_.width <= 0.f || bounds_.height <= 0.f) return;
    rebuild();
    scroll_ = std::clamp(scroll_, 0.f, maxScroll());
}

bool PreviewView::stale() const {
    if (document_ == nullptr) return false;
    if (layoutVersion_ != document_->version()) return true;
    if (layoutBounds_ != bounds_) return true;
    return signatureFor(runs_, keys_) != runSignature_;
}

float PreviewView::maxScroll() const {
    return std::max(0.f, contentHeight_ - bounds_.height);
}

void PreviewView::resetScroll() {
    scroll_ = 0.f;
}

void PreviewView::onWheel(float delta) {
    scroll_ = std::clamp(scroll_ - delta * theme::metrics.lineHeight * 3.f, 0.f, maxScroll());
}

std::string PreviewView::codeOf(const md::Block& block) const {
    return util::join(block.code, "\n");
}

void PreviewView::onMousePressed(const sf::Vector2f& position) {
    (void)position;
}

void PreviewView::onMouseDragged(const sf::Vector2f& position) {
    (void)position;
}

void PreviewView::onMouseReleased(const sf::Vector2f& position) {
    if (!bounds_.contains(position)) return;
    for (const BlockItem& item : items_) {
        if (item.runButton.bounds.contains(position) || item.stopButton.bounds.contains(position)) return;
    }
    const float y = position.y + scroll_;
    for (const BlockItem& item : items_) {
        for (const LinkRegion& link : item.links) {
            if (y < link.y || y > link.y + link.height) continue;
            if (position.x < link.x0 || position.x > link.x1) continue;
            if (!link.url.empty()) platform::openExternal(link.url);
            return;
        }
    }
}

void PreviewView::rebuild() {
    if (document_ == nullptr) return;
    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;
    const std::vector<md::Block> blocks = md::parse(document_->lines());
    const float left = bounds_.left + 22.f;
    const float width = std::max(120.f, bounds_.width - 44.f);

    items_.clear();
    keys_.clear();

    float y = bounds_.top + m.pad;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const md::Block& block = blocks[index];
        BlockItem item;
        item.blockIndex = static_cast<int>(index);
        item.type = block.type;
        item.textColor = pal.text;
        item.markerColor = pal.accent;

        if (block.type == md::BlockType::Code) {
            item.rawLang = block.lang;
            item.displayName = syntax::displayName(block.lang);
            item.key = "preview:" + std::to_string(block.openLine) + ":" +
                       syntax::normalizeLanguage(block.lang);
            keys_.push_back(item.key);
            item.code = util::join(block.code, "\n");
            item.codeRows.clear();
            for (const std::string& row : block.code) item.codeRows.push_back(expandTabs(row));
            item.spans = syntax::highlight(item.code, item.rawLang);
            float bodyTop = y + m.codeHeaderHeight;
            for (std::size_t line = 0; line < block.code.size(); ++line) {
                item.codeLines.push_back(LineItem{bodyTop, m.codeLineHeight});
                bodyTop += m.codeLineHeight;
            }
            float bodyBottom = bodyTop + m.codePadY * 0.5f;
            const RunState* state = runs_ != nullptr ? runs_->find(item.key) : nullptr;
            if (state != nullptr && (!state->output.empty() || state->running || !state->error.empty())) {
                item.hasOutput = true;
                std::string text = state->output;
                if (state->running && !text.empty() && text.back() != '\n') text += '\n';
                item.outputRows = util::split(text, '\n');
                const std::size_t limit = 10;
                if (item.outputRows.size() > limit) {
                    item.outputRows.erase(item.outputRows.begin(),
                                          item.outputRows.end() - static_cast<long>(limit));
                }
                float outputTop = bodyBottom;
                for (std::size_t row = 0; row < item.outputRows.size() + 1; ++row) {
                    item.outputLines.push_back(LineItem{outputTop, m.codeLineHeight});
                    outputTop += m.codeLineHeight;
                }
                bodyBottom = outputTop + m.codePadY * 0.5f;
                item.errorText = state->error;
                if (state->running) {
                    item.statusText = "running " + ::formatSeconds(state->seconds);
                    item.statusColor = pal.warning;
                } else if (state->exitCode == 0) {
                    item.statusText = "exit 0   " + ::formatSeconds(state->seconds);
                    item.statusColor = pal.success;
                } else {
                    item.statusText = "exit " + std::to_string(state->exitCode);
                    item.statusColor = pal.error;
                }
            }
            item.bounds = {left, y, width, bodyBottom - y};
            item.headerRect = {item.bounds.left, y, item.bounds.width, m.codeHeaderHeight};
            item.runButton.bounds = {item.bounds.left + item.bounds.width - 92.f, y + 3.f, 86.f,
                                     m.codeHeaderHeight - 6.f};
            item.runButton.label = "Run";
        item.runButton.iconName = "play";
            item.runButton.enabled = true;
            item.stopButton.bounds = item.runButton.bounds;
            item.stopButton.label = "Stop";
        item.stopButton.iconName = "stop";
            item.stopButton.enabled = false;
            y = bodyBottom + m.blockGap;
            items_.push_back(std::move(item));
            continue;
        }

        if (block.type == md::BlockType::Rule) {
            item.textLines.push_back(LineItem{y + m.smallText * 0.4f, 1.f});
            y += m.smallText * 0.9f;
            items_.push_back(std::move(item));
            continue;
        }

        const bool heading = block.type == md::BlockType::Heading;
        const bool quote = block.type == md::BlockType::Quote;
        const bool listItem = block.type == md::BlockType::ListItem;
        item.indent = (quote ? 16.f : 0.f) +
                      (listItem ? 22.f + static_cast<float>(block.indent) * 0.4f : 0.f);
        const unsigned int size = heading ? headingSize(block.level, m.uiText + 1.f)
                                          : static_cast<unsigned int>(m.uiText);
        const float lineHeight = m.lineHeight * (heading ? 1.14f : 1.f);
        item.textSize = size;
        if (heading) {
            item.textColor = pal.heading;
            item.bold = true;
            item.underline = block.level <= 2;
        } else if (quote) {
            item.textColor = pal.synComment;
        }
        if (listItem) {
            item.marker = block.ordered ? "1." : "\xE2\x80\xA2";  // bullet
            item.markerColor = pal.accent;
        }

        const std::vector<std::string> wrapped = richtext::wrap(
            expandTabs(block.text), theme::uiFont, size, std::max(60.f, width - item.indent));
        for (std::size_t w = 0; w < wrapped.size(); ++w) {
            item.textLines.push_back(LineItem{y, lineHeight});
            item.pieces.push_back(richtext::split(wrapped[w], previewStyle(), false));
            float offset = 0.f;
            for (const richtext::Piece& piece : item.pieces.back()) {
                const float pieceWidth = draw::textWidth(theme::uiFont, piece.text, size, piece.bold);
                if (!piece.link.empty()) {
                    LinkRegion region;
                    region.y = y;
                    region.height = lineHeight;
                    region.x0 = left + item.indent + offset;
                    region.x1 = region.x0 + pieceWidth;
                    region.url = piece.link;
                    item.links.push_back(std::move(region));
                }
                offset += pieceWidth;
            }
            y += lineHeight;
        }
        if (heading) y += 2.f;
        y += m.smallText * 0.5f;
        items_.push_back(std::move(item));
    }

    contentHeight_ = std::max(y, bounds_.top + m.pad);
    layoutVersion_ = document_->version();
    layoutBounds_ = bounds_;
    runSignature_ = signatureFor(runs_, keys_);
}

void PreviewView::update(const MouseState& mouse, float dt) {
    if (stale()) rebuild();
    if (bounds_.contains(mouse.position)) {
        const float railLeft = bounds_.right() - theme::metrics.scrollbarWidth - 10.f;
        scrollHover_ = mouse.position.x >= railLeft;
    } else {
        scrollHover_ = false;
    }
    for (BlockItem& item : items_) {
        if (item.type != md::BlockType::Code || item.runButton.bounds.width <= 0.f) continue;
        const RunState* state = runs_ != nullptr ? runs_->find(item.key) : nullptr;
        const bool isRunning = state != nullptr && state->running;
        item.runButton.enabled = !isRunning;
        item.stopButton.enabled = isRunning;
        item.runButton.update(mouse, dt);
        item.stopButton.update(mouse, dt);
        if ((item.runButton.clickedFlag || item.stopButton.clickedFlag) && runs_ != nullptr &&
            document_ != nullptr) {
            const std::vector<md::Block> blocks = md::parse(document_->lines());
            if (item.blockIndex >= 0 && item.blockIndex < static_cast<int>(blocks.size())) {
                if (item.stopButton.clickedFlag) {
                    runs_->stop(item.key);
                } else {
                    runs_->start(item.key, codeOf(blocks[static_cast<std::size_t>(item.blockIndex)]),
                                 blocks[static_cast<std::size_t>(item.blockIndex)].lang);
                }
            }
        }
    }
}

void PreviewView::drawItem(sf::RenderTarget& target, const BlockItem& item, const richtext::Style& style) {
    const theme::Metrics& m = theme::metrics;
    const theme::Palette& pal = theme::pal;
    const float left = bounds_.left + 22.f;

    if (item.type == md::BlockType::Code) {
        if (item.bounds.height <= 0.f) return;
        draw::shadowedPanel(target, item.bounds, m.radius, pal.codeBlockBg, pal.border);
        draw::vGradient(target,
                        {item.bounds.left + 1.f, item.bounds.top + 1.f, item.bounds.width - 2.f,
                         m.codeHeaderHeight},
                        pal.codeBlockHeader, pal.codeBlockBg);
        draw::hline(target, item.headerRect.top + m.codeHeaderHeight, item.bounds.left + 1.f,
                    item.bounds.right() - 1.f, pal.border);
        if (item.hasOutput) {
            const float outputTop = item.outputLines.empty() ? item.bounds.top + m.codeHeaderHeight
                                                            : item.outputLines.front().y;
            draw::hline(target, outputTop, item.bounds.left + 1.f, item.bounds.right() - 1.f, pal.border);
            draw::rect(target,
                       {item.bounds.left + 1.f, outputTop, 2.f,
                        item.bounds.top + item.bounds.height - outputTop},
                       pal.accent);
        }

        const float chipX = item.headerRect.left + 12.f;
        const float chipY = item.headerRect.top + m.codeHeaderHeight * 0.5f;
        const unsigned int chipSize = static_cast<unsigned int>(m.smallText - 1.f);
        const float chipW = draw::textWidth(theme::uiFont, item.displayName, chipSize) + 30.f;
        draw::roundedRect(target, {chipX, chipY - 10.f, chipW, 20.f}, 10.f, pal.accentSoft);
        draw::icon(target, "dot", {chipX + 12.f, chipY}, 11.f, pal.accent);
        draw::text(target, theme::uiFont, item.displayName, chipSize, pal.accent,
                   {chipX + 20.f, chipY - 7.f}, true);
        if (!item.statusText.empty()) {
            const unsigned int statusSize = static_cast<unsigned int>(m.tinyText);
            const float statusWidth = draw::textWidth(theme::uiFont, item.statusText, statusSize);
            const Rect pill{item.runButton.bounds.left - 30.f - statusWidth, chipY - 9.f,
                            statusWidth + 18.f, 18.f};
            draw::roundedRect(target, pill, 9.f, pal.pill);
            draw::icon(target, "dot", {pill.left + 10.f, chipY}, 9.f, item.statusColor);
            draw::text(target, theme::uiFont, item.statusText, statusSize, item.statusColor,
                       {pill.left + 18.f, chipY - 6.f});
        }
        const RunState* state = runs_ != nullptr ? runs_->find(item.key) : nullptr;
        if (state == nullptr || !state->running) {
            item.runButton.draw(target, theme::uiFont, 5.f, pal.codeBlockHeader, pal.accent,
                                pal.accent, pal.accent, pal.onAccent);
        } else {
            item.stopButton.draw(target, theme::uiFont, 5.f, pal.error, pal.error, pal.error,
                                 pal.onAccent, pal.onAccent);
        }

        const unsigned int codeSize = static_cast<unsigned int>(m.codeText);
        std::size_t offset = 0;
        for (std::size_t line = 0; line < item.codeLines.size() && line < item.codeRows.size(); ++line) {
            const LineItem& lineItem = item.codeLines[line];
            const std::string& text = item.codeRows[line];
            const float baseline = lineItem.y + (lineItem.height - static_cast<float>(codeSize)) * 0.5f - 1.f;
            draw::text(target, theme::monoFont, text, codeSize, pal.synPlain,
                       {item.headerRect.left + m.codePadX, baseline});
            for (const syntax::Span& span : item.spans) {
                if (span.start + span.length <= offset) continue;
                if (span.start >= offset + text.size()) break;
                const std::size_t begin = span.start > offset ? span.start - offset : 0;
                const std::size_t stop =
                    std::min(span.start + span.length, offset + text.size()) - offset;
                if (stop <= begin) continue;
                const std::string piece = text.substr(begin, stop - begin);
                const float pieceX =
                    draw::textWidth(theme::monoFont, std::string_view(text).substr(0, begin), codeSize);
                draw::text(target, theme::monoFont, piece, codeSize, colorFor(span.token),
                           {item.headerRect.left + m.codePadX + pieceX, baseline});
            }
            offset += text.size() + 1;
        }

        if (item.hasOutput) {
            const unsigned int outSize = static_cast<unsigned int>(m.codeText - 2.f);
            for (std::size_t row = 0; row < item.outputLines.size(); ++row) {
                const LineItem& lineItem = item.outputLines[row];
                std::string value;
                if (row < item.outputRows.size()) {
                    value = item.outputRows[row];
                } else if (!item.errorText.empty()) {
                    value = item.errorText;
                } else {
                    break;
                }
                if (value.empty()) continue;
                draw::text(target, theme::monoFont, value, outSize, pal.synPlain,
                           {item.headerRect.left + m.codePadX, lineItem.y});
            }
        }
        return;
    }

    if (item.type == md::BlockType::Rule) {
        for (const LineItem& lineItem : item.textLines) {
            draw::hline(target, lineItem.y + lineItem.height * 0.5f, left + item.indent,
                        left + item.indent + (bounds_.width - 44.f) - item.indent, pal.borderStrong);
        }
        return;
    }

    const float textX = left + item.indent;
    if (!item.marker.empty() && !item.textLines.empty()) {
        const LineItem& first = item.textLines.front();
        draw::text(target, theme::uiFont, item.marker, item.textSize, item.markerColor,
                   {textX - 16.f, first.y + (first.height - static_cast<float>(item.textSize)) * 0.5f - 1.f});
    }
    if (item.type == md::BlockType::Quote && !item.textLines.empty()) {
        const float top = item.textLines.front().y;
        const float bottom = item.textLines.back().y + item.textLines.back().height;
        // Rounded quote rule plus a faint wash behind the block.
        draw::roundedRect(target, {textX - 12.f, top, bounds_.width - 44.f, bottom - top}, 4.f,
                          sf::Color(pal.quoteBar.r, pal.quoteBar.g, pal.quoteBar.b, 40));
        draw::roundedRect(target, {textX - 12.f, top, 3.f, bottom - top}, 1.5f, pal.quoteBar);
    }

    for (std::size_t line = 0; line < item.textLines.size() && line < item.pieces.size(); ++line) {
        const LineItem& lineItem = item.textLines[line];
        const float baseline = lineItem.y + (lineItem.height - static_cast<float>(item.textSize)) * 0.5f - 1.f;
        richtext::draw(target, theme::uiFont, item.pieces[line], item.textSize, {textX, baseline}, style);
        if (item.underline) {
            draw::hline(target, lineItem.y + lineItem.height - 3.f, textX,
                        textX + richtext::width(theme::uiFont, item.pieces[line], item.textSize), pal.border);
        }
    }
}

void PreviewView::draw(sf::RenderTarget& target) {
    if (document_ == nullptr) return;
    if (stale()) rebuild();

    const theme::Palette& pal = theme::pal;
    draw::rect(target, bounds_, pal.previewBg);
    draw::endText();
    const sf::View previous = target.getView();
    const sf::Vector2f frame = previous.getSize();
    target.setView(sf::View(sf::Vector2f(frame.x * 0.5f, frame.y * 0.5f + scroll_), frame));
    // Glyphs are clipped to the pane so long lines never bleed into the
    // neighbouring panel.
    draw::beginClip(target, bounds_);

    const richtext::Style style = previewStyle();

    for (const BlockItem& item : items_) {
        drawItem(target, item, style);
    }

    draw::endClip();
    draw::scrollbar(target,
                    {bounds_.left + 4.f, bounds_.top + 4.f, bounds_.width - 8.f, bounds_.height - 8.f},
                    scroll_, contentHeight_, bounds_.height, scrollHover_, false);
    target.setView(previous);
}
