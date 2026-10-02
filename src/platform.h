#pragma once

#include <SFML/System/Vector2.hpp>

#include <filesystem>
#include <string>

namespace platform {

// Opts the process into per monitor DPI awareness so nothing gets blurry or
// stretched on scaled displays. Must be called before creating the window.
void enableDpiAwareness();

// Scale factor of the window (1.0 at 96 DPI, 1.5 at 150%).
float windowScale(const void* nativeHandle);

// Usable desktop area of the primary monitor, in pixels.
sf::Vector2u workAreaSize();

void openExternal(const std::string& target);
std::filesystem::path executableDirectory();
std::filesystem::path notesDirectory();
std::filesystem::path tempDirectory();

}  // namespace platform
