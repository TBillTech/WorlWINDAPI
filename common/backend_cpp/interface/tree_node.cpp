#include "tree_node.h"
#include "backend.h"

using namespace std;

TreeNodeVersion fromYAMLNode(const YAML::Node& node) {
    TreeNodeVersion version;
    // Provide default values if fields are missing
    version.version_number = node["version_number"] ? node["version_number"].as<uint16_t>() : 0;
    version.max_version_sequence = node["max_version_sequence"] ? node["max_version_sequence"].as<uint16_t>() : 256;
    version.policy = node["policy"] ? node["policy"].as<std::string>() : "default";
    
    if (node["authorial_proof"]) {
        version.authorial_proof = fplus::just(node["authorial_proof"].as<std::string>());
    } else {
        version.authorial_proof = fplus::nothing<std::string>();
    }
    if (node["authors"]) {
        version.authors = fplus::just(node["authors"].as<std::string>());
    } else {
        version.authors = fplus::nothing<std::string>();
    }
    if (node["readers"]) {
        version.readers = fplus::just(node["readers"].as<std::string>());
    } else {
        version.readers = fplus::nothing<std::string>();
    }
    if (node["collision_depth"]) {
        version.collision_depth = fplus::just(node["collision_depth"].as<int>());
    } else {
        version.collision_depth = fplus::nothing<int>();
    }
    return version;
}


TreeNode::TreeNode()
    : label_rule(""), description(""), property_infos({}), 
      version(), child_names({}), property_data(global_no_chunk_header, false),
      query_how_to(fplus::maybe<string>()), qa_sequence(fplus::maybe<string>())
    {}

TreeNode::TreeNode(const std::string& label_rule, const std::string& description, 
        const std::vector<TreeNode::PropertyInfo>& property_infos,
        const TreeNodeVersion& version,
        const std::vector<std::string>& child_names,
        shared_span<>&& property_data, 
        const fplus::maybe<std::string>& query_how_to,
        const fplus::maybe<std::string>& qa_sequence)
    : label_rule(label_rule), description(description), property_infos(property_infos),
      version(version), child_names(child_names), property_data(std::move(property_data)), 
      query_how_to(query_how_to), qa_sequence(qa_sequence)
    {
        // label_rule cannot have whitespace
        if (label_rule.find_first_of(" \t\n\r") != string::npos) {
            throw invalid_argument("Label rule cannot contain whitespace: " + label_rule);
        }
        for (auto child_name: child_names) {
            // Child names cannot have whitespace
            if (child_name.find_first_of(" \t\n\r") != string::npos) {
                throw invalid_argument("Child names cannot contain whitespace: " + child_name);
            }
        }
        if (query_how_to == fplus::maybe<string>("")) {
            this->query_how_to = fplus::maybe<string>();
        }
        if (qa_sequence == fplus::maybe<string>("")) {
            this->qa_sequence = fplus::maybe<string>();
        }
    }

// The assignment operators are for the most part typical, except for the handling of the property_data.
// When doing move semantics, the property_data need to use move semantics.
// When doing copy semantics, the property_data need to use the shared_span restrict method to get a reference counted view of the data.
TreeNode& TreeNode::operator=(const TreeNode& other) {
    if (this != &other) {
        label_rule = other.label_rule;
        description = other.description;
        property_infos = other.property_infos;
        version = other.version;
        child_names = other.child_names;
        auto content_total_range = make_pair(0, other.property_data.size());
        property_data = std::move(other.property_data.restrict(content_total_range));
        query_how_to = other.query_how_to;
        qa_sequence = other.qa_sequence;
    }
    return *this;
}

TreeNode& TreeNode::operator=(TreeNode&& other) noexcept {
    if (this != &other) {
        label_rule = std::move(other.label_rule);
        description = std::move(other.description);
        property_infos = std::move(other.property_infos);
        version = std::move(other.version);
        child_names = std::move(other.child_names);
        property_data = std::move(other.property_data);
        version = std::move(other.version);
        query_how_to = std::move(other.query_how_to);
        qa_sequence = std::move(other.qa_sequence);
    }
    return *this;
}

TreeNode::TreeNode(TreeNode&& other) noexcept
    : label_rule(std::move(other.label_rule)), description(std::move(other.description)),
      property_infos(std::move(other.property_infos)), version(std::move(other.version)),
      child_names(std::move(other.child_names)), property_data(std::move(other.property_data)),
      query_how_to(std::move(other.query_how_to)), qa_sequence(std::move(other.qa_sequence))
      {}

TreeNode::TreeNode(const TreeNode& other)
    : label_rule(other.label_rule), description(other.description),
      property_infos(other.property_infos), version(other.version), 
      child_names(other.child_names), property_data(other.property_data),
      query_how_to(other.query_how_to), qa_sequence(other.qa_sequence)
      {}

