#include "highlight.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

#include "util.h"

namespace syntax {
namespace {

struct LangSpec {
    std::string id;
    std::string display;
    std::vector<std::string> lineComments;
    std::string blockOpen;
    std::string blockClose;
    std::string stringDelimiters = "\"'";
    bool backtick = false;
    bool tripleQuote = false;
    bool dollarInIdentifier = false;
    bool preprocessor = false;
    std::unordered_set<std::string> keywords;
    std::unordered_set<std::string> types;
    std::unordered_set<std::string> builtins;
    std::unordered_set<std::string> constants;
};

std::unordered_set<std::string> set(std::initializer_list<const char*> items) {
    std::unordered_set<std::string> out;
    for (const char* item : items) out.insert(item);
    return out;
}

const LangSpec& specFor(const std::string& id) {
    static const std::unordered_map<std::string, LangSpec>* table = [] {
        auto* map = new std::unordered_map<std::string, LangSpec>();

        auto add = [map](LangSpec spec) {
            map->emplace(spec.id, std::move(spec));
        };

        LangSpec c;
        c.id = "c";
        c.display = "C";
        c.lineComments = {"//"};
        c.blockOpen = "/*";
        c.blockClose = "*/";
        c.preprocessor = true;
        c.keywords = set({"auto", "break", "case", "const", "continue", "default", "do", "else",
                          "enum", "extern", "for", "goto", "if", "inline", "register",
                          "restrict", "return", "sizeof", "static", "struct", "switch", "typedef",
                          "union", "volatile", "while", "_Alignas", "_Atomic", "_Bool", "_Static_assert",
                          "asm", "__attribute__", "__inline__", "thread_local"});
        c.types = set({"char", "double", "float", "int", "long", "short", "signed", "unsigned",
                       "void", "size_t", "ssize_t", "int8_t", "int16_t", "int32_t", "int64_t",
                       "uint8_t", "uint16_t", "uint32_t", "uint64_t", "FILE", "bool"});
        c.constants = set({"NULL", "true", "false"});
        add(std::move(c));

        LangSpec cpp;
        cpp.id = "cpp";
        cpp.display = "C++";
        cpp.lineComments = {"//"};
        cpp.blockOpen = "/*";
        cpp.blockClose = "*/";
        cpp.preprocessor = true;
        cpp.keywords = set({"alignas", "alignof", "and", "asm", "auto", "break", "case", "catch",
                            "class", "concept", "const", "consteval", "constexpr", "constinit",
                            "const_cast", "continue", "co_await", "co_return", "co_yield",
                            "decltype", "default", "delete", "do", "dynamic_cast", "else",
                            "enum", "explicit", "export", "extern", "for", "friend", "goto", "if",
                            "inline", "mutable", "namespace", "new", "noexcept", "operator",
                            "private", "protected", "public", "requires", "return", "sizeof",
                            "static", "static_assert", "static_cast", "struct", "switch", "template",
                            "this", "thread_local", "throw", "try", "typedef", "typeid",
                            "typename", "union", "using", "virtual", "volatile", "while", "xor"});
        cpp.types = set({"auto", "bool", "char", "char8_t", "char16_t", "char32_t", "double",
                         "float", "int", "long", "short", "signed", "unsigned", "void", "wchar_t",
                         "size_t", "string", "string_view", "vector", "map", "set", "optional",
                         "unique_ptr", "shared_ptr", "uint32_t", "int64_t"});
        cpp.constants = set({"nullptr", "true", "false", "NULL"});
        add(std::move(cpp));

        LangSpec js;
        js.id = "javascript";
        js.display = "JavaScript";
        js.lineComments = {"//"};
        js.blockOpen = "/*";
        js.blockClose = "*/";
        js.backtick = true;
        js.dollarInIdentifier = true;
        js.keywords = set({"as", "async", "await", "break", "case", "catch", "class", "const",
                           "continue", "debugger", "default", "delete", "do", "else", "export",
                           "extends", "finally", "for", "from", "function", "get", "if", "import",
                           "in", "instanceof", "let", "new", "of", "return", "set", "static",
                           "super", "switch", "this", "throw", "try", "typeof", "var", "void",
                           "while", "with", "yield"});
        js.builtins = set({"console", "document", "window", "Math", "JSON", "Object", "Array",
                           "String", "Number", "Boolean", "Promise", "Symbol", "Map", "Set",
                           "Date", "RegExp", "Error", "process", "require", "module", "exports",
                           "globalThis", "fetch", "setTimeout", "setInterval", "clearTimeout"});
        js.constants = set({"true", "false", "null", "undefined", "NaN", "Infinity"});
        add(std::move(js));

        LangSpec ts = js;
        ts.id = "typescript";
        ts.display = "TypeScript";
        ts.keywords = set({"abstract", "any", "as", "async", "await", "break", "case", "catch",
                           "class", "const", "constructor", "continue", "declare", "default",
                           "delete", "do", "else", "enum", "export", "extends", "finally", "for",
                           "from", "function", "get", "if", "implements", "import", "in",
                           "infer", "instanceof", "interface", "keyof", "let", "namespace",
                           "never", "new", "of", "private", "protected", "public", "readonly",
                           "return", "satisfies", "set", "static", "super", "switch", "this",
                           "throw", "try", "type", "typeof", "undefined", "unknown", "var",
                           "void", "while", "yield"});
        ts.types = set({"string", "number", "boolean", "object", "symbol", "bigint", "any",
                        "unknown", "never", "void", "Array", "Promise", "Record", "Partial"});
        add(std::move(ts));

        LangSpec py;
        py.id = "python";
        py.display = "Python";
        py.lineComments = {"#"};
        py.tripleQuote = true;
        py.dollarInIdentifier = false;
        py.keywords = set({"and", "as", "assert", "async", "await", "break", "class", "continue",
                           "def", "del", "elif", "else", "except", "finally", "for", "from",
                           "global", "if", "import", "in", "is", "lambda", "match", "nonlocal",
                           "not", "or", "pass", "raise", "return", "try", "while", "with",
                           "yield", "case"});
        py.builtins = set({"abs", "all", "any", "bool", "dict", "enumerate", "filter", "float",
                           "format", "frozenset", "hasattr", "hash", "input", "int", "isinstance",
                           "iter", "len", "list", "map", "max", "min", "next", "object", "open",
                           "ord", "pow", "print", "range", "repr", "reversed", "round", "set",
                           "sorted", "str", "sum", "super", "tuple", "type", "zip", "self",
                           "cls", "Exception", "ValueError", "TypeError", "KeyError",
                           "IndexError", "RuntimeError"});
        py.constants = set({"True", "False", "None"});
        add(std::move(py));

        LangSpec go;
        go.id = "go";
        go.display = "Go";
        go.lineComments = {"//"};
        go.blockOpen = "/*";
        go.blockClose = "*/";
        go.backtick = true;
        go.keywords = set({"break", "case", "chan", "const", "continue", "default", "defer", "else",
                           "fallthrough", "for", "func", "go", "goto", "if", "import", "interface",
                           "map", "package", "range", "return", "select", "struct", "switch",
                           "type", "var"});
        go.types = set({"bool", "byte", "complex64", "complex128", "error", "float32", "float64",
                        "int", "int8", "int16", "int32", "int64", "rune", "string", "uint",
                        "uint8", "uint16", "uint32", "uint64", "uintptr", "any"});
        go.constants = set({"true", "false", "nil", "iota"});
        go.builtins = set({"append", "cap", "close", "copy", "delete", "len", "make", "new",
                           "panic", "print", "println", "recover", "fmt"});
        add(std::move(go));

        LangSpec rs;
        rs.id = "rust";
        rs.display = "Rust";
        rs.lineComments = {"//"};
        rs.blockOpen = "/*";
        rs.blockClose = "*/";
        rs.keywords = set({"as", "async", "await", "break", "const", "continue", "crate", "dyn",
                           "else", "enum", "extern", "fn", "for", "if", "impl", "in", "let",
                           "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
                           "self", "static", "struct", "super", "trait", "type", "unsafe",
                           "use", "where", "while"});
        rs.types = set({"bool", "char", "f32", "f64", "i8", "i16", "i32", "i64", "i128", "isize",
                        "str", "String", "u8", "u16", "u32", "u64", "u128", "usize", "Vec",
                        "Option", "Result", "Box", "HashMap"});
        rs.constants = set({"true", "false", "None", "Some", "Ok", "Err"});
        rs.builtins = set({"println", "print", "format", "vec", "panic", "assert", "assert_eq",
                           "write", "writeln", "dbg"});
        add(std::move(rs));

        LangSpec sh;
        sh.id = "shell";
        sh.display = "Shell";
        sh.lineComments = {"#"};
        sh.dollarInIdentifier = true;
        sh.stringDelimiters = "\"'";
        sh.keywords = set({"if", "then", "else", "elif", "fi", "for", "while", "do", "done",
                           "case", "esac", "function", "return", "in", "select", "until", "break",
                           "continue", "local", "export", "readonly", "declare", "source", "eval",
                           "exit", "trap", "set"});
        sh.builtins = set({"echo", "cd", "pwd", "ls", "cat", "grep", "sed", "awk", "curl", "git",
                           "npm", "node", "python", "python3", "make", "sudo", "chmod", "chown",
                           "mkdir", "rm", "cp", "mv", "find", "sort", "head", "tail", "printf",
                           "test", "kill", "ps", "env", "which", "date", "sleep", "diff"});
        sh.constants = set({"true", "false"});
        add(std::move(sh));

        LangSpec lua;
        lua.id = "lua";
        lua.display = "Lua";
        lua.lineComments = {"--"};
        lua.blockOpen = "--[[";
        lua.blockClose = "]]";
        lua.keywords = set({"and", "break", "do", "else", "elseif", "end", "for", "function",
                            "goto", "if", "in", "local", "not", "or", "repeat", "return", "then",
                            "until", "while"});
        lua.builtins = set({"print", "pairs", "ipairs", "type", "tostring", "tonumber", "table",
                            "string", "math", "io", "os", "require", "pcall", "error", "select",
                            "setmetatable", "getmetatable", "rawget", "rawset", "assert"});
        lua.constants = set({"true", "false", "nil"});
        add(std::move(lua));

        LangSpec rb;
        rb.id = "ruby";
        rb.display = "Ruby";
        rb.lineComments = {"#"};
        rb.keywords = set({"alias", "and", "begin", "break", "case", "class", "def", "defined?",
                           "do", "else", "elsif", "end", "ensure", "for", "if", "in", "module",
                           "next", "not", "or", "redo", "rescue", "retry", "return", "self",
                           "super", "then", "undef", "unless", "until", "when", "while", "yield"});
        rb.builtins = set({"attr_accessor", "attr_reader", "attr_writer", "define_method", "each",
                           "include", "lambda", "loop", "map", "new", "puts", "print", "p",
                           "pp", "raise", "require", "require_relative", "select", "to_s"});
        rb.constants = set({"true", "false", "nil", "__FILE__", "__LINE__"});
        add(std::move(rb));

        LangSpec sql;
        sql.id = "sql";
        sql.display = "SQL";
        sql.lineComments = {"--"};
        sql.blockOpen = "/*";
        sql.blockClose = "*/";
        sql.keywords = set({"ADD", "ALL", "ALTER", "AND", "AS", "ASC", "BEGIN", "BETWEEN", "BY",
                            "CASE", "CREATE", "DELETE", "DESC", "DISTINCT", "DROP", "ELSE",
                            "END", "EXISTS", "FROM", "FULL", "GROUP", "HAVING", "IN", "INDEX",
                            "INNER", "INSERT", "INTO", "IS", "JOIN", "LEFT", "LIKE", "LIMIT",
                            "NOT", "NULL", "OFFSET", "ON", "OR", "ORDER", "OUTER", "PRIMARY",
                            "REFERENCES", "RIGHT", "SELECT", "SET", "TABLE", "THEN", "UNION",
                            "UNIQUE", "UPDATE", "VALUES", "VIEW", "WHEN", "WHERE", "WITH",
                            "select", "from", "where", "join", "left", "right", "inner", "outer",
                            "on", "group", "order", "by", "limit", "offset", "insert", "into",
                            "values", "update", "set", "delete", "create", "table", "drop", "alter",
                            "and", "or", "not", "null", "as", "distinct", "union", "all",
                            "having", "case", "when", "then", "else", "end"});
        sql.types = set({"INT", "INTEGER", "BIGINT", "SMALLINT", "TEXT", "VARCHAR", "CHAR",
                         "REAL", "DOUBLE", "BLOB", "BOOLEAN", "DATE", "TIMESTAMP", "NUMERIC",
                         "FLOAT"});
        add(std::move(sql));

        LangSpec json;
        json.id = "json";
        json.display = "JSON";
        json.constants = set({"true", "false", "null"});
        add(std::move(json));

        LangSpec html;
        html.id = "html";
        html.display = "HTML";
        add(std::move(html));

        LangSpec css;
        css.id = "css";
        css.display = "CSS";
        css.blockOpen = "/*";
        css.blockClose = "*/";
        css.lineComments.clear();
        add(std::move(css));

        LangSpec java;
        java.id = "java";
        java.display = "Java";
        java.lineComments = {"//"};
        java.blockOpen = "/*";
        java.blockClose = "*/";
        java.keywords = set({"abstract", "assert", "break", "case", "catch", "class", "const",
                             "continue", "default", "do", "else", "enum", "extends", "final",
                             "finally", "for", "if", "implements", "import", "instanceof",
                             "interface", "native", "new", "package", "private", "protected",
                             "public", "return", "static", "strictfp", "super", "switch",
                             "synchronized", "this", "throw", "throws", "transient", "try",
                             "volatile", "while", "var", "record", "sealed"});
        java.types = set({"boolean", "byte", "char", "double", "float", "int", "long", "short",
                          "void", "String", "Integer", "Double", "Boolean", "Object", "List",
                          "Map", "Set"});
        java.constants = set({"true", "false", "null"});
        add(std::move(java));

        LangSpec kt;
        kt.id = "kotlin";
        kt.display = "Kotlin";
        kt.lineComments = {"//"};
        kt.blockOpen = "/*";
        kt.blockClose = "*/";
        kt.keywords = set({"as", "break", "class", "companion", "continue", "data", "do", "else",
                           "enum", "false", "for", "fun", "if", "import", "in", "interface",
                           "internal", "is", "object", "package", "private", "protected",
                           "public", "return", "sealed", "super", "this", "throw", "try",
                           "typealias", "val", "var", "when", "while"});
        kt.types = set({"Boolean", "Byte", "Char", "Double", "Float", "Int", "Long", "Short",
                        "String", "Unit", "List", "Map"});
        kt.constants = set({"true", "false", "null"});
        add(std::move(kt));

        LangSpec cs;
        cs.id = "csharp";
        cs.display = "C#";
        cs.lineComments = {"//"};
        cs.blockOpen = "/*";
        cs.blockClose = "*/";
        cs.preprocessor = true;
        cs.keywords = set({"abstract", "as", "async", "await", "base", "break", "case", "catch",
                           "class", "const", "continue", "default", "delegate", "do", "else",
                           "enum", "event", "explicit", "extern", "finally", "for", "foreach",
                           "goto", "if", "implicit", "in", "interface", "internal", "is", "lock",
                           "namespace", "new", "operator", "out", "override", "params",
                           "private", "protected", "public", "readonly", "record", "ref",
                           "return", "sealed", "static", "struct", "switch", "this", "throw",
                           "try", "typeof", "using", "var", "virtual", "volatile", "while",
                           "yield"});
        cs.types = set({"bool", "byte", "char", "decimal", "double", "float", "int", "long",
                        "object", "sbyte", "short", "string", "uint", "ulong", "ushort", "void",
                        "List", "Dictionary", "Task", "Console"});
        cs.constants = set({"true", "false", "null"});
        add(std::move(cs));

        LangSpec php;
        php.id = "php";
        php.display = "PHP";
        php.lineComments = {"//", "#"};
        php.blockOpen = "/*";
        php.blockClose = "*/";
        php.dollarInIdentifier = true;
        php.keywords = set({"abstract", "and", "array", "as", "break", "callable", "case",
                            "catch", "class", "clone", "const", "continue", "declare", "default",
                            "do", "echo", "else", "elseif", "empty", "enddeclare", "endfor",
                            "endforeach", "endif", "endswitch", "endwhile", "extends", "final",
                            "finally", "fn", "for", "foreach", "function", "global", "if",
                            "include", "instanceof", "interface", "isset", "list", "match",
                            "namespace", "new", "or", "print", "private", "protected", "public",
                            "require", "return", "static", "switch", "throw", "trait", "try",
                            "unset", "use", "var", "while", "xor", "yield"});
        php.constants = set({"true", "false", "null", "TRUE", "FALSE", "NULL"});
        php.builtins = set({"echo", "print_r", "var_dump", "count", "array_map", "array_filter",
                            "strlen", "sprintf"});
        add(std::move(php));

        LangSpec swift;
        swift.id = "swift";
        swift.display = "Swift";
        swift.lineComments = {"//"};
        swift.blockOpen = "/*";
        swift.blockClose = "*/";
        swift.keywords = set({"associatedtype", "class", "deinit", "enum", "extension", "func",
                              "if", "import", "in", "init", "inout", "internal", "let", "open",
                              "operator", "private", "protocol", "public", "return", "self",
                              "static", "struct", "subscript", "typealias", "var", "where",
                              "while", "guard", "defer", "do", "catch", "throw", "throws",
                              "try", "switch", "case", "default", "for", "in", "repeat", "while",
                              "is", "as", "some", "any", "init", "extension", "lazy", "weak",
                              "unowned", "final", "override", "required", "convenience",
                              "mutating", "nonmutating"});
        swift.types = set({"Int", "Double", "Float", "Bool", "String", "Character", "Array",
                           "Dictionary", "Set", "Optional", "Void"});
        swift.constants = set({"true", "false", "nil"});
        add(std::move(swift));

        LangSpec zig;
        zig.id = "zig";
        zig.display = "Zig";
        zig.lineComments = {"//"};
        zig.keywords = set({"align", "allowzero", "and", "anytype", "asm", "async", "await",
                            "break", "catch", "comptime", "const", "continue", "defer", "else",
                            "enum", "errdefer", "error", "export", "extern", "fn", "for", "if",
                            "inline", "noalias", "nosuspend", "opaque", "or", "orelse", "packed",
                            "pub", "resume", "return", "struct", "suspend", "switch", "test",
                            "threadlocal", "try", "union", "unreachable", "usingnamespace", "var",
                            "volatile", "while"});
        zig.types = set({"bool", "f16", "f32", "f64", "i8", "i16", "i32", "i64", "u8", "u16",
                         "u32", "u64", "void", "type"});
        zig.constants = set({"true", "false", "null", "undefined"});
        add(std::move(zig));

        LangSpec dart;
        dart.id = "dart";
        dart.display = "Dart";
        dart.lineComments = {"//"};
        dart.blockOpen = "/*";
        dart.blockClose = "*/";
        dart.keywords = set({"abstract", "as", "async", "await", "break", "case", "catch", "class",
                             "const", "continue", "covariant", "default", "deferred", "do",
                             "dynamic", "else", "enum", "export", "extends", "extension",
                             "external", "factory", "final", "finally", "for", "get", "if",
                             "implements", "import", "in", "is", "late", "library", "mixin",
                             "new", "on", "operator", "part", "required", "rethrow", "return",
                             "set", "show", "static", "super", "switch", "sync", "this", "throw",
                             "try", "typedef", "var", "void", "while", "with", "yield"});
        dart.types = set({"bool", "double", "dynamic", "int", "num", "String", "List", "Map",
                          "Set", "Future", "Stream"});
        dart.constants = set({"true", "false", "null"});
        add(std::move(dart));

        LangSpec r;
        r.id = "r";
        r.display = "R";
        r.lineComments = {"#"};
        r.keywords = set({"if", "else", "repeat", "while", "function", "for", "next", "break",
                          "TRUE", "FALSE", "NULL", "Inf", "NaN", "NA", "in"});
        r.builtins = set({"c", "cat", "length", "list", "matrix", "mean", "paste", "plot",
                          "print", "seq", "sum", "vector"});
        r.constants = set({"TRUE", "FALSE", "NULL", "NA", "Inf", "NaN"});
        add(std::move(r));

        LangSpec diff;
        diff.id = "diff";
        diff.display = "Diff";
        add(std::move(diff));

        LangSpec text;
        text.id = "text";
        text.display = "Plain text";
        add(std::move(text));

        return map;
    }();
    static const LangSpec fallback = [] {
        LangSpec spec;
        spec.id = "text";
        spec.display = "Plain text";
        return spec;
    }();
    const auto it = table->find(id);
    return it == table->end() ? fallback : it->second;
}

bool isIdentStart(char c, bool dollar) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || (dollar && c == '$');
}

bool isIdentChar(char c, bool dollar) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || (dollar && c == '$');
}

