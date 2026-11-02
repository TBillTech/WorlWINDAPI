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
    auto dir = std::filesystem::temp_directory_path() / "wwatp_inotify_watcher_test";
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

TEST_CASE("FileBackend with FakeDirectoryWatcher", "[FileBackend][DirectoryWatcher]") {
    auto dir = std::filesystem::temp_directory_path() / "wwatp_fake_watcher_test";
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
    auto dir = std::filesystem::temp_directory_path() / "wwatp_fake_watcher_fail_test";
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
