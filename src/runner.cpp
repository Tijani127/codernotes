#include "runner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "highlight.h"
#include "platform.h"
#include "util.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <csignal>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {

#if defined(_WIN32)
const char* const kExecutableSuffix = ".exe";
#else
const char* const kExecutableSuffix = "";
#endif

struct LanguageRecipe {
    const char* extension;
    const char* runner;  // %s is the quoted snippet path, %b the quoted compiled binary
    const char* hint;
};

const LanguageRecipe* recipeFor(const std::string& language) {
    static const std::vector<std::pair<std::string, LanguageRecipe>> table = {
        {"javascript", {"js", "node %s", "requires Node.js on your PATH"}},
        {"typescript", {"ts", "npx --yes tsx %s", "requires Node.js and network access for tsx"}},
        {"python", {"py", "python %s", "requires Python on your PATH (try: py -3)"}},
        {"c", {"c", "gcc %s -std=c17 -O1 -o %b && %b", "requires gcc on your PATH"}},
        {"cpp", {"cpp", "g++ %s -std=c++20 -O1 -o %b && %b", "requires g++ on your PATH"}},
        {"java", {"java", "java %s", "requires JDK 11 or newer on your PATH"}},
        {"go", {"go", "go run %s", "requires the Go toolchain on your PATH"}},
        {"rust", {"rs", "rustc %s -O -o %b && %b", "requires rustc on your PATH"}},
        {"ruby", {"rb", "ruby %s", "requires Ruby on your PATH"}},
        {"shell", {"sh", "sh %s", "requires a POSIX shell on your PATH"}},
        {"lua", {"lua", "lua %s", "requires Lua on your PATH"}},
        {"powershell", {"ps1", "pwsh -NoProfile -File %s", "requires PowerShell on your PATH"}},
        {"php", {"php", "php %s", "requires PHP on your PATH"}},
        {"swift", {"swift", "swift %s", "requires the Swift toolchain on your PATH"}},
        {"kotlin", {"kt", "kotlin %s", "requires the Kotlin compiler on your PATH"}},
        {"zig", {"zig", "zig run %s", "requires Zig on your PATH"}},
        {"dart", {"dart", "dart run %s", "requires the Dart SDK on your PATH"}},
        {"r", {"r", "Rscript %s", "requires Rscript on your PATH"}},
        {"sql", {"sql", "sqlite3 < %s", "requires the sqlite3 shell on your PATH"}},
    };
    for (const auto& entry : table) {
        if (entry.first == language) return &entry.second;
    }
    return nullptr;
}

