#include "theme.h"

#include <SFML/System/Vector2.hpp>

#include <array>
#include <fstream>
#include <vector>

#include "util.h"

namespace theme {

Palette pal;
Metrics metrics;
sf::Font uiFont;
sf::Font monoFont;

namespace {

struct Candidate {
    const char* relative;
    const char* absolute;
};

const std::array<Candidate, 12> kUiFonts{{
    {"assets/fonts/ui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
    {"assets/fonts/ui.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf"},
    {"assets/fonts/ui.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"},
    {"assets/fonts/ui.ttf", "C:/Windows/Fonts/segoeui.ttf"},
    {"assets/fonts/ui.ttf", "C:/Windows/Fonts/arial.ttf"},
    {"assets/fonts/ui.ttf", "C:/Windows/Fonts/tahoma.ttf"},
    {"assets/fonts/ui.ttf", "C:/Windows/Fonts/verdana.ttf"},
    {"", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
    {"", "/usr/share/fonts/TTF/DejaVuSans.ttf"},
    {"", "/System/Library/Fonts/Supplemental/Arial.ttf"},
    {"", "C:/Windows/Fonts/segoeui.ttf"},
    {"", "C:/Windows/Fonts/arial.ttf"},
}};

const std::array<Candidate, 12> kMonoFonts{{
    {"assets/fonts/mono.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},
    {"assets/fonts/mono.ttf", "/usr/share/fonts/TTF/DejaVuSansMono.ttf"},
    {"assets/fonts/mono.ttf", "/System/Library/Fonts/Menlo.ttc"},
    {"assets/fonts/mono.ttf", "C:/Windows/Fonts/consola.ttf"},
    {"assets/fonts/mono.ttf", "C:/Windows/Fonts/consolab.ttf"},
    {"assets/fonts/mono.ttf", "C:/Windows/Fonts/cour.ttf"},
    {"assets/fonts/mono.ttf", "C:/Windows/Fonts/CascadiaMono.ttf"},
    {"", "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},
    {"", "/usr/share/fonts/TTF/DejaVuSansMono.ttf"},
    {"", "/System/Library/Fonts/Supplemental/Courier New.ttf"},
    {"", "C:/Windows/Fonts/consola.ttf"},
    {"", "C:/Windows/Fonts/cour.ttf"},
}};

bool tryOpen(sf::Font& font, const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    return font.openFromFile(path);
}

Palette midnightPalette() {
    Palette p;
    p.windowBg = {13, 17, 23, 255};
    p.sidebarBg = {17, 21, 29, 255};
    p.editorBg = {13, 17, 23, 255};
    p.previewBg = {11, 14, 20, 255};
    p.headerBg = {19, 24, 33, 255};
    p.statusBg = {16, 21, 28, 255};
    p.border = {38, 46, 58, 255};
    p.borderSoft = {27, 33, 43, 255};
    p.borderStrong = {55, 66, 82, 255};
    p.codeBlockBg = {18, 23, 32, 255};
    p.codeBlockHeader = {25, 31, 43, 255};
    p.codeBlockActive = {21, 28, 38, 255};
    p.hover = {32, 41, 54, 255};
    p.activeRow = {30, 45, 66, 255};
    p.caretLine = {24, 31, 41, 255};
    p.gutterBg = {15, 19, 26, 255};
    p.guide = {46, 56, 70, 255};
    p.quoteBar = {58, 72, 92, 255};
    p.text = {203, 211, 219, 255};
    p.textDim = {138, 147, 158, 255};
    p.textFaint = {106, 115, 126, 255};
    p.heading = {244, 248, 252, 255};
    p.accent = {88, 166, 255, 255};
    p.accentSoft = {30, 62, 103, 255};
    p.onAccent = {236, 242, 250, 255};
    p.success = {86, 211, 100, 255};
    p.warning = {227, 179, 65, 255};
    p.error = {255, 99, 92, 255};
    p.magenta = {218, 175, 255, 255};
    p.selection = {56, 139, 253, 120};
    p.selectionInactive = {110, 120, 132, 60};
    p.caret = {88, 166, 255, 255};
    p.shadow = {0, 0, 0, 120};
    p.scrollTrack = {16, 21, 28, 255};
    p.scrollThumb = {56, 66, 80, 255};
    p.scrollThumbHover = {78, 91, 108, 255};
    p.pill = {30, 38, 50, 255};
    p.codeChipBg = {30, 38, 52, 255};
    p.chromeHi = {22, 28, 38, 255};
    p.accentHi = {120, 184, 255, 255};
    p.synPlain = {173, 184, 197, 255};
    p.synKeyword = {201, 124, 232, 255};
    p.synType = {232, 199, 133, 255};
    p.synString = {158, 206, 129, 255};
    p.synNumber = {219, 164, 110, 255};
    p.synComment = {100, 109, 122, 255};
    p.synFunction = {111, 184, 247, 255};
    p.synBuiltin = {100, 193, 205, 255};
    p.synConstant = {219, 164, 110, 255};
    p.synOperator = {100, 193, 205, 255};
    p.synPunct = {173, 184, 197, 255};
    p.synTag = {232, 121, 131, 255};
    p.synAttribute = {219, 164, 110, 255};
    p.synProperty = {111, 184, 247, 255};
    p.synMeta = {126, 231, 135, 255};
    return p;
}

// Neutral warm grey with a single sand coloured accent.
Palette graphitePalette() {
    Palette p;
    p.windowBg = {18, 18, 20, 255};
    p.sidebarBg = {24, 24, 27, 255};
    p.editorBg = {18, 18, 20, 255};
    p.previewBg = {15, 15, 17, 255};
    p.headerBg = {28, 28, 32, 255};
    p.statusBg = {24, 24, 27, 255};
    p.border = {48, 48, 54, 255};
    p.borderSoft = {36, 36, 41, 255};
    p.borderStrong = {70, 70, 78, 255};
    p.codeBlockBg = {24, 24, 28, 255};
    p.codeBlockHeader = {33, 33, 38, 255};
    p.codeBlockActive = {28, 28, 33, 255};
    p.hover = {41, 41, 47, 255};
    p.activeRow = {46, 50, 60, 255};
    p.caretLine = {31, 31, 36, 255};
    p.gutterBg = {20, 20, 23, 255};
    p.guide = {56, 56, 64, 255};
    p.quoteBar = {74, 74, 84, 255};
    p.text = {214, 214, 218, 255};
    p.textDim = {150, 150, 157, 255};
    p.textFaint = {116, 116, 124, 255};
    p.heading = {250, 250, 252, 255};
    p.accent = {240, 182, 120, 255};
    p.accentSoft = {78, 57, 36, 255};
    p.onAccent = {34, 24, 16, 255};
    p.success = {141, 204, 120, 255};
    p.warning = {232, 190, 110, 255};
    p.error = {240, 120, 110, 255};
    p.magenta = {226, 170, 220, 255};
    p.selection = {240, 182, 120, 100};
    p.selectionInactive = {150, 150, 158, 55};
    p.caret = {240, 182, 120, 255};
    p.shadow = {0, 0, 0, 120};
    p.scrollTrack = {22, 22, 25, 255};
    p.scrollThumb = {64, 64, 72, 255};
    p.scrollThumbHover = {88, 88, 97, 255};
    p.pill = {39, 39, 45, 255};
    p.codeChipBg = {40, 39, 44, 255};
    p.chromeHi = {33, 33, 38, 255};
    p.accentHi = {255, 205, 160, 255};
    p.synPlain = {190, 188, 184, 255};
    p.synKeyword = {226, 168, 220, 255};
    p.synType = {240, 205, 150, 255};
    p.synString = {168, 196, 130, 255};
    p.synNumber = {226, 168, 130, 255};
    p.synComment = {118, 116, 114, 255};
    p.synFunction = {200, 180, 240, 255};
    p.synBuiltin = {140, 192, 190, 255};
    p.synConstant = {226, 168, 130, 255};
    p.synOperator = {140, 192, 190, 255};
    p.synPunct = {190, 188, 184, 255};
    p.synTag = {236, 130, 120, 255};
    p.synAttribute = {226, 168, 130, 255};
    p.synProperty = {200, 180, 240, 255};
    p.synMeta = {150, 210, 150, 255};
    return p;
}

// Cool, low contrast Nord style ramp with frost accents.
Palette nordPalette() {
    Palette p;
    p.windowBg = {46, 52, 64, 255};
    p.sidebarBg = {59, 66, 82, 255};
    p.editorBg = {46, 52, 64, 255};
    p.previewBg = {42, 47, 58, 255};
    p.headerBg = {68, 77, 95, 255};
    p.statusBg = {59, 66, 82, 255};
    p.border = {78, 88, 108, 255};
    p.borderSoft = {63, 72, 89, 255};
    p.borderStrong = {97, 109, 133, 255};
    p.codeBlockBg = {53, 60, 74, 255};
    p.codeBlockHeader = {64, 73, 90, 255};
    p.codeBlockActive = {58, 66, 81, 255};
    p.hover = {72, 82, 101, 255};
    p.activeRow = {66, 84, 110, 255};
    p.caretLine = {63, 72, 89, 255};
    p.gutterBg = {50, 56, 69, 255};
    p.guide = {85, 96, 116, 255};
    p.quoteBar = {104, 117, 140, 255};
    p.text = {216, 222, 233, 255};
    p.textDim = {150, 159, 175, 255};
    p.textFaint = {122, 131, 147, 255};
    p.heading = {236, 239, 244, 255};
    p.accent = {136, 192, 208, 255};
    p.accentSoft = {58, 84, 96, 255};
    p.onAccent = {32, 44, 52, 255};
    p.success = {163, 190, 140, 255};
    p.warning = {235, 203, 139, 255};
    p.error = {191, 97, 106, 255};
    p.magenta = {180, 142, 173, 255};
    p.selection = {136, 192, 208, 90};
    p.selectionInactive = {150, 159, 175, 55};
    p.caret = {136, 192, 208, 255};
    p.shadow = {0, 0, 0, 70};
    p.scrollTrack = {52, 59, 73, 255};
    p.scrollThumb = {86, 97, 116, 255};
    p.scrollThumbHover = {110, 123, 144, 255};
    p.pill = {67, 76, 93, 255};
    p.codeChipBg = {68, 77, 94, 255};
    p.chromeHi = {75, 84, 102, 255};
    p.accentHi = {162, 211, 225, 255};
    p.synPlain = {216, 222, 233, 255};
    p.synKeyword = {129, 161, 193, 255};
    p.synType = {143, 188, 187, 255};
    p.synString = {163, 190, 140, 255};
    p.synNumber = {180, 142, 173, 255};
    p.synComment = {86, 96, 116, 255};
    p.synFunction = {129, 161, 193, 255};
    p.synBuiltin = {208, 135, 112, 255};
    p.synConstant = {208, 135, 112, 255};
    p.synOperator = {129, 161, 193, 255};
    p.synPunct = {216, 222, 233, 255};
    p.synTag = {163, 190, 140, 255};
    p.synAttribute = {143, 188, 187, 255};
    p.synProperty = {129, 161, 193, 255};
    p.synMeta = {235, 203, 139, 255};
    return p;
}

// Light theme, tuned so the same chrome stays readable.
Palette daylightPalette() {
    Palette p;
    p.windowBg = {244, 245, 248, 255};
    p.sidebarBg = {238, 240, 244, 255};
    p.editorBg = {253, 253, 254, 255};
    p.previewBg = {247, 248, 250, 255};
    p.headerBg = {255, 255, 255, 255};
    p.statusBg = {238, 240, 244, 255};
    p.border = {214, 218, 226, 255};
    p.borderSoft = {231, 234, 240, 255};
    p.borderStrong = {184, 190, 200, 255};
    p.codeBlockBg = {246, 247, 250, 255};
    p.codeBlockHeader = {235, 238, 243, 255};
    p.codeBlockActive = {250, 250, 252, 255};
    p.hover = {229, 233, 240, 255};
    p.activeRow = {219, 230, 248, 255};
    p.caretLine = {240, 243, 248, 255};
    p.gutterBg = {247, 248, 251, 255};
    p.guide = {214, 219, 228, 255};
    p.quoteBar = {168, 180, 196, 255};
    p.text = {45, 50, 58, 255};
    p.textDim = {99, 107, 118, 255};
    p.textFaint = {124, 132, 144, 255};
    p.heading = {20, 24, 31, 255};
    p.accent = {26, 105, 224, 255};
    p.accentSoft = {214, 232, 255, 255};
    p.onAccent = {255, 255, 255, 255};
    p.success = {26, 138, 62, 255};
    p.warning = {172, 112, 8, 255};
    p.error = {206, 46, 42, 255};
    p.magenta = {138, 72, 196, 255};
    p.selection = {26, 105, 224, 70};
    p.selectionInactive = {136, 144, 155, 45};
    p.caret = {26, 105, 224, 255};
    p.shadow = {22, 28, 38, 46};
    p.scrollTrack = {240, 241, 246, 255};
    p.scrollThumb = {198, 204, 214, 255};
    p.scrollThumbHover = {168, 176, 188, 255};
    p.pill = {232, 235, 241, 255};
    p.codeChipBg = {231, 236, 243, 255};
    p.chromeHi = {252, 252, 254, 255};
    p.accentHi = {58, 130, 235, 255};
    p.synPlain = {60, 66, 74, 255};
    p.synKeyword = {175, 58, 190, 255};
    p.synType = {26, 105, 224, 255};
    p.synString = {20, 128, 61, 255};
    p.synNumber = {190, 110, 20, 255};
    p.synComment = {126, 134, 146, 255};
    p.synFunction = {40, 80, 200, 255};
    p.synBuiltin = {12, 138, 150, 255};
    p.synConstant = {190, 110, 20, 255};
    p.synOperator = {40, 80, 200, 255};
    p.synPunct = {92, 100, 112, 255};
    p.synTag = {20, 118, 140, 255};
    p.synAttribute = {190, 110, 20, 255};
    p.synProperty = {40, 80, 200, 255};
    p.synMeta = {128, 88, 20, 255};
    return p;
}

std::vector<Theme> buildThemes() {
    return {
        {"midnight", "Midnight", true, midnightPalette()},
        {"graphite", "Graphite", true, graphitePalette()},
        {"nord", "Nord", true, nordPalette()},
        {"daylight", "Daylight", false, daylightPalette()},
    };
}

const char* preferenceFileName() { return "theme.txt"; }

std::filesystem::path preferencePath(const std::filesystem::path& dataDir) {
    return dataDir / preferenceFileName();
}

bool resolve(const std::filesystem::path& resourceDir, const std::array<Candidate, 12>& table,
             sf::Font& font) {
    for (const Candidate& candidate : table) {
        if (candidate.relative && candidate.relative[0] != '\0') {
            if (tryOpen(font, resourceDir / candidate.relative)) return true;
        }
        if (candidate.absolute && candidate.absolute[0] != '\0') {
            if (tryOpen(font, std::filesystem::path(candidate.absolute))) return true;
        }
    }
    return false;
}

}  // namespace

void applyScale(float scale) {
    if (scale <= 0.f) return;
    // Keep the unscaled metrics so a later DPI change (dragging the window to
    // another monitor) can rescale everything again.
    static const Metrics base = metrics;
    metrics = base;
    Metrics& m = metrics;
    m.uiText *= scale;
    m.codeText *= scale;
    m.smallText *= scale;
    m.gutterWidth *= scale;
    m.pad *= scale;
    m.radius *= scale;
    m.headerHeight *= scale;
    m.statusHeight *= scale;
    m.sidebarWidth *= scale;
    m.codeHeaderHeight *= scale;
    m.codePadX *= scale;
    m.codePadY *= scale;
    m.blockGap *= scale;
    m.scrollbarWidth *= scale;
    m.rowHeight *= scale;
    m.searchHeight *= scale;
    m.tinyText *= scale;
    m.scale = scale;
}

bool loadFonts(const std::filesystem::path& resourceDir, std::string& error) {
    if (!resolve(resourceDir, kUiFonts, uiFont)) {
        error = "could not load a UI font (looked in assets/fonts and the system font folders)";
        return false;
    }
    if (!resolve(resourceDir, kMonoFonts, monoFont)) {
        error = "could not load a monospace font (looked in assets/fonts and the system font folders)";
        return false;
    }
    metrics.lineHeight = lineHeight(uiFont, static_cast<unsigned>(metrics.uiText));
    metrics.codeLineHeight = lineHeight(monoFont, static_cast<unsigned>(metrics.codeText));
    if (metrics.codeLineHeight <= 0.f) metrics.codeLineHeight = metrics.codeText * 1.35f;
    if (metrics.lineHeight <= 0.f) metrics.lineHeight = metrics.uiText * 1.35f;
    metrics.guideStep =
        monoFont.getGlyph(U' ', static_cast<unsigned>(metrics.codeText), false).advance;
    return true;
}

float lineHeight(const sf::Font& font, unsigned int size) {
    return font.getLineSpacing(size);
}

namespace {
std::size_t& themeIndex() {
    static std::size_t index = 0;
    return index;
}
}  // namespace

const std::vector<Theme>& themes() {
    static const std::vector<Theme> table = buildThemes();
    return table;
}

std::size_t currentIndex() { return themeIndex(); }

const Theme& currentTheme() { return themes()[themeIndex()]; }

bool applyTheme(std::string_view id) {
    const std::vector<Theme>& table = themes();
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].id != id) continue;
        themeIndex() = i;
        pal = table[i].palette;
        return true;
    }
    return false;
}

std::string_view cycleTheme() {
    const std::vector<Theme>& table = themes();
    themeIndex() = (themeIndex() + 1) % table.size();
    pal = table[themeIndex()].palette;
    return table[themeIndex()].id;
}

bool loadPreference(const std::filesystem::path& dataDir) {
    std::ifstream file(preferencePath(dataDir));
    if (!file) return false;
    std::string id;
    std::getline(file, id);
    while (!id.empty() && (id.back() == '\r' || id.back() == ' ' || id.back() == '\n')) {
        id.pop_back();
    }
    if (id.empty()) return false;
    return applyTheme(id);
}

bool savePreference(const std::filesystem::path& dataDir) {
    std::error_code ec;
    std::filesystem::create_directories(dataDir, ec);
    std::ofstream file(preferencePath(dataDir), std::ios::trunc);
    if (!file) return false;
    file << currentTheme().id << '\n';
    return file.good();
}

}  // namespace theme
