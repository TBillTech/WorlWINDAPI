
#include <catch2/catch_test_macros.hpp>
#include "simple_backend.h"
#include "backend_testbed.h"
#include "cloning_mediator.h"
#include "yaml_mediator.h"
#include "file_backend.h"
#include "wwatp_service.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

TEST_CASE("Cloning Mediator copy test", "[CloningMediator]") {
    // Create a YAML configuration string for a Cloning Mediator with backends
    string config_yaml = R"(config:
  child_names: [backends, frontends]
  backends:
    child_names: [a, b]
    a:
      type: simple
    b:
      type: simple
  frontends:
    child_names: [cloning_mediator]
    cloning_mediator:
      type: cloning_mediator
      backend_a: a
      backend_b: b
)";
    
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);

    auto simple_backend_A = wwatp_service->getBackend("a");
    auto simple_backend_B = wwatp_service->getBackend("b");
    {
        BackendTestbed tester_A(*simple_backend_A);
        tester_A.addAnimalsToBackend();
        tester_A.addNotesPageTree();
    }
    {
        BackendTestbed tester_B(*simple_backend_B);
        tester_B.testBackendLogically();
    }
}

TEST_CASE("Cloning Mediator versioned test", "[CloningMediator][versioned]") {
    // Create a YAML configuration string for a Cloning Mediator with backends
    string config_yaml = R"(config:
  child_names: [backends, frontends]
  backends:
    child_names: [a, b]
    a:
      type: simple
    b:
      type: simple
  frontends:
    child_names: [cloning_mediator]
    cloning_mediator:
      type: cloning_mediator
      backend_a: a
      backend_b: b
      versioned: true
)";
    
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);

    auto simple_backend_A = wwatp_service->getBackend("a");
    auto simple_backend_B = wwatp_service->getBackend("b");    {
        BackendTestbed tester_A(*simple_backend_A);
        tester_A.addAnimalsToBackend();
        tester_A.addNotesPageTree();
    }
    {
        BackendTestbed tester_B(*simple_backend_B);
        tester_B.addAnimalsToBackend();
        tester_B.addNotesPageTree();
    }
    auto prior_node = simple_backend_A->getNode("lion");
    {   // Test that updating to a new version on A shows up on B
        auto lion_node = simple_backend_A->getNode("lion");
        auto lion = lion_node.get_with_default(TreeNode());
        lion.setDescription("A king of the jungle, known for its majestic mane.");
        ++lion; // Increment the version
        simple_backend_A->upsertNode({lion});
        auto other_lion_node = simple_backend_B->getNode("lion");
        REQUIRE(other_lion_node.get_with_default(TreeNode()) == lion);
    }
    {   // Test that updating to an old version on A does not change B
        auto lion_node = prior_node.get_with_default(TreeNode());
        simple_backend_A->upsertNode({lion_node});
        auto other_lion_node = simple_backend_B->getNode("lion");
        REQUIRE(other_lion_node.get_with_default(TreeNode()) != lion_node);
    }
    {   // Test that updating to a new version on B shows up on A
        auto lion_node = simple_backend_B->getNode("lion");
        auto lion = lion_node.get_with_default(TreeNode());
        lion.setDescription("A large carnivorous feline. (Revised)");
        ++lion; // Increment the version
        simple_backend_B->upsertNode({lion});
        auto other_lion_node = simple_backend_A->getNode("lion");
        REQUIRE(other_lion_node.get_with_default(TreeNode()) == lion);
    }
}

