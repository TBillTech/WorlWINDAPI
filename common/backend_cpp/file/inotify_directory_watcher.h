#pragma once

#include "directory_watcher.h"

#ifndef _WIN32
#include <sys/inotify.h>

// Linux inotify implementation of DirectoryWatcher.  See directory_watcher.h for lifecycle and threading rules.
// The destructor closes the inotify file descriptor.
class InotifyDirectoryWatcher : public DirectoryWatcher {
public:
    InotifyDirectoryWatcher();
    ~InotifyDirectoryWatcher() override;
    InotifyDirectoryWatcher(const InotifyDirectoryWatcher&) = delete;
    InotifyDirectoryWatcher& operator=(const InotifyDirectoryWatcher&) = delete;

    // False if inotify could not be initialized; add() then always fails and poll() returns nothing.
    bool isValid() const { return inotify_fd_ >= 0; }

    WatchHandle add(const std::string& path, uint32_t event_mask) override;
    bool remove(WatchHandle handle) override;
    std::vector<WatchEvent> poll() override;

private:
    int inotify_fd_ = -1;
    char buffer_[4096]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));
};
#endif
