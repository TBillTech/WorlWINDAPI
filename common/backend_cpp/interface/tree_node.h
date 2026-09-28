#pragma once

#include <string>
#include <vector>
#include <cstdint> // For uint8_t
#include <fplus/fplus.hpp>
#include "shared_chunk.h"
#include <set>
#include <yaml-cpp/yaml.h>

// operators for reading and writing a fplus::maybe<T> to a stream
namespace fplus {
    template<typename T>
    std::ostream& operator<<(std::ostream& os, const maybe<T>& m) {
        if (m.is_just()) {
            os << "Just " << m.get_with_default(T());
        } else {
            os << "Nothing";
        }
        return os;
    }

    template<typename T>
    std::istream& operator>>(std::istream& is, maybe<T>& m) {
        std::string str;
        is >> str;
        if (str == "Nothing") {
            m = maybe<T>();
        } else {
            T value;
            is >> value;
            m = just(value);
        }
        return is;
    }
}

void write_length_string(std::ostream& os, const std::string& label, const std::string& str);
pair<string,string> read_length_string(std::istream& is);

std::ostream& hide_contents(std::ostream& os);
std::ostream& show_contents(std::ostream& os);

class TreeNodeVersion {
    public:
        uint16_t version_number = 0;
        uint16_t max_version_sequence = 256;
        std::string policy = "default";
        fplus::maybe<std::string> authorial_proof;
        fplus::maybe<std::string> authors;
        fplus::maybe<std::string> readers;
        fplus::maybe<int> collision_depth;

        TreeNodeVersion& operator++() {
            assert(max_version_sequence > 0);
            version_number = (version_number + 1) % max_version_sequence;
            return *this; 
        }
        
        bool isDefault() const {
            return (version_number == 0 && max_version_sequence == 256 && policy == "default" &&
                   authorial_proof.is_nothing() && authors.is_nothing() && 
                   readers.is_nothing() && collision_depth.is_nothing());
        }
        bool operator==( const TreeNodeVersion& other ) const {
            return version_number == other.version_number &&
                max_version_sequence == other.max_version_sequence &&
                policy == other.policy &&
                authorial_proof == other.authorial_proof &&
                authors == other.authors &&
                readers == other.readers &&
                collision_depth == other.collision_depth;
        }
        bool operator<( const TreeNodeVersion& other ) const {
            if (*this == other) {
                return false; // They are equal, so not less than
            }
            int32_t this_version = static_cast<int32_t>(version_number);
            int32_t other_version = static_cast<int32_t>(other.version_number);
            int32_t max_sequence = static_cast<int32_t>(max_version_sequence);
            int32_t half_max_sequence = max_sequence / 2;
            // This logic is maybe a little strange, but since the version can wrap, then if the two versions are closer than half the max version sequence apart, 
            // then the one with the lower version number is considered less.  Otherwise, they are considered wrapped, so add the max version sequence to the other version number.
            if (other_version - this_version > 0) {
                if(other_version - this_version <= half_max_sequence) {
                    return true;
                }
            } else {
                if(this_version - other_version > half_max_sequence) {
                    return true;
                }
            }
            return false;
        }
        bool operator<=( const TreeNodeVersion& other ) const {
            return (*this < other) || (version_number == other.version_number);
        }
        bool operator>=( const TreeNodeVersion& other ) const {
            return !(*this < other);
        }
        bool operator>( const TreeNodeVersion& other ) const {
            return !(*this <= other);
        }
        friend std::ostream& operator<<(std::ostream& os, const TreeNodeVersion& version) {
            if (version.policy.empty()) {
                throw std::runtime_error("TreeNodeVersion policy cannot be empty.  'default' should be used at minimum.");
            }
            os << "Version: " << version.version_number << " Max_Version: " << version.max_version_sequence
            << " Policy: " << version.policy << " Authorial_Proof: " << version.authorial_proof
            << " Authors: " << version.authors << " Readers: " << version.readers
            << " Collision_Depth: " << version.collision_depth << " ";
            return os;
        }
        friend std::istream& operator>>(std::istream& is, TreeNodeVersion& version) {
            std::string label;
            is >> label >> version.version_number;
            is >> label >> version.max_version_sequence;
            is >> label >> version.policy;
            is >> label >> version.authorial_proof;
            is >> label >> version.authors;
            is >> label >> version.readers;
            is >> label >> version.collision_depth;
            is.get(); // consume the space
            return is;
        }
        YAML::Node asYAMLNode() const {
            YAML::Node node;
            node["version_number"] = version_number;
            node["max_version_sequence"] = max_version_sequence;
            node["policy"] = policy;
            if (authorial_proof.is_just()) {
                node["authorial_proof"] = authorial_proof.unsafe_get_just();
            }
            if (authors.is_just()) {
                node["authors"] = authors.unsafe_get_just();
            }
            if (readers.is_just()) {
                node["readers"] = readers.unsafe_get_just();
            }
            if (collision_depth.is_just()) {
                node["collision_depth"] = collision_depth.unsafe_get_just();
            }
            return node;
        }
};