void push(std::vector<Span>& spans, std::size_t start, std::size_t end, Token token) {
    if (end > start) spans.push_back({start, end - start, token});
}

bool startsWithAt(std::string_view code, std::size_t i, std::string_view needle) {
    return needle.size() > 0 && code.compare(i, needle.size(), needle) == 0;
}

void highlightClike(std::string_view code, const LangSpec& spec, std::vector<Span>& spans) {
    const std::size_t n = code.size();
    std::size_t i = 0;
    bool atLineStart = true;
    while (i < n) {
        const char c = code[i];
        if (c == '\n') {
            atLineStart = true;
            ++i;
            continue;
        }
        if (atLineStart && spec.preprocessor && c == '#') {
            const std::size_t start = i;
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n && code[i + 1] == '\n') {
                    i += 2;
                    continue;
                }
                if (code[i] == '\n') break;
                ++i;
            }
            push(spans, start, i, Token::Meta);
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
            continue;
        }
        atLineStart = false;

        bool matchedComment = false;
        for (const std::string& marker : spec.lineComments) {
            if (startsWithAt(code, i, marker)) {
                const std::size_t start = i;
                while (i < n && code[i] != '\n') ++i;
                push(spans, start, i, Token::Comment);
                matchedComment = true;
                break;
            }
        }
        if (matchedComment) continue;
        if (!spec.blockOpen.empty() && startsWithAt(code, i, spec.blockOpen)) {
            const std::size_t start = i;
            i += spec.blockOpen.size();
            while (i < n && !startsWithAt(code, i, spec.blockClose)) ++i;
            if (i < n) i += spec.blockClose.size();
            push(spans, start, i, Token::Comment);
            continue;
        }

        const bool triple = spec.tripleQuote && (startsWithAt(code, i, "\"\"\"") ||
                                                 startsWithAt(code, i, "'''"));
        char delim = 0;
        if (triple) {
            delim = code[i];
        } else if (spec.stringDelimiters.find(c) != std::string::npos) {
            delim = c;
        } else if (spec.backtick && c == '`') {
            delim = c;
        }
        if (delim != 0) {
            const std::size_t start = i;
            if (triple) {
                const std::string fence(3, delim);
                i += 3;
                while (i < n && !startsWithAt(code, i, fence)) ++i;
                if (i < n) i += 3;
            } else {
                ++i;
                while (i < n && code[i] != delim) {
                    if (code[i] == '\\' && i + 1 < n) {
                        i += 2;
                        continue;
                    }
                    if (code[i] == '\n' && delim != '`') break;
                    ++i;
                }
                if (i < n && code[i] == delim) ++i;
            }
            push(spans, start, i, Token::String);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c)) != 0 ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(code[i + 1])) != 0)) {
            const std::size_t start = i;
            if (c == '0' && i + 1 < n && (code[i + 1] == 'x' || code[i + 1] == 'X' ||
                                          code[i + 1] == 'b' || code[i + 1] == 'B' ||
                                          code[i + 1] == 'o' || code[i + 1] == 'O')) {
                i += 2;
            }
            while (i < n && (std::isalnum(static_cast<unsigned char>(code[i])) != 0 ||
                             code[i] == '.' || code[i] == '_')) {
                ++i;
            }
            push(spans, start, i, Token::Number);
            continue;
        }

        if (isIdentStart(c, spec.dollarInIdentifier)) {
            const std::size_t start = i;
            while (i < n && isIdentChar(code[i], spec.dollarInIdentifier)) ++i;
            const std::string word(code.substr(start, i - start));
            Token token = Token::Plain;
            if (spec.keywords.count(word) != 0) {
                token = Token::Keyword;
            } else if (spec.types.count(word) != 0) {
                token = Token::Type;
            } else if (spec.builtins.count(word) != 0) {
                token = Token::Builtin;
            } else if (spec.constants.count(word) != 0) {
                token = Token::Constant;
            } else {
                std::size_t probe = i;
                while (probe < n && (code[probe] == ' ' || code[probe] == '\t')) ++probe;
                if (probe < n && (code[probe] == '(' || code[probe] == '<')) {
                    token = Token::Function;
                } else if (!word.empty() && std::isupper(static_cast<unsigned char>(word[0])) != 0) {
                    token = Token::Type;
                }
            }
            push(spans, start, i, token);
            continue;
        }

        if (std::ispunct(static_cast<unsigned char>(c)) != 0) {
            const std::size_t start = i;
            while (i < n && std::ispunct(static_cast<unsigned char>(code[i])) != 0) ++i;
            push(spans, start, i, Token::Operator);
            continue;
        }
        ++i;
    }
}

