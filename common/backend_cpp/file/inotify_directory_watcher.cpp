#include "inotify_directory_watcher.h"

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace std;

InotifyDirectoryWatcher::InotifyDirectoryWatcher()
{
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}

InotifyDirectoryWatcher::~InotifyDirectoryWatcher()
{
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
    }
}

WatchHandle InotifyDirectoryWatcher::add(const std::string& path, uint32_t event_mask)
{
    if (inotify_fd_ < 0) {
        return kInvalidWatchHandle;
    }
    uint32_t mask = 0;
    if (event_mask & kWatchModify) mask |= IN_MODIFY;
    if (event_mask & kWatchCreate) mask |= IN_CREATE;
    if (event_mask & kWatchDelete) mask |= IN_DELETE;
    int wd = inotify_add_watch(inotify_fd_, path.c_str(), mask | IN_IGNORED);
    return wd < 0 ? kInvalidWatchHandle : wd;
}

bool InotifyDirectoryWatcher::remove(WatchHandle handle)
{
    if (inotify_fd_ < 0 || handle < 0) {
        return false;
    }
    return inotify_rm_watch(inotify_fd_, handle) == 0;
}

std::vector<WatchEvent> InotifyDirectoryWatcher::poll()
{
    std::vector<WatchEvent> events;
    if (inotify_fd_ < 0) {
        return events;
    }
    ssize_t length = read(inotify_fd_, buffer_, sizeof(buffer_));
    if (length < 0) {
        int err = errno;
        if (err == EAGAIN) {
            // No events available, not an error in non-blocking mode
            return events;
        }
        std::cerr << "[inotify] read() returned " << length << ", errno=" << err << ": " << strerror(err) << std::endl << flush;
        return events;
    }

    ssize_t offset = 0;
    while (offset < length) {
        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer_[offset]);
        int wd = event->wd;
        uint32_t mask = event->mask;
        if ((mask & IN_MODIFY) == IN_MODIFY) {
            events.push_back({wd, WatchEventKind::modify, ""});
        }
        if (event->len > 0) {
            string name = event->name;
            if ((mask & IN_CREATE) == IN_CREATE) {
                events.push_back({wd, WatchEventKind::create, name});
            }
            if ((mask & IN_DELETE) == IN_DELETE) {
                events.push_back({wd, WatchEventKind::remove, name});
            }
        }
        // IN_IGNORED (watch removed because its target was deleted) produces no event; FileBackend
        // cleans up its bookkeeping through the delete events of the parent directory.
        offset += sizeof(struct inotify_event) + event->len;
    }
    return events;
}
#endif