const std::string& TreeNode::getLabelRule() const {
    return label_rule;
}

void TreeNode::setLabelRule(const std::string& label_rule) {
    this->label_rule = label_rule;
}

const std::string TreeNode::getNodeName() const {
    auto pos = label_rule.find_last_of('/');
    if (pos != std::string::npos) {
        return label_rule.substr(pos + 1);
    }
    return label_rule;
}

const std::string TreeNode::getNodePath() const {
    auto pos = label_rule.find_last_of('/');
    if (pos != std::string::npos) {
        return label_rule.substr(0, pos + 1);
    }
    return "";
}

const std::string& TreeNode::getDescription() const {
    return description;
}

void TreeNode::setDescription(const std::string& description) {
    this->description = description;
}

const fplus::maybe<std::string>& TreeNode::getQueryHowTo() const {    
    return query_how_to;
}

void TreeNode::setQueryHowTo(const fplus::maybe<std::string>& query_how_to) {
    if (query_how_to == fplus::maybe<string>("")) {
        this->query_how_to = fplus::maybe<string>();
    }else {
        this->query_how_to = query_how_to;
    }
}

const fplus::maybe<std::string>& TreeNode::getQaSequence() const {
    return qa_sequence;
}

void TreeNode::setQaSequence(const fplus::maybe<std::string>& qa_sequence) {
    if (qa_sequence == fplus::maybe<string>("")) {
        this->qa_sequence = fplus::maybe<string>();
    } else {
        this->qa_sequence = qa_sequence;
    }
}

const std::vector<TreeNode::PropertyInfo>& TreeNode::getPropertyInfo() const {
    return property_infos;
}

void TreeNode::setPropertyInfo(const std::vector<TreeNode::PropertyInfo>& property_infos) {
    this->property_infos = property_infos;
}

const string& TreeNode::getPolicy() const {
    return version.policy;
}

void TreeNode::setPolicy(const string& policy) {
    this->version.policy = policy;
}

const std::vector<std::string>& TreeNode::getChildNames() const {
    return child_names;
}

std::vector<std::string> TreeNode::getAbsoluteChildNames() const {
    std::vector<std::string> absolute_child_names;
    for (const auto& child_name : child_names) {
        absolute_child_names.push_back(label_rule + "/" + child_name);
    }
    return absolute_child_names;
}

void TreeNode::setChildNames(const std::vector<std::string>& child_names) {
    this->child_names = child_names;
}

const shared_span<>& TreeNode::getPropertyData() const {
    return property_data;
}

void TreeNode::setPropertyData(shared_span<>&& property_data) {
    this->property_data = std::move(property_data);
}

template<typename T>
string typenameToString() {
    if constexpr (std::is_same_v<T, int64_t>) {
        return "int64";
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        return "uint64";
    } else if constexpr (std::is_same_v<T, float>) {
        return "float";
    } else if constexpr (std::is_same_v<T, double>) {
        return "double";
    } else if constexpr (std::is_same_v<T, bool>) {
        return "bool";
    } else {
        throw std::invalid_argument("Unsupported type for propertyDataAs: " + std::string(typeid(T).name()));
    }
}

template<typename T>
tuple<uint64_t, T, shared_span<>> TreeNode::getPropertyValue(const string& name) const
{
    pair<string, string> property_info = {typenameToString<T>(), name};
    auto it = std::find(property_infos.begin(), property_infos.end(), property_info);
    if (it == property_infos.end()) {
        throw std::invalid_argument("Property not found when calling GetPropertyDataAs: " + name);
    }
    shared_span<> remaining_data(property_data);
    size_t cur_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == property_info) {
            break;            
        }
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto cur_span = remaining_data.restrict(pair(0, sizeof(T)));
    return make_tuple(cur_size, *remaining_data.begin<T>(), cur_span);
}

tuple<uint64_t, string> TreeNode::getPropertyString(const string& name) const
{
    auto [size, size_span, data_span] = getPropertyValueSpan(name);
    return make_tuple(size, string(data_span.begin<const char>(), data_span.end<const char>()));
}

tuple<uint64_t, shared_span<>, shared_span<>> TreeNode::getPropertyValueSpan(const string& name) const
{
    auto it = std::find_if(property_infos.begin(), property_infos.end(), [&](const auto& info) {
        return info.second == name;
    });
    if (it == property_infos.end()) {
        throw std::invalid_argument("Property not found when calling GetPropertyDataAs: " + name);
    }
    auto property_info = *it;
    shared_span<> remaining_data(property_data);
    size_t cur_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == property_info) {
            break;            
        }
        assert(cur_size <= remaining_data.size());
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto size_span = remaining_data.restrict(pair(0, sizeof(uint64_t)));
    auto just_yaml_text = remaining_data.restrict(pair(sizeof(uint64_t), cur_size - sizeof(uint64_t)));
    return make_tuple(cur_size - sizeof(uint64_t), size_span, just_yaml_text);
}

