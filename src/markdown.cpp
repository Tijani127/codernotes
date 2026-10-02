#include "markdown.h"

#include <algorithm>
#include <cctype>

#include "util.h"

namespace md {
namespace {

struct FenceStart {
    std::size_t indent = 0;
    char marker = 0;
    std::size_t markerCount = 0;
    std::string info;
    bool valid = false;
};

FenceStart parseFenceStart(std::string_view line) {
    FenceStart out;
    std::size_t i = 0;
    while (i < line.size() && line[i] == ' ' && i < 4) ++i;
    out.indent = i;
    if (i >= line.size()) return out;
    const char c = line[i];
    if (c != '`' && c != '~') return out;
    std::size_t count = 0;
    while (i + count < line.size() && line[i + count] == c) ++count;
    if (count < 3) return out;
    out.marker = c;
    out.markerCount = count;
    std::string info(line.substr(i + count));
    if (c == '`' && info.find('`') != std::string::npos) return out;
    out.info = util::trim(info);
    out.valid = true;
    return out;
}

bool isClosingFence(std::string_view line, char marker) {
    const FenceStart start = parseFenceStart(line);
    if (!start.valid || start.marker != marker) return false;
    return start.info.empty();
}

int leadingSpaces(std::string_view line) {
    int n = 0;
    for (char c : line) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

std::string_view dropIndent(std::string_view line, int amount) {
    std::size_t i = 0;
    int removed = 0;
    while (i < line.size() && removed < amount) {
        if (line[i] == ' ') {
            ++i;
            ++removed;
        } else if (line[i] == '\t') {
            i += 1;
            removed += 4;
        } else {
            break;
        }
    }
    return line.substr(i);
}

struct ListMarker {
    bool valid = false;
    bool ordered = false;
    std::size_t contentStart = 0;
    int indent = 0;
    std::size_t markerWidth = 0;
};

ListMarker parseListMarker(std::string_view line) {
    ListMarker out;
    out.indent = leadingSpaces(line);
    std::size_t i = 0;
    while (i < line.size() && line[i] == ' ') ++i;
    if (i >= line.size()) return out;
    const char c = line[i];
    if ((c == '-' || c == '*' || c == '+') && i + 1 < line.size() && line[i + 1] == ' ') {
        out.valid = true;
        out.ordered = false;
        out.contentStart = i + 2;
        out.markerWidth = 1;
        return out;
    }
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
        std::size_t j = i;
        while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j])) != 0) ++j;
        if (j < line.size() && (line[j] == '.' || line[j] == ')') && j + 1 < line.size() &&
            line[j + 1] == ' ') {
            out.valid = true;
            out.ordered = true;
            out.contentStart = j + 2;
            out.markerWidth = j + 2 - i;
            return out;
        }
    }
    return out;
}

bool isRuleLine(std::string_view line) {
    const std::string trimmed = util::trim(line);
    if (trimmed.size() < 3) return false;
    const char c = trimmed[0];
    if (c != '-' && c != '*' && c != '_') return false;
    int count = 0;
    for (char ch : trimmed) {
        if (ch == c) ++count;
        else if (ch != ' ') return false;
    }
    return count >= 3;
}

}  // namespace

std::vector<Fence> scanFences(const std::vector<std::string>& lines) {
    std::vector<Fence> fences;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const FenceStart start = parseFenceStart(lines[i]);
        if (!start.valid) continue;
        int close = -1;
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            if (isClosingFence(lines[j], start.marker)) {
                close = static_cast<int>(j);
                break;
            }
        }
        Fence fence;
        fence.openLine = static_cast<int>(i);
        fence.closeLine = close;
        fence.lang = start.info;
        fence.info = start.info;
        fence.marker = start.marker;
        fence.indent = static_cast<int>(start.indent);
        fences.push_back(fence);
        if (close >= 0) {
            i = static_cast<std::size_t>(close);
        }
    }
    return fences;
}

