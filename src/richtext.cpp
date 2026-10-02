#include "richtext.h"

#include <algorithm>

#include "draw.h"
#include "util.h"

namespace richtext {
namespace {

struct Marker {
    std::size_t start;
    std::size_t length;
    bool bold = false;
    bool code = false;
    bool italic = false;
};

struct Content {
    std::size_t start;
    std::size_t length;
    md::InlineStyle style;
};

// True when a span carries its own delimiters, e.g. "**bold**" or "`code`".
bool hasDelimiters(std::string_view text, const md::InlineSpan& span) {
    const std::size_t end = span.start + span.length;
    if (span.length < 2 || end > text.size()) return false;
    const char first = text[span.start];
    const char last = text[end - 1];
    switch (span.style) {
        case md::InlineStyle::Bold: return span.length >= 4 && first == '*' && last == '*';
        case md::InlineStyle::Code: return first == '`' && last == '`';
        case md::InlineStyle::Italic: return (first == '*' || first == '_') && last == first;
        default: return false;
    }
}

bool isStyled(md::InlineStyle style) {
    return style == md::InlineStyle::Bold || style == md::InlineStyle::Italic ||
           style == md::InlineStyle::Code;
}

}  // namespace

std::vector<Piece> split(std::string_view text, const std::vector<md::InlineSpan>& spans,
                         const Style& style, bool dimMarkers) {
    enum class Kind { Plain, Bold, Italic, Code, Link, Marker };
    struct Run {
        std::size_t start = 0;
        std::size_t length = 0;
        Kind kind = Kind::Plain;
        std::string link;
    };

    std::vector<Marker> markers;
    std::vector<Run> runs;
    // Link targets are metadata and the brackets only make sense in the editor,
    // so both can be dropped entirely.
    std::vector<std::pair<std::size_t, std::size_t>> hidden;

    for (const md::InlineSpan& span : spans) {
        const std::size_t end = span.start + span.length;
        if ((!dimMarkers && span.style == md::InlineStyle::LinkTarget) ||
            (!dimMarkers && span.style == md::InlineStyle::LinkMarker)) {
            hidden.emplace_back(span.start, end);
            continue;
        }
        // The editor keeps every character of the source: delimiters and link
        // targets are dimmed instead of dropped.
        if (span.style == md::InlineStyle::LinkTarget) {
            runs.push_back({span.start, span.length, Kind::Marker, {}});
            continue;
        }
        if (span.style == md::InlineStyle::LinkMarker) {
            if (dimMarkers) runs.push_back({span.start, span.length, Kind::Marker, {}});
            continue;
        }
        if (span.style == md::InlineStyle::Link) {
            runs.push_back({span.start, span.length, Kind::Link, span.link});
            continue;
        }
        if (!isStyled(span.style)) continue;

        const Kind kind = span.style == md::InlineStyle::Bold   ? Kind::Bold
                          : span.style == md::InlineStyle::Code ? Kind::Code
                                                                : Kind::Italic;
        if (!hasDelimiters(text, span)) {
            runs.push_back({span.start, span.length, kind, {}});
            continue;
        }
        // Only the delimiters themselves are hidden or dimmed; the content stays.
        const std::size_t skip = kind == Kind::Bold ? 2 : 1;
        const Marker marker{span.start, skip, kind == Kind::Bold, kind == Kind::Code,
                            kind == Kind::Italic};
        if (dimMarkers) {
            markers.push_back(marker);
            markers.push_back({end - skip, skip, marker.bold, marker.code, marker.italic});
        } else {
            hidden.emplace_back(span.start, span.start + skip);
            hidden.emplace_back(end - skip, end);
        }
        if (span.length > 2 * skip) {
            runs.push_back({span.start + skip, span.length - 2 * skip, kind, {}});
        }
    }

    std::sort(markers.begin(), markers.end(),
              [](const Marker& a, const Marker& b) { return a.start < b.start; });
    std::sort(runs.begin(), runs.end(),
              [](const Run& a, const Run& b) { return a.start < b.start; });

    std::vector<Piece> pieces;

    const auto isHidden = [&](std::size_t index) {
        for (const auto& range : hidden) {
            if (index >= range.first && index < range.second) return true;
        }
        return false;
    };
    const auto skipHidden = [&](std::size_t index) {
        while (index < text.size() && isHidden(index)) ++index;
        return index;
    };
    const auto nextHiddenStart = [&](std::size_t index) {
        std::size_t start = text.size();
        for (const auto& range : hidden) {
            if (range.first >= index) start = std::min(start, range.first);
        }
        return start;
    };

    // Emits [start, end) honouring dimmed delimiters and hidden ranges.
    auto pushRun = [&](std::size_t start, std::size_t end, const sf::Color& color, bool bold,
                       bool code, const std::string& link) {
        while (start < end) {
            if (isHidden(start)) {
                start = skipHidden(start);
                if (start >= end) return;
            }
            std::size_t stop = std::min(end, nextHiddenStart(start));
            const Marker* marker = nullptr;
            for (const Marker& candidate : markers) {
                if (candidate.start == start) {
                    marker = &candidate;
                    break;
                }
                if (candidate.start > start) {
                    stop = std::min(stop, candidate.start);
                    break;
                }
            }
            if (stop > start) {
                Piece piece;
                piece.text = std::string(text.substr(start, stop - start));
                piece.color = color;
                piece.bold = bold;
                piece.code = code;
                piece.link = link;
                pieces.push_back(std::move(piece));
                start = stop;
            }
            if (marker == nullptr || start >= end) continue;
            if (dimMarkers) {
                Piece piece;
                piece.text = std::string(text.substr(marker->start, marker->length));
                piece.color = style.marker;
                piece.bold = marker->bold;
                pieces.push_back(std::move(piece));
            }
            start = marker->start + marker->length;
        }
    };

    std::size_t cursor = 0;
    std::size_t runIndex = 0;
    while (cursor < text.size()) {
        cursor = skipHidden(cursor);
        if (cursor >= text.size()) break;
        while (runIndex < runs.size() && runs[runIndex].start + runs[runIndex].length <= cursor) {
            ++runIndex;
        }
        if (runIndex < runs.size() && runs[runIndex].start == cursor) {
            const Run& run = runs[runIndex];
            const std::size_t end = run.start + run.length;
            switch (run.kind) {
                case Kind::Bold:
                    pushRun(cursor, end, style.emphasis, true, false, {});
                    break;
                case Kind::Italic:
                    pushRun(cursor, end, style.italic, false, false, {});
                    break;
                case Kind::Code:
                    pushRun(cursor, end, style.code, false, true, {});
                    break;
                case Kind::Link:
                    pushRun(cursor, end, style.link, false, false, run.link);
                    break;
                case Kind::Marker:
                    pushRun(cursor, end, style.marker, false, false, {});
                    break;
                default:
                    pushRun(cursor, end, style.normal, false, false, {});
                    break;
            }
            cursor = end;
            ++runIndex;
            continue;
        }
        std::size_t stop = runIndex < runs.size() ? runs[runIndex].start : text.size();
        stop = std::min(stop, nextHiddenStart(cursor));
        if (stop <= cursor) stop = cursor + 1;
        pushRun(cursor, stop, style.normal, false, false, {});
        cursor = stop;
    }

    if (pieces.empty() && !text.empty()) {
        Piece piece;
        piece.text = std::string(text);
        piece.color = style.normal;
        pieces.push_back(std::move(piece));
    }
    return pieces;
}

std::vector<Piece> split(std::string_view text, const Style& style, bool dimMarkers) {
    return split(text, md::inlineSpans(text), style, dimMarkers);
}

std::vector<std::string> wrap(std::string_view text, const sf::Font& font, unsigned int size,
                              float maxWidth) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.emplace_back();
        return lines;
    }
    std::string current;
    float currentWidth = 0.f;
    std::size_t index = 0;
    auto measure = [&](std::string_view piece) { return draw::textWidth(font, piece, size); };
    while (index < text.size()) {
        std::size_t space = text.find(' ', index);
        std::size_t wordEnd = space == std::string_view::npos ? text.size() : space;
        std::string_view word = text.substr(index, wordEnd - index);
        const float wordWidth = measure(word);
        if (!current.empty() && currentWidth + wordWidth > maxWidth) {
            lines.push_back(current);
            current.clear();
            currentWidth = 0.f;
        }
        if (wordWidth > maxWidth) {
            // hard break very long words (urls, base64, ...)
            std::size_t chunk = 0;
            float chunkWidth = 0.f;
            std::size_t consumed = 0;
            for (const util::Utf8Char& ch : util::decodeUtf8(word)) {
                const std::string glyph = util::encodeUtf8(ch.codePoint);
                const float glyphWidth = measure(glyph);
                if (chunkWidth + glyphWidth > maxWidth && chunk > 0) {
                    current += word.substr(consumed, chunk);
                    consumed += chunk;
                    lines.push_back(current);
                    current.clear();
                    currentWidth = 0.f;
                    chunk = 0;
                    chunkWidth = 0.f;
                }
                chunk += ch.bytes;
                chunkWidth += glyphWidth;
            }
            current += word.substr(consumed, chunk);
            currentWidth += chunkWidth;
            index = wordEnd;
            if (space != std::string_view::npos) ++index;
            continue;
        }
        if (!current.empty()) {
            current += ' ';
            currentWidth += measure(" ");
        }
        current.append(word);
        currentWidth += wordWidth;
        index = wordEnd;
        if (space != std::string_view::npos) ++index;
    }
    lines.push_back(current);
    return lines;
}

float width(const sf::Font& font, const std::vector<Piece>& pieces, unsigned int size) {
    float total = 0.f;
    for (const Piece& piece : pieces) {
        total += draw::textWidth(font, piece.text, size, piece.bold);
    }
    return total;
}

float draw(sf::RenderTarget& target, const sf::Font& font, const std::vector<Piece>& pieces,
           unsigned int size, const sf::Vector2f& origin, const Style& style) {
    float x = origin.x;
    const float y = origin.y;
    for (const Piece& piece : pieces) {
        if (piece.code) {
            const float pieceWidth = draw::textWidth(font, piece.text, size, piece.bold);
            if (style.codeBackground.a > 0 && pieceWidth > 0.f) {
                draw::roundedRect(target,
                                  {x - 2.f, y - 1.f, pieceWidth + 4.f, static_cast<float>(size) + 3.f},
                                  3.f, style.codeBackground);
            }
        }
        draw::text(target, font, piece.text, size, piece.color, {x, y}, piece.bold);
        x += draw::textWidth(font, piece.text, size, piece.bold);
    }
    return x;
}

}  // namespace richtext