template<typename T>
void TreeNode::setPropertyValue(const string& name, const T& value)
{
    pair<string, string> property_info = {typenameToString<T>(), name};
    auto it = std::find(property_infos.begin(), property_infos.end(), property_info);
    if (it == property_infos.end()) {
        throw std::invalid_argument("Property not found when calling SetPropertyDataAs: " + name);
    }
    shared_span<> remaining_data(property_data);
    size_t cur_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == property_info) {
            break;            
        }
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto value_span = remaining_data.restrict(pair(0, sizeof(T)));
    value_span.copy_type(value);
}

void TreeNode::setPropertyString(const string& name, const string& value)
{
    // convert string to shared_span<>
    payload_chunk_header header(4, payload_chunk_header::SIGNAL_OTHER_CHUNK, value.size());
    shared_span<> data_span(header, std::span<const char>(value.data(), value.size()));
    // set the property data
    setPropertyValueSpan(name, std::move(data_span));
}

void TreeNode::setPropertyValueSpan(const string& name, const shared_span<>&& data)
{
    auto it = std::find_if(property_infos.begin(), property_infos.end(), [&](const auto& info) {
        return info.second == name;
    });
    if (it == property_infos.end()) {
        throw std::invalid_argument("Property not found when calling SetPropertyDataAsBytes: " + name);
    }
    auto property_info = *it;
    shared_span<> remaining_data(property_data);
    size_t cur_size = 0;
    size_t total_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == property_info) {
            break;            
        }
        total_size += cur_size;
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto size_span = remaining_data.restrict(pair(0, sizeof(uint64_t)));
    auto prior_span = property_data.restrict(pair(0, total_size + sizeof(uint64_t)));
    size_span.begin<uint64_t>().set(data.size());
    auto following_span = property_data.restrict(pair(total_size + cur_size, property_data.size() - total_size - cur_size));
    vector<shared_span<>> spans({prior_span, std::move(data), following_span});
    shared_span<> concatted(spans.begin(), spans.end());
    property_data = std::move(concatted);
    property_data.compress();
}

template<typename T>
void TreeNode::insertProperty(size_t index, const string& name, const T& value)
{
    auto find_it = std::find_if(property_infos.begin(), property_infos.end(),
        [&name](const PropertyInfo& info) { return info.second == name; });
    if (find_it != property_infos.end()) {
        throw std::invalid_argument("Property already exists: " + name);
    }
    auto next_info = property_infos.size() ? property_infos[min(index, property_infos.size() - 1)] : pair<string, string>();
    pair<string, string> property_info = {typenameToString<T>(), name};
    auto next_it = std::find(property_infos.begin(), property_infos.end(), next_info);
    shared_span<> remaining_data(property_data);
    size_t total_size = 0;
    size_t cur_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == next_info) {
            break;            
        }
        total_size += cur_size;
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto prior_span = property_data.restrict(pair(0, total_size));
    vector<shared_span<>> data_spans({prior_span});
    auto following_span = property_data.restrict(pair(total_size, property_data.size() - total_size));
    payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, sizeof(T));
    data_spans.emplace_back(header, std::span<const T>(reinterpret_cast<const T*>(&value), 1));
    data_spans.push_back(following_span);
    shared_span<> concatted(data_spans.begin(), data_spans.end());
    property_data = std::move(concatted);
    property_infos.insert(next_it, {typenameToString<T>(), name});
    property_data.compress();
}

void TreeNode::insertPropertyString(size_t index, const string& name, const string& type, const string& value)
{
    // convert string to shared_span<>
    payload_chunk_header header(4, payload_chunk_header::SIGNAL_OTHER_CHUNK, value.size());
    shared_span<> data_span(header, std::span<const char>(value.data(), value.size()));
    // insert the property data
    insertPropertySpan(index, name, type, std::move(data_span));
}

void TreeNode::insertPropertySpan(size_t index, const string& name, const string& type, const shared_span<>&& data)
{
    auto find_it = std::find_if(property_infos.begin(), property_infos.end(),
        [&name](const PropertyInfo& info) { return info.second == name; });
    if (find_it != property_infos.end()) {
        throw std::invalid_argument("Property already exists: " + name);
    }
    auto next_info = index <property_infos.size() ? property_infos[min(index, property_infos.size() - 1)] : pair<string, string>();
    auto next_it = std::find(property_infos.begin(), property_infos.end(), next_info);
    shared_span<> remaining_data(property_data);
    size_t cur_size = 0;
    size_t total_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == next_info) {
            break;            
        }
        total_size += cur_size;
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto prior_span = property_data.restrict(pair(0, total_size));
    vector<shared_span<>> data_spans({prior_span});
    payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
    uint64_t buffer_size = data.size();
    data_spans.emplace_back(header, std::span<uint64_t>(reinterpret_cast<uint64_t*>(&buffer_size), 1));
    payload_chunk_header data_header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, data.size());
    data_spans.push_back(data);
    data_spans.push_back(property_data.restrict(pair(total_size, property_data.size() - total_size)));
    shared_span<> concatted(data_spans.begin(), data_spans.end());
    property_data = std::move(concatted);
    property_infos.insert(next_it, {type, name});
    property_data.compress();
}