bool isHeadingStart(std::string_view line) {
    const std::string_view trimmed = util::ltrim(line);
    if (trimmed.empty() || trimmed[0] != '#') return false;
    int level = 0;
    while (level < static_cast<int>(trimmed.size()) && trimmed[level] == '#') ++level;
    if (level > 6) return false;
    return level < static_cast<int>(trimmed.size()) && trimmed[level] == ' ';
}

std::string headingText(std::string_view line) {
    if (!isHeadingStart(line)) return std::string(util::trim(line));
    const std::string_view trimmed = util::ltrim(line);
    std::size_t i = 0;
    while (i < trimmed.size() && trimmed[i] == '#') ++i;
    ++i;
    std::string text(util::rtrim(trimmed.substr(i)));
    while (!text.empty() && text.back() == '#') text.pop_back();
    return util::trim(text);
}

std::vector<InlineSpan> inlineSpans(std::string_view text, bool markers) {
    std::vector<InlineSpan> spans;
    const std::size_t n = text.size();
    std::size_t i = 0;
    while (i < n) {
        if (text[i] == '`') {
            std::size_t ticks = 0;
            while (i + ticks < n && text[i + ticks] == '`') ++ticks;
            const std::string fence(ticks, '`');
            const std::size_t open = i + ticks;
            const std::size_t close = text.find(fence, open);
            if (close != std::string_view::npos) {
                spans.push_back({i, close + ticks - i, InlineStyle::Code, {}});
                i = close + ticks;
                continue;
            }
        }
        if (text[i] == '*' || text[i] == '_') {
            const char marker = text[i];
            const bool doubled = i + 1 < n && text[i + 1] == marker;
            const std::size_t openLength = doubled ? 2 : 1;
            const std::size_t contentStart = i + openLength;
            // Find a closing delimiter, skipping candidates that belong to a
            // longer run (e.g. the first "*" of "**bold**").
            std::size_t search = contentStart;
            std::size_t close = std::string_view::npos;
            while (true) {
                const std::size_t candidate = text.find(marker, search);
                if (candidate == std::string_view::npos) break;
                if (!doubled && candidate + 1 < n && text[candidate + 1] == marker) {
                    search = candidate + 2;
                    continue;
                }
                close = candidate;
                break;
            }
            if (close != std::string_view::npos && close > contentStart) {
                InlineSpan outer;
                outer.start = i;
                outer.length = close + openLength - i;
                outer.style = doubled ? InlineStyle::Bold : InlineStyle::Italic;
                InlineSpan inner;
                inner.start = contentStart;
                inner.length = close - contentStart;
                inner.style = outer.style;
                if (markers) spans.push_back(outer);
                spans.push_back(inner);
                i = close + openLength;
                continue;
            }
        }
        if (text[i] == '[') {
            const std::size_t labelEnd = text.find(']', i + 1);
            if (labelEnd != std::string_view::npos && labelEnd + 1 < n && text[labelEnd + 1] == '(') {
                const std::size_t urlEnd = text.find(')', labelEnd + 2);
                if (urlEnd != std::string_view::npos) {
                    InlineSpan marker;
                    marker.start = i;
                    marker.length = 1;  // the leading '['
                    marker.style = InlineStyle::LinkMarker;
                    marker.link = std::string(text.substr(labelEnd + 2, urlEnd - labelEnd - 2));
                    InlineSpan closing;
                    closing.start = labelEnd;
                    closing.length = 1;  // the closing ']'
                    closing.style = InlineStyle::LinkMarker;
                    InlineSpan label;
                    label.start = i + 1;
                    label.length = labelEnd - i - 1;
                    label.style = InlineStyle::Link;
                    label.link = marker.link;
                    InlineSpan target;
                    target.start = labelEnd + 1;
                    target.length = urlEnd - labelEnd;
                    target.style = InlineStyle::LinkTarget;
                    if (markers) spans.push_back(marker);
                    spans.push_back(label);
                    if (markers) {
                        spans.push_back(closing);
                        spans.push_back(target);
                    }
                    i = labelEnd + 1;
                    continue;
                }
            }
        }
        ++i;
    }
    return spans;
}

