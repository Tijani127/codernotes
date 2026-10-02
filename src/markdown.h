#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace md {

enum class BlockType {
    Heading,
    Paragraph,
    Code,
    ListItem,
    Quote,
    Rule,
    Spacer,
};

enum class InlineStyle {
    Normal,
    Bold,
    Italic,
    BoldItalic,
    Code,
    Link,        // the clickable label
    LinkMarker,  // the surrounding brackets, only shown in the editor
    LinkTarget,  // "(https://...)", never rendered
    Marker,
};

struct InlineSpan {
    std::size_t start = 0;
    std::size_t length = 0;
    InlineStyle style = InlineStyle::Normal;
    std::string link;
};

struct Block {
    BlockType type = BlockType::Paragraph;
    int level = 0;        // heading level / list depth
    int indent = 0;       // leading spaces (list + quote)
    std::string text;     // rendered-ish source text without markers
    std::vector<InlineSpan> spans;
    std::string lang;     // code fence language
    std::string info;     // full fence info string
    std::vector<std::string> code;
    int openLine = 0;     // document line of the opening fence
    int closeLine = -1;   // document line of the closing fence (-1 when unterminated)
    bool ordered = false;
    int markerWidth = 0;
};

struct Fence {
    int openLine = 0;
    int closeLine = -1;
    std::string lang;
    std::string info;
    char marker = '`';
    int indent = 0;
};

// Scans the source for fenced code blocks. Fences start at the beginning of a
// line (up to three spaces of indentation) with three or more backticks/tildes.
std::vector<Fence> scanFences(const std::vector<std::string>& lines);

std::vector<Block> parse(const std::vector<std::string>& lines);
std::vector<InlineSpan> inlineSpans(std::string_view text, bool markers = true);
std::string stripInline(std::string_view text);

bool isHeadingStart(std::string_view line);
std::string headingText(std::string_view line);

}  // namespace md
