#pragma once

#include "directory_watcher.h"

#ifdef _WIN32
#include <map>
#include <memory>

// Windows implementation of DirectoryWatcher using ReadDirectoryChangesW.  Win32 headers are only included in
// the .cpp file.  See directory_watcher.h for the general lifecycle and threading rules.
//
// Threading:  No internal thread.  Each handle owns one overlapped ReadDirectoryChangesW request that is
//   checked (without waiting) inside poll(), on the caller's thread.
// Watching:   Windows watches at directory level.  add() on a file watches the parent directory and filters
//   events by file name.  Each add() call gets its own directory handle, so several adds on one directory work.
// Events:     ADDED -> create, REMOVED -> remove, MODIFIED -> modify, RENAMED_OLD_NAME/RENAMED_NEW_NAME ->
//   remove(old) then create(new).  Consecutive modify events for the same handle are coalesced within a poll().
// Lifecycle:  remove() and the destructor cancel pending I/O with CancelIoEx and wait for the cancellation to
//   complete before the buffer, OVERLAPPED and handles are released.
// Overflow:   If the bounded buffer overflows (zero-byte completion) the change list is lost.  The overflow is
//   logged and the read is re-armed; the missed changes are not reported.  In order not to lose the final state
//   of a watched file, a modify event is reported for a file watch, and a directory watch gets nothing (the owner
//   must resync if it needs to).
// Deleted directory: a watched directory that is deleted yields a remove event with an empty name and the
//   handle is invalidated (it stays removable, remove() then returns true once).
class WindowsDirectoryWatcher : public DirectoryWatcher {
public:
    WindowsDirectoryWatcher();
    ~WindowsDirectoryWatcher() override;
    WindowsDirectoryWatcher(const WindowsDirectoryWatcher&) = delete;
    WindowsDirectoryWatcher& operator=(const WindowsDirectoryWatcher&) = delete;

    WatchHandle add(const std::string& path, uint32_t event_mask) override;
    bool remove(WatchHandle handle) override;
    std::vector<WatchEvent> poll() override;

private:
    struct Watch;
    std::map<WatchHandle, std::unique_ptr<Watch>> watches_;
    WatchHandle next_handle_ = 1;
};
#endif
