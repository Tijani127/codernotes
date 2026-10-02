#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct NoteMeta {
    std::string id;
    std::string title;
    std::string preview;
    std::string stamp;
    bool dirty = false;
};

// Notes are plain markdown files inside a single folder (notes/ by default).
class NoteStore {
public:
    explicit NoteStore(std::filesystem::path directory);

    bool load(std::string& error);
    const std::vector<NoteMeta>& notes() const { return notes_; }
    std::vector<NoteMeta>& notes() { return notes_; }
    int indexOf(std::string_view id) const;
    const NoteMeta* find(std::string_view id) const;

    std::string read(std::string_view id) const;
    bool write(std::string_view id, std::string_view text, std::string& error) const;
    bool remove(std::string_view id, std::string& error);
    void create(std::string_view starterText);

    const std::filesystem::path& directory() const { return directory_; }
    static std::string titleFromText(std::string_view text, std::string_view fallback);

private:
    void scan(std::string& error);
    std::filesystem::path pathFor(std::string_view id) const;
    bool readFile(const std::filesystem::path& path, std::string& out) const;
    static std::string previewOf(std::string_view text);

    std::filesystem::path directory_;
    std::vector<NoteMeta> notes_;
};
