#include <catch2/catch_test_macros.hpp>
#include "simple_backend.h"
#include "threadsafe_backend.h"
#include "transactional_backend.h"
#include "composite_backend.h"
#include "redirected_backend.h"
#include "backend_testbed.h"
#include "file_backend.h"
#include "wwatp_service.h"
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

TEST_CASE("WWATPService SimpleBackend test via TreeNodes", "[WWATPService][SimpleBackend]") {
    // Create a configuration backend to hold the service configuration
    auto config_memory_tree = make_shared<MemoryTree>();
    SimpleBackend config_backend(config_memory_tree);
    
    // Create the configuration structure for a SimpleBackend
    // The WWATPService expects config/backends/test_simple with a "type" property
    shared_span<> no_content1(global_no_chunk_header, false);
    shared_span<> no_content2(global_no_chunk_header, false);
    
    // Create config root node
    TreeNode config_root("config", "Configuration root", {}, DEFAULT_TREE_NODE_VERSION, {"backends"}, std::move(no_content1), nothing<string>(), nothing<string>());
    
    // Create backends node as a child of config  
    TreeNode backends_node("config/backends", "Backend configurations", {}, DEFAULT_TREE_NODE_VERSION, {"test_simple"}, std::move(no_content2), nothing<string>(), nothing<string>());
    
    // Create the simple backend config using the same pattern as createAnimalNode
    TreeNode simple_backend_config("config/backends/test_simple",
        "Test simple backend configuration", {}, DEFAULT_TREE_NODE_VERSION, {}, std::move(no_content1), nothing<string>(), nothing<string>());
    // Set the type property to "simple"
    simple_backend_config.insertPropertyString(0, "type", "string", "simple");

    // Insert the backend configuration into the hierarchy
    REQUIRE(config_backend.upsertNode({config_root}));
    REQUIRE(config_backend.upsertNode({backends_node}));
    REQUIRE(config_backend.upsertNode({simple_backend_config}));
    
    // Create the WWATPService with the configuration backend
    auto wwatp_service = make_shared<WWATPService>("test_service", make_shared<SimpleBackend>(config_backend), "config");
    
    // Initialize the service to construct all backends
    wwatp_service->initialize();
    
    // Get the constructed SimpleBackend by name
    auto simple_backend = wwatp_service->getBackend("test_simple");
    REQUIRE(simple_backend != nullptr);
    
    // Run the same logical tests as the original SimpleBackend test
    BackendTestbed tester(*simple_backend);
    tester.addAnimalsToBackend();
    tester.addNotesPageTree();
    tester.testBackendLogically();
}

