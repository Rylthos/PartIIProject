#include "file_watcher.hpp"

#include "logger.hpp"

#include "sys/inotify.h"
#include "unistd.h"

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
        close(std::get<1>(fileWatch.second));
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
        LOG_INFO("Add watch {}", filename);
    }

    m_FileWatches[filename] = std::make_tuple(id, watchFD, callback);
}

void FileWatcher::runThread()
{
    size_t bufSize = sizeof(inotify_event) + PATH_MAX + 1;
    inotify_event* event = (inotify_event*)malloc(bufSize);

    int fd, watchFD;
    FunctionCallback callback;

    while (m_RunThread) {
        m_FileMutex.lock();
        auto fileCopy = m_FileWatches;
        m_FileMutex.unlock();

        for (const auto& fileWatch : fileCopy) {
            std::tie(fd, watchFD, callback) = fileWatch.second;

            ssize_t len = read(fd, event, bufSize);
            if (len > 0) {
                if (event->mask & IN_MODIFY) {
                    LOG_INFO("Modified {}", fileWatch.first);
                    callback(fileWatch.first);
                }
                if (event->mask & IN_DELETE_SELF) {
                    LOG_INFO("Modified {}", fileWatch.first);
                    callback(fileWatch.first);
                    close(fd);
                    addWatcher(fileWatch.first, callback);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    free(event);
}
