#include "doc.h"

#include <algorithm>
#include <cctype>

#include "util.h"

namespace {

bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string commentMarkerFor(const std::string& line) {
    if (util::ltrim(line).empty()) return {};
    const std::string trimmed = util::trim(line);
    if (util::startsWith(trimmed, "//")) return "//";
    if (util::startsWith(trimmed, "#")) return "#";
    if (util::startsWith(trimmed, "--")) return "--";
    if (util::startsWith(trimmed, ";")) return ";";
    return {};
}

}  // namespace

void Document::setText(std::string_view text, bool clearHistory) {
    setLines(util::splitLines(text), clearHistory);
}

void Document::setLines(std::vector<std::string> lines, bool clearHistory) {
    if (lines.empty()) lines.emplace_back();
    lines_ = std::move(lines);
    caret_ = Position{0, 0};
    anchor_ = caret_;
    selectionActive_ = false;
    if (clearHistory) {
        undo_.clear();
        redo_.clear();
    }
    modified_ = false;
    ++version_;
}

std::string Document::text() const {
    return util::join(lines_, "\n");
}

const std::string& Document::line(int index) const {
    static const std::string empty;
    if (index < 0 || index >= static_cast<int>(lines_.size())) return empty;
    return lines_[static_cast<std::size_t>(index)];
}

bool Document::contains(const Position& position) const {
    return position.line >= 0 && position.line < static_cast<int>(lines_.size()) &&
           position.column >= 0 &&
           position.column <= static_cast<int>(line(position.line).size());
}

Position Document::clamp(Position position) const {
    position.line = std::clamp(position.line, 0, static_cast<int>(lines_.size()) - 1);
    position.column = std::clamp(position.column, 0, static_cast<int>(line(position.line).size()));
    while (position.column > 0 && !util::isUtf8Boundary(line(position.line), position.column)) {
        --position.column;
    }
    return position;
}

Position Document::positionAfter(const Position& position) const {
    Position next = clamp(position);
    const std::string& current = line(next.line);
    const std::size_t after = util::nextCharIndex(current, static_cast<std::size_t>(next.column));
    if (after >= current.size()) {
        if (next.line + 1 < static_cast<int>(lines_.size())) next = Position{next.line + 1, 0};
    } else {
        next.column = static_cast<int>(after);
    }
    return next;
}

Position Document::positionBefore(const Position& position) const {
    Position previous = clamp(position);
    if (previous.column == 0) {
        if (previous.line > 0) {
            --previous.line;
            previous.column = static_cast<int>(line(previous.line).size());
        }
    } else {
        previous.column =
            static_cast<int>(util::prevCharIndex(line(previous.line),
                                                  static_cast<std::size_t>(previous.column)));
    }
    return previous;
}

Position Document::selectionStart() const {
    return caret_ < anchor_ ? caret_ : anchor_;
}

Position Document::selectionEnd() const {
    return caret_ < anchor_ ? anchor_ : caret_;
}

void Document::setCaret(Position position, bool extendSelection) {
    position = clamp(position);
    caret_ = position;
    if (extendSelection) {
        selectionActive_ = true;
    } else {
        anchor_ = position;
        selectionActive_ = false;
    }
}

void Document::normalizeSelection() {
    selectionActive_ = caret_ != anchor_;
}

std::string Document::selectedText() const {
    if (caret_ == anchor_) return {};
    const Position from = selectionStart();
    const Position to = selectionEnd();
    if (from.line == to.line) {
        const std::string& current = line(from.line);
        return current.substr(static_cast<std::size_t>(from.column),
                              static_cast<std::size_t>(to.column - from.column));
    }
    std::string out = line(from.line).substr(static_cast<std::size_t>(from.column));
    for (int i = from.line + 1; i < to.line; ++i) {
        out += '\n';
        out += line(i);
    }
    out += '\n';
    out += line(to.line).substr(0, static_cast<std::size_t>(to.column));
    return out;
}

void Document::selectAll() {
    anchor_ = Position{0, 0};
    caret_ = Position{static_cast<int>(lines_.size()) - 1,
                      static_cast<int>(line(static_cast<int>(lines_.size()) - 1).size())};
    selectionActive_ = true;
}

void Document::clearSelection() {
    anchor_ = caret_;
    selectionActive_ = false;
}

void Document::pushUndo() {
    undo_.push_back(lines_);
    if (undo_.size() > 256) undo_.erase(undo_.begin());
    redo_.clear();
}

void Document::deleteRange(Position from, Position to) {
    from = clamp(from);
    to = clamp(to);
    if (from == to) return;
    std::string& first = lines_[static_cast<std::size_t>(from.line)];
    std::string& last = lines_[static_cast<std::size_t>(to.line)];
    const std::string merged = first.substr(0, static_cast<std::size_t>(from.column)) +
                               last.substr(static_cast<std::size_t>(to.column));
    lines_[static_cast<std::size_t>(from.line)] = merged;
    lines_.erase(lines_.begin() + from.line + 1, lines_.begin() + to.line + 1);
    caret_ = from;
    anchor_ = from;
    selectionActive_ = false;
}

