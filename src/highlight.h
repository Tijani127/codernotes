#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace syntax {

enum class Token {
    Plain,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Function,
    Builtin,
    Constant,
    Operator,
    Punctuation,
    Tag,
    Attribute,
    Property,
    Meta,
    Removed,
};

struct Span {
    std::size_t start = 0;
    std::size_t length = 0;
    Token token = Token::Plain;
};

// Canonical lowercase id: js, javascript -> javascript, py -> python, ...
std::string normalizeLanguage(std::string_view raw);
std::string displayName(std::string_view raw);
std::vector<Span> highlight(std::string_view code, std::string_view language);

}  // namespace syntax
