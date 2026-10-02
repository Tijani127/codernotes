#include "clipboard.h"

#include <cstring>
#include <cwchar>
#include <vector>

#include "util.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace clipboard {
namespace {

std::string g_internal;

}  // namespace

bool set(std::string_view text) {
    g_internal = std::string(text);
#if defined(_WIN32)
    std::vector<wchar_t> wide(g_internal.size() + 1, L'\0');
    if (!MultiByteToWideChar(CP_UTF8, 0, g_internal.data(),
                             static_cast<int>(g_internal.size()), wide.data(),
                             static_cast<int>(g_internal.size()))) {
        return false;
    }
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    const std::size_t bytes = (wide.size()) * sizeof(wchar_t);
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (handle == nullptr) {
        CloseClipboard();
        return false;
    }
    void* memory = GlobalLock(handle);
    if (memory == nullptr) {
        GlobalFree(handle);
        CloseClipboard();
        return false;
    }
    std::memcpy(memory, wide.data(), bytes);
    GlobalUnlock(handle);
    const bool placed = SetClipboardData(CF_UNICODETEXT, handle) != nullptr;
    CloseClipboard();
    return placed;
#else
    return true;
#endif
}

std::string get() {
#if defined(_WIN32)
    if (OpenClipboard(nullptr)) {
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle != nullptr) {
            const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(handle));
            std::string result;
            if (data != nullptr) {
                const int length = static_cast<int>(wcslen(data));
                const int needed = WideCharToMultiByte(CP_UTF8, 0, data, length, nullptr, 0, nullptr,
                                                       nullptr);
                if (needed > 0) {
                    std::vector<char> buffer(static_cast<std::size_t>(needed) + 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, data, length, buffer.data(), needed, nullptr,
                                        nullptr);
                    buffer[static_cast<std::size_t>(needed)] = '\0';
                    result.assign(buffer.data(), static_cast<std::size_t>(needed));
                }
                GlobalUnlock(handle);
            }
            CloseClipboard();
            if (!result.empty()) return result;
        } else {
            CloseClipboard();
        }
    }
#endif
    return g_internal;
}

}  // namespace clipboard