std::string stripInline(std::string_view text) {
    std::string out;
    const std::vector<InlineSpan> spans = inlineSpans(text, false);
    std::size_t i = 0;
    for (const InlineSpan& span : spans) {
        if (span.start > i) out.append(text.substr(i, span.start - i));
        if (span.style == InlineStyle::Bold || span.style == InlineStyle::Italic ||
            span.style == InlineStyle::LinkTarget) {
            i = span.start + span.length;
            continue;
        }
        out.append(text.substr(span.start, span.length));
        i = span.start + span.length;
    }
    if (i < text.size()) out.append(text.substr(i));
    return out;
}

std::vector<Block> parse(const std::vector<std::string>& lines) {
    std::vector<Block> blocks;
    const std::vector<Fence> fences = scanFences(lines);

    std::vector<std::string> paragraph;
    auto flushParagraph = [&]() {
        if (paragraph.empty()) return;
        Block block;
        block.type = BlockType::Paragraph;
        std::string text = util::join(paragraph, " ");
        block.text = text;
        block.spans = inlineSpans(text);
        blocks.push_back(block);
        paragraph.clear();
    };

    std::size_t lineIndex = 0;
    while (lineIndex < lines.size()) {
        const auto fenceIt =
            std::find_if(fences.begin(), fences.end(),
                         [&](const Fence& fence) { return fence.openLine == static_cast<int>(lineIndex); });
        if (fenceIt != fences.end()) {
            flushParagraph();
            Block block;
            block.type = BlockType::Code;
            block.lang = fenceIt->lang;
            block.info = fenceIt->info;
            block.openLine = fenceIt->openLine;
            block.closeLine = fenceIt->closeLine;
            const int end = fenceIt->closeLine >= 0 ? fenceIt->closeLine
                                                    : static_cast<int>(lines.size()) - 1;
            for (int i = fenceIt->openLine + 1; i < end; ++i) {
                block.code.push_back(std::string(dropIndent(lines[static_cast<std::size_t>(i)], fenceIt->indent)));
            }
            blocks.push_back(block);
            lineIndex = static_cast<std::size_t>(end) + 1;
            continue;
        }

        const std::string& line = lines[lineIndex];
        if (util::trim(line).empty()) {
            flushParagraph();
            ++lineIndex;
            continue;
        }

        if (isRuleLine(line)) {
            flushParagraph();
            Block block;
            block.type = BlockType::Rule;
            blocks.push_back(block);
            ++lineIndex;
            continue;
        }

        if (isHeadingStart(line)) {
            flushParagraph();
            Block block;
            block.type = BlockType::Heading;
            const std::string_view trimmed = util::ltrim(line);
            int level = 0;
            while (level < static_cast<int>(trimmed.size()) && trimmed[level] == '#') ++level;
            block.level = level;
            block.text = headingText(line);
            block.spans = inlineSpans(block.text);
            blocks.push_back(block);
            ++lineIndex;
            continue;
        }

        const ListMarker list = parseListMarker(line);
        if (list.valid) {
            flushParagraph();
            Block block;
            block.type = BlockType::ListItem;
            block.ordered = list.ordered;
            block.indent = list.indent;
            block.markerWidth = static_cast<int>(list.markerWidth);
            std::string content(line.substr(list.contentStart));
            if (util::trim(content).empty() && lineIndex + 1 < lines.size()) {
                content = lines[lineIndex + 1];
                ++lineIndex;
            }
            block.text = util::trim(content);
            block.spans = inlineSpans(block.text);
            blocks.push_back(block);
            ++lineIndex;
            continue;
        }

        if (util::ltrim(line)[0] == '>') {
            flushParagraph();
            Block block;
            block.type = BlockType::Quote;
            std::string content = util::trim(line);
            if (!content.empty() && content[0] == '>') content = util::trim(content.substr(1));
            block.text = content;
            block.spans = inlineSpans(content);
            blocks.push_back(block);
            ++lineIndex;
            continue;
        }

        paragraph.push_back(line);
        ++lineIndex;
    }
    flushParagraph();
    return blocks;
}

}  // namespace md
