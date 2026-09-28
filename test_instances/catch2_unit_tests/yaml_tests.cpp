#include <catch2/catch_test_macros.hpp>
#include "tree_node.h"
#include "backend_testbed.h"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

using namespace std;

TEST_CASE("TreeNodeVersion asYAMLNode <-> fromYAMLNode are inverses", "[TreeNodeVersion][YAML]") {
    // Test various combinations of member settings
    std::vector<TreeNodeVersion> test_versions = {
        // Default
        TreeNodeVersion{0, 10, "default", {}, {}, {}, {}},
        // All fields set
        TreeNodeVersion{5, 100, "custom", fplus::just(std::string("proof")), fplus::just(std::string("author1,author2")), fplus::just(std::string("reader1,reader2")), fplus::just(3)},
        // Only some optionals set
        TreeNodeVersion{1, 2, "policyA", fplus::nothing<std::string>(), fplus::just(std::string("authorX")), fplus::nothing<std::string>(), fplus::just(0)},
        // Edge case: max_version_sequence = 1
        TreeNodeVersion{0, 1, "edge", {}, {}, {}, {}},
        // Edge case: version_number = max_version_sequence - 1
        TreeNodeVersion{9, 10, "wrap", {}, {}, {}, {}},
    };

    for (const auto& original : test_versions) {
        YAML::Node node = original.asYAMLNode();
        TreeNodeVersion roundtrip = fromYAMLNode(node);
        REQUIRE(roundtrip == original);

        // Test the other direction: YAML -> C++ -> YAML
        YAML::Node node2 = roundtrip.asYAMLNode();
        // YAML::Node does not have operator==, so compare the fields again
        TreeNodeVersion roundtrip2 = fromYAMLNode(node2);
        REQUIRE(roundtrip2 == original);
    }
}

TEST_CASE("TreeNodeVersion asYAMLNode produces expected YAML structure", "[TreeNodeVersion][YAML]") {
    TreeNodeVersion v{42, 99, "test_policy", fplus::just(std::string("proofy")), fplus::just(std::string("auth")), fplus::nothing<std::string>(), fplus::just(7)};
    YAML::Node node = v.asYAMLNode();
    REQUIRE(node["version_number"].as<uint16_t>() == 42);
    REQUIRE(node["max_version_sequence"].as<uint16_t>() == 99);
    REQUIRE(node["policy"].as<std::string>() == "test_policy");
    REQUIRE(node["authorial_proof"].as<std::string>() == "proofy");
    REQUIRE(node["authors"].as<std::string>() == "auth");
    REQUIRE_FALSE(node["readers"]); // Should be missing or null
    REQUIRE(node["collision_depth"].as<int>() == 7);
}

TEST_CASE("fromYAMLNode handles missing optional fields", "[TreeNodeVersion][YAML]") {
    YAML::Node node;
    node["version_number"] = 1;
    node["max_version_sequence"] = 2;
    node["policy"] = "p";
    // No authorial_proof, authors, readers, collision_depth
    TreeNodeVersion v = fromYAMLNode(node);
    REQUIRE(v.version_number == 1);
    REQUIRE(v.max_version_sequence == 2);
    REQUIRE(v.policy == "p");
    REQUIRE(v.authorial_proof.is_nothing());
    REQUIRE(v.authors.is_nothing());
    REQUIRE(v.readers.is_nothing());
    REQUIRE(v.collision_depth.is_nothing());
}

TEST_CASE("TreeNode asYAMLNode no children tests", "[TreeNode][YAML]") {
    vector<TreeNode> lion_nodes = createLionNodes();
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memory_tree);
    simple_backend.upsertNode(lion_nodes);

    TreeNode lion_node = simple_backend.getNode("lion").unsafe_get_just();
    YAML::Node yaml_node = lion_node.asYAMLNode(simple_backend, false);
    vector<TreeNode> from_yaml_nodes = fromYAMLNode(yaml_node, "", "lion", false);

    REQUIRE(from_yaml_nodes[0] == lion_node);
}

TEST_CASE("TreeNode asYAMLNode with children tests", "[TreeNode][YAML]") {
    vector<TreeNode> lion_nodes = createLionNodes();
    auto memory_tree = make_shared<MemoryTree>();
    SimpleBackend simple_backend(memory_tree);
    simple_backend.upsertNode(lion_nodes);

    TreeNode lion_node = simple_backend.getNode("lion").unsafe_get_just();
    YAML::Node yaml_node = lion_node.asYAMLNode(simple_backend, true);
    vector<TreeNode> from_yaml_nodes = fromYAMLNode(yaml_node, "", "lion", true);

    for (const auto& tn : from_yaml_nodes) {
        string label_rule = tn.getLabelRule();
        fplus::maybe<TreeNode> maybe_node = simple_backend.getNode(label_rule);
        REQUIRE(maybe_node.is_just());
        REQUIRE(maybe_node.unsafe_get_just() == tn);
    }
    REQUIRE(lion_nodes.size() == from_yaml_nodes.size());
}