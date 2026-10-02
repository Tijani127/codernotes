#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct RunState {
    bool running = false;
    bool finished = false;
    std::string key;
    std::string language;
    std::string displayName;
    std::string command;
    std::string output;
    std::string error;
    int exitCode = -1;
    double seconds = 0.0;
    bool truncated = false;
};

class ProcessHandle;

struct RunPlan {
    bool supported = false;
    std::string command;
    std::string directory;
    std::string language;
    std::string displayName;
    std::string hint;
};

// Builds the shell command used to execute a snippet. Supported fences are
// mapped to whatever interpreter is installed on the machine.
RunPlan planRun(std::string_view code, std::string_view language);
std::string runnerHint(std::string_view language);
std::string formatSeconds(double seconds);

// Owns every running snippet so the UI stays non blocking.
class RunManager {
public:
    RunManager();
    ~RunManager();
    RunManager(const RunManager&) = delete;
    RunManager& operator=(const RunManager&) = delete;

    RunState* find(const std::string& key);
    const RunState* find(const std::string& key) const;
    bool start(const std::string& key, std::string_view code, std::string_view language);
    void stop(const std::string& key);
    void stopAll();
    void update();
    void retainOnly(const std::vector<std::string>& keys);

private:
    struct Entry {
        RunState state;
        std::unique_ptr<ProcessHandle> process;
        std::filesystem::path workDir;
    };

    Entry* findEntry(const std::string& key);
    void eraseEntry(Entry& entry);

    std::vector<Entry> entries_;
};