std::string makeCommand(std::string_view pattern, const std::string& quotedSource,
                        const std::string& quotedBinary) {
    std::string out;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '%' && i + 1 < pattern.size()) {
            if (pattern[i + 1] == 's') {
                out += quotedSource;
                ++i;
                continue;
            }
            if (pattern[i + 1] == 'b') {
                out += quotedBinary;
                ++i;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

void removeDirectoryQuietly(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}



}  // namespace

class ProcessHandle {
public:
    std::mutex mutex;
    std::string output;
    std::thread reader;
    std::atomic<bool> done{false};
    std::chrono::steady_clock::time_point started;
    bool running() const { return !done.load(); }

#if defined(_WIN32)
    HANDLE process = nullptr;
    HANDLE writeEnd = nullptr;
    HANDLE readEnd = nullptr;
#else
    pid_t pid = -1;
    int writeFd = -1;
    int readFd = -1;
#endif

    void closeHandles() {
#if defined(_WIN32)
        if (writeEnd != nullptr) {
            CloseHandle(writeEnd);
            writeEnd = nullptr;
        }
        if (readEnd != nullptr) {
            CloseHandle(readEnd);
            readEnd = nullptr;
        }
#else
        if (writeFd >= 0) {
            close(writeFd);
            writeFd = -1;
        }
        if (readFd >= 0) {
            close(readFd);
            readFd = -1;
        }
#endif
    }
};

namespace {

bool startProcess(ProcessHandle& handle, const std::string& command, const std::string& directory,
                  std::string& error) {
    handle.started = std::chrono::steady_clock::now();
#if defined(_WIN32)
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) {
        error = "could not create a pipe for the snippet output";
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    std::string commandLine = "cmd.exe /d /s /c \"" + command + "\"";
    std::vector<char> mutableLine(commandLine.begin(), commandLine.end());
    mutableLine.push_back('\0');
    PROCESS_INFORMATION info{};
    const BOOL created = CreateProcessA(
        nullptr, mutableLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
        directory.empty() ? nullptr : directory.c_str(), &startup, &info);
    if (!created) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        error = "could not launch the snippet process";
        return false;
    }
    CloseHandle(writePipe);
    handle.process = info.hProcess;
    handle.readEnd = readPipe;
    CloseHandle(info.hThread);
    handle.reader = std::thread([&handle]() {
        char buffer[4096];
        DWORD read = 0;
        while (true) {
            const BOOL ok = ReadFile(handle.readEnd, buffer, sizeof(buffer), &read, nullptr);
            if (!ok || read == 0) break;
            std::lock_guard<std::mutex> lock(handle.mutex);
            handle.output.append(buffer, read);
        }
        handle.done.store(true);
    });
    return true;
#else
    int fds[2] = {-1, -1};
    if (pipe(fds) != 0) {
        error = "could not create a pipe for the snippet output";
        return false;
    }
    const pid_t child = fork();
    if (child < 0) {
        close(fds[0]);
        close(fds[1]);
        error = "could not fork the snippet process";
        return false;
    }
    if (child == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        if (!directory.empty()) {
            if (chdir(directory.c_str()) != 0) _exit(127);
        }
        execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(fds[1]);
    handle.pid = child;
    handle.readFd = fds[0];
    handle.reader = std::thread([&handle]() {
        char buffer[4096];
        while (true) {
            const ssize_t count = read(handle.readFd, buffer, sizeof(buffer));
            if (count <= 0) break;
            std::lock_guard<std::mutex> lock(handle.mutex);
            handle.output.append(buffer, static_cast<std::size_t>(count));
        }
        handle.done.store(true);
    });
    return true;
#endif
}

bool pollProcess(ProcessHandle& handle, int& exitCode) {
#if defined(_WIN32)
    if (handle.process == nullptr) return false;
    const DWORD result = WaitForSingleObject(handle.process, 0);
    if (result == WAIT_TIMEOUT) return false;
    DWORD code = 0;
    if (GetExitCodeProcess(handle.process, &code)) exitCode = static_cast<int>(code);
    return true;
#else
    if (handle.pid <= 0) return false;
    int status = 0;
    const pid_t result = waitpid(handle.pid, &status, WNOHANG);
    if (result == 0) return false;
    exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    handle.pid = -1;
    return true;
#endif
}

void killProcess(ProcessHandle& handle) {
#if defined(_WIN32)
    if (handle.process != nullptr) {
        TerminateProcess(handle.process, 1);
        WaitForSingleObject(handle.process, 2000);
    }
#else
    if (handle.pid > 0) {
        kill(handle.pid, SIGKILL);
        int status = 0;
        waitpid(handle.pid, &status, 0);
        handle.pid = -1;
    }
#endif
    handle.closeHandles();
    if (handle.reader.joinable()) handle.reader.join();
    handle.done.store(true);
}

std::string drainOutput(ProcessHandle& handle) {
    std::lock_guard<std::mutex> lock(handle.mutex);
    return handle.output;
}

bool looksMissing(const std::string& output) {
    const std::string lower = util::toLower(output);
    return lower.find("is not recognized as an internal or external command") != std::string::npos ||
           lower.find("command not found") != std::string::npos ||
           lower.find("no such file or directory") != std::string::npos ||
           lower.find("not recognized") != std::string::npos ||
           lower.find("unable to locate program") != std::string::npos ||
           lower.find("' is not recognized") != std::string::npos;
}

}  // namespace

std::string formatSeconds(double seconds) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
    return buffer;
}

RunPlan planRun(std::string_view code, std::string_view language) {
    RunPlan plan;
    const std::string id = syntax::normalizeLanguage(language);
    plan.language = id;
    plan.displayName = syntax::displayName(id);
    const LanguageRecipe* recipe = recipeFor(id);
    if (recipe == nullptr) {
        plan.supported = false;
        plan.hint = "no runner is wired up for " + plan.displayName +
                    " - try js, python, c, cpp, java, go, rust, sh, or powershell";
        return plan;
    }
    plan.supported = true;
    plan.hint = recipe->hint;

    const std::filesystem::path directory = platform::tempDirectory() / util::uniqueId("run");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    const std::filesystem::path file = directory / (std::string("snippet.") + recipe->extension);
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        if (!stream) {
            plan.supported = false;
            plan.hint = "could not write the snippet to " + file.string();
            return plan;
        }
        stream.write(code.data(), static_cast<std::streamsize>(code.size()));
    }
    const std::string quoted = util::escapeShellPath(file.string());
    const std::filesystem::path binary = directory / (std::string("snippet-bin") + kExecutableSuffix);
    plan.command = makeCommand(recipe->runner, quoted, util::escapeShellPath(binary.string()));
    plan.directory = directory.string();
    return plan;
}

std::string runnerHint(std::string_view language) {
    const LanguageRecipe* recipe = recipeFor(syntax::normalizeLanguage(language));
    return recipe != nullptr ? std::string(recipe->hint) : std::string();
}RunManager::RunManager() = default;

