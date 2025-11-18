#include "file_watcher.hpp"

#include "logger.hpp"

#include "sys/inotify.h"
#include "unistd.h"
#include <cassert>
#include <cstdio>

FileWatcher* FileWatcher::getInstance()
{
    static FileWatcher watcher;
    return &watcher;
}

void FileWatcher::init() { m_FileThread = std::thread(std::bind(&FileWatcher::runThread, this)); }

void FileWatcher::stop()
{
    m_RunThread = false;
    m_FileThread.join();

    for (auto& fileWatch : m_FileWatches) {
        close(fileWatch.second.inotifyFD);
    }
    m_FileWatches.clear();
}

void FileWatcher::addWatcher(const std::string& filename, FunctionCallback callback)
{
    int id = inotify_init1(IN_NONBLOCK);

    int watchFD = inotify_add_watch(id, filename.c_str(), IN_DELETE_SELF | IN_MODIFY);
    if (watchFD < 0) {
        LOG_ERROR("Failed to add watch to inotify: {} | {}", filename, std::strerror(errno));
    } else {
        LOG_DEBUG("Add watch {}", filename);
    }

    std::unique_lock<std::mutex> _lock(m_FileMutex);
    m_FileWatches[filename] = {
        .inotifyFD = id,
        .watchFD = watchFD,
        .callback = callback,
    };
}

void FileWatcher::removeWatcher(const std::string& filename)
{
    assert(m_FileWatches.contains(filename) && "Unable to remove file watch that doesn't exist");

    auto& fileWatch = m_FileWatches[filename];
    close(fileWatch.inotifyFD);

    std::unique_lock<std::mutex> _lock(m_FileMutex);
    m_FileWatches.erase(filename);
}

void FileWatcher::runThread()
{
    size_t bufSize = sizeof(inotify_event) + PATH_MAX + 1;
    inotify_event* event = (inotify_event*)malloc(bufSize);

    FunctionCallback callback;

    while (m_RunThread) {
        m_FileMutex.lock();
        auto fileCopy = m_FileWatches;
        m_FileMutex.unlock();

        for (const auto& fileWatch : fileCopy) {
            ssize_t len = read(fileWatch.second.inotifyFD, event, bufSize);
            if (len > 0) {
                if (event->mask & IN_MODIFY) {
                    LOG_DEBUG("Modified {}", fileWatch.first);
                    callback(fileWatch.first);
                }
                if (event->mask & IN_DELETE_SELF) {
                    LOG_DEBUG("Modified {}", fileWatch.first);
                    fileWatch.second.callback(fileWatch.first);
                    removeWatcher(fileWatch.first);
                    addWatcher(fileWatch.first, fileWatch.second.callback);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    free(event);
}