void TreeNode::deleteProperty(const string& name)
{
    auto it = std::find_if(property_infos.begin(), property_infos.end(),
        [&name](const PropertyInfo& info) { return info.second == name; });
    if (it == property_infos.end()) {
        return;
    }
    auto property_info = *it;
    shared_span<> remaining_data(property_data);
    size_t total_size = 0;
    size_t cur_size = 0;
    for (const auto& info : property_infos) {
        if ((info.first == "int64") || (info.first == "uint64"))
        {
            cur_size = sizeof(uint64_t);
        }
        if (info.first == "float")
        {
            cur_size = sizeof(float);
        }
        if (info.first == "double")
        {
            cur_size = sizeof(double);
        }
        if (info.first == "bool")
        {
            cur_size = sizeof(bool);
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            cur_size = *remaining_data.begin<uint64_t>();
            cur_size += sizeof(uint64_t);
        }
        if (info == property_info) {
            break;            
        }
        total_size += cur_size;
        remaining_data = remaining_data.restrict(pair(cur_size, remaining_data.size() - cur_size));
    }
    auto prior_span = property_data.restrict(pair(0, total_size));
    auto following_span = property_data.restrict(pair(total_size + cur_size, property_data.size() - total_size - cur_size));
    vector<shared_span<>> data_spans({prior_span, following_span});
    shared_span<> concatted(data_spans.begin(), data_spans.end());
    property_data = std::move(concatted);
    property_infos.erase(it);
    property_data.compress();
}

const TreeNodeVersion& TreeNode::getVersion() const {
    return version;
}

void TreeNode::setVersion(const TreeNodeVersion& version) {
    this->version = version;
}

bool TreeNode::operator==(const TreeNode& other) const {
    bool properties_equal = true;
    properties_equal = label_rule == other.label_rule && properties_equal;
    properties_equal = description == other.description && properties_equal;
    properties_equal = property_infos == other.property_infos && properties_equal;
    properties_equal = version == other.version && properties_equal;
    properties_equal = child_names == other.child_names && properties_equal;
    auto query_how_to_value = query_how_to.get_with_default("");
    auto other_query_how_to_value = other.query_how_to.get_with_default("");
    properties_equal = query_how_to_value == other_query_how_to_value && properties_equal;
    properties_equal = qa_sequence == other.qa_sequence && properties_equal;
    bool contents_equal = property_data.size() == other.property_data.size();
    if (contents_equal) {
        auto this_it = property_data.begin<uint8_t>();
        auto other_it = other.property_data.begin<uint8_t>();
        for (size_t i = 0; i < property_data.size(); ++i) {
            if (*this_it != *other_it) {
                contents_equal = false;
                break;
            }
            ++this_it;
            ++other_it;
        }
    }
    return properties_equal && contents_equal;
}

void writeContentsToYAML(std::vector<TreeNode::PropertyInfo> infos, const shared_span<>& property_data, YAML::Node& node) {
    shared_span<> remaining_span(property_data);
    for (const auto& info : infos) {
        auto type = info.first; // Type of the property
        auto name = info.second + "." + info.first; // Name of the property
        if (type == "int64") {
            if (remaining_span.size() < sizeof(int64_t)) {
                throw std::runtime_error("Not enough data for int64 property: " + name);
            }
            int64_t value = *remaining_span.begin<int64_t>();
            node[name] = value;
            remaining_span = remaining_span.restrict(pair(sizeof(int64_t), remaining_span.size() - sizeof(int64_t)));
        }
        if (type == "uint64") {
            if (remaining_span.size() < sizeof(uint64_t)) {
                throw std::runtime_error("Not enough data for uint64 property: " + name);
            }
            uint64_t value = *remaining_span.begin<uint64_t>();
            node[name] = value;
            remaining_span = remaining_span.restrict(pair(sizeof(uint64_t), remaining_span.size() - sizeof(uint64_t)));
        }
        if (type == "double") {
            if (remaining_span.size() < sizeof(double)) {
                throw std::runtime_error("Not enough data for double property: " + name);
            }
            double value = *remaining_span.begin<double>();
            node[name] = value;
            remaining_span = remaining_span.restrict(pair(sizeof(double), remaining_span.size() - sizeof(double)));
        }
        if (type == "float") {
            if (remaining_span.size() < sizeof(float)) {
                throw std::runtime_error("Not enough data for float property: " + name);
            }
            float value = *remaining_span.begin<float>();
            node[name] = value;
            remaining_span = remaining_span.restrict(pair(sizeof(float), remaining_span.size() - sizeof(float)));
        }
        if (type == "bool") {
            if (remaining_span.size() < sizeof(bool)) {
                throw std::runtime_error("Not enough data for bool property: " + name);
            }
            bool value = *remaining_span.begin<bool>();
            node[name] = value;
            remaining_span = remaining_span.restrict(pair(sizeof(bool), remaining_span.size() - sizeof(bool)));
        }
        // For variable size infos, we need to read the size first, then the content
        if (fixed_size_types.find(type) == fixed_size_types.end()) {
            uint64_t the_size = *remaining_span.begin<uint64_t>();
            shared_span<> content_span = remaining_span.restrict(pair(sizeof(uint64_t), the_size));
            string content(content_span.begin<char>(), content_span.end<char>());
            if (type == "yaml") {
                // If the content is a YAML file, parse it
                YAML::Node content_node = YAML::Load(content);
                node[name] = content_node;
            } else {
                // Otherwise, just store the string
                node[name] = content;
            }
            remaining_span = remaining_span.restrict(pair(sizeof(uint64_t) + the_size, remaining_span.size() - sizeof(uint64_t) - the_size));
        }
    }
}

