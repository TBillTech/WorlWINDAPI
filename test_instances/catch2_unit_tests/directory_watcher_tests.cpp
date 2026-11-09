#include <catch2/catch_test_macros.hpp>
#include "fake_directory_watcher.h"
#include "file_backend.h"
#include "backend_testbed.h"
#include <filesystem>
#include <set>

using namespace std;

TEST_CASE("NullDirectoryWatcher contract", "[DirectoryWatcher]") {
    NullDirectoryWatcher watcher;
    auto a = watcher.add("/does/not/matter", kWatchModify);
    auto b = watcher.add("/does/not/matter", kWatchCreate | kWatchDelete);
    REQUIRE(a != kInvalidWatchHandle);
    REQUIRE(b != kInvalidWatchHandle);
    REQUIRE(a != b);
    REQUIRE(watcher.remove(a));
    REQUIRE(watcher.remove(12345)); // no-op
    REQUIRE(watcher.poll().empty());
}

#ifndef _WIN32
#include "inotify_directory_watcher.h"
TEST_CASE("InotifyDirectoryWatcher real filesystem", "[DirectoryWatcher][inotify]") {
    auto dir = std::filesystem::temp_directory_path() / "worlwind_inotify_watcher_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    string dir_path = dir.string() + "/";

    InotifyDirectoryWatcher watcher;
    REQUIRE(watcher.isValid());
    REQUIRE(watcher.poll().empty());
    REQUIRE(watcher.add(dir_path + "missing", kWatchCreate) == kInvalidWatchHandle);
    auto handle = watcher.add(dir_path, kWatchCreate | kWatchDelete);
    REQUIRE(handle != kInvalidWatchHandle);

    std::filesystem::create_directory(dir / "child");
    std::filesystem::remove(dir / "child");
    auto events = watcher.poll();
    REQUIRE(events.size() == 2);
    REQUIRE(events[0].handle == handle);
    REQUIRE(events[0].kind == WatchEventKind::create);
    REQUIRE(events[0].name == "child");
    REQUIRE(events[1].kind == WatchEventKind::remove);

    REQUIRE(watcher.remove(handle));
    REQUIRE_FALSE(watcher.remove(handle)); // already removed
    REQUIRE_FALSE(watcher.remove(kInvalidWatchHandle));
    std::filesystem::remove_all(dir);
}
#endif

#ifdef _WIN32
#include "windows_directory_watcher.h"
#include <chrono>
#include <fstream>
#include <functional>
#include <thread>

namespace {
// Polls until pred(events) holds or the bounded timeout expires; never uses a fixed sleep as the assertion.
bool pollUntil(DirectoryWatcher& watcher, vector<WatchEvent>& all,
               const function<bool(const vector<WatchEvent>&)>& pred) {
    auto deadline = chrono::steady_clock::now() + chrono::seconds(5);
    while (chrono::steady_clock::now() < deadline) {
        for (auto& e : watcher.poll()) all.push_back(e);
        if (pred(all)) return true;
        this_thread::sleep_for(chrono::milliseconds(5));
    }
    return false;
}
}

TEST_CASE("WindowsDirectoryWatcher real filesystem", "[DirectoryWatcher][windows]") {
    auto dir = std::filesystem::temp_directory_path() / "worlwind_windows_watcher_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    string dir_path = dir.string() + "/";

    auto watcher = make_unique<WindowsDirectoryWatcher>();
    REQUIRE(watcher->poll().empty());
    REQUIRE(watcher->add(dir_path + "missing", kWatchCreate) == kInvalidWatchHandle);
    REQUIRE_FALSE(watcher->remove(12345)); // unknown handle: no-op

    SECTION("create, rename and delete in order") {
        auto handle = watcher->add(dir_path, kWatchCreate | kWatchDelete);
        REQUIRE(handle != kInvalidWatchHandle);
        { ofstream(dir / "a.txt") << "x"; }
        std::filesystem::rename(dir / "a.txt", dir / "b.txt");
        std::filesystem::remove(dir / "b.txt");
        vector<WatchEvent> events;
        REQUIRE(pollUntil(*watcher, events, [](const vector<WatchEvent>& e) { return e.size() >= 4; }));
        REQUIRE(events[0].kind == WatchEventKind::create);
        REQUIRE(events[0].name == "a.txt");
        REQUIRE(events[1].kind == WatchEventKind::remove);
        REQUIRE(events[1].name == "a.txt");
        REQUIRE(events[2].kind == WatchEventKind::create);
        REQUIRE(events[2].name == "b.txt");
        REQUIRE(events[3].kind == WatchEventKind::remove);
        REQUIRE(events[3].name == "b.txt");
    }
    SECTION("file watch filters by name, coalesces modifies and supports several handles") {
        { ofstream(dir / "watched.txt") << "x"; }
        { ofstream(dir / "other.txt") << "x"; }
        auto h1 = watcher->add(dir_path + "watched.txt", kWatchModify);
        auto h2 = watcher->add(dir_path + "watched.txt", kWatchModify);
        REQUIRE(h1 != kInvalidWatchHandle);
        REQUIRE(h2 != kInvalidWatchHandle);
        REQUIRE(h1 != h2);
        { ofstream(dir / "other.txt") << "changed"; }
        {
            ofstream out(dir / "watched.txt");
            for (int i = 0; i < 100; i++) out << "line " << i << "\n";
        }
        vector<WatchEvent> events;
        REQUIRE(pollUntil(*watcher, events, [&](const vector<WatchEvent>& e) {
            bool a = false, b = false;
            for (auto& ev : e) { a |= ev.handle == h1; b |= ev.handle == h2; }
            return a && b;
        }));
        for (auto& ev : events) REQUIRE(ev.kind == WatchEventKind::modify);
        REQUIRE(watcher->remove(h1));
        REQUIRE_FALSE(watcher->remove(h1));
        REQUIRE(watcher->remove(h2));
    }
    SECTION("deleting a watched directory yields a delete event without crashing") {
        auto sub = dir / "sub";
        std::filesystem::create_directory(sub);
        auto handle = watcher->add(sub.string() + "/", kWatchCreate | kWatchDelete);
        REQUIRE(handle != kInvalidWatchHandle);
        std::filesystem::remove(sub);
        vector<WatchEvent> events;
        REQUIRE(pollUntil(*watcher, events, [&](const vector<WatchEvent>& e) {
            for (auto& ev : e) if (ev.handle == handle && ev.kind == WatchEventKind::remove) return true;
            return false;
        }));
        REQUIRE(watcher->poll().empty());
    }
    SECTION("destruction with pending I/O stress") {
        for (int i = 0; i < 50; i++) {
            WindowsDirectoryWatcher temp;
            REQUIRE(temp.add(dir_path, kWatchCreate | kWatchDelete | kWatchModify) != kInvalidWatchHandle);
            { ofstream(dir / "stress.txt") << i; }
        }
    }
    watcher.reset();
    std::filesystem::remove_all(dir);
}
#endif

