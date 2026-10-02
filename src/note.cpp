#include "note.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "markdown.h"
#include "util.h"

namespace {

std::string stripBom(std::string text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

}  // namespace

NoteStore::NoteStore(std::filesystem::path directory) : directory_(std::move(directory)) {}

std::filesystem::path NoteStore::pathFor(std::string_view id) const {
    return directory_ / (std::string(id) + ".md");
}

std::string NoteStore::titleFromText(std::string_view text, std::string_view fallback) {
    for (const std::string& line : util::splitLines(text)) {
        if (util::trim(line).empty()) continue;
        if (md::isHeadingStart(line)) {
            const std::string heading = util::collapseSpaces(md::stripInline(md::headingText(line)));
            if (!heading.empty()) return heading;
            continue;
        }
        const std::string collapsed = util::collapseSpaces(md::stripInline(line));
        if (!collapsed.empty()) return collapsed;
    }
    return std::string(fallback);
}

std::string NoteStore::previewOf(std::string_view text) {
    for (const std::string& line : util::splitLines(text)) {
        const std::string collapsed = util::collapseSpaces(md::stripInline(line));
        if (collapsed.empty() || md::isHeadingStart(line)) continue;
        return collapsed;
    }
    return {};
}

bool NoteStore::readFile(const std::filesystem::path& path, std::string& out) const {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = stripBom(buffer.str());
    return true;
}

void NoteStore::scan(std::string& error) {
    notes_.clear();
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory_, ec)) {
        if (!entry.is_regular_file()) continue;
        if (util::toLower(entry.path().extension().string()) != ".md") continue;
        files.push_back(entry.path());
    }
    if (ec) {
        error = "could not read the notes folder: " + ec.message();
        return;
    }
    std::sort(files.begin(), files.end(), [&](const std::filesystem::path& a,
                                            const std::filesystem::path& b) {
        const auto left = std::filesystem::last_write_time(a, ec);
        const auto right = std::filesystem::last_write_time(b, ec);
        if (left != right) return left > right;
        return a.filename().string() > b.filename().string();
    });
    for (const std::filesystem::path& file : files) {
        NoteMeta meta;
        meta.id = file.stem().string();
        meta.stamp = meta.id;
        std::string text;
        if (readFile(file, text)) {
            meta.title = titleFromText(text, meta.id);
            meta.preview = previewOf(text);
        } else {
            meta.title = meta.id;
        }
        notes_.push_back(meta);
    }
}

bool NoteStore::load(std::string& error) {
    scan(error);
    return error.empty();
}

int NoteStore::indexOf(std::string_view id) const {
    for (std::size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

const NoteMeta* NoteStore::find(std::string_view id) const {
    const int index = indexOf(id);
    return index < 0 ? nullptr : &notes_[static_cast<std::size_t>(index)];
}

std::string NoteStore::read(std::string_view id) const {
    std::string text;
    if (readFile(pathFor(id), text)) return text;
    return {};
}

bool NoteStore::write(std::string_view id, std::string_view text, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(directory_, ec);
    const std::filesystem::path path = pathFor(id);
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "could not open " + path.string() + " for writing";
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) {
            error = "failed while writing " + path.string();
            return false;
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        error = "could not replace " + path.string() + ": " + ec.message();
        return false;
    }
    return true;
}

bool NoteStore::remove(std::string_view id, std::string& error) {
    std::error_code ec;
    const std::filesystem::path path = pathFor(id);
    if (!std::filesystem::remove(path, ec)) {
        error = "could not delete " + path.string();
        return false;
    }
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                [&](const NoteMeta& meta) { return meta.id == id; }),
                 notes_.end());
    return true;
}

void NoteStore::create(std::string_view starterText) {
    NoteMeta meta;
    meta.id = util::uniqueId("note");
    meta.stamp = meta.id;
    meta.title = titleFromText(starterText, "Untitled note");
    meta.preview = previewOf(starterText);
    std::string error;
    write(meta.id, starterText, error);
    notes_.insert(notes_.begin(), std::move(meta));
}