YAML::Node TreeNode::asYAMLNode(Backend &backend, bool loadChildren) const
{
    YAML::Node node;
    YAML::Node child_names_seq(YAML::NodeType::Sequence);
    for (const auto& child_name : child_names) {
        auto child_label = child_name.find("/") != std::string::npos ? child_name : label_rule + "/" + child_name;
        auto m_node = backend.getNode(child_label);
        child_names_seq.push_back(child_name);
        if (loadChildren){
            // Load the child node's properties
            auto child_node = m_node.lift_def(YAML::Node(YAML::NodeType::Map), [&backend, loadChildren](const TreeNode& child) {
                return child.asYAMLNode(backend, loadChildren);
            });
            node[child_name] = child_node;
        }
        else {
            // Just store the child name as an empty map
            node[child_name] = YAML::Node(YAML::NodeType::Map);
        }
    }
    node["child_names"] = child_names_seq;
    return updateYAMLNode(node);
}

YAML::Node& TreeNode::updateYAMLNode(YAML::Node& yaml) const
{
    // Only include description if it differs from the node name
    if (description != getNodeName()) {
        yaml["description"] = description;
    }
    if (!version.isDefault()) {
        yaml["version"] = version.asYAMLNode();
    }
    if (query_how_to.is_just()) {
        yaml["query_how_to"] = query_how_to.unsafe_get_just();
    }
    if (qa_sequence.is_just()) {
        yaml["qa_sequence"] = qa_sequence.unsafe_get_just();
    }
    writeContentsToYAML(property_infos, property_data, yaml);
    return yaml;
}