// Global default TreeNodeVersion constant for convenient use
inline const TreeNodeVersion DEFAULT_TREE_NODE_VERSION;

TreeNodeVersion fromYAMLNode(const YAML::Node& node);
class Backend;

// Represents a node in the tree structure.
class TreeNode {
public:
    using PropertyInfo = std::pair<std::string, std::string>; // Type and name of the property

    // Constructors
    TreeNode();
    TreeNode(const std::string& label_rule, const std::string& description, 
        const std::vector<PropertyInfo>& property_infos,
        const TreeNodeVersion& version,
        const std::vector<std::string>& child_names,
        shared_span<>&& property_data, 
        const fplus::maybe<std::string>& query_how_to,
        const fplus::maybe<std::string>& qa_sequence);

    // Assignment operator
    TreeNode& operator++() {
        ++version;
        return *this;
    }
    TreeNode& operator=(const TreeNode& other);
    TreeNode& operator=(TreeNode&& other) noexcept;
    TreeNode(TreeNode&& other) noexcept;
    TreeNode(const TreeNode& other);
    ~TreeNode() = default;

    // Getters and setters
    const std::string& getLabelRule() const;
    void setLabelRule(const std::string& label_rule);

    // The label rule can be composed from combining the getNodePath with the getNodeName.
    // getNodeName separates the label rule into parts based on the '/' character, and returns the last part as the node name.
    const std::string getNodeName() const;
    // getNodePath separates the label rule into parts based on the '/' character, and returns everthing up to the node name.
    const std::string getNodePath() const;

    const std::string& getDescription() const;
    void setDescription(const std::string& description);

    const fplus::maybe<std::string>& getQueryHowTo() const;
    void setQueryHowTo(const fplus::maybe<std::string>& query_how_to);

    const fplus::maybe<std::string>& getQaSequence() const;
    void setQaSequence(const fplus::maybe<std::string>& qa_sequence);

    const std::vector<PropertyInfo>& getPropertyInfo() const;
    void setPropertyInfo(const std::vector<PropertyInfo>& property_infos);

    const string& getPolicy() const;
    void setPolicy(const string& policy);

    const std::vector<std::string>& getChildNames() const;
    std::vector<std::string> getAbsoluteChildNames() const;
    void setChildNames(const std::vector<std::string>& child_names);

    const shared_span<>& getPropertyData() const;
    void setPropertyData(shared_span<>&& property_data);

    // The following methods deal with the property data by reading, writing, and inserting properties into the concatenated property_data block.
    // This is to help the user work with the property data, which is stored as a concatenated block of data.
    // Each pair of methods is split into two: one for the fixed-size types, and one for blocks of bytes stored in a shared_span<>.
    // The fixed-size types are currently: int, double, float, bool.
    // The blocks of bytes are text or especially yaml, but may be unencoded binary data too.
    // The property_infos store the type and name of each property, so that the get and set methods do not need to 
    // provide the type string explicitly (although, obviously, there may be cases where the compiler needs the template parameter).

    // getPropertyValue retrieves the property data as a specific type T, it accepts the property name and returns:
    //    the size of the property data, the value of the property, and the raw property value stored in a shared_span<>.
    template<typename T>
    tuple<uint64_t, T, shared_span<>> getPropertyValue(const string& name) const;
    tuple<uint64_t, string> getPropertyString(const string& name) const;
    // getPropertyValueSpan retrieves the property data as a block of bytes, it accepts the property name and returns:
    //    the size of the property data, and the raw property size stored in a shared_span<>, and the property value as a shared_span<>.
    tuple<uint64_t, shared_span<>, shared_span<>> getPropertyValueSpan(const string& name) const;
    