TEST_CASE("YAMLMediator construction test", "[YAMLMediator]") {
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend nodeful_backend(memory_tree);
    auto yaml_memory_tree = make_shared<MemoryTree>();
    SimpleBackend yaml_backend(yaml_memory_tree);
    auto other_memory_tree = make_shared<MemoryTree>();
    SimpleBackend other_backend(other_memory_tree);
    {
        BackendTestbed tester(nodeful_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    string label_rule = "universe/yaml";
    string property_name = "zoo";
    string property_type = "yaml";
    PropertySpecifier specifier(label_rule, property_name, property_type);
    {
        YAMLMediator yaml_mediator("yaml_mediator", nodeful_backend, yaml_backend, specifier, false);
    }
    {
        YAMLMediator yaml_mediator("yaml_mediator", other_backend, yaml_backend, specifier, true);
    }
    {
        BackendTestbed tester(other_backend);
        tester.testBackendLogically();
    }
}

TEST_CASE("WWATPService YAMLMediator construction test", "[WWATPService][YAMLMediator]") {
    // Create a YAML configuration string for YAMLMediator with backends
    string config_yaml = R"(config:
  child_names: [backends, frontends]
  backends:
    child_names: [nodeful_backend, yaml_backend, other_backend]
    nodeful_backend:
      type: simple
    yaml_backend:
      type: simple
    other_backend:
      type: simple
  frontends:
    child_names: [yaml_mediator1, yaml_mediator2]
    yaml_mediator1:
      type: yaml_mediator
      tree_backend: nodeful_backend
      yaml_backend: yaml_backend
      node_label: universe/yaml
      property_name: zoo
      property_type: yaml
      initialize_from_yaml: false
    yaml_mediator2:
      type: yaml_mediator
      tree_backend: other_backend
      yaml_backend: yaml_backend
      node_label: universe/yaml
      property_name: zoo
      property_type: yaml
      initialize_from_yaml: true
)";
    
    auto wwatp_service = make_shared<WWATPService>("test_service", config_yaml);

    auto nodeful_backend = wwatp_service->getBackend("nodeful_backend");
    auto yaml_backend = wwatp_service->getBackend("yaml_backend");
    auto other_backend = wwatp_service->getBackend("other_backend");
    
    REQUIRE(nodeful_backend != nullptr);
    REQUIRE(yaml_backend != nullptr);
    REQUIRE(other_backend != nullptr);
    
    // Add test data to nodeful_backend (this should sync to yaml_backend via yaml_mediator1)
    {
        BackendTestbed tester(*nodeful_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    
    // Process notifications to allow yaml_mediator1 to sync nodeful->yaml
    nodeful_backend->processNotifications();
    yaml_backend->processNotifications();
    
    // The yaml_mediator2 should initialize other_backend from yaml_backend
    // Test that other_backend now has the same data as nodeful_backend
    {
        BackendTestbed tester(*other_backend);
        tester.testBackendLogically();
    }
}

TEST_CASE("YAMLMediator notification test", "[YAMLMediator][notification]") {
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend nodeful_backend(memory_tree);
    auto yaml_memory_tree = make_shared<MemoryTree>();
    SimpleBackend yaml_backend(yaml_memory_tree);
    auto other_memory_tree = make_shared<MemoryTree>();
    SimpleBackend other_backend(other_memory_tree);
    {
        BackendTestbed tester(nodeful_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    string label_rule = "universe/city/newyork";
    string property_name = "zoo";
    string property_type = "yaml";
    PropertySpecifier specifier(label_rule, property_name, property_type);
    {
        YAMLMediator nodeful_mediator("nodeful_mediator", nodeful_backend, yaml_backend, specifier, false);
        BackendTestbed tester(nodeful_backend);
        YAMLMediator other_mediator("other_mediator", other_backend, yaml_backend, specifier, true);
        uint32_t notification_delay = 100;
        tester.testPeerNotification(other_backend, [notification_delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(notification_delay));
        });
    }
}

TEST_CASE("YAMLMediator SimpleBackend-FileBackend integration test", "[YAMLMediator][integration]") {
    size_t inotify_delay = 40; // milliseconds
    // Setup sandbox path and clean up any existing universe/city directory
    std::string sandbox_path = "/home/tom/sandbox/";
    std::string city_path = sandbox_path + "universe/city";
    std::string zoo_file_path = sandbox_path + "universe/city/newyork.0.zoo.yaml";
    
    // Recursively delete universe/city directory if it exists
    if (std::filesystem::exists(city_path)) {
        std::filesystem::remove_all(city_path);
    }
    
    // Require sandbox directory exists
    REQUIRE(std::filesystem::exists(sandbox_path));
    
    // Create SimpleBackend with test data
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memory_tree);
    {
        BackendTestbed tester(simple_backend);
        tester.addAnimalsToBackend();
        tester.addNotesPageTree();
    }
    
    // Create FileBackend for YAML storage
    FileBackend file_backend(sandbox_path);
    
    // Setup YAMLMediator with SimpleBackend->FileBackend (initialize_from_yaml = false)
    string label_rule = "universe/city/newyork";
    string property_name = "zoo";
    string property_type = "yaml";
    PropertySpecifier specifier(label_rule, property_name, property_type);
    
    YAMLMediator yaml_mediator("yaml_mediator", simple_backend, file_backend, specifier, false);
    
    // Process any pending notifications to ensure initial sync
    std::this_thread::sleep_for(std::chrono::milliseconds(inotify_delay));
    simple_backend.processNotifications();
    file_backend.processNotifications();
    
    // Verify that the YAML file was created and contains data from SimpleBackend
    REQUIRE(std::filesystem::exists(zoo_file_path));
    
    std::ifstream yaml_file(zoo_file_path);
    std::string yaml_content((std::istreambuf_iterator<char>(yaml_file)),
                            std::istreambuf_iterator<char>());
    yaml_file.close();
    
    // Verify the YAML contains some expected data (lion should be in there)
    REQUIRE(yaml_content.find("lion") != std::string::npos);
    
    // Test 1: Modify SimpleBackend data and verify it updates the YAML file
    auto lion_node_maybe = simple_backend.getNode("lion");
    REQUIRE(lion_node_maybe.is_just());
    
    auto lion_node = lion_node_maybe.unsafe_get_just();
    lion_node.setDescription("A magnificent predator of the African savanna");
    ++lion_node; // Increment version
    simple_backend.upsertNode({lion_node});
    
    // Process notifications and wait for file system sync
    std::this_thread::sleep_for(std::chrono::milliseconds(inotify_delay));
    simple_backend.processNotifications();
    file_backend.processNotifications();
    
    // Verify the YAML file was updated
    std::ifstream updated_yaml_file(zoo_file_path);
    std::string updated_yaml_content((std::istreambuf_iterator<char>(updated_yaml_file)),
                                    std::istreambuf_iterator<char>());
    updated_yaml_file.close();
    
    REQUIRE(updated_yaml_content.find("A magnificent predator of the African savanna") != std::string::npos);
    
    // Test 2: Manually modify the YAML file and verify it updates SimpleBackend
    // First, load and parse the current YAML
    YAML::Node yaml_doc = YAML::Load(updated_yaml_content);
    
    // Find and modify the elephant description in the YAML
    if (yaml_doc["elephant"] && yaml_doc["elephant"]["description"]) {
        yaml_doc["elephant"]["description"] = "A gentle giant with incredible memory";
    }
    
    // Write the modified YAML back to the file
    std::ofstream out_yaml_file(zoo_file_path);
    out_yaml_file << yaml_doc;
    out_yaml_file.close();
    
    // Process file system notifications and wait for sync
    std::this_thread::sleep_for(std::chrono::milliseconds(inotify_delay));
    file_backend.processNotifications();
    simple_backend.processNotifications();
    
    // Verify the SimpleBackend was updated
    auto elephant_node_maybe = simple_backend.getNode("elephant");
    REQUIRE(elephant_node_maybe.is_just());
    
    auto elephant_node = elephant_node_maybe.unsafe_get_just();
    REQUIRE(elephant_node.getDescription() == "A gentle giant with incredible memory");
}
    