void highlightMarkup(std::string_view code, std::vector<Span>& spans) {
    const std::size_t n = code.size();
    std::size_t i = 0;
    while (i < n) {
        if (code[i] == '<') {
            if (code.compare(i, 4, "<!--") == 0) {
                const std::size_t start = i;
                i += 4;
                while (i < n && code.compare(i, 3, "-->") != 0) ++i;
                if (i < n) i += 3;
                push(spans, start, i, Token::Comment);
                continue;
            }
            const std::size_t tagStart = i;
            ++i;
            push(spans, tagStart, i, Token::Punctuation);
            const bool closing = i < n && code[i] == '/';
            if (closing) {
                ++i;
                push(spans, i - 1, i, Token::Punctuation);
            }
            const std::size_t nameStart = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(code[i])) != 0 || code[i] == '-')) ++i;
            push(spans, nameStart, i, Token::Tag);
            while (i < n && code[i] != '>') {
                while (i < n && (code[i] == ' ' || code[i] == '\n' || code[i] == '\t' || code[i] == '\r')) ++i;
                if (i >= n || code[i] == '>' || code[i] == '/') break;
                const std::size_t attrStart = i;
                while (i < n && code[i] != '=' && code[i] != '>' && code[i] != ' ' && code[i] != '\n') ++i;
                push(spans, attrStart, i, Token::Attribute);
                if (i < n && code[i] == '=') {
                    push(spans, i, i + 1, Token::Operator);
                    ++i;
                    if (i < n && (code[i] == '"' || code[i] == '\'')) {
                        const char quote = code[i];
                        const std::size_t valueStart = i;
                        ++i;
                        while (i < n && code[i] != quote) ++i;
                        if (i < n) ++i;
                        push(spans, valueStart, i, Token::String);
                    }
                }
            }
            if (i < n) {
                push(spans, i, i + 1, Token::Punctuation);
                ++i;
            }
            continue;
        }
        const std::size_t start = i;
        while (i < n && code[i] != '<') ++i;
        push(spans, start, i, Token::Plain);
    }
}