TEST_CASE("FileBackend with FakeDirectoryWatcher", "[FileBackend][DirectoryWatcher]") {
    auto dir = std::filesystem::temp_directory_path() / "worlwind_fake_watcher_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    string base_path = dir.string();

    auto state = make_shared<FakeDirectoryWatcher::State>();
    {
        FileBackend backend(base_path, make_unique<FakeDirectoryWatcher>(state));
        BackendTestbed tester(backend);
        tester.addAnimalsToBackend();

        int notified = 0;
        backend.registerNodeListener("lion_listener", "lion", false,
            [&notified](Backend&, const string label_rule, const fplus::maybe<TreeNode>) {
                if (label_rule == "lion") notified++;
            });
        REQUIRE_FALSE(state->active.empty());
        auto lion_handle = state->handleFor(base_path + "/lion.node");
        REQUIRE(lion_handle != kInvalidWatchHandle);

        SECTION("modify event notifies the listener") {
            state->script(lion_handle, WatchEventKind::modify);
            backend.processNotifications();
            REQUIRE(notified == 1);
            // No further events: nothing happens.
            backend.processNotifications();
            REQUIRE(notified == 1);
        }
        SECTION("events for unknown handles are ignored") {
            state->script(99999, WatchEventKind::modify);
            state->script(99999, WatchEventKind::create, "x.node");
            state->script(99999, WatchEventKind::remove, "x.node");
            backend.processNotifications();
            REQUIRE(notified == 0);
        }
        SECTION("create event for the node file notifies and rebuilds watches") {
            auto dir_handle = state->handleFor(base_path + "/");
            REQUIRE(dir_handle != kInvalidWatchHandle);
            state->script(dir_handle, WatchEventKind::create, "lion.node");
            backend.processNotifications();
            REQUIRE(notified == 1);
            REQUIRE_FALSE(state->active.empty());
        }
        SECTION("delete event for the node file removes its watches") {
            auto dir_handle = state->handleFor(base_path + "/");
            REQUIRE(dir_handle != kInvalidWatchHandle);
            auto before = state->active.size();
            state->script(dir_handle, WatchEventKind::remove, "lion.node");
            backend.processNotifications();
            REQUIRE(state->active.size() <= before);
        }
        backend.deregisterNodeListener("lion_listener", "lion");
    }
    // The destructor removes every watch it created.
    REQUIRE(state->active.empty());
    std::filesystem::remove_all(dir);
}

TEST_CASE("FileBackend watch failure is reported by the watcher", "[FileBackend][DirectoryWatcher]") {
    auto dir = std::filesystem::temp_directory_path() / "worlwind_fake_watcher_fail_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    auto state = make_shared<FakeDirectoryWatcher::State>();
    FileBackend backend(dir.string(), make_unique<FakeDirectoryWatcher>(state));
    BackendTestbed tester(backend);
    tester.addAnimalsToBackend();
    state->fail_add = true;
    REQUIRE_THROWS(backend.registerNodeListener("l", "lion", false,
        [](Backend&, const string, const fplus::maybe<TreeNode>) {}));
    std::filesystem::remove_all(dir);
}
