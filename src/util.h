#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace util {

std::string trim(std::string_view s);
std::string_view ltrim(std::string_view s);
std::string_view rtrim(std::string_view s);
bool startsWith(std::string_view s, std::string_view prefix);
bool endsWith(std::string_view s, std::string_view suffix);
std::string toLower(std::string_view s);

std::vector<std::string> splitLines(std::string_view text);
std::vector<std::string> split(std::string_view text, char separator);
std::string join(const std::vector<std::string>& parts, std::string_view separator);

struct Utf8Char {
    char32_t codePoint{};
    std::size_t bytes{};
};

std::vector<Utf8Char> decodeUtf8(std::string_view text);
void appendUtf8(std::string& out, char32_t codePoint);
std::string encodeUtf8(char32_t codePoint);
bool isUtf8Boundary(std::string_view text, std::size_t index);
std::size_t nextCharIndex(std::string_view text, std::size_t index);
std::size_t prevCharIndex(std::string_view text, std::size_t index);

std::string uniqueId(std::string_view prefix);
std::string formatTimestamp(const std::string& id);
std::string escapeShellPath(const std::string& path);
std::string collapseSpaces(std::string_view text);

}  // namespace util
