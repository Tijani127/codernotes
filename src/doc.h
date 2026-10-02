#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Position {
    int line = 0;
    int column = 0;  // byte offset inside the line

    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    bool operator!=(const Position& other) const { return !(*this == other); }
    bool operator<(const Position& other) const {
        return line != other.line ? line < other.line : column < other.column;
    }
};

struct EditContext {
    bool inCodeBlock = false;
    bool onFenceLine = false;
};

// Text buffer with a caret, selection, and snapshot based undo history.
class Document {
public:
    void setText(std::string_view text, bool clearHistory = true);
    void setLines(std::vector<std::string> lines, bool clearHistory = true);
    std::string text() const;
    const std::vector<std::string>& lines() const { return lines_; }
    const std::string& line(int index) const;
    int lineCount() const { return static_cast<int>(lines_.size()); }
    bool contains(const Position& position) const;
    Position clamp(Position position) const;
    Position positionAfter(const Position& position) const;
    Position positionBefore(const Position& position) const;

    Position caret() const { return caret_; }
    void setCaret(Position position, bool extendSelection = false);
    bool hasSelection() const { return selectionActive_; }
    Position selectionStart() const;
    Position selectionEnd() const;
    std::string selectedText() const;
    void selectAll();
    void clearSelection();

    bool insertText(std::string_view utf8, const EditContext& context);
    bool insertNewline(const EditContext& context);
    bool insertIndent(const EditContext& context);
    bool removeBackward(const EditContext& context);
    bool removeForward(const EditContext& context);
    bool toggleLineComment(const EditContext& context);
    bool indentSelection(const EditContext& context, bool forward);
    bool wrapSelection(std::string_view prefix, std::string_view suffix);

    void moveCaret(int lineDelta, int columnDelta, bool extendSelection);
    void moveWord(int direction, bool extendSelection);
    void moveToLineEdge(bool toStart, bool extendSelection);
    void moveToDocument(bool toStart, bool extendSelection);

    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    bool modified() const { return modified_; }
    void markSaved() { modified_ = false; }
    unsigned version() const { return version_; }

private:
    void pushUndo();
    void normalizeSelection();
    void insertAtCaret(std::string_view utf8, const EditContext& context);
    void deleteRange(Position from, Position to);

    std::vector<std::string> lines_;
    Position caret_;
    Position anchor_;
    bool selectionActive_ = false;
    bool modified_ = false;
    unsigned version_ = 0;
    std::vector<std::vector<std::string>> undo_;
    std::vector<std::vector<std::string>> redo_;
};
