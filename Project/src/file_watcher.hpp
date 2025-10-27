#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <thread>

class FileWatcher {
    using FunctionCallback = std::function<void(const std::string&)>;

    struct FileWatchInfo {
        int inotifyFD;
        int watchFD;
        FunctionCallback callback;
    };

  public:
    static FileWatcher* getInstance();
    void init();
    void stop();

    void addWatcher(const std::string& filename, FunctionCallback callback);
    void removeWatcher(const std::string& filename);

  private:
    FileWatcher() { }

    void runThread();
    void reAddWatcher();

  private:
    std::map<std::string, FileWatchInfo> m_FileWatches;

    bool m_RunThread = true;
    std::thread m_FileThread;
    std::mutex m_FileMutex;
};