void highlightStylesheet(std::string_view code, std::vector<Span>& spans) {
    const std::size_t n = code.size();
    std::size_t i = 0;
    bool inBlock = false;
    bool inValue = false;
    while (i < n) {
        const char c = code[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++i;
            continue;
        }
        if (c == '/' && code.compare(i, 2, "/*") == 0) {
            const std::size_t start = i;
            i += 2;
            while (i < n && code.compare(i, 2, "*/") != 0) ++i;
            if (i < n) i += 2;
            push(spans, start, i, Token::Comment);
            continue;
        }
        if (c == '"' || c == '\'') {
            const std::size_t start = i;
            const char quote = c;
            ++i;
            while (i < n && code[i] != quote) ++i;
            if (i < n) ++i;
            push(spans, start, i, Token::String);
            continue;
        }
        if (c == '{') {
            inBlock = true;
            inValue = false;
            push(spans, i, i + 1, Token::Punctuation);
            ++i;
            continue;
        }
        if (c == '}') {
            inBlock = false;
            inValue = false;
            push(spans, i, i + 1, Token::Punctuation);
            ++i;
            continue;
        }
        if (c == ';') {
            inValue = false;
            push(spans, i, i + 1, Token::Punctuation);
            ++i;
            continue;
        }
        if (c == ':' && inBlock && !inValue) {
            inValue = true;
            push(spans, i, i + 1, Token::Operator);
            ++i;
            continue;
        }
        const std::size_t start = i;
        while (i < n && std::isspace(static_cast<unsigned char>(code[i])) == 0 &&
               code[i] != '{' && code[i] != '}' && code[i] != ';' && code[i] != ':') {
            ++i;
        }
        if (i == start) {
            ++i;
            continue;
        }
        if (inBlock && inValue) {
            push(spans, start, i, Token::Constant);
        } else if (inBlock) {
            push(spans, start, i, Token::Property);
        } else {
            push(spans, start, i, Token::Function);
        }
    }
}

