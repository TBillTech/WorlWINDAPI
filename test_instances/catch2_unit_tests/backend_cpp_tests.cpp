#include <catch2/catch_test_macros.hpp>
#include "simple_backend.h"
#include "threadsafe_backend.h"
#include "transactional_backend.h"
#include "composite_backend.h"
#include "redirected_backend.h"
#include "backend_testbed.h"
#include "file_backend.h"
#include "yaml_mediator.h"
#include <sstream>
#include <filesystem>

using namespace std;
using namespace fplus;

TEST_CASE("SimpleBackend logical test", "[SimpleBackend]") {
    // set to pool size for the type UDPChunk to 4 GB
    memory_pool.setPoolSize<UDPChunk>(static_cast<uint64_t>(4) * 1024 * 1024 * 1024 / UDPChunk::chunk_size);

    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    BackendTestbed tester(simple_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    tester.testBackendLogically();
}

TEST_CASE("ThreadsafeBackend logical test", "[ThreadsafeBackend]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    ThreadsafeBackend threadsafe_backend(simple_backend);
    BackendTestbed tester(threadsafe_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    tester.testBackendLogically();
}

TEST_CASE("TransactionalBackend logical test", "[TransactionalBackend]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    TransactionalBackend transactional_backend(simple_backend);
    BackendTestbed tester(transactional_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    tester.testBackendLogically();
}

TEST_CASE("MemoryTree IO test", "[MemoryTree][IO]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    BackendTestbed tester(simple_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    std::ostringstream oss;
    oss << *memoryTree;
    string output = oss.str();
    std::istringstream iss(output);
    MemoryTree loaded_tree;
    iss >> loaded_tree;
    REQUIRE(*memoryTree == loaded_tree);
}

TEST_CASE("CompositeBackend basic test", "[CompositeBackend]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    CompositeBackend composite_backend(simple_backend);
    BackendTestbed tester(composite_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    tester.testBackendLogically();
}

TEST_CASE("CompositeBackend mountBackend test", "[CompositeBackend][mountBackend]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    CompositeBackend composite_backend(simple_backend);
    auto zoo_memory_tree = make_shared<MemoryTree>();
    SimpleBackend zoo_backend(zoo_memory_tree);
    composite_backend.mountBackend("zoo", zoo_backend);
    auto museum_memory_tree = make_shared<MemoryTree>();
    SimpleBackend museum_backend(museum_memory_tree);
    composite_backend.mountBackend("museum", museum_backend);
    BackendTestbed zoo_tester(zoo_backend);
    zoo_tester.addAnimalsToBackend();
    zoo_tester.addNotesPageTree();
    BackendTestbed museum_tester(museum_backend);
    museum_tester.addAnimalsToBackend();
    museum_tester.addNotesPageTree();
    BackendTestbed tester(composite_backend);
    tester.testBackendLogically("zoo/");
    tester.testBackendLogically("museum/");
}

TEST_CASE("RedirectedBackend test", "[RedirectedBackend]") {
    auto memoryTree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memoryTree);
    auto zoo_memory_tree = make_shared<MemoryTree>();
    SimpleBackend zoo_backend(zoo_memory_tree);
    CompositeBackend composite_backend(simple_backend);
    composite_backend.mountBackend("zoo", zoo_backend);
    {
        BackendTestbed tester(zoo_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    {
        RedirectedBackend redirected_backend(composite_backend, "zoo");
        BackendTestbed tester(redirected_backend);
        tester.testBackendLogically();
    }
}

TEST_CASE("SimpleBackend stress test", "[SimpleBackend][stress]") {
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memory_tree);
    BackendTestbed tester(simple_backend);
    tester.stressTestConstructions(1000); // Stress test with 1000 constructions
}

TEST_CASE("FileBackend test", "[FileBackend]") {
    string base_path = "/home/tom/sandbox/file_backend_test_store";
    // First, clean up the file backend store directory if it exists
    std::filesystem::remove_all(base_path);
    std::filesystem::create_directory(base_path);

    {
        FileBackend file_backend(base_path);
        BackendTestbed tester(file_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
        tester.testBackendLogically();
        // This should ready the backend for testBackendLogically
        tester.addAnimalsToBackend();
    }
    {
        // This picks up the files from the previous test
        // and tests the logical consistency of the backend again.
        FileBackend file_backend(base_path);
        BackendTestbed tester(file_backend);
        tester.testBackendLogically();
        // This rewrites the files one more time so the human can browse them.
        tester.addAnimalsToBackend();
    }
#ifndef _WIN32
    {
        // This is a paranoid check to see if inotify works correctly.
        // It will directly create a infotify_fd_, and a wd for the file:
        // base_path/lion.node
        // Then it will write some bytes to that file, overwriting it.
        // Then it will test to see if the inotify_fd_ is notified of the change.
        {
            if (std::filesystem::exists(base_path + "/lion.node")) {
                std::filesystem::remove(base_path + "/lion.node"); 
            }
            ofstream lion_file(base_path + "/lion.node");
            lion_file << "Some initial bytes.\n";
            lion_file.close();
        }
        auto inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotify_fd < 0) {
            throw std::runtime_error("Failed to initialize inotify");
        }
        int wd = inotify_add_watch(inotify_fd, (base_path + "/lion.node").c_str(), IN_MODIFY | IN_IGNORED);
        if (wd < 0) {
            perror("inotify_add_watch");
            throw std::runtime_error("Failed to add inotify watch for lion.node");
        }
        // Now write some bytes to the file
        std::ofstream lion_file(base_path + "/lion.node");
        if (!lion_file) {
            throw std::runtime_error("Failed to open lion.node for writing");
        }
        lion_file << "This is a test of inotify.\n";
        lion_file.close();
        // Now read the inotify events
        char buffer[4096];
        int loops = 10000;
        int sleep_time = 500; // milliseconds
        int total = loops*sleep_time;
        bool notified = false;
        while (loops-- > 0) {
            ssize_t length = read(inotify_fd, buffer, sizeof(buffer));
            if (length < 0) {
                int err = errno;
                if (err == EAGAIN) {
                    // No events available, not an error in non-blocking mode
                } else {
                    std::cerr << "Error reading inotify events: " << strerror(err) << std::endl;
                    throw std::runtime_error("Failed to read inotify events");
                }
            }
            if (length > 0) {
                // We got some events, let's check if we got the one we expected
                size_t offset = 0;
                while (offset < length) {
                    struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer[offset]);
                    if (event->wd == wd && ((event->mask & IN_MODIFY) == IN_MODIFY)) {
                        notified = true;
                        break; // We found the event we were looking for
                    }
                    offset += sizeof(struct inotify_event) + event->len;
                }
            }
            if (notified) {
                break; // We got the notification we were looking for
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
        inotify_rm_watch(inotify_fd, wd);
        close(inotify_fd);
        if(!notified) {
            cerr << "Could not get inotify notification for lion.node modification even after " << total << " milliseconds" << endl << flush;
        }
        REQUIRE(notified); // We should have been notified of the modification
    }
#endif
    {
        FileBackend to_be_notified_backend(base_path);
        BackendTestbed tester(to_be_notified_backend);
        FileBackend to_be_modified_backend(base_path);
        uint32_t notification_delay = 40;
        tester.testPeerNotification(to_be_modified_backend, [notification_delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(notification_delay));
        });
    }
    {
        FileBackend to_be_notified_backend(base_path);
        BackendTestbed tester(to_be_notified_backend);
        FileBackend to_be_modified_backend(base_path);
        uint32_t notification_delay = 40;
        tester.testPeerNotification(to_be_modified_backend, [notification_delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(notification_delay));
        }, "zoo/Seattle/");
    }
    {
        uint64_t notification_delay = 40; // milliseconds
        FileBackend backend(base_path);
        BackendTestbed tester(backend);
        // This rewrites the files one more time so the human can browse them.
        tester.addAnimalsToBackend();
        // Perform a check to see if notifications of modifications work correctly.  This does not need to test creating deleting nodes, since that is done earlier.
        // First create a yaml property in the lion node of the backend. 
        auto lion_node_maybe = backend.getNode("lion");
        REQUIRE(lion_node_maybe.is_just());
        auto lion_node = lion_node_maybe.unsafe_get_just();
        string statistics_yaml = "Average_weight: 4200 kg\nAverage_height: 3.3 m\nAverage_age: 60 years";
        lion_node.insertPropertySpan(2, "statistics", "yaml", shared_span<>(global_no_chunk_header, std::span<const char>(statistics_yaml.data(), statistics_yaml.size())));
        REQUIRE(backend.upsertNode({lion_node}));
        // Create a listener for the lion node
        string listener_name = "lion_statistics_listener";
        auto backend_address = &backend;
        bool lion_node_modified = false;
        backend.registerNodeListener(listener_name, "lion", false, 
            [&lion_node_modified, backend_address](Backend& notified_backend, const string label_rule, const fplus::maybe<TreeNode> node) {
                REQUIRE("lion" == label_rule);
                REQUIRE(backend_address == &notified_backend);
                if (node.is_just()) {
                    auto found_node = node.get_with_default(TreeNode());
                    if (found_node.getLabelRule() == "lion") {
                        lion_node_modified = true;
                    }
                }
            });
        this_thread::sleep_for(chrono::milliseconds(notification_delay));
        // Now modify the lion node directly on disk: 
        auto lion_node_path = base_path + "/lion.node";
        {
            std::ifstream read_lion_file(lion_node_path);
            if (!read_lion_file) {
                throw std::runtime_error("Failed to open lion.node for reading");
            }
            TreeNode raw_lion_node;
            read_lion_file >> raw_lion_node; // Read the node from the file
            read_lion_file.close();
            std::ofstream lion_file(lion_node_path);
            if (!lion_file) {
                throw std::runtime_error("Failed to open lion.node for writing");
            }
            // Now modify the node
            raw_lion_node.setDescription("A majestic creature with a loud roar");
            lion_file << hide_contents << raw_lion_node; // Write the modified node back to the file
            lion_file.close();
        }
        this_thread::sleep_for(chrono::milliseconds(notification_delay));
        backend.processNotifications();
        REQUIRE(lion_node_modified); // We should have been notified of the modification
        lion_node_modified = false; // Reset the flag for the next test
        // Now modify the yaml property in the lion node directory on disk:
        string lion_yaml_path = base_path + "/lion.0.lion_statistics.yaml";
        {
            std::ofstream lion_yaml_file(lion_yaml_path);
            if (!lion_yaml_file) {
                throw std::runtime_error("Failed to open lion_statistics.yaml for writing");
            }
            string new_statistics_yaml = "Average_weight: 4500 kg\nAverage_height: 3.5 m\nAverage_age: 65 years";
            lion_yaml_file << new_statistics_yaml; // Write the yaml content
            lion_yaml_file.close();   
        }
        this_thread::sleep_for(chrono::milliseconds(notification_delay));
        backend.processNotifications();
        REQUIRE(lion_node_modified); // We should have been notified of the modification
        // Cleanup: Deregister the listener and remove the lion node
        backend.deregisterNodeListener(listener_name, "lion");
    }
    {
        FileBackend file_backend(base_path);
        BackendTestbed tester(file_backend);
        // This rewrites the files one more time so the human can browse them.
        tester.addAnimalsToBackend();
    }
}
