#include <catch2/catch_test_macros.hpp>
#include "tree_node.h"
#include "backend_testbed.h"
#include <string>
#include <vector>
#include <stdexcept>

using namespace std;

TreeNode consTreeNode(const std::string label_rule, const std::string description, 
        const std::vector<TreeNode::PropertyInfo> property_infos,
        const TreeNodeVersion version,
        const std::vector<std::string> child_names,
        shared_span<>&& property_data, 
        const fplus::maybe<std::string> query_how_to,
        const fplus::maybe<std::string> qa_sequence)
{
    return TreeNode(label_rule, description, property_infos, version, child_names, std::move(property_data), query_how_to, qa_sequence);
}

TEST_CASE("TreeNode getNodeName and getNodePath basic and edge cases", "[TreeNode][getNodeName][getNodePath]") {
    TreeNode n1 = consTreeNode("foo/bar/baz", "desc", {}, {}, {}, shared_span<>(global_no_chunk_header, false), {}, {});
    REQUIRE(n1.getNodeName() == "baz");
    REQUIRE(n1.getNodePath() == "foo/bar/");

    TreeNode n2 = consTreeNode("root", "desc", {}, {}, {}, shared_span<>(global_no_chunk_header, false), {}, {});
    REQUIRE(n2.getNodeName() == "root");
    REQUIRE(n2.getNodePath() == "");

    TreeNode n3 = consTreeNode("/leading/slash", "desc", {}, {}, {}, shared_span<>(global_no_chunk_header, false), {}, {});
    REQUIRE(n3.getNodeName() == "slash");
    REQUIRE(n3.getNodePath() == "/leading/");

    TreeNode n4 = consTreeNode("trailing/", "desc", {}, {}, {}, shared_span<>(global_no_chunk_header, false), {}, {});
    REQUIRE(n4.getNodeName() == "");
    REQUIRE(n4.getNodePath() == "trailing/");

    TreeNode n5 = consTreeNode("", "desc", {}, {}, {}, shared_span<>(global_no_chunk_header, false), {}, {});
    REQUIRE(n5.getNodeName() == "");
    REQUIRE(n5.getNodePath() == "");
}