void Document::insertAtCaret(std::string_view utf8, const EditContext& context) {
    if (utf8.empty()) return;
    if (selectionActive_ && caret_ != anchor_) {
        deleteRange(selectionStart(), selectionEnd());
    }
    std::string& current = lines_[static_cast<std::size_t>(caret_.line)];
    current.insert(static_cast<std::size_t>(caret_.column), utf8);
    caret_.column += static_cast<int>(utf8.size());
    anchor_ = caret_;
    selectionActive_ = false;
    (void)context;
    modified_ = true;
    ++version_;
}

bool Document::insertText(std::string_view utf8, const EditContext& context) {
    if (utf8.empty()) return false;
    std::string plain(utf8);
    if (plain.find('\n') == std::string::npos && plain.find('\t') == std::string::npos) {
        pushUndo();
        insertAtCaret(plain, context);
        return true;
    }
    std::vector<std::string> parts = util::split(plain, '\n');
    pushUndo();
    if (selectionActive_ && caret_ != anchor_) deleteRange(selectionStart(), selectionEnd());
    std::string& current = lines_[static_cast<std::size_t>(caret_.line)];
    const std::string head = current.substr(0, static_cast<std::size_t>(caret_.column));
    const std::string tail = current.substr(static_cast<std::size_t>(caret_.column));
    std::vector<std::string> replacement;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        std::string part = parts[i];
        std::replace(part.begin(), part.end(), '\t', ' ');
        if (i == 0) part = head + part;
        if (i + 1 == parts.size()) part += tail;
        replacement.push_back(part);
    }
    const int caretLine = caret_.line;
    lines_.erase(lines_.begin() + caret_.line);
    lines_.insert(lines_.begin() + caret_.line, replacement.begin(), replacement.end());
    caret_ = Position{caretLine + static_cast<int>(parts.size()) - 1,
                      static_cast<int>(parts.back().size())};
    anchor_ = caret_;
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::insertNewline(const EditContext& context) {
    pushUndo();
    if (selectionActive_ && caret_ != anchor_) deleteRange(selectionStart(), selectionEnd());
    std::string& current = lines_[static_cast<std::size_t>(caret_.line)];
    const std::string head = current.substr(0, static_cast<std::size_t>(caret_.column));
    std::string tail = current.substr(static_cast<std::size_t>(caret_.column));

    std::string indent;
    if (context.inCodeBlock) {
        const std::string_view leading = util::ltrim(head);
        int spaces = 0;
        for (char c : leading) {
            if (c == ' ') ++spaces;
            else break;
        }
        indent.assign(static_cast<std::size_t>(spaces), ' ');
        bool opensBlock = false;
        const std::string trimmed = util::trim(head);
        if (util::endsWith(trimmed, "{") || util::endsWith(trimmed, "[") ||
            util::endsWith(trimmed, "(") || util::endsWith(trimmed, ":")) {
            opensBlock = true;
        }
        if (opensBlock) indent += "    ";
        if (!tail.empty() && util::trim(tail) != tail) {
            tail = util::trim(tail);
        }
    } else {
        const std::string_view leading = util::ltrim(head);
        int spaces = 0;
        for (char c : leading) {
            if (c == ' ') ++spaces;
            else break;
        }
        const std::string trimmed = util::trim(head);
        std::size_t markerLength = 0;
        bool listItem = false;
        if (!trimmed.empty() && (trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+')) {
            listItem = true;
            markerLength = 2;
        } else if (trimmed.size() > 2 && std::isdigit(static_cast<unsigned char>(trimmed[0])) != 0) {
            std::size_t probe = 0;
            while (probe < trimmed.size() &&
                   std::isdigit(static_cast<unsigned char>(trimmed[probe])) != 0) {
                ++probe;
            }
            if (probe < trimmed.size() && (trimmed[probe] == '.' || trimmed[probe] == ')')) {
                listItem = true;
                markerLength = probe + 2;
            }
        }
        if (listItem) {
            const std::string content =
                trimmed.size() > markerLength ? util::trim(trimmed.substr(markerLength))
                                              : std::string();
            if (content.empty()) {
                lines_[static_cast<std::size_t>(caret_.line)] = std::string(static_cast<std::size_t>(spaces), ' ');
                caret_ = Position{caret_.line, spaces};
                anchor_ = caret_;
                selectionActive_ = false;
                modified_ = true;
                ++version_;
                return true;
            }
            indent.assign(static_cast<std::size_t>(spaces), ' ');
            indent += trimmed.substr(0, markerLength);
        } else {
            indent.assign(static_cast<std::size_t>(spaces), ' ');
        }
    }

    lines_[static_cast<std::size_t>(caret_.line)] = head;
    lines_.insert(lines_.begin() + caret_.line + 1, indent + tail);
    caret_ = Position{caret_.line + 1, static_cast<int>(indent.size())};
    anchor_ = caret_;
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::insertIndent(const EditContext& context) {
    const std::string unit = context.inCodeBlock ? "    " : "  ";
    if (hasSelection()) return indentSelection(context, true);
    pushUndo();
    insertAtCaret(unit, context);
    return true;
}

bool Document::removeBackward(const EditContext& context) {
    pushUndo();
    if (selectionActive_ && caret_ != anchor_) {
        deleteRange(selectionStart(), selectionEnd());
        modified_ = true;
        ++version_;
        return true;
    }
    if (caret_.column == 0) {
        if (caret_.line == 0) {
            undo_.pop_back();
            return false;
        }
        if (context.onFenceLine && context.inCodeBlock == false) {
            lines_.erase(lines_.begin() + caret_.line);
            caret_ = Position{caret_.line - 1, static_cast<int>(line(caret_.line - 1).size())};
            anchor_ = caret_;
            selectionActive_ = false;
            modified_ = true;
            ++version_;
            return true;
        }
        const int previousLine = caret_.line - 1;
        caret_.column = static_cast<int>(line(previousLine).size());
        lines_[static_cast<std::size_t>(previousLine)] += lines_[static_cast<std::size_t>(caret_.line)];
        lines_.erase(lines_.begin() + caret_.line);
        caret_.line = previousLine;
        anchor_ = caret_;
        selectionActive_ = false;
        modified_ = true;
        ++version_;
        return true;
    }
    const std::string& current = line(caret_.line);
    const std::size_t previous = util::prevCharIndex(current, static_cast<std::size_t>(caret_.column));
    const std::size_t removed = caret_.column - static_cast<int>(previous);
    if (removed == 1 && caret_.column >= 2 && current[static_cast<std::size_t>(caret_.column - 1)] == ' ') {
        std::size_t spaces = 0;
        std::size_t probe = static_cast<std::size_t>(caret_.column);
        while (probe > 0 && current[probe - 1] == ' ') {
            --probe;
            ++spaces;
        }
        if (spaces > 1 && spaces % 2 == 0) {
            lines_[static_cast<std::size_t>(caret_.line)] =
                current.substr(0, probe) + current.substr(static_cast<std::size_t>(caret_.column));
            caret_.column -= 2;
            anchor_ = caret_;
            modified_ = true;
            ++version_;
            return true;
        }
    }
    lines_[static_cast<std::size_t>(caret_.line)] =
        current.substr(0, previous) + current.substr(static_cast<std::size_t>(caret_.column));
    caret_.column = static_cast<int>(previous);
    anchor_ = caret_;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::removeForward(const EditContext& context) {
    pushUndo();
    if (selectionActive_ && caret_ != anchor_) {
        deleteRange(selectionStart(), selectionEnd());
        modified_ = true;
        ++version_;
        return true;
    }
    const Position next = positionAfter(caret_);
    if (next == caret_) {
        undo_.pop_back();
        return false;
    }
    const Position from = caret_;
    deleteRange(from, next);
    caret_ = from;
    anchor_ = caret_;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::indentSelection(const EditContext& context, bool forward) {
    if (!hasSelection()) return false;
    const int first = selectionStart().line;
    const int last = selectionEnd().line;
    const std::string unit = context.inCodeBlock ? "    " : "  ";
    pushUndo();
    for (int i = first; i <= last; ++i) {
        if (forward) {
            lines_[static_cast<std::size_t>(i)].insert(0, unit);
        } else if (!util::ltrim(lines_[static_cast<std::size_t>(i)]).empty()) {
            const std::string& current = lines_[static_cast<std::size_t>(i)];
            std::size_t remove = std::min(unit.size(), current.size());
            while (remove > 0 && current[remove - 1] == ' ') --remove;
            if (remove > 0) lines_[static_cast<std::size_t>(i)] = current.substr(remove);
        }
    }
    caret_ = selectionEnd();
    caret_ = clamp(caret_);
    anchor_ = caret_;
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::toggleLineComment(const EditContext& context) {
    const std::string marker = context.inCodeBlock ? "//" : "#";
    int first = caret_.line;
    int last = caret_.line;
    if (hasSelection()) {
        first = std::min(selectionStart().line, selectionEnd().line);
        last = std::max(selectionStart().line, selectionEnd().line);
    }
    std::string probe = line(first);
    if (line(first).empty() && first + 1 < lineCount()) probe = line(first + 1);
    std::string actual = commentMarkerFor(probe);
    if (actual.empty()) actual = marker;
    bool allCommented = true;
    for (int i = first; i <= last; ++i) {
        if (line(i).empty()) continue;
        const std::string trimmed = util::trim(line(i));
        if (!util::startsWith(trimmed, actual)) {
            allCommented = false;
            break;
        }
    }
    pushUndo();
    for (int i = first; i <= last; ++i) {
        std::string& current = lines_[static_cast<std::size_t>(i)];
        const std::string_view trimmed = util::ltrim(current);
        if (trimmed.empty()) continue;
        std::size_t offset = static_cast<std::size_t>(current.size() - trimmed.size());
        if (allCommented) {
            if (util::startsWith(trimmed, actual + ' ')) {
                current.erase(offset, actual.size() + 1);
            } else if (util::startsWith(trimmed, actual)) {
                current.erase(offset, actual.size());
            }
        } else {
            current.insert(offset, actual + " ");
        }
    }
    caret_ = clamp(caret_);
    anchor_ = caret_;
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::wrapSelection(std::string_view prefix, std::string_view suffix) {
    const std::string inner = hasSelection() ? selectedText() : std::string();
    pushUndo();
    if (hasSelection()) deleteRange(selectionStart(), selectionEnd());
    insertAtCaret(std::string(prefix) + inner + std::string(suffix), EditContext{});
    if (!inner.empty()) {
        caret_.column -= static_cast<int>(suffix.size());
        anchor_ = Position{caret_.line, caret_.column - static_cast<int>(inner.size())};
        selectionActive_ = true;
    } else {
        selectionActive_ = false;
    }
    return true;
}

void Document::moveCaret(int lineDelta, int columnDelta, bool extendSelection) {
    Position target = caret_;
    if (lineDelta != 0) {
        target.line = std::clamp(target.line + lineDelta, 0, static_cast<int>(lines_.size()) - 1);
        const int wanted = target.column;
        target.column = std::min(wanted, static_cast<int>(line(target.line).size()));
        target = clamp(target);
    }
    if (columnDelta != 0) {
        target.column += columnDelta;
        target.column = std::clamp(target.column, 0, static_cast<int>(line(target.line).size()));
        target = clamp(target);
    }
    setCaret(target, extendSelection);
}

void Document::moveWord(int direction, bool extendSelection) {
    Position target = caret_;
    const std::string& current = line(target.line);
    if (direction < 0) {
        if (target.column == 0) {
            setCaret(positionBefore(target), extendSelection);
            return;
        }
        std::size_t probe = static_cast<std::size_t>(target.column);
        while (probe > 0 && !isWordChar(current[probe - 1])) --probe;
        while (probe > 0 && isWordChar(current[probe - 1])) --probe;
        target.column = static_cast<int>(probe);
    } else {
        const std::size_t size = current.size();
        std::size_t probe = static_cast<std::size_t>(target.column);
        while (probe < size && !isWordChar(current[probe])) ++probe;
        while (probe < size && isWordChar(current[probe])) ++probe;
        if (probe >= size && target.line + 1 < static_cast<int>(lines_.size())) {
            target = Position{target.line + 1, 0};
        } else {
            target.column = static_cast<int>(probe);
        }
    }
    setCaret(target, extendSelection);
}

void Document::moveToLineEdge(bool toStart, bool extendSelection) {
    Position target = caret_;
    const std::string& current = line(target.line);
    if (toStart) {
        std::size_t column = 0;
        while (column < current.size() && (current[column] == ' ' || current[column] == '\t')) ++column;
        if (column == static_cast<std::size_t>(target.column)) column = 0;
        target.column = static_cast<int>(column);
    } else {
        std::size_t column = current.size();
        while (column > 0 && (current[column - 1] == ' ' || current[column - 1] == '\t')) --column;
        if (column == static_cast<std::size_t>(target.column)) column = current.size();
        target.column = static_cast<int>(column);
    }
    setCaret(target, extendSelection);
}

void Document::moveToDocument(bool toStart, bool extendSelection) {
    if (toStart) {
        setCaret(Position{0, 0}, extendSelection);
    } else {
        setCaret(Position{static_cast<int>(lines_.size()) - 1,
                          static_cast<int>(line(static_cast<int>(lines_.size()) - 1).size())},
                 extendSelection);
    }
}

bool Document::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(lines_);
    lines_ = std::move(undo_.back());
    undo_.pop_back();
    caret_ = clamp(caret_);
    anchor_ = clamp(anchor_);
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(lines_);
    lines_ = std::move(redo_.back());
    redo_.pop_back();
    caret_ = clamp(caret_);
    anchor_ = clamp(anchor_);
    selectionActive_ = false;
    modified_ = true;
    ++version_;
    return true;
}