RunManager::~RunManager() {
    stopAll();
}

RunManager::Entry* RunManager::findEntry(const std::string& key) {
    for (Entry& entry : entries_) {
        if (entry.state.key == key) return &entry;
    }
    return nullptr;
}

RunState* RunManager::find(const std::string& key) {
    Entry* entry = findEntry(key);
    return entry != nullptr ? &entry->state : nullptr;
}

const RunState* RunManager::find(const std::string& key) const {
    for (const Entry& entry : entries_) {
        if (entry.state.key == key) return &entry.state;
    }
    return nullptr;
}

bool RunManager::start(const std::string& key, std::string_view code, std::string_view language) {
    Entry* previous = findEntry(key);
    if (previous != nullptr) {
        eraseEntry(*previous);
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& entry) { return entry.state.key == key; }),
                       entries_.end());
    }

    Entry entry;
    entry.state.key = key;
    entry.state.language = syntax::normalizeLanguage(language);
    entry.state.displayName = syntax::displayName(language);
    entry.state.output.clear();
    entry.state.error.clear();
    entry.state.exitCode = -1;
    entry.state.seconds = 0.0;
    entry.state.truncated = false;
    entry.state.finished = false;
    entry.state.running = true;

    const RunPlan plan = planRun(code, language);
    entry.state.command = plan.command;
    if (!plan.supported) {
        entry.state.running = false;
        entry.state.finished = true;
        entry.state.error = plan.hint;
        entry.state.output = plan.hint;
        entry.state.exitCode = -1;
        entries_.push_back(std::move(entry));
        return false;
    }

    entry.workDir = plan.directory;
    auto handle = std::make_unique<ProcessHandle>();
    std::string error;
    if (!startProcess(*handle, plan.command, plan.directory, error)) {
        entry.state.running = false;
        entry.state.finished = true;
        entry.state.error = error;
        entry.state.output = error;
        entries_.push_back(std::move(entry));
        return false;
    }
    entry.process = std::move(handle);
    entries_.push_back(std::move(entry));
    return true;
}

void RunManager::eraseEntry(Entry& entry) {
    if (entry.process) {
        killProcess(*entry.process);
        entry.process.reset();
        if (!entry.workDir.empty()) {
            removeDirectoryQuietly(entry.workDir);
            entry.workDir.clear();
        }
    }
    entry.state.running = false;
}

void RunManager::stop(const std::string& key) {
    Entry* entry = findEntry(key);
    if (entry != nullptr) eraseEntry(*entry);
}

void RunManager::stopAll() {
    for (Entry& entry : entries_) eraseEntry(entry);
    entries_.clear();
}

void RunManager::update() {
    for (Entry& entry : entries_) {
        if (!entry.process) continue;
        if (!entry.state.running) continue;
        int exitCode = -1;
        const bool finished = pollProcess(*entry.process, exitCode);
        entry.state.output = drainOutput(*entry.process);
        if (entry.state.output.size() > 200000) {
            entry.state.output.erase(0, entry.state.output.size() - 200000);
            entry.state.truncated = true;
        }
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - entry.process->started)
                .count();
        entry.state.seconds = elapsed;
        if (!finished && elapsed > 30.0) {
            killProcess(*entry.process);
            entry.state.running = false;
            entry.state.finished = true;
            entry.state.exitCode = -1;
            entry.state.output = drainOutput(*entry.process);
            entry.state.error = "stopped after 30 seconds";
            if (!entry.state.output.empty()) entry.state.output += "\n";
            entry.state.output += "snippet stopped after 30 seconds";
            entry.process.reset();
            if (!entry.workDir.empty()) {
                removeDirectoryQuietly(entry.workDir);
                entry.workDir.clear();
            }
            continue;
        }
        if (!finished) continue;

        entry.state.running = false;
        entry.state.finished = true;
        entry.state.exitCode = exitCode;
        if (entry.process->reader.joinable()) entry.process->reader.join();
        entry.process->closeHandles();
        entry.state.output = drainOutput(*entry.process);
        if (looksMissing(entry.state.output)) {
            entry.state.error = runnerHint(entry.state.language);
        }
        entry.process.reset();
        if (!entry.workDir.empty()) {
            removeDirectoryQuietly(entry.workDir);
            entry.workDir.clear();
        }
    }
}

void RunManager::retainOnly(const std::vector<std::string>& keys) {
    for (Entry& entry : entries_) {
        if (std::find(keys.begin(), keys.end(), entry.state.key) != keys.end()) continue;
        eraseEntry(entry);
    }
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [&](const Entry& entry) {
                                      if (entry.process != nullptr) return false;
                                      return std::find(keys.begin(), keys.end(), entry.state.key) ==
                                             keys.end();
                                  }),
                   entries_.end());
}