    // setPropertyValue sets the property data as a specific type T, it accepts the property name and value. 
    // It MUST already exist in the properties, otherwise it will throw an exception.
    template<typename T>
    void setPropertyValue(const string& name, const T& value);
    void setPropertyString(const string& name, const string& value);
    // setPropertyValueSpan sets the property data as a block of bytes, it accepts the property name and the raw property value stored in a shared_span<>.
    // It MUST already exist in the properties, otherwise it will throw an exception.
    // The property value stored in data IS allowed to be of a different size than the prior value (for flexibility).
    void setPropertyValueSpan(const string& name, const shared_span<>&& data);
    
    // insertProperty inserts the property data as a specific type T at the specified index, it accepts the property name and value.
    // It MUST NOT already exist in the properties, since this is used to insert new properties.
    // index values are 0-based, but indexes beyond the current size of the property_infos vector induce append behavior.
    template<typename T>
    void insertProperty(size_t index, const string& name, const T& value);
    void insertPropertyString(size_t index, const string& name, const string& type, const string& value);
    // insertPropertySpan inserts the property data as a block of bytes at the specified index, it accepts the property name and type, and the raw property value stored in a shared_span<>.
    // It MUST NOT already exist in the properties, since this is used to insert new properties.
    // index values are 0-based, but indexes beyond the current size of the property_infos vector induce append behavior.
    // Examples of types are "string", "text", "yaml", "png", etc.
    void insertPropertySpan(size_t index, const string& name, const string& type, const shared_span<>&& data);

    // deletePropertyData deletes the property data given by the property name.
    void deleteProperty(const string& name);

    const TreeNodeVersion& getVersion() const;
    void setVersion(const TreeNodeVersion& version);

    bool operator==(const TreeNode& other) const;
    bool operator!=(const TreeNode& other) const {
        return !(*this == other);
    }

    // Operators for reading and writing to a stream
    friend std::ostream& operator<<(std::ostream& os, const TreeNode& node);
    friend std::istream& operator>>(std::istream& is, TreeNode& node);

    void prefixLabels(const std::string& prefix);
    void shortenLabels(const std::string& prefix);

    // Convert the TreeNode to a YAML node for serialization
    // If loadChildren is true, then the child nodes will be interogated on the backend, and loaded as well.
    // However, if loadChildren is false, then the backend will not be used.
    YAML::Node asYAMLNode(Backend &backend, bool loadChildren) const;
    YAML::Node& updateYAMLNode(YAML::Node& yaml) const;

private:
    std::string label_rule;
    std::string description;
    std::vector<PropertyInfo> property_infos;
    TreeNodeVersion version;
    std::vector<std::string> child_names;
    shared_span<> property_data;
    fplus::maybe<std::string> query_how_to;
    fplus::maybe<std::string> qa_sequence;
};

vector<TreeNode> fromYAMLNode(const YAML::Node& node, const std::string& label_prefix, const std::string& name, bool loadChildren);

// Common set of fixed-size property types for TreeNode serialization
inline const std::set<std::string> fixed_size_types = {"int64", "uint64", "double", "float", "bool"};

// For tracking transactions on nodes, each tree node modification has a prior version sequence number
// attached to the Node. 
using NewNodeVersion = std::pair<fplus::maybe<uint16_t>, std::pair<std::string, fplus::maybe<TreeNode>>>;
// A transactional node modification tracks the parent node, and all the descendants.  The prior version
// of the descendants is tracked for one reason:  If the transaction has a prior version that does not match
// at the time the transaction is being applied, then the transaction will fail.  However, the new
// version of the descendants is allowed to be anything at all, because it is being completely "overwritten".
using SubTransaction = std::pair<NewNodeVersion, std::vector<NewNodeVersion>>;
// A transaction is a list of subtransactions.  The transaction is atomic, meaning that all the
using Transaction = std::vector<SubTransaction>;

void prefixTransactionLabels(const std::string& prefix, Transaction& transaction);
void shortenTransactionLabels(const std::string& prefix, Transaction& transaction);

void prefixSubTransactionLabels(const std::string& prefix, SubTransaction& sub_transaction);
void shortenSubTransactionLabels(const std::string& prefix, SubTransaction& sub_transaction);

void prefixNewNodeVersionLabels(const std::string& prefix, NewNodeVersion& new_node_version);
void shortenNewNodeVersionLabels(const std::string& prefix, NewNodeVersion& new_node_version);