vector<TreeNode> fromYAMLNode(const YAML::Node& node, const std::string& label_prefix, const std::string& name, bool loadChildren)
{
    if (!node.IsMap()) {
        throw std::runtime_error("YAML node is not a map");
    }

    std::string label_rule = label_prefix + name;

    // Use node name as default description if not specified in YAML
    std::string description;
    if (node["description"]) {
        description = node["description"].as<std::string>();
    } else {
        description = name;
    }

    TreeNodeVersion version;
    if (node["version"]) {
        version = fromYAMLNode(node["version"]);
    } else {
        // Use default version: {0, 256, "default", all optionals empty}
        version.version_number = 0;
        version.max_version_sequence = 256;
        version.policy = "default";
        version.authorial_proof = fplus::nothing<std::string>();
        version.authors = fplus::nothing<std::string>();
        version.readers = fplus::nothing<std::string>();
        version.collision_depth = fplus::nothing<int>();
    }
    std::set<std::string> children;
    if (node["child_names"]) {
        for (const auto& child_name : node["child_names"]) {
            string child_name_str = child_name.as<std::string>();
            children.insert(child_name_str);
        }
    }

    fplus::maybe<std::string> query_how_to;
    if (node["query_how_to"]) {
        query_how_to = node["query_how_to"].as<std::string>();
        if (query_how_to.get_with_default("") == "") {
            query_how_to = fplus::nothing<std::string>();
        }
    }

    fplus::maybe<std::string> qa_sequence;
    if (node["qa_sequence"]) {
        qa_sequence = node["qa_sequence"].as<std::string>();
        if (qa_sequence.get_with_default("") == "") {
            qa_sequence = fplus::nothing<std::string>();
        }
    }

    std::vector<shared_span<>> data_spans;
    std::vector<TreeNode::PropertyInfo> infos;
    std::vector<std::pair<YAML::Node, std::string>> child_nodes;
    // All the other properties need to be stored in property_infos and contents, unless they match the child_names.
    for (const auto& it : node) {
        if (it.first.as<std::string>() == "label_rule" || it.first.as<std::string>() == "description" ||
            it.first.as<std::string>() == "version" || it.first.as<std::string>() == "child_names" ||
            it.first.as<std::string>() == "query_how_to" || it.first.as<std::string>() == "qa_sequence") {
            continue; // Skip these keys
        }
        std::string full_name = it.first.as<std::string>();
        if (children.find(full_name) != children.end()) {
            child_nodes.push_back({it.second, full_name});
            continue;
        }
        std::string name = it.first.as<std::string>();
        std::string type;
        if (name.find('.') != std::string::npos) {
            // Split the full name into type and name
            auto pos = name.find('.');
            type = name.substr(pos + 1);
            name = name.substr(0, pos);
            if (name.empty()) {
                throw std::runtime_error("Invalid property name: " + it.first.as<std::string>());
            }
        }
        else {
            // Default type is "string" if not specified
            type = "string";
            // However, attempt to do better:
            if (it.second.IsScalar()) {
                std::string value = it.second.as<std::string>();
                if (value == "true" || value == "True" || value == "false" || value == "False") {
                    type = "bool";
                }
                // If it is a number, we treat it as an int64
                else if (std::all_of(value.begin(), value.end(), ::isdigit) || (value[0] == '-' && std::all_of(value.begin() + 1, value.end(), ::isdigit))) {
                    type = "int64";
                }
                // If it is a number with a period, then treat it as a double
                else if (value.find('.') != std::string::npos && std::all_of(value.begin(), value.end(), [](char c) { return ::isdigit(c) || c == '.' || c == '-'; })) {
                    type = "double";
                }
            }
        }
        infos.push_back({type, name});

        if (fixed_size_types.find(type) == fixed_size_types.end()) {
            if (type == "yaml")
            {
                ostringstream yaml_stream;
                yaml_stream << it.second;
                string yaml_content = yaml_stream.str();
                size_t value_size = yaml_content.size();
                // First, add a chunk for the size of the data
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                data_spans.emplace_back(header, std::span<uint64_t>(reinterpret_cast<uint64_t*>(&value_size), 1));
                payload_chunk_header data_header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, value_size);
                data_spans.emplace_back(data_header, std::span<uint8_t>(reinterpret_cast<uint8_t*>(yaml_content.data()), value_size));
            }
            else {
                // First, add a chunk for the size of the data
                string value_bytes = it.second.as<string>();
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                uint64_t value_size = value_bytes.size();
                data_spans.emplace_back(header, std::span<uint64_t>(reinterpret_cast<uint64_t*>(&value_size), 1));
                payload_chunk_header data_header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, value_size);
                data_spans.emplace_back(data_header, std::span<const char>(reinterpret_cast<const char*>(value_bytes.data()), value_size));
            }
        }
        else {
            if (type == "int64") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                int64_t the_int = it.second.as<int64_t>(0);
                data_spans.emplace_back(header, std::span<int64_t>(&the_int, 1));
            }
            if (type == "uint64") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                uint64_t the_int = it.second.as<uint64_t>(0);
                data_spans.emplace_back(header, std::span<uint64_t>(&the_int, 1));
            }
            if (type == "double") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                double the_double = it.second.as<double>(0.0);
                data_spans.emplace_back(header, std::span<double>(&the_double, 1));
            }
            if (type == "float") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                float the_float = it.second.as<float>(0.0f);
                data_spans.emplace_back(header, std::span<float>(&the_float, 1));
            }
            if (type == "bool") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                bool the_bool = it.second.as<bool>(false);
                data_spans.emplace_back(header, std::span<bool>(&the_bool, 1));
            }
        }
    }

    auto property_data = shared_span<>(data_spans.begin(), data_spans.end());
    vector<string> child_names;
    for (const auto& [__, childname] : child_nodes) {
        child_names.push_back(childname);
    };
    vector<TreeNode> nodes = {TreeNode(label_rule, description, infos, version, std::move(child_names), std::move(property_data), query_how_to, qa_sequence)};

    if(loadChildren) {
        // Load the child nodes recursively
        for (const auto& child_node : child_nodes) {
            if (get<1>(child_node).find("/") != std::string::npos) {
                continue; // Because the child will be loaded by the actual parent node not this node, which is detectable when there are slashes in the name.
            }
            auto child_label_prefix = label_rule + "/";
            auto child_nodes = fromYAMLNode(child_node.first, child_label_prefix, child_node.second, loadChildren);
            nodes.insert(nodes.end(), child_nodes.begin(), child_nodes.end());
        }
    }

    return nodes;
}


