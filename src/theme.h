#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Color.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace theme {

struct Palette {
    sf::Color windowBg{13, 17, 23, 255};
    sf::Color sidebarBg{17, 21, 29, 255};
    sf::Color editorBg{13, 17, 23, 255};
    sf::Color previewBg{11, 14, 20, 255};
    sf::Color headerBg{19, 24, 33, 255};
    sf::Color statusBg{16, 21, 28, 255};

    sf::Color border{38, 46, 58, 255};
    sf::Color borderSoft{27, 33, 43, 255};
    sf::Color borderStrong{55, 66, 82, 255};
    sf::Color codeBlockBg{18, 23, 32, 255};
    sf::Color codeBlockHeader{25, 31, 43, 255};
    sf::Color codeBlockActive{21, 28, 38, 255};
    sf::Color hover{32, 41, 54, 255};
    sf::Color activeRow{30, 45, 66, 255};
    sf::Color caretLine{24, 31, 41, 255};
    sf::Color gutterBg{15, 19, 26, 255};
    sf::Color guide{46, 56, 70, 255};
    sf::Color quoteBar{58, 72, 92, 255};

    sf::Color text{203, 211, 219, 255};
    sf::Color textDim{138, 147, 158, 255};
    sf::Color textFaint{106, 115, 126, 255};
    sf::Color heading{244, 248, 252, 255};
    sf::Color accent{88, 166, 255, 255};
    sf::Color accentSoft{30, 62, 103, 255};
    sf::Color onAccent{236, 242, 250, 255};
    sf::Color success{86, 211, 100, 255};
    sf::Color warning{227, 179, 65, 255};
    sf::Color error{255, 99, 92, 255};
    sf::Color magenta{218, 175, 255, 255};

    sf::Color selection{56, 139, 253, 120};
    sf::Color selectionInactive{110, 120, 132, 60};
    sf::Color caret{88, 166, 255, 255};
    sf::Color shadow{0, 0, 0, 120};
    sf::Color scrollTrack{16, 21, 28, 255};
    sf::Color scrollThumb{56, 66, 80, 255};
    sf::Color scrollThumbHover{78, 91, 108, 255};
    sf::Color pill{30, 38, 50, 255};
    sf::Color codeChipBg{30, 38, 52, 255};
    sf::Color chromeHi{22, 28, 38, 255};  // header gradient highlight
    sf::Color accentHi{120, 184, 255, 255};  // accent hover / pressed

    // syntax
    sf::Color synPlain{173, 184, 197, 255};
    sf::Color synKeyword{201, 124, 232, 255};
    sf::Color synType{232, 199, 133, 255};
    sf::Color synString{158, 206, 129, 255};
    sf::Color synNumber{219, 164, 110, 255};
    sf::Color synComment{100, 109, 122, 255};
    sf::Color synFunction{111, 184, 247, 255};
    sf::Color synBuiltin{100, 193, 205, 255};
    sf::Color synConstant{219, 164, 110, 255};
    sf::Color synOperator{100, 193, 205, 255};
    sf::Color synPunct{173, 184, 197, 255};
    sf::Color synTag{232, 121, 131, 255};
    sf::Color synAttribute{219, 164, 110, 255};
    sf::Color synProperty{111, 184, 247, 255};
    sf::Color synMeta{126, 231, 135, 255};
};

struct Metrics {
    float scale = 1.f;
    float uiText = 15.f;
    float codeText = 15.f;
    float smallText = 13.f;
    float tinyText = 11.5f;
    float lineHeight = 0.f;
    float codeLineHeight = 0.f;
    float gutterWidth = 46.f;
    float pad = 14.f;
    float radius = 6.f;
    float headerHeight = 46.f;
    float statusHeight = 26.f;
    float sidebarWidth = 236.f;
    float codeHeaderHeight = 26.f;
    float codePadX = 12.f;
    float codePadY = 8.f;
    float blockGap = 6.f;
    float scrollbarWidth = 10.f;
    float rowHeight = 58.f;
    float searchHeight = 28.f;
    float guideStep = 0.f;  // monospace advance used for indent guides
};

struct Theme {
    std::string_view id;
    std::string_view name;
    bool dark = true;
    Palette palette;
};

extern Palette pal;
extern Metrics metrics;
extern sf::Font uiFont;
extern sf::Font monoFont;

bool loadFonts(const std::filesystem::path& resourceDir, std::string& error);
void applyScale(float scale);

float lineHeight(const sf::Font& font, unsigned int size);

// Themes are looked up by id so a saved preference survives a reorder.
const std::vector<Theme>& themes();
const Theme& currentTheme();
std::size_t currentIndex();
// Applies a theme by id. Returns false when the id is unknown.
bool applyTheme(std::string_view id);
// Applies the next theme in the list and returns its id.
std::string_view cycleTheme();

// Reads and writes the chosen theme in <dataDir>/theme.txt.
bool loadPreference(const std::filesystem::path& dataDir);
bool savePreference(const std::filesystem::path& dataDir);

}  // namespace theme
