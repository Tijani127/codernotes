#include "platform.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <ShellAPI.h>
#else
#include <unistd.h>
#endif

namespace platform {

void enableDpiAwareness() {
#if defined(_WIN32)
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) return;
    SetProcessDPIAware();
#endif
}

float windowScale(const void* nativeHandle) {
#if defined(_WIN32)
    HWND handle = nullptr;
    static_assert(sizeof(handle) <= sizeof(nativeHandle), "window handle is wider than void*");
    std::memcpy(&handle, &nativeHandle, sizeof(handle));
    const UINT dpi = GetDpiForWindow(handle);
    if (dpi == 0) return 1.f;
    return std::clamp(static_cast<float>(dpi) / 96.f, 1.f, 3.f);
#else
    (void)nativeHandle;
    return 1.f;
#endif
}

sf::Vector2u workAreaSize() {
#if defined(_WIN32)
    RECT area{};
    if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &area, 0) != 0) {
        const auto width = static_cast<unsigned>(area.right - area.left);
        const auto height = static_cast<unsigned>(area.bottom - area.top);
        if (width > 0 && height > 0) return {width, height};
    }
#endif
    return {1920, 1080};
}

void openExternal(const std::string& target) {
    if (target.empty()) return;
#if defined(_WIN32)
    const HINSTANCE result =
        ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    (void)result;
#else
    std::system(("open \"" + target + "\" >/dev/null 2>&1 &").c_str());
#endif
}

std::filesystem::path executableDirectory() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        const std::filesystem::path self = std::filesystem::path(buffer).parent_path();
        if (!self.empty()) return self;
    }
#endif
    const std::filesystem::path self =
        std::filesystem::canonical(std::filesystem::path("/proc/self/exe"), ec);
    if (!ec && !self.empty()) return self.parent_path();
    return std::filesystem::current_path();
}

std::filesystem::path notesDirectory() {
    std::error_code ec;
    const std::filesystem::path preferred = std::filesystem::current_path() / "notes";
    if (std::filesystem::exists(preferred, ec)) return preferred;
    const std::filesystem::path beside = executableDirectory() / "notes";
    if (std::filesystem::exists(beside, ec)) return beside;
    return preferred;
}

std::filesystem::path tempDirectory() {
    std::error_code ec;
    std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec || base.empty()) base = std::filesystem::current_path();
    return base / "codernotes-run";
}

}  // namespace platform
