#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// DirectoryWatcher abstracts platform-specific file system change notification for FileBackend.
// The watcher only PRODUCES events.  The owner (FileBackend) keeps all bookkeeping about which handle
// belongs to which path/listener.
//
// Ownership:  A DirectoryWatcher is owned by whoever holds the unique_ptr (normally the FileBackend it was
//   injected into).  It must outlive all use of its handles.  Destroying the watcher releases every handle that
//   is still outstanding, so the owner should call remove() for its handles first and then destroy the watcher.
// Threading:  Not thread-safe.  All calls, including poll(), must be made from one thread, which is the thread
//   that calls FileBackend::processNotifications().  There is no internal thread.
// Cancellation: remove(handle) cancels a watch.  Events already produced for a removed handle may still be
//   returned by a later poll(); the owner must ignore events for handles it no longer knows.
// Shutdown:   There is no separate shutdown call; destruction is shutdown.  poll() never blocks.

using WatchHandle = int;
constexpr WatchHandle kInvalidWatchHandle = -1;

enum class WatchEventKind { modify, create, remove };  // remove is the "delete" kind (delete is a C++ keyword)

// Abstract event mask bits passed to add().
constexpr uint32_t kWatchModify = 1u << 0;
constexpr uint32_t kWatchCreate = 1u << 1;
constexpr uint32_t kWatchDelete = 1u << 2;

struct WatchEvent {
    WatchHandle handle;
    WatchEventKind kind;
    std::string name;  // Name of the affected directory entry (empty for events about the watched path itself).
};

class DirectoryWatcher {
public:
    virtual ~DirectoryWatcher() = default;

    // Starts watching path for the events in event_mask.  Returns kInvalidWatchHandle on failure
    // (for example if the path does not exist).  Does not print or throw.
    virtual WatchHandle add(const std::string& path, uint32_t event_mask) = 0;

    // Stops watching.  Returns false if the handle is unknown or already removed.  Does not print or throw.
    virtual bool remove(WatchHandle handle) = 0;

    // Returns pending events without blocking, in the order they occurred per directory.
    // Returns an empty vector (without allocating) when there are none.
    virtual std::vector<WatchEvent> poll() = 0;
};

// Watcher for platforms/configurations without notification support: add succeeds with a unique handle,
// remove is a no-op, and poll never returns events.
class NullDirectoryWatcher : public DirectoryWatcher {
public:
    WatchHandle add(const std::string&, uint32_t) override { return next_handle_++; }
    bool remove(WatchHandle) override { return true; }
    std::vector<WatchEvent> poll() override { return {}; }
private:
    WatchHandle next_handle_ = 1;
};

// Explicit factory: the inotify watcher on Linux (falling back to the null watcher if inotify cannot be
// initialized), the ReadDirectoryChangesW watcher on Windows, the null watcher elsewhere.
std::unique_ptr<DirectoryWatcher> makeDefaultDirectoryWatcher();
