#include "yaml_mediator.h"

#include <atomic>
#include "tree_node.h"
#include "simple_backend.h"

using namespace std;
using namespace fplus;


static shared_ptr<MemoryTree> global_empty_memory_tree = make_shared<MemoryTree>();
static SimpleBackend global_null_backend(global_empty_memory_tree); 

namespace {
    static std::atomic<size_t> yaml_mediator_instance_counter{0};
}

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }
    return tokens;
}

bool isMapNode(maybe<const YAML::Node>& node) {
    return node.lift_def(false, [](auto& node) { return node.IsMap(); });
}

template<typename T>
maybe<T> getChildNode(T& parentNode, const string& childName) {
    for (auto it = parentNode.begin(); it != parentNode.end(); ++it) {
        auto name = it->first.template as<string>();
        if (name == childName) {
            return maybe<T>(it->second);
        }
    }
    return maybe<T>();
}

void updateYAMLFrom(YAML::Node yaml, string nodeLabel, const maybe<TreeNode>& m_modifiedNode, Backend &backend, bool loadChildren) {
    // Update the YAML node with the data from the TreeNode
    // First, get the chain of parent/child names from the label_rule by splitting it by the '/' character.
    vector<string> parts = nodeLabel.empty() ? vector<string>{} : split(nodeLabel, '/');
    // Since you can't have an empty label rule in a node, parts has to always contain at least one element.
    if (yaml.IsNull()) {
        if (m_modifiedNode.is_nothing()) {
            return; // If the YAML node is null and m_modifiedNode is nothing, do nothing
        }
        yaml = YAML::Node();
    }
    maybe<YAML::Node> curNode(yaml);
    for (size_t i = 0; i < parts.size(); ++i) {
        YAML::Node parentNode = curNode.get_with_default(yaml);
        curNode = getChildNode(parentNode, parts[i]);
        if (i == parts.size() - 1) 
        {   
            if (m_modifiedNode.is_just()) {
                parentNode[parts[i]] = m_modifiedNode.unsafe_get_just().asYAMLNode(backend, loadChildren);
            } else {
                if (curNode.is_just()) {
                    // If the node does not exist, remove it from YAML
                    parentNode.remove(parts[i]);
                }
            }
        }
        else {
            if (curNode.is_nothing()) {
                if (m_modifiedNode.is_nothing())
                    break;
                parentNode[parts[i]] = YAML::Node();
                curNode = maybe<YAML::Node>(parentNode[parts[i]]);
            }
        }
    }
}

vector<shared_span<>> createSharedSpanFromYAML(const YAML::Node yaml) {
    vector<shared_span<>> data_spans;
    ostringstream ss;
    ss << yaml;
    string yaml_string = ss.str();
    size_t yaml_size = yaml_string.size();
    payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
    data_spans.emplace_back(header, std::span<uint64_t>(reinterpret_cast<uint64_t*>(&yaml_size), 1));
    payload_chunk_header data_header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, yaml_size);
    data_spans.emplace_back(data_header, std::span<const char>(yaml_string));
    return data_spans;
}

void toYAMLCallback(Backend& yamlBackend, Backend& backend, YAML::Node yaml, PropertySpecifier const& specifier, atomic<bool>& setProcessing, atomic<bool>& isProcessing, 
        const string& label_rule, const maybe<TreeNode>& m_modifiedNode) {
    // To YAML callback will do these steps:
    // 1. Update the YAML representation of the node by updating the value of the YAML according to the label rule.
    // 2. Update the YAML property at the PropertySpecifier.
    if (isProcessing.load()) {
        return; // Ignore if already processing
    }
    setProcessing.store(true);
    updateYAMLFrom(yaml, label_rule, m_modifiedNode, backend, true);
    
    vector<shared_span<>> yaml_data = createSharedSpanFromYAML(yaml);
    string node_label = specifier.getNodeLabel();
    auto m_to = yamlBackend.getNode(node_label);
    if (m_to.is_nothing()) {
        shared_span<> concatted(yaml_data.begin(), yaml_data.end());
        TreeNode yamlNode(node_label, 
            node_label + " YAML Container", 
            { {specifier.getPropertyType(), specifier.getPropertyName()} },
            DEFAULT_TREE_NODE_VERSION, // Default version for YAML nodes
            {},
            std::move(concatted), 
            maybe<string>(), // No query how-to for YAML nodes
            maybe<string>() // No QA sequence for YAML nodes
        );
        yamlBackend.upsertNode({yamlNode});
        setProcessing.store(false);
        return;
    } else {
        auto toNode = m_to.unsafe_get_just();
        auto property_infos = toNode.getPropertyInfo();
        TreeNode::PropertyInfo property_info(specifier.getPropertyType(), specifier.getPropertyName());
        auto findit = find(property_infos.begin(), property_infos.end(), property_info);
        shared_span<> value_span = yaml_data[1];
        if (findit != property_infos.end()) {
            toNode.setPropertyValueSpan(specifier.getPropertyName(), std::move(value_span));
        } else {
            toNode.insertPropertySpan(property_infos.size(), specifier.getPropertyName(), specifier.getPropertyType(), std::move(value_span));
            property_infos.push_back(property_info);
            toNode.setPropertyInfo(property_infos);
        }
        yamlBackend.upsertNode({toNode});
    }
    setProcessing.store(false);
}