TEST_CASE("TreeNode property data: insert, set, get, bytes, and error handling", "[TreeNode][propertyData]") {
    // Local union/variant type for property values
    struct PropVal {
        enum Type { INT64, UINT64, FLOAT, DOUBLE, BOOL, STRING, URL, BYTES } type;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
        bool b;
        std::string s;
        std::vector<uint8_t> bytes;
        PropVal(int64_t v) : type(INT64), i64(v) {}
        PropVal(uint64_t v) : type(UINT64), u64(v) {}
        PropVal(float v) : type(FLOAT), f(v) {}
        PropVal(double v) : type(DOUBLE), d(v) {}
        PropVal(bool v) : type(BOOL), b(v) {}
        PropVal(const std::string& v, bool is_url = false) : type(is_url ? URL : STRING), s(v) {}
        PropVal(const char* v, bool is_url = false) : type(is_url ? URL : STRING), s(v) {}
        PropVal(const std::vector<uint8_t>& v) : type(BYTES), bytes(v) {}
        size_t size() const {
            switch (type) {
                case INT64: return sizeof(i64);
                case UINT64: return sizeof(u64);
                case FLOAT: return sizeof(f);
                case DOUBLE: return sizeof(d);
                case BOOL: return sizeof(b);
                case STRING: return s.size();
                case URL: return s.size();
                case BYTES: return bytes.size();
            }
            throw std::runtime_error("Unknown PropVal type");
        }
        bool is_fixed_size() const {
            return type == INT64 || type == UINT64 || type == FLOAT || type == DOUBLE || type == BOOL;
        }
        shared_span<> to_shared_span() const {
            if (!is_fixed_size()) {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, bytes.size());
                if (type == PropVal::STRING || type == PropVal::URL) {
                    return shared_span<>(header, std::span<const char>(s.data(), s.size()));
                }
                if (type == PropVal::BYTES) {
                    string bytes_str(bytes.begin(), bytes.end());
                    return shared_span<>(header, std::span<const char>(bytes_str.data(), bytes_str.size()));
                }
            }
            throw std::runtime_error("Converting value to shared_span not implemented for this type");
        }
        bool operator==(shared_span<> val) const {
            shared_span<> result = to_shared_span();
            // Use the shared_span's iterators to compare
            auto it1 = result.begin<uint8_t>();
            auto it2 = val.begin<uint8_t>();
            while (it1 != result.end<uint8_t>() && it2 != val.end<uint8_t>()) {
                if (*it1 != *it2) {
                    return false; // Mismatch found
                }
                ++it1;
                ++it2;
            }
            return it1 == result.end<uint8_t>() && it2 == val.end<uint8_t>();   
        }
    };
    // Reusable function to check property correctness (must be after PropVal definition)
    auto check_property = [](const TreeNode& node, size_t index, const std::string& type, const std::string& name, const PropVal& property) {
        if (!property.is_fixed_size()) {
            auto [size, size_span, value_span] = node.getPropertyValueSpan(name);
            REQUIRE(size == static_cast<uint64_t>(property.size()));
            REQUIRE(*size_span.begin<uint64_t>() == property.size());
            REQUIRE(size_span.size() == sizeof(uint64_t));
            REQUIRE(value_span.size() == property.size());
            TreeNode::PropertyInfo prop_info = node.getPropertyInfo()[index];
            REQUIRE(prop_info.first == type);
            REQUIRE(prop_info.second == name);
            REQUIRE(property == value_span);
        } else {
            switch (property.type) {
                case PropVal::INT64:
                    {
                        auto [size, value, __] = node.getPropertyValue<int64_t>(name);
                        REQUIRE(size == property.size());
                        REQUIRE(value == property.i64);
                    }
                    break;
                case PropVal::UINT64:
                    {
                        auto [size, value, __] = node.getPropertyValue<uint64_t>(name);
                        REQUIRE(size == property.size());
                        REQUIRE(value == property.u64);
                    }
                    break;
                case PropVal::FLOAT:
                    {
                        auto [size, value, __] = node.getPropertyValue<float>(name);
                        REQUIRE(size == property.size());
                        REQUIRE(value == property.f);
                    }
                    break;
                case PropVal::DOUBLE:
                    {
                        auto [size, value, __] = node.getPropertyValue<double>(name);
                        REQUIRE(size == property.size());
                        REQUIRE(value == property.d);
                    }
                    break;
                case PropVal::BOOL:
                    {
                        auto [size, value, __] = node.getPropertyValue<bool>(name);
                        REQUIRE(size == property.size());
                        REQUIRE(value == property.b);
                    }
                    break;
                default:
                    throw std::runtime_error("Unexpected fixed-size type");
            }
            TreeNode::PropertyInfo prop_info = node.getPropertyInfo()[index];
            REQUIRE(prop_info.first == type);
            REQUIRE(prop_info.second == name);
        }
    };
    // Make random number generation repeatable for unit test
    srand(12345);

    // Helper to make random bytes
    auto rand_bytes = [](size_t n) {
        std::vector<uint8_t> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(rand() % 256);
        return v;
    };

    using PropValMapValueType = std::tuple<std::string, std::string, PropVal, PropVal>;
    // Map: name -> (type, name, property, value2)
    std::map<std::string, PropValMapValueType> test_props = {
        // int64 edge cases
        {"int64_min", {"int64", "int64_min", PropVal(std::numeric_limits<int64_t>::min()), PropVal(int64_t(-1))}},
        {"int64_max", {"int64", "int64_max", PropVal(std::numeric_limits<int64_t>::max()), PropVal(int64_t(1))}},
        {"int64_zero", {"int64", "int64_zero", PropVal(int64_t(0)), PropVal(int64_t(42))}},
        // uint64 edge cases
        {"uint64_min", {"uint64", "uint64_min", PropVal(uint64_t(0)), PropVal(uint64_t(123))}},
        {"uint64_max", {"uint64", "uint64_max", PropVal(std::numeric_limits<uint64_t>::max()), PropVal(uint64_t(999999999))}},
        // float edge cases
        {"float_min", {"float", "float_min", PropVal(std::numeric_limits<float>::lowest()), PropVal(-1.0f)}},
        {"float_max", {"float", "float_max", PropVal(std::numeric_limits<float>::max()), PropVal(1.0f)}},
        {"float_zero", {"float", "float_zero", PropVal(0.0f), PropVal(3.14f)}},
        // double edge cases
        {"double_min", {"double", "double_min", PropVal(std::numeric_limits<double>::lowest()), PropVal(-2.0)}},
        {"double_max", {"double", "double_max", PropVal(std::numeric_limits<double>::max()), PropVal(2.0)}},
        {"double_zero", {"double", "double_zero", PropVal(0.0), PropVal(2.718)}},
        // bool
        {"bool_true", {"bool", "bool_true", PropVal(true), PropVal(false)}},
        {"bool_false", {"bool", "bool_false", PropVal(false), PropVal(true)}},
        // strings
        {"string_empty", {"string", "string_empty", PropVal(""), PropVal("not empty")}},
        {"string_short", {"string", "string_short", PropVal("hi"), PropVal("bye")}},
        {"string_long", {"string", "string_long", PropVal(std::string(100, 'a')), PropVal(std::string(50, 'b'))}},
        // urls
        {"url_http", {"url", "url_http", PropVal("http://example.com", true), PropVal("http://different.com", true)}},
        {"url_https", {"url", "url_https", PropVal("https://example.com/path?query=1", true), PropVal("https://other.com/", true)}},
        // random bytes (different lengths)
        {"bytes_39", {"bytes", "bytes_39", PropVal(rand_bytes(39)), PropVal(rand_bytes(40))}},
        {"bytes_101", {"bytes", "bytes_101", PropVal(rand_bytes(101)), PropVal(rand_bytes(99))}},
        {"bytes_343", {"bytes", "bytes_343", PropVal(rand_bytes(343)), PropVal(rand_bytes(350))}},
        {"bytes_5331", {"bytes", "bytes_5331", PropVal(rand_bytes(5331)), PropVal(rand_bytes(5330))}},
    };

    int test_loops = 100;
    for (int i = 0; i < test_loops; ++i) {
        int element_count = rand() % 10 + 1; // Random element count between 1 and 10
        std::list<PropValMapValueType> test_elements;
        for (int j = 0; j < element_count; ++j) {
            auto it = test_props.begin();
            int map_size = test_props.size();
            std::advance(it, rand() % map_size);
            test_elements.push_back(it->second);
        }
        std::vector<PropValMapValueType> elements_in_node;

        TreeNode elephant = createAnimalNode(
        "elephant", 
        "Largest land animal", 
        {},
        {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)}, 
        {"Dumbo", "Babar"}, 
        {},
        "url duh!", 
        "Zookeeper1: Elephants are so strong.\nZookeeper2: And they have great memory!"
        );

        for (auto& element : test_elements)
        {
            // choose a random index to insert the property
            size_t prop_count = elephant.getPropertyInfo().size();
            size_t index = prop_count ? rand() % prop_count : 0;
            auto& [type, name, property, __] = element;
            try {
                if (property.is_fixed_size()) {
                    switch (property.type) {
                        case PropVal::INT64:
                            elephant.insertProperty<int64_t>(index, name, property.i64);
                            break;
                        case PropVal::UINT64:
                            elephant.insertProperty<uint64_t>(index, name, property.u64);
                            break;
                        case PropVal::FLOAT:
                            elephant.insertProperty<float>(index, name, property.f);
                            break;
                        case PropVal::DOUBLE:
                            elephant.insertProperty<double>(index, name, property.d);
                            break;
                        case PropVal::BOOL:
                            elephant.insertProperty<bool>(index, name, property.b);
                            break;
                        default:
                            throw std::runtime_error("Unexpected fixed-size type");
                    }
                } else {
                    // Insert the property with shared_span value
                    elephant.insertPropertySpan(index, name, type, property.to_shared_span());
                }
            }
            catch (const std::invalid_argument& e) {
                // If the property already exists, skip it
                if (std::string(e.what()).find("Property already exists") != std::string::npos) {
                    // Check if it really does exist in elements_in_node
                    auto it = std::find_if(elements_in_node.begin(), elements_in_node.end(),
                        [&name](const PropValMapValueType& elem) { return std::get<1>(elem) == name; });
                    if (it != elements_in_node.end()) {
                        continue; // Skip this property since it already exists
                    }
                }
                throw; // Rethrow unexpected exceptions
            }
            auto it_insert = elements_in_node.begin();
            std::advance(it_insert, index);
            elements_in_node.insert(it_insert, element);

            check_property(elephant, index, type, name, property);
        }
        // Check all the properties again:
        for (size_t index = 0; index < elements_in_node.size(); ++index) {
            auto& [type, name, property, __] = elements_in_node[index];
            check_property(elephant, index, type, name, property);
        }
        // Now test the setProperty method
        vector<int> indices;
        // Randomize the indices for a set order
        for (size_t i = 0; i < elements_in_node.size(); ++i) {
            indices.push_back(i);
        }
        std::mt19937 rng(12345); // fixed seed for reproducibility
        std::shuffle(indices.begin(), indices.end(), rng);
        for (size_t index : indices) {
            auto& [type, name, __, property] = elements_in_node[index];
            if (property.is_fixed_size()) {
                switch (property.type) {
                    case PropVal::INT64:
                        elephant.setPropertyValue<int64_t>(name, property.i64);
                        break;
                    case PropVal::UINT64:
                        elephant.setPropertyValue<uint64_t>(name, property.u64);
                        break;
                    case PropVal::FLOAT:
                        elephant.setPropertyValue<float>(name, property.f);
                        break;
                    case PropVal::DOUBLE:
                        elephant.setPropertyValue<double>(name, property.d);
                        break;
                    case PropVal::BOOL:
                        elephant.setPropertyValue<bool>(name, property.b);
                        break;
                    default:
                        throw std::runtime_error("Unexpected fixed-size type");
                }
            } else {
                // Set the property with shared_span value
                elephant.setPropertyValueSpan(name, property.to_shared_span());
            }
            check_property(elephant, index, type, name, property);
        }
        // Check all the properties again:
        for (size_t index = 0; index < elements_in_node.size(); ++index) {
            auto& [type, name, __, property] = elements_in_node[index];
            check_property(elephant, index, type, name, property);
        }
        // Delete one property at random, check the rest, and repeat until all properties are deleted
        while (!elements_in_node.empty()) {
            size_t index = rand() % elements_in_node.size();
            auto& [type, name, __, property] = elements_in_node[index];
            elephant.deleteProperty(name);
            elements_in_node.erase(elements_in_node.begin() + index);
            // Check the remaining properties
            for (size_t i = 0; i < elements_in_node.size(); ++i) {
                auto& [type, name, __, property] = elements_in_node[i];
                check_property(elephant, i, type, name, property);
            }
        }
    }
}