TEST_CASE("WWATPService Basic Backend test via YAMLMediator", "[WWATPService][YAMLMediator]") {
    // Create a YAML configuration string for the SimpleBackend
    // Note: version fields and descriptions are omitted when they match defaults
    string base_path = "/home/tom/sandbox/file_backend_test_store";
    if (std::filesystem::exists(base_path)) {
        std::filesystem::remove_all(base_path); // Clean up the directory if it exists
    }
    std::filesystem::create_directory(base_path);
    string config_yaml = R"(config:
  child_names: [backends]
  backends:
    child_names: [test_simple, for_threadsafe, test_threadsafe, for_transactional, test_transactional, file_backend]
    test_simple:
      type: simple
    for_threadsafe:
      type: simple
    test_threadsafe:
      type: threadsafe
      backend: for_threadsafe
    for_transactional:
      type: simple
    test_transactional:
      type: transactional
      backend: for_transactional
    file_backend:
      type: file
      root_path: )" + base_path + R"(
)";
    
    // Create the WWATPService with YAML configuration (automatically initializes)
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);
    
    // Get the constructed SimpleBackend by name
    auto simple_backend = wwatp_service->getBackend("test_simple");
    REQUIRE(simple_backend != nullptr);
    {
        // Run the same logical tests as the original SimpleBackend test
        BackendTestbed tester(*simple_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
        tester.testBackendLogically();
    }

    // Get the constructed ThreadsafeBackend by name
    auto threadsafe_backend = wwatp_service->getBackend("test_threadsafe");
    REQUIRE(threadsafe_backend != nullptr);
    // Run the same logical tests as the original ThreadsafeBackend test
    {
        BackendTestbed tester(*threadsafe_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
        tester.testBackendLogically();
    }

    // Get the constructed TransactionalBackend by name
    auto transactional_backend = wwatp_service->getBackend("test_transactional");
    REQUIRE(transactional_backend != nullptr);
    // Run the same logical tests as the original TransactionalBackend test
    {
        BackendTestbed tester(*transactional_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
        tester.testBackendLogically();
    }

    auto file_backend = wwatp_service->getBackend("file_backend");
    REQUIRE(file_backend != nullptr);
    {
        size_t notification_delay = 40; // milliseconds
        BackendTestbed tester(*file_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
        this_thread::sleep_for(chrono::milliseconds(notification_delay));
        file_backend->processNotifications();
        // put a lion node listener on file_backend_b
        string lion_listener_name = "lion_listener";
        bool lion_node_modified = false;
        file_backend->registerNodeListener(lion_listener_name, "lion", false,
            [&lion_node_modified](Backend& notified_backend, const string label_rule, const fplus::maybe<TreeNode> node) {
                REQUIRE("lion" == label_rule);
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
        file_backend->processNotifications();
        REQUIRE(lion_node_modified); // We should have been notified of the modification
    }
}

TEST_CASE("WWATPService CompositeBackend mountBackend test via YAMLMediator", "[WWATPService][CompositeBackend][YAMLMediator]") {
    // Create a YAML configuration string for CompositeBackend with mounted backends
    string config_yaml = R"(config:
  child_names: [backends]
  backends:
    child_names: [root_simple, zoo_simple, museum_simple, test_composite]
    root_simple:
      type: simple
    zoo_simple:
      type: simple
    museum_simple:
      type: simple
    test_composite:
      type: composite
      backend: root_simple
      child_names: [zoo_simple, museum_simple]
      zoo_simple:
        mount_path: zoo
      museum_simple:
        mount_path: museum
)";
    
    // Create the WWATPService with YAML configuration (automatically initializes)
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);
    
    // Get the individual backends for adding data
    auto zoo_backend = wwatp_service->getBackend("zoo_simple");
    auto museum_backend = wwatp_service->getBackend("museum_simple");
    auto composite_backend = wwatp_service->getBackend("test_composite");
    
    REQUIRE(zoo_backend != nullptr);
    REQUIRE(museum_backend != nullptr);
    REQUIRE(composite_backend != nullptr);
    
    // Add animals to zoo and museum backends directly
    {
        BackendTestbed zoo_tester(*zoo_backend);
        zoo_tester.addAnimalsToBackend();
        zoo_tester.addNotesPageTree();
    }
    {
        BackendTestbed museum_tester(*museum_backend);
        museum_tester.addAnimalsToBackend();
        museum_tester.addNotesPageTree();
    }
    
    // Test the composite backend with prefixed paths (zoo/ and museum/)
    BackendTestbed tester(*composite_backend);
    tester.testBackendLogically("zoo/");
    tester.testBackendLogically("museum/");
}

TEST_CASE("WWATPService RedirectedBackend test via YAMLMediator", "[WWATPService][RedirectedBackend][YAMLMediator]") {
    // Create a YAML configuration string for RedirectedBackend
    string config_yaml = R"(config:
  child_names: [backends]
  backends:
    child_names: [test_redirected, underlying_composite, for_composite, zoo_simple]
    test_redirected:
      type: redirected
      backend: underlying_composite
      redirect_root: zoo
    underlying_composite:
      type: composite
      backend: for_composite
      child_names: [zoo_simple]
      zoo_simple:
        mount_path: zoo
    for_composite:
      type: simple
    zoo_simple:
      type: simple
)";
    
    // Create the WWATPService with YAML configuration (automatically initializes)
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);
    
    {
        auto zoo_backend = wwatp_service->getBackend("zoo_simple");
        REQUIRE(zoo_backend != nullptr);
        BackendTestbed tester(*zoo_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    {
        auto redirected_backend = wwatp_service->getBackend("test_redirected");
        REQUIRE(redirected_backend != nullptr);
        BackendTestbed tester(*redirected_backend);
        tester.testBackendLogically();
    }
}

TEST_CASE("WWATPService RedirectedBackend test via path WWATP", "[WWATPService][RedirectedBackend][path]") {
    // Setup sandbox path and clean up any existing redirect_service directory
    string sandbox_path = "/home/tom/sandbox/";
    string redirect_service_path = sandbox_path + "redirect_service";
    
    // Clean up any existing redirect_service directory
    if (std::filesystem::exists(redirect_service_path)) {
        std::filesystem::remove_all(redirect_service_path);
    }
    
    // Require sandbox directory exists
    REQUIRE(std::filesystem::exists(sandbox_path));
    
    // Stage 1: Create FileBackend and write redirect_service config to sandbox
    {
        FileBackend file_backend(sandbox_path);
        
        // Create the redirect_service node with config_yaml as a property
        string redirect_config_yaml = R"(config:
  child_names: [backends]
  backends:
    child_names: [test_redirected, underlying_composite, for_composite, zoo_simple]
    test_redirected:
      type: redirected
      backend: underlying_composite
      redirect_root: zoo
    underlying_composite:
      type: composite
      backend: for_composite
      child_names: [zoo_simple]
      zoo_simple:
        mount_path: zoo
    for_composite:
      type: simple
    zoo_simple:
      type: simple
)";
        
        TreeNode redirect_service_node = createNoContentTreeNode("redirect_service", "Redirected backend service configuration", {}, 
            DEFAULT_TREE_NODE_VERSION, {}, nothing<string>(), nothing<string>());
        
        // Add the config_yaml as a yaml property
        redirect_service_node.insertPropertySpan(0, "config", "yaml", 
            shared_span<>(global_no_chunk_header, std::span<const char>(redirect_config_yaml.data(), redirect_config_yaml.size())));
        
        // Write the service node to the file backend
        REQUIRE(file_backend.upsertNode({redirect_service_node}));
        
        // Process notifications to ensure file is written
        file_backend.processNotifications();
        
        // Verify the redirect_service directory was created
        REQUIRE(std::filesystem::exists(redirect_service_path));
    }
    
    // Stage 2: Use path constructor to load config and test RedirectedBackend
    {
        // Create WWATPService using the path constructor
        auto wwatp_service = make_shared<WWATPService>(redirect_service_path);
        
        // Add test data to zoo_simple backend
        {
            auto zoo_backend = wwatp_service->getBackend("zoo_simple");
            REQUIRE(zoo_backend != nullptr);
            BackendTestbed tester(*zoo_backend);
            tester.addAnimalsToBackend();
            tester.addNotesPageTree();
        }
        
        // Test the redirected backend
        {
            auto redirected_backend = wwatp_service->getBackend("test_redirected");
            REQUIRE(redirected_backend != nullptr);
            BackendTestbed tester(*redirected_backend);
            tester.testBackendLogically();
        }
    }
}