string getYAMLString(const TreeNode& node, const PropertySpecifier& specifier) {
    auto property_data = node.getPropertyValueSpan(specifier.getPropertyName());
    auto yaml_span = get<2>(property_data);
    string yaml(yaml_span.begin<char>(), yaml_span.end<char>());
    return yaml;
}

bool isNodeModified(const TreeNode& node, const YAML::Node yaml) {
    // First, check if the node label is in yaml
    auto label_rule = node.getLabelRule();
    vector<string> parts = split(label_rule, '/');
    maybe<const YAML::Node> m_curNode(yaml);
    for (const auto& part : parts) {
        if (isMapNode(m_curNode)) {
            m_curNode = getChildNode(m_curNode.unsafe_get_just(), part);
        } else {            
            return true; // If any part of the label rule is not found in YAML, the node is deleted and therefore modified
        }
    }
    if (m_curNode.is_nothing()) {
        return true; // If we didn't find the node in YAML, it is considered modified (deleted)
    }
    auto curNode = m_curNode.unsafe_get_just();
    // Now check each of the properties in the node
    vector<TreeNode> compare_nodes = fromYAMLNode(curNode, "", label_rule, false);
    if (compare_nodes.empty()) {
        return true; // If we couldn't parse the YAML node, it is considered modified (deleted)
    }
    TreeNode& compare_node = compare_nodes[0];
    return node != compare_node;
}

set<string> walk_children(string prefix, const YAML::Node node) {
    set<string> names;
    maybe<const YAML::Node> childNames = getChildNode(node, "child_names");
    if (childNames.is_just()) {
        for (const auto& it : childNames.get_with_default(YAML::Node())) {
            string name = prefix.empty() ? it.as<string>() : prefix + "/" + it.as<string>();
            names.insert(name);
            maybe<const YAML::Node> childNode = getChildNode(node, it.as<string>());
            auto child_names = walk_children(name, childNode.get_with_default(YAML::Node()));
            names.insert(child_names.begin(), child_names.end());
        }
    }
    return names;
}

set<string> walk_names(string prefix, const YAML::Node node) {
    set<string> names;
    if (node.IsMap()) {
        for (const auto& it : node) {
            string name = prefix.empty() ? it.first.as<string>() : prefix + "/" + it.first.as<string>();
            names.insert(name);
            auto child_names = walk_children(name, it.second);
            names.insert(child_names.begin(), child_names.end());
        }
    } else if (node.IsSequence()) {
        for (size_t i = 0; i < node.size(); ++i) {
            string name = prefix + "[" + to_string(i) + "]";
            names.insert(name);
            auto child_names = walk_children(name, node[i]);
            names.insert(child_names.begin(), child_names.end());
        }
    }
    return names;
}

maybe<TreeNode> getNodeFromYAML(const string& label_rule, const YAML::Node yaml) {
    vector<string> parts = split(label_rule, '/');
    maybe<const YAML::Node> curNode(yaml);
    for (const auto& part : parts) {
        if (isMapNode(curNode)) {
            curNode = getChildNode(curNode.unsafe_get_just(), part);
        } else {
            return nothing<TreeNode>(); // If any part of the label rule is not found in YAML, return nothing
        }
    }
    auto label_prefix = parts.size() > 1 ? label_rule.substr(0, label_rule.rfind('/') + 1) : "";
    // Now we have the current node, we can convert it to a TreeNode
    return curNode.lift_def(maybe<TreeNode>(), [label_prefix, parts](const YAML::Node& node) {
        vector<TreeNode> result = fromYAMLNode(node, label_prefix, parts.back(), false);
        return result.empty() ? nothing<TreeNode>() : just(result[0]);
    });
}