// We need a string-length-value function in order to implement stream operators for long strings that contain arbitrary data.
// In particular, the concern is that sometimes description might contain statements about property_infos or TreeNodes and so on.
// So, this function accepts a label for the string, and writes out the length of the string, followed by the string itself.
void write_length_string(std::ostream& os, const std::string& label, const std::string& str) {
    os << label << " " << str.size() << " :\n";
    os << str;
    os << "\n";
}
pair<string,string> read_length_string(std::istream& is) {
    size_t length;
    std::string label;
    is >> label;
    is >> length;
    std::string delimiter;
    is >> delimiter; // Read the delimiter (":")
    is.get(); // Consume the newline after the delimiter
    string out_str;
    out_str.resize(length);
    is.read(&out_str[0], length);
    is.get(); // Consume the newline after the string
    return {label, out_str};
}

// Custom flag management
long& hide_contents_flag(std::ios_base& ios) {
    static int index = std::ios_base::xalloc();
    return ios.iword(index);
}

// Manipulator to set the hide_contents flag
std::ostream& hide_contents(std::ostream& os) {
    hide_contents_flag(os) = 1;
    return os;
}

// Manipulator to clear the hide_contents flag
std::ostream& show_contents(std::ostream& os) {
    hide_contents_flag(os) = 0;
    return os;
}

std::ostream& operator<<(std::ostream& os, const TreeNode& node)
{
    os << "TreeNode(\n";
    os << "label_rule: " << node.getLabelRule() << "\n";
    write_length_string(os, "description", node.getDescription());
    os << "property_infos: [ ";
    for (const auto& info : node.getPropertyInfo()) {
        auto type = info.first; // Type of the property
        auto name = info.second; // Name of the property
        os << type << " (" << name << "), ";
    }
    os << "], ";
    os << "version: " << node.getVersion() << ", ";

    os << "child_names: [ ";
    for (const auto& child_name : node.getChildNames()) {
        os << child_name << ", ";
    }
    os << "], ";

    write_length_string(os, "query_how_to", node.getQueryHowTo().get_with_default(""));
    write_length_string(os, "qa_sequence", node.getQaSequence().get_with_default(""));

    if (hide_contents_flag(os) == 0) {
        os << node.getPropertyData();
    } else {
        shared_span<> empty(global_no_chunk_header, false);
        os << empty;
    }

    os << ")\n";
    return os;
}

std::istream& operator>>(std::istream& is, TreeNode& node)
{
    std::string label;
    is >> label; // Consume "TreeNode("

    std::string label_rule;
    is >> label >> label_rule; // Read "label_rule:" and the value
    node.setLabelRule(label_rule);

    auto description = read_length_string(is);
    node.setDescription(description.second);

    std::vector<TreeNode::PropertyInfo> property_infos;
    is >> label; // Consume "property_infos:"
    is >> label; // Consume " ["
    is.get(); // Consume the space after "["
    while (is.peek() != ']') {
        std::string type;
        std::string name;
        is >> type;
        is >> name; // Read the property name
        if (name.back() == ',') {
            name.pop_back(); // Remove the trailing comma
        }
        if (name.front() == '(' && name.back() == ')') {
            name = name.substr(1, name.size() - 2); // Remove parentheses
        }
        if (type.back() == ',') {
            type.pop_back(); // Remove the trailing comma
        }
        if (is.peek() == ' ') {
            is.get(); // Consume the space after the comma
        }
        property_infos.push_back({type, name});
    }
    is >> label; // Consume "]"
    node.setPropertyInfo(property_infos);

    TreeNodeVersion version;
    is >> label >> version; // Read "version:" and the value
    node.setVersion(version);
    is >> label; // Consume "," after version

    std::vector<std::string> child_names;
    is >> label; // Consume "child_names:"
    is >> label; // Consume " ["
    is.get(); // Consume the space after "["
    while (is.peek() != ']') {
        std::string child_name;
        is >> child_name;
        if (child_name.back() == ',') {
            child_name.pop_back(); // Remove the trailing comma
        }
        child_names.push_back(child_name);
        if (is.peek() == ' ') {
            is.get(); // Consume the space after the comma
        }
    }
    is >> label; // Consume "]"
    node.setChildNames(child_names);

    auto query_how_to_str = read_length_string(is);
    if (query_how_to_str.first == "query_how_to") {
        if (query_how_to_str.second.empty()) {
            node.setQueryHowTo(fplus::nothing<std::string>());
        } else {
            node.setQueryHowTo(fplus::just(query_how_to_str.second));
        }
    } else {
        throw std::runtime_error("Expected 'query_how_to' label, got: " + query_how_to_str.first);
    }

    auto qa_sequence_str = read_length_string(is);
    if (qa_sequence_str.first == "qa_sequence") {
        if (qa_sequence_str.second.empty()) {
            node.setQaSequence(fplus::nothing<std::string>());
        } else {
            node.setQaSequence(fplus::just(qa_sequence_str.second));
        }
    } else {
        throw std::runtime_error("Expected 'qa_sequence' label, got: " + qa_sequence_str.first);
    }

    is >> node.property_data; // Read the property_data

    is >> label; // Consume ")"
    is.get(); // Consume the newline after the closing parenthesis

    return is;
}

