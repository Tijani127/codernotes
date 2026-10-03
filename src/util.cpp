#include "util.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace util {
namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

}  // namespace

std::string_view ltrim(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && isSpace(s[i])) ++i;
    return s.substr(i);
}

std::string_view rtrim(std::string_view s) {
    std::size_t n = s.size();
    while (n > 0 && isSpace(s[n - 1])) --n;
    return s.substr(0, n);
}

std::string trim(std::string_view s) {
    return std::string(ltrim(rtrim(s)));
}

bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::size_t findFold(std::string_view haystack, std::string_view loweredNeedle) {
    if (loweredNeedle.empty()) return 0;
    if (loweredNeedle.size() > haystack.size()) return std::string_view::npos;
    const auto fold = [](char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    const std::size_t last = haystack.size() - loweredNeedle.size();
    for (std::size_t start = 0; start <= last; ++start) {
        std::size_t i = 0;
        while (i < loweredNeedle.size() && fold(haystack[start + i]) == loweredNeedle[i]) ++i;
        if (i == loweredNeedle.size()) return start;
    }
    return std::string_view::npos;
}

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            std::string_view line = text.substr(start, i - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            lines.emplace_back(line);
            start = i + 1;
        }
    }
    if (text.empty()) lines.emplace_back();
    return lines;
}

std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == separator) {
            parts.emplace_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += separator;
        out += parts[i];
    }
    return out;
}

std::vector<Utf8Char> decodeUtf8(std::string_view text) {
    std::vector<Utf8Char> result;
    result.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        char32_t cp = 0;
        std::size_t len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1Fu;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0Fu;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07u;
            len = 4;
        } else {
            cp = 0xFFFD;
        }
        if (i + len > text.size()) {
            cp = 0xFFFD;
            len = 1;
        } else {
            for (std::size_t k = 1; k < len; ++k) {
                unsigned char cc = static_cast<unsigned char>(text[i + k]);
                if ((cc & 0xC0) != 0x80) {
                    cp = 0xFFFD;
                    len = 1;
                    break;
                }
                cp = (cp << 6) | (cc & 0x3Fu);
            }
        }
        result.push_back({cp, len});
        i += len;
    }
    return result;
}

void appendUtf8(std::string& out, char32_t codePoint) {
    if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) codePoint = 0xFFFD;
    if (codePoint < 0x80) {
        out += static_cast<char>(codePoint);
    } else if (codePoint < 0x800) {
        out += static_cast<char>(0xC0 | (codePoint >> 6));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint < 0x10000) {
        out += static_cast<char>(0xE0 | (codePoint >> 12));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codePoint >> 18));
        out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

std::string encodeUtf8(char32_t codePoint) {
    std::string out;
    appendUtf8(out, codePoint);
    return out;
}

bool isUtf8Boundary(std::string_view text, std::size_t index) {
    if (index == 0 || index >= text.size()) return true;
    return (static_cast<unsigned char>(text[index]) & 0xC0) != 0x80;
}

std::size_t nextCharIndex(std::string_view text, std::size_t index) {
    if (index >= text.size()) return text.size();
    ++index;
    while (index < text.size() && !isUtf8Boundary(text, index)) ++index;
    return index;
}

std::size_t prevCharIndex(std::string_view text, std::size_t index) {
    if (index == 0) return 0;
    std::size_t i = index - 1;
    while (i > 0 && !isUtf8Boundary(text, i)) --i;
    return i;
}

std::string uniqueId(std::string_view prefix) {
    const auto now = std::chrono::system_clock::now();
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s-%lld-%03lld", std::string(prefix).c_str(),
                  static_cast<long long>(secs), static_cast<long long>(millis));
    return buffer;
}

std::string formatTimestamp(const std::string& id) {
    std::time_t raw = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif
    const std::time_t now = raw;
    std::tm parsed{};
    bool haveParsed = false;
    const std::size_t dash = id.find('-');
    if (dash != std::string::npos) {
        char* end = nullptr;
        const long long value = std::strtoll(id.c_str() + dash + 1, &end, 10);
        if (end && end != id.c_str() + dash + 1) {
            const std::time_t stamp = static_cast<std::time_t>(value);
#if defined(_WIN32)
            if (localtime_s(&parsed, &stamp) == 0) haveParsed = true;
#else
            if (localtime_r(&stamp, &parsed) != nullptr) haveParsed = true;
#endif
        }
    }
    const std::tm& use = haveParsed ? parsed : tm;
    const std::time_t when = haveParsed ? static_cast<std::time_t>(std::mktime(&parsed)) : now;
    const std::time_t delta = now - when;
    char buffer[32];
    if (delta < 60) return "just now";
    if (delta < 3600) {
        std::snprintf(buffer, sizeof(buffer), "%lldm ago", static_cast<long long>(delta / 60));
    } else if (delta < 86400) {
        std::snprintf(buffer, sizeof(buffer), "%lldh ago", static_cast<long long>(delta / 3600));
    } else if (delta < 86400 * 7) {
        std::snprintf(buffer, sizeof(buffer), "%lldd ago", static_cast<long long>(delta / 86400));
    } else {
        std::strftime(buffer, sizeof(buffer), "%b %d", &use);
    }
    return buffer;
}

std::string escapeShellPath(const std::string& path) {
    std::string out;
    out.reserve(path.size() + 2);
    out += '"';
    for (char c : path) {
        if (c == '"') out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

std::string collapseSpaces(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool space = false;
    for (char c : text) {
        if (c == ' ' || c == '\t') {
            space = true;
            continue;
        }
        if (space && !out.empty()) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

}  // namespace util