void fromYAMLCallback(Backend& backend, YAML::Node yaml, PropertySpecifier const& specifier, atomic<bool>& setProcessing, atomic<bool>& isProcessing, 
        const string& label_rule, const maybe<TreeNode>& m_modifiedYAMLNode) {
    // From YAML callback will do these steps:
    // 1. Reload the YAML representation of the node by using the YAML::Load function to parse the YAML string.
    // 2. Update the to backend based on any changes noticed between the new YAML representation and the old one.
    // 3. Replace the old YAML representation with the new one.
    if (isProcessing.load()) {
        return; // Ignore if already processing
    }
    setProcessing.store(true);
    if (m_modifiedYAMLNode.is_nothing()) {
        throw runtime_error("Cannot update backend without an existing YAML node: " + label_rule);
    }
    TreeNode fromNode = m_modifiedYAMLNode.unsafe_get_just();
    vector<TreeNode> nodes = backend.getFullTree();
    // Create a set of node_labels for detecting created nodes
    yaml = YAML::Load(getYAMLString(fromNode, specifier));
    auto walked_names = walk_names("", yaml);
    set<string> created_nodes(walked_names.begin(), walked_names.end());
    vector<TreeNode> modifiedNodes;
    vector<TreeNode> deletedNodes;
    for (auto node : nodes) {
        if (isNodeModified(node, yaml)) {
            auto label_rule = node.getLabelRule();
            auto m_node = getNodeFromYAML(label_rule, yaml);
            if (m_node.is_nothing()) {
                deletedNodes.push_back(node);
            } else {
                modifiedNodes.push_back(m_node.unsafe_get_just());
            }
        }
        created_nodes.erase(node.getLabelRule());
    }
    // For each created node, create a new TreeNode and add it to the modifiedNodes
    for (const auto& created_node : created_nodes) {
        auto m_node = getNodeFromYAML(created_node, yaml);
        if (m_node.is_nothing()) {
            continue; // If the node is not found in YAML, skip it
        }
        modifiedNodes.push_back(m_node.unsafe_get_just());
    }
    if (!modifiedNodes.empty()) {
        backend.upsertNode(modifiedNodes);
    }
    for (const auto& deleted_node : deletedNodes) {
        backend.deleteNode(deleted_node.getLabelRule());
    }
    setProcessing.store(false);
}

YAMLMediator::YAMLMediator(const std::string& name, Backend& tree, Backend& yamlTree, const PropertySpecifier& specifier, bool initialize_from_yaml)
    : name_(name), backendTree_(tree), backendYAMLTree_(yamlTree), specifier_(specifier)
{
    size_t instance_id = yaml_mediator_instance_counter++;
    treeListenerName_ = "YAMLMediatorTree_" + specifier.getNodeLabel() + "_" + std::to_string(instance_id);
    yamlTreeListenerName_ = "YAMLMediatorYAMLTree_" + specifier.getNodeLabel() + "_" + std::to_string(instance_id);
    yamlRepresentation_ = YAML::Node();
    if (initialize_from_yaml) {
        // Initialize the backend tree with the YAML data source
        auto m_yamlMode = yamlTree.getNode(specifier.getNodeLabel());
        if (m_yamlMode.is_nothing()) {
            throw runtime_error("Node with label " + specifier.getNodeLabel() +
                                     " does not exist in the YAML tree.");
        }
        fromYAMLCallback(backendTree_, yamlRepresentation_, specifier_, processingYAMLTree_, processingTree_,
            specifier.getNodeLabel(), m_yamlMode);
    } else {
        // Initialize the YAML data source with the information from the backend tree
        auto fullTree = backendTree_.getFullTree();
        maybe<TreeNode> m_node;
        for (const auto& node : fullTree) {
            if (m_node.is_nothing()) {
                m_node = just(node);
                continue;
            } 
            updateYAMLFrom(yamlRepresentation_, node.getLabelRule(), just(node), global_null_backend, false);
        }
        auto label_rule = m_node.lift_def("", [](const TreeNode& node) { return node.getLabelRule(); });
        toYAMLCallback(backendYAMLTree_, backendTree_, yamlRepresentation_, specifier_, processingTree_, processingYAMLTree_,
            label_rule, m_node); // Use the first node as a reference for the YAML representation
    }
    processingTree_.store(false);
    processingYAMLTree_.store(false);
    // Create a callback for the tree backend to listen for changes
    string root_node_label_rule = ""; 
    backendTree_.registerNodeListener(treeListenerName_, root_node_label_rule, true, [this](Backend&, const string& label_rule, const maybe<TreeNode>& m_node) {
        toYAMLCallback(backendYAMLTree_, backendTree_, yamlRepresentation_, specifier_, processingTree_, processingYAMLTree_, label_rule, m_node);
    });

    // Create a callback for the YAML tree backend to listen for changes
    backendYAMLTree_.registerNodeListener(yamlTreeListenerName_, specifier_.getNodeLabel(), false, [this](Backend&, const string& label_rule, const maybe<TreeNode>& m_node) {
        fromYAMLCallback(backendTree_, yamlRepresentation_, specifier_, processingYAMLTree_, processingTree_, label_rule, m_node);
    });
}

YAMLMediator::~YAMLMediator() {
    // Unregister the callbacks
    backendTree_.deregisterNodeListener(treeListenerName_, "");
    backendYAMLTree_.deregisterNodeListener(yamlTreeListenerName_, "");
}