void TreeNode::prefixLabels(const std::string& prefix)
{
    if (prefix.empty()) {
        return;
    }
    if (prefix.back() != '/') {
        throw std::runtime_error("Prefix must end with a '/'");
    }
    if (label_rule.find(prefix) != 0) {
        label_rule = prefix + label_rule;
    }
    auto new_child_names = std::vector<std::string>(child_names.size());
    for (size_t i = 0; i < child_names.size(); ++i) {
        if (child_names[i].find(prefix) != 0) {
            new_child_names[i] = prefix + child_names[i];
        } else {
            new_child_names[i] = child_names[i];
        }
    }
    child_names = std::move(new_child_names);
}

void TreeNode::shortenLabels(const std::string& prefix)
{
    if (prefix.empty()) {
        return;
    }
    if (label_rule.find(prefix) == 0) {
        label_rule = label_rule.substr(prefix.size());
    }
    for (auto& child_name : child_names) {
        if (child_name.find(prefix) == 0) {
            child_name = child_name.substr(prefix.size());
        }
    }
}


void prefixTransactionLabels(const std::string& prefix, Transaction& transaction)
{
    if (prefix.empty()) {
        return;
    }
    for (auto& sub_transaction : transaction) {
        prefixSubTransactionLabels(prefix, sub_transaction);
    }
}

void shortenTransactionLabels(const std::string& prefix, Transaction& transaction)
{
    if (prefix.empty()) {
        return;
    }
    for (auto& sub_transaction : transaction) {
        shortenSubTransactionLabels(prefix, sub_transaction);
    }
}

void prefixSubTransactionLabels(const std::string& prefix, SubTransaction& sub_transaction)
{
    if (prefix.empty()) {
        return;
    }
    prefixNewNodeVersionLabels(prefix, sub_transaction.first);
    for (auto& new_node_version : sub_transaction.second) {
        prefixNewNodeVersionLabels(prefix, new_node_version);
    }
}

void shortenSubTransactionLabels(const std::string& prefix, SubTransaction& sub_transaction)
{
    if (prefix.empty()) {
        return;
    }
    shortenNewNodeVersionLabels(prefix, sub_transaction.first);
    for (auto& new_node_version : sub_transaction.second) {
        shortenNewNodeVersionLabels(prefix, new_node_version);
    }
}

void prefixNewNodeVersionLabels(const std::string& prefix, NewNodeVersion& new_node_version)
{
    if (prefix.empty()) {
        return;
    }
    if (prefix.back() != '/') {
        throw std::runtime_error("Prefix must end with a '/'");
    }
    if (new_node_version.second.second.is_just()) {
        new_node_version.second.second.unsafe_get_just().prefixLabels(prefix);
    }
    if (new_node_version.second.first.find(prefix) != 0) {
        new_node_version.second.first = prefix + new_node_version.second.first;
    }
}

void shortenNewNodeVersionLabels(const std::string& prefix, NewNodeVersion& new_node_version)
{
    if (prefix.empty()) {
        return;
    }
    if (new_node_version.second.first.find(prefix) == 0) {
        new_node_version.second.first = new_node_version.second.first.substr(prefix.size());
    }
    if (new_node_version.second.second.is_just()) {
        new_node_version.second.second.unsafe_get_just().shortenLabels(prefix);
    }
}

template tuple<uint64_t, int64_t, shared_span<>> TreeNode::getPropertyValue<int64_t>(const string&) const;
template void TreeNode::setPropertyValue<int64_t>(const string&, const int64_t&);
template void TreeNode::insertProperty<int64_t>(size_t, const string&, const int64_t&);
template tuple<uint64_t, uint64_t, shared_span<>> TreeNode::getPropertyValue<uint64_t>(const string&) const;
template void TreeNode::setPropertyValue<uint64_t>(const string&, const uint64_t&);
template void TreeNode::insertProperty<uint64_t>(size_t, const string&, const uint64_t&);
template tuple<uint64_t, double, shared_span<>> TreeNode::getPropertyValue<double>(const string&) const;
template void TreeNode::setPropertyValue<double>(const string&, const double&);
template void TreeNode::insertProperty<double>(size_t, const string&, const double&);
template tuple<uint64_t, float, shared_span<>> TreeNode::getPropertyValue<float>(const string&) const;
template void TreeNode::setPropertyValue<float>(const string&, const float&);
template void TreeNode::insertProperty<float>(size_t, const string&, const float&);
template tuple<uint64_t, bool, shared_span<>> TreeNode::getPropertyValue<bool>(const string&) const;
template void TreeNode::setPropertyValue<bool>(const string&, const bool&);
template void TreeNode::insertProperty<bool>(size_t, const string&, const bool&);