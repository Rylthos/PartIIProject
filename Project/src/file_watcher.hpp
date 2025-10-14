#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <thread>

class FileWatcher {
    using FunctionCallback = std::function<void(const std::string&)>;

  public:
    static FileWatcher* getInstance();
    void init();
    void stop();

    void addWatcher(const std::string& filename, FunctionCallback callback);

  private:
    FileWatcher() { }

    void runThread();
    void reAddWatcher();

  private:
    std::map<std::string, std::tuple<int, int, FunctionCallback>> m_FileWatches;

    bool m_RunThread = true;
    std::thread m_FileThread;
    std::mutex m_FileMutex;
};