void highlightDiff(std::string_view code, std::vector<Span>& spans) {
    std::size_t start = 0;
    for (std::size_t i = 0; i <= code.size(); ++i) {
        if (i == code.size() || code[i] == '\n') {
            if (i > start) {
                const char head = code[start];
                Token token = Token::Plain;
                if (head == '+') token = Token::String;
                else if (head == '-') token = Token::Removed;
                else if (head == '@') token = Token::Meta;
                push(spans, start, i, token);
            }
            start = i + 1;
        }
    }
}

}  // namespace

std::string normalizeLanguage(std::string_view raw) {
    std::string id = util::toLower(util::trim(raw));
    if (const std::size_t space = id.find_first_of(" \t:{("); space != std::string::npos) {
        id = id.substr(0, space);
    }
    static const std::unordered_map<std::string, std::string> aliases = {
        {"js", "javascript"},        {"jsx", "javascript"},   {"mjs", "javascript"},
        {"cjs", "javascript"},       {"node", "javascript"},  {"ts", "typescript"},
        {"tsx", "typescript"},       {"py", "python"},        {"python3", "python"},
        {"py3", "python"},           {"rb", "ruby"},          {"rs", "rust"},
        {"golang", "go"},            {"sh", "shell"},         {"bash", "shell"},
        {"zsh", "shell"},            {"shellsession", "shell"},
        {"c++", "cpp"},              {"cxx", "cpp"},          {"cc", "cpp"},
        {"hpp", "cpp"},              {"c#", "csharp"},        {"cs", "csharp"},
        {"dotnet", "csharp"},        {"kt", "kotlin"},        {"kts", "kotlin"},
        {"ps1", "powershell"},       {"posh", "powershell"},  {"pwsh", "powershell"},
        {"yml", "yaml"},             {"md", "text"},          {"markdown", "text"},
        {"txt", "text"},             {"", "text"},            {"plaintext", "text"},
        {"plain", "text"},           {"text", "text"},        {"log", "text"},
        {"htm", "html"},              {"xml", "html"},         {"svg", "html"},
        {"jsonc", "json"},            {"jsx-attr", "javascript"}};
    const auto it = aliases.find(id);
    if (it != aliases.end()) return it->second;
    return id;
}

std::string displayName(std::string_view raw) {
    return specFor(normalizeLanguage(raw)).display;
}

std::vector<Span> highlight(std::string_view code, std::string_view language) {
    std::vector<Span> spans;
    if (code.empty()) return spans;
    const std::string id = normalizeLanguage(language);
    const LangSpec& spec = specFor(id);
    if (id == "html") {
        highlightMarkup(code, spans);
    } else if (id == "css") {
        highlightStylesheet(code, spans);
    } else if (id == "diff") {
        highlightDiff(code, spans);
    } else if (id == "text") {
        return spans;
    } else if (id == "json") {
        LangSpec json = spec;
        json.lineComments.clear();
        json.blockOpen.clear();
        json.tripleQuote = false;
        json.dollarInIdentifier = true;
        json.stringDelimiters = "\"";
        highlightClike(code, json, spans);
    } else {
        highlightClike(code, spec, spans);
    }
    std::sort(spans.begin(), spans.end(),
              [](const Span& a, const Span& b) { return a.start < b.start; });
    return spans;
}

}  // namespace syntax
