#pragma once

#include "directory_watcher.h"
#include <map>

// Scripted DirectoryWatcher for deterministic tests: no file system and no timing.
// Tests push events with script() and inspect registrations; state is shared through a
// State object so the test can keep access after the watcher is moved into a FileBackend.
class FakeDirectoryWatcher : public DirectoryWatcher {
public:
    struct State {
        WatchHandle next_handle = 100;
        std::map<WatchHandle, std::pair<std::string, uint32_t>> active; // handle -> (path, mask)
        std::vector<WatchEvent> pending;
        bool fail_add = false;
        int removed_count = 0;

        WatchHandle handleFor(const std::string& path) const {
            for (const auto& it : active) {
                if (it.second.first == path) return it.first;
            }
            return kInvalidWatchHandle;
        }
        void script(WatchHandle handle, WatchEventKind kind, std::string name = "") {
            pending.push_back({handle, kind, std::move(name)});
        }
    };

    explicit FakeDirectoryWatcher(std::shared_ptr<State> state) : state_(std::move(state)) {}

    WatchHandle add(const std::string& path, uint32_t event_mask) override {
        if (state_->fail_add) return kInvalidWatchHandle;
        WatchHandle handle = state_->next_handle++;
        state_->active[handle] = {path, event_mask};
        return handle;
    }
    bool remove(WatchHandle handle) override {
        if (state_->active.erase(handle) == 0) return false;
        state_->removed_count++;
        return true;
    }
    std::vector<WatchEvent> poll() override {
        std::vector<WatchEvent> events;
        events.swap(state_->pending);
        return events;
    }
private:
    std::shared_ptr<State> state_;
};
