#include "file_backend.h"
#include "simple_backend.h"
#include <dirent.h>
#include <sys/stat.h>
#include <filesystem>

using namespace std;
using namespace fplus;

#ifdef _WIN32
int inotify_init1(int flag) { return -1; }
int inotify_rm_watch(int fd, int wd) { return -1; }
int mkdir(const char* dir, int flag) { return -1; }
int inotify_add_watch(int fd, const char* astr, uint32_t flag) { return -1; }
bool is_dir_dirent(struct dirent* ent) { return false; }
#endif

FileBackend::FileBackend(std::string basePath)
{
    if (basePath.empty()) {
        throw std::invalid_argument("Base Path cannot be empty");
    }
    basePath_ = basePath;
    if (basePath_.back() != '/') {
        basePath_ += '/';
    }
    // Initialize inotify for file system notifications
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotify_fd_ < 0) {
        throw std::runtime_error("Failed to initialize inotify");
    }
}

FileBackend::~FileBackend()
{
    for (const auto& it : wd_to_watchchain_)
    {
        int wd = it.first;
        inotify_rm_watch(inotify_fd_, wd);
    }
    close(inotify_fd_);
}


std::string getNodeDirectoryPath(const std::string& base_path, const std::string& label_rule)
{
    if (label_rule.empty()) {
        return base_path;
    }
    if (label_rule.back() == '/') {
        return base_path + label_rule;
    }
    return base_path + label_rule + "/";
}

std::string getNodeName(const std::string& base_path, const std::string& label_rule)
{
    // The node name is just the final name of the directory path.
    auto node_dir = getNodeDirectoryPath(base_path, label_rule);
    // Remove the trailing slash if it exists.
    if (node_dir.back() == '/') {
        node_dir.pop_back();
    }
    // Find the last slash and return the substring after it.
    auto last_slash_pos = node_dir.find_last_of('/');
    if (last_slash_pos == std::string::npos) {
        throw std::runtime_error("Invalid node directory path: " + node_dir);
    }
    return node_dir.substr(last_slash_pos + 1);
}

std::string getNodeParentPath(const std::string& base_path, const std::string& label_rule)
{
    // The parent path is the same as the getNodeDirectoryPath, but dropping the last part of the path.
    // For example, the parent path of base_path = /mnt/d/code/wwatp/ and label_rule = "Zoo/Lion" is /mnt/d/code/wwatp/Zoo/.
    auto node_dir = getNodeDirectoryPath(base_path, label_rule);
    if (node_dir == base_path) {
        // If the node directory is the same as the base path, return the base path.
        return base_path;
    }
    // the node_dir _always_ has a slash, so remove it first:
    if (node_dir.back() == '/') {
        node_dir.pop_back();
    }
    // Now find the last slash and return the substring up to that point.
    auto last_slash_pos = node_dir.find_last_of('/');
    if (last_slash_pos == std::string::npos) {
        throw std::runtime_error("Invalid node directory path: " + node_dir);
    }
    return node_dir.substr(0, last_slash_pos + 1);
}

std::string getNodeFileName(const std::string& base_path, const std::string& label_rule)
{
    return base_path + label_rule + ".node";
}

std::string getContentFileName(const std::string& base_path, const std::string& label_rule, int order, const TreeNode::PropertyInfo& property_info)
{
    if (property_info.second.empty())
    {
        return base_path + label_rule + "." + std::to_string(order) + "." + property_info.first;
    }
    return base_path + label_rule + "." + std::to_string(order) + "." + property_info.second + "." + property_info.first;
}

std::vector<std::string> getContentFileNames(const std::string& base_path, const std::string& label_rule, const std::vector<TreeNode::PropertyInfo>& property_infos)
{
    std::vector<std::string> content_file_names;
    for (size_t order = 0; order < property_infos.size(); ++order) {
        content_file_names.push_back(getContentFileName(base_path, label_rule, order, property_infos[order]));
    }
    return content_file_names;
}

std::string getLabelRuleFromFileName(const std::string& base_path, const std::string& file_name)
{
    // There are three cases:
    // 1. The file name is a node file: "label_rule.node"
    // 2. The file name is a content file: "label_rule.order.property_info"
    // 3. The file name is a directory: "label_rule/"

    // Case 1 is handled by checking if the file name ends with ".node".
    if (file_name.ends_with(".node")) {
        // Extract the label rule from the file name by removing the ".node" suffix
        std::string label_rule = file_name.substr(base_path.length());
        label_rule = label_rule.substr(0, label_rule.find_last_of('.'));
        return label_rule;
    }
    // Case 2 is handled by checking if the file name contains a dot.
    if (file_name.find('.') != std::string::npos) {
        // Extract the label rule from the file name by removing the base path and everything after the first dot
        std::string label_rule = file_name.substr(base_path.length());
        label_rule = label_rule.substr(0, label_rule.find('.'));
        return label_rule;
    }   
    // Otherwise, it has to be case 3, which is a directory.
    // In this case, the label rule is simply the file name without the base path.
    if (file_name.back() == '/') {
        // Remove the trailing slash
        std::string label_rule = file_name.substr(0, file_name.length() - 1);
        if (file_name.length() <= base_path.length()) {
            // If the file name is shorter than or equal to the base path, return an empty label rule
            return "";
        }
        // Remove the base path
        label_rule = label_rule.substr(base_path.length());
        return label_rule;
    }   
    // Extract the label rule from the file name
    std::string label_rule = file_name.substr(base_path.length());
    return label_rule;
}

vector<std::string> readContentFileNames(const std::string& base_path, const std::string& label_rule)
{
    std::string node_dir = getNodeParentPath(base_path, label_rule);
    auto node_name = getNodeName(base_path, label_rule);
    std::vector<std::string> content_file_names;
    
    // Open the directory
    DIR* dir = opendir(node_dir.c_str());
    if (!dir) {
        if (errno == ENOENT) {
            return content_file_names; // Return empty vector if the directory does not exist
        }
        perror("opendir");
        return content_file_names; // Return empty vector on error
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        // Skip entries that do not start with the node name
        if (strncmp(entry->d_name, node_name.c_str(), node_name.length()) != 0) {
            continue;
        }
        // Skip entries that end in ".node" since they are not content files
        size_t name_len = strlen(entry->d_name);
        if (name_len >= 5 && strcmp(entry->d_name + name_len - 5, ".node") == 0) {
            continue;
        }
        // Check if the entry is a content file
        if (strstr(entry->d_name, ".") != nullptr) {
            content_file_names.push_back(node_dir + entry->d_name);
        }
    }
    
    closedir(dir);
    // Perform a simple lexicographical sort on the file names
    std::sort(content_file_names.begin(), content_file_names.end());
    return content_file_names;
}

vector<TreeNode::PropertyInfo> parsePropertyInfos(vector<std::string> content_file_names)
{
    std::vector<TreeNode::PropertyInfo> property_infos;
    for (const auto& file_name : content_file_names) {
        // Extract the literal type from the file name
        size_t last_dot = file_name.find_last_of('.');
        size_t last_slash = file_name.find_last_of('/');
        size_t first_dot = file_name.find_first_of('.', last_slash + 1);
        size_t second_dot = string::npos;
        if (first_dot != std::string::npos) {
            second_dot = file_name.find_first_of('.', first_dot + 1);
        }
        if (last_dot != std::string::npos) {
            std::string property_type = file_name.substr(last_dot + 1);
            std::string property_name;
            if ((second_dot != std::string::npos) && (second_dot > first_dot + 1)) {
                // If there is a second dot, the property name is between the first and second dot
                property_name = file_name.substr(second_dot + 1, last_dot - second_dot - 1);
            }
            property_infos.push_back({property_type, property_name});
        }
    }
    return property_infos;
}

shared_span<> readContentFiles(vector<std::string> content_file_names)
{
    auto infos = parsePropertyInfos(content_file_names);
    // Some infos are fixed size, known in advance, such as int, double, string, etc.
    // The rest of the infos are variable size.
    //static set<std::string> variable_size_types = {"string", "text", "json", "yaml", "xml", "html", "css", "py"};
    std::vector<shared_span<>> data_spans;
    size_t order = 0;
    for (const auto& file_name : content_file_names) {
        // Open the file
        std::ifstream file(file_name, std::ios::binary);
        if (!file) {
            std::cerr << "Error opening file: " << file_name << std::endl;
            continue; // Skip this file if it cannot be opened
        }

        // Read the file property_data into a buffer
        std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(file), {});
        // if this is not a fixed size type, we need to add the size of the buffer in a span first:
        if (fixed_size_types.find(infos[order].first) == fixed_size_types.end()) {
            // First, add a chunk for the size of the data
            payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
            uint64_t buffer_size = buffer.size();
            data_spans.emplace_back(header, std::span<uint64_t>(reinterpret_cast<uint64_t*>(&buffer_size), 1));
            payload_chunk_header data_header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, buffer.size());
            data_spans.emplace_back(data_header, std::span<uint8_t>(buffer));
        }
        else {
            if (infos[order].first == "int64") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                int64_t the_int = 0;
                stringstream int_stream(std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
                int_stream >> the_int;
                data_spans.emplace_back(header, std::span<int64_t>(&the_int, 1));
            }
            if (infos[order].first == "uint64") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                uint64_t the_int = 0;
                stringstream int_stream(std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
                int_stream >> the_int;
                data_spans.emplace_back(header, std::span<uint64_t>(&the_int, 1));
            }
            if (infos[order].first == "double") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                double the_double = 0.0;
                stringstream double_stream(std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
                double_stream >> the_double;
                data_spans.emplace_back(header, std::span<double>(&the_double, 1));
            }
            if (infos[order].first == "float") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                float the_float = 0.0f;
                stringstream float_stream(std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
                float_stream >> the_float;
                data_spans.emplace_back(header, std::span<float>(&the_float, 1));
            }
            if (infos[order].first == "bool") {
                payload_chunk_header header(0, payload_chunk_header::SIGNAL_OTHER_CHUNK, 8);
                bool the_bool = false;
                stringstream bool_stream(std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
                std::string bool_str;
                bool_stream >> bool_str;
                if (bool_str == "TRUE" || bool_str == "True" || bool_str == "true" || bool_str == "1") {
                    the_bool = true;
                } else if (bool_str == "FALSE" || bool_str == "False" || bool_str == "false" || bool_str == "0" || bool_str == "") {
                    the_bool = false;
                } else {
                    std::cerr << "Error parsing bool from file: " << file_name << std::endl;
                }
                data_spans.emplace_back(header, std::span<bool>(&the_bool, 1));
            }
        }
        order++;
        // Close the file
        file.close();
    }

    // Return a shared span of the data
    auto concatted_span = shared_span<>(data_spans.begin(), data_spans.end());
    return concatted_span;
}

fplus::maybe<TreeNode> readNodeFile(const std::string& base_path, const std::string& label_rule)
{
    std::string node_file_name = getNodeFileName(base_path, label_rule);
    if (!std::filesystem::exists(node_file_name)) {
        return maybe<TreeNode>(); // Return nothing if the file does not exist
    }
    std::ifstream node_file(node_file_name, std::ios::binary);
    if (!node_file) {
        return maybe<TreeNode>(); // Return nothing if the file cannot be opened
    }
    
    TreeNode node;
    try {
        // Read the node data from the file
        node_file >> node; // Use the overloaded operator>> to read the node
    } catch (const std::exception& e) {
        std::cerr << "Error reading node file: " << e.what() << std::endl;
        return maybe<TreeNode>(); // Return nothing if there is an error reading the file
    }
    
    return maybe<TreeNode>(node); // Return the populated TreeNode wrapped in a maybe
}

std::vector<TreeNode> readNodesRecursively(string basePath_, string relative_path)
{
    std::vector<TreeNode> tree;
    string base_directory = basePath_ + relative_path;
    vector<string> directories_to_read;
    // Open the base directory and read all files
    for (const auto& entry : std::filesystem::directory_iterator(base_directory)) {
        // Every node must have a directory, so entries not directories are skipped.
        if (entry.is_directory()) {
            // also skip "." and ".." directories
            if (entry.path().filename() == "." || entry.path().filename() == "..")
                continue;
            auto node_label = getLabelRuleFromFileName(basePath_, base_directory + entry.path().filename().string());
            auto node_directory = getNodeDirectoryPath(basePath_, node_label);
            auto node = readNodeFromFiles(basePath_, node_label);
            if (node.is_just()) {
                tree.push_back(node.unsafe_get_just());
                directories_to_read.push_back(node_directory);
            }
        }
    }
    for (auto dir : directories_to_read) {
        auto children = readNodesRecursively(basePath_, dir.substr(basePath_.length()));
        tree.insert(tree.end(), children.begin(), children.end());
    }

    return tree;
}

bool createNodeDirectory(const std::string& base_path, const std::string& label_rule)
{
    if (label_rule.empty() || label_rule == "/") {
        return true;
    }
    auto parent_path = getNodeParentPath(base_path, label_rule);
    std::string parent_label_rule = getLabelRuleFromFileName(base_path, parent_path);
    createNodeDirectory(base_path, parent_label_rule);
    // Get the node directory path
    std::string node_dir = getNodeDirectoryPath(base_path, label_rule);
    // Create the directory if it does not exist
    if (mkdir(node_dir.c_str(), 0755) == -1) {
        if (errno != EEXIST) {
            perror("mkdir");
            return false; // Return false if the directory could not be created
        }
    }
    return true; // Return true if the directory was created or already exists
}

void writeNodeFile(const std::string& base_path, const TreeNode& node)
{
    std::string node_file_name = getNodeFileName(base_path, node.getLabelRule());
    std::ofstream node_file(node_file_name, std::ios::binary);
    if (!node_file) {
        throw std::runtime_error("Error opening node file for writing: " + node_file_name);
    }
    // Write the node data to the file, but since the property_data are handled separately, pass the ofstream the flag to hide property_data.
    node_file << hide_contents << node;
    node_file.close();
}

void writeContentFiles(vector<TreeNode::PropertyInfo> property_infos, vector<std::string> content_file_names, const shared_span<>& const_data)
{
    shared_span<> remaining_span(const_data);
    for (size_t i = 0; i < property_infos.size(); ++i) {
        std::ofstream content_file(content_file_names[i], std::ios::binary);
        if (!content_file) {
            throw std::runtime_error("Error opening content file for writing: " + content_file_names[i]);
        }
        // Check if the content type is fixed size or variable size
        if (property_infos[i].first == "int64") {
            int64_t the_int = *remaining_span.begin<int64_t>();
            stringstream int_stream;
            int_stream << the_int;
            std::string int_str = int_stream.str();
            content_file.write(int_str.c_str(), int_str.size());
            remaining_span = remaining_span.restrict(pair(sizeof(int64_t), remaining_span.size() - sizeof(int64_t)));
        }
        if (property_infos[i].first == "uint64") {
            uint64_t the_int = *remaining_span.begin<uint64_t>();
            stringstream int_stream;
            int_stream << the_int;
            std::string int_str = int_stream.str();
            content_file.write(int_str.c_str(), int_str.size());
            remaining_span = remaining_span.restrict(pair(sizeof(uint64_t), remaining_span.size() - sizeof(uint64_t)));
        }
        if (property_infos[i].first == "double") {
            double the_double = *remaining_span.begin<double>();
            stringstream double_stream;
            double_stream << the_double;
            std::string double_str = double_stream.str();
            content_file.write(double_str.c_str(), double_str.size());
            remaining_span = remaining_span.restrict(pair(sizeof(double), remaining_span.size() - sizeof(double)));
        }
        if (property_infos[i].first == "float") {
            float the_float = *remaining_span.begin<float>();
            stringstream float_stream;
            float_stream << the_float;
            std::string float_str = float_stream.str();
            content_file.write(float_str.c_str(), float_str.size());
            remaining_span = remaining_span.restrict(pair(sizeof(float), remaining_span.size() - sizeof(float)));
        }
        if (property_infos[i].first == "bool") {
            bool the_bool = *remaining_span.begin<bool>();
            std::string bool_str = the_bool ? "true" : "false";
            content_file.write(bool_str.c_str(), bool_str.size());
            remaining_span = remaining_span.restrict(pair(sizeof(bool), remaining_span.size() - sizeof(bool)));
        }
        // For variable size infos, we need to write the size first, then the content
        if (fixed_size_types.find(property_infos[i].first) == fixed_size_types.end()) {
            uint64_t the_size = *remaining_span.begin<uint64_t>();
            shared_span<> content_span = remaining_span.restrict(pair(sizeof(uint64_t), the_size));
            for (auto byte : content_span.range<uint8_t>()) {
                content_file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
            }
            remaining_span = remaining_span.restrict(pair(sizeof(uint64_t) + the_size, remaining_span.size() - sizeof(uint64_t) - the_size));
        }
        content_file.close();
    }
}

bool writeNodeToFiles(const std::string& base_path, const TreeNode& node)
{
    try {
        createNodeDirectory(base_path, node.getLabelRule());
        writeNodeFile(base_path, node);
        auto filenames = getContentFileNames(base_path, node.getLabelRule(), node.getPropertyInfo());
        auto property_infos = parsePropertyInfos(filenames);
        writeContentFiles(property_infos, filenames, node.getPropertyData());
    } catch (const std::exception& e) {
        std::cerr << "Error writing node to files: " << e.what() << std::endl;
        return false;
    }
    return true;
}

fplus::maybe<TreeNode> readNodeFromFiles(const std::string& base_path, const std::string& label_rule)
{
    try {
        auto node = readNodeFile(base_path, label_rule);
        if (node.is_nothing()) {
            return node;
        }
        auto content_files = readContentFileNames(base_path, label_rule);
        auto property_infos = parsePropertyInfos(content_files);
        node.unsafe_get_just().setPropertyData(readContentFiles(content_files));
        // Override the property_info with the parsed property_infos, because users will usually just modify the file names, not the .node file.
        node.unsafe_get_just().setPropertyInfo(property_infos);
        return node;
    } catch (const std::exception& e) {
        std::cerr << "Error reading node from files: " << e.what() << std::endl;
        return maybe<TreeNode>();
    }
}

bool deleteDirectoryRecursively(const std::string& base_path, const std::string& dir_path)
{
    // base_path is passed in for paranoia checks, so we refuse to delete anything that does not start with it.
    if (dir_path.substr(0, base_path.size()) != base_path) {
        std::cerr << "Refusing to delete directory outside of base path: " << dir_path << std::endl;
        return false; // Return false if the directory is outside the base path
    }
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        // If the directory is absent, then just return true.
        if (errno == ENOENT) {
            return true; // Directory does not exist, so nothing to delete
        }
        perror("opendir");
        return false; // Return false if the directory could not be opened
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip the current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string full_path = dir_path + "/" + entry->d_name;
        if (is_dir_dirent(entry)) {
            // Recursively delete subdirectories
            if (!deleteDirectoryRecursively(base_path, full_path)) {
                closedir(dir);
                return false; // Return false if a subdirectory could not be deleted
            }
        } else {
            // Remove the file
            if (full_path.substr(0, base_path.size()) != base_path) {
                std::cerr << "Refusing to delete file outside of base path: " << full_path << std::endl;
                closedir(dir);
                return false; // Return false if the file is outside the base path
            }
            if (remove(full_path.c_str()) == -1) {
                perror("remove");
                closedir(dir);
                return false; // Return false if a file could not be removed
            }
        }
    }
    closedir(dir);

    // Remove the directory itself
    if (rmdir(dir_path.c_str()) == -1) {
        perror("rmdir");
        return false; // Return false if the directory could not be removed
    }
    return true; // Return true if the directory and its property_data were removed successfully
}

bool deleteNodeFiles(const std::string& base_path, const std::string& label_rule, bool recursive)
{
    std::string node_dir = getNodeDirectoryPath(base_path, label_rule);
    bool success = true;
    if (recursive) {
        success = deleteDirectoryRecursively(base_path, node_dir);
    }
    if (!success) {
        std::cerr << "Failed to delete node directory: " << node_dir << std::endl;
    }
    // Also delete each content file individually
    auto content_files = readContentFileNames(base_path, label_rule);
    for (const auto& content_file : content_files) {
        if (content_file.substr(0, base_path.size()) != base_path) {
            std::cerr << "Refusing to delete content file outside of base path: " << content_file << std::endl;
            continue; // Skip files outside the base path
        }
        if (remove(content_file.c_str()) == -1) {
            // But if it fails because it is already deleted, we just ignore it.
            if (errno == ENOENT) {
                continue; // File does not exist, so we can ignore this error
            }
            perror("remove");
            std::cerr << "Failed to delete content file: " << content_file << std::endl;
            success = false; // Set success to false if any file could not be deleted
        }
    }
    // Also delete the node file
    std::string node_file = getNodeFileName(base_path, label_rule);
    if (node_file.substr(0, base_path.size()) != base_path) {
        std::cerr << "Refusing to delete node file outside of base path: " << node_file << std::endl;
        return false; // Skip if the node file is outside the base path
    }
    if (remove(node_file.c_str()) == -1) {
        if (errno != ENOENT) {
            perror("remove");
            std::cerr << "Failed to delete node file: " << node_file << std::endl;
            success = false; // Set success to false if the node file could not be deleted
        }
        // else if it fails because it is already deleted, we just ignore it.
    }
    return success;
}


// Retrieve a node by its label rule.
fplus::maybe<TreeNode> FileBackend::getNode(const std::string& label_rule) const
{
    // Read the node from the files.
    return readNodeFromFiles(basePath_, label_rule);
}

// Add or update a parent node and its children in the tree.
bool FileBackend::upsertNode(const std::vector<TreeNode>& nodes)
{
    // Iterate through each node and write it to the file system.
    for (const auto& node : nodes) {
        string label_rule = node.getLabelRule();
        auto m_old_node = getNode(label_rule);
        if (m_old_node.is_just()) {
            // If the old node exists, we need to look for properties that are now missing:
            auto old_node = m_old_node.unsafe_get_just();
            auto old_property_infos = old_node.getPropertyInfo();
            auto new_property_infos = node.getPropertyInfo();
            // Find properties that are in the old node but not in the new node
            std::vector<pair<size_t, TreeNode::PropertyInfo>> missing_properties;
            size_t order = 0;
            for (const auto& old_property : old_property_infos) {
                if (std::find(new_property_infos.begin(), new_property_infos.end(), old_property) == new_property_infos.end()) {
                    missing_properties.push_back({order, old_property});
                }
                order++;
            }
            // If there are missing properties, we need to delete the corresponding content files
            if (!missing_properties.empty()) {
                for (const auto& missing_property : missing_properties) {
                    std::string content_file_name = getContentFileName(basePath_, label_rule, missing_property.first, missing_property.second);
                    if (content_file_name.substr(0, basePath_.length()) != basePath_) {
                        std::cerr << "Refusing to delete content file outside of base path: " << content_file_name << std::endl;
                        continue; // Skip files outside the base path
                    }
                    if (remove(content_file_name.c_str()) == -1) {
                        if (errno != ENOENT) {
                            perror("remove");
                            std::cerr << "Failed to delete content file: " << content_file_name << std::endl;
                            return false; // Return false if any file could not be deleted
                        }
                        // else if it fails because it is already deleted, we just ignore it.
                    }
                }
            }
        }
        if (!writeNodeToFiles(basePath_, node)) {
            std::cerr << "Failed to upsert node: " << node.getLabelRule() << std::endl;
            return false; // Return false if any node fails to write
        }
        notifyListeners(node.getLabelRule(), node);
    }
    return true; // Return true if all nodes were successfully written
}

// Delete a node and its children from the tree.
bool FileBackend::deleteNode(const std::string& label_rule)
{
    // Delete the node files from the file system.
    if (!deleteNodeFiles(basePath_, label_rule, true)) {
        std::cerr << "Failed to delete node: " << label_rule << std::endl;
        return false; // Return false if the node could not be deleted
    }
    notifyListeners(label_rule, maybe<TreeNode>());
    return true; // Return true if the node was successfully deleted
}

std::vector<TreeNode> FileBackend::getPageTree(const std::string& page_node_label_rule) const
{
    // Check if the page node exists
    auto page_node = getNode(page_node_label_rule);
    if (page_node.is_nothing()) {
        return {};
    }
    // Now collect the children label rules
    auto children_label_rules = page_node.lift_def(std::vector<std::string>{}, [](const TreeNode& node) {
        return node.getChildNames();
    });
    vector<TreeNode> children_nodes;
    for (const auto& child_label_rule : children_label_rules) {
        auto child_node = getNode(child_label_rule);
        if (child_node.is_nothing()) {
            std::cerr << "Failed to get child node: " << child_label_rule << std::endl;
            continue; // Skip this child if it cannot be retrieved
        }
        children_nodes.push_back(child_node.unsafe_get_just());
    }
    return children_nodes;
}

std::vector<TreeNode> FileBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const
{
    // concatenate the label rule of the node with the label rule of the page node
    std::string full_label_rule = node.getLabelRule() + "/" + page_node_label_rule;
    // Check if the page node exists
    auto page_node = getNode(full_label_rule);
    if (page_node.is_nothing()) {
        throw std::runtime_error("Page node not found: " + full_label_rule);
    }
    return getPageTree(full_label_rule);
}

std::vector<TreeNode> FileBackend::queryNodesFromPath(const std::string& base_directory, const std::string& label_rule) const
{    
    std::vector<TreeNode> result;
    set<std::string> label_rules_set; // To avoid duplicates
    DIR* dir = opendir(base_directory.c_str());
    if (!dir) {
        if (errno == ENOENT) {
            return result; // Return empty vector if the directory does not exist
        }
        perror("opendir");
        return result; // Return empty vector on error
    }
    std::vector<std::string> subdirs_to_recurse; // To store subdirectories for later recursion
    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (is_dir_dirent(entry)) {
            // Skip the "." and ".." directories
            if (entry->d_name == std::string(".") || entry->d_name == std::string("..")) {
                continue;
            }
            std::string node_label = getLabelRuleFromFileName(basePath_, base_directory + entry->d_name);
            // Check if the directory name matches the label rule
            if (node_label.find(label_rule) != std::string::npos) {
                // If it matches, read the node from the file system
                // Avoid duplicates by checking if the label rule is already in the set
                if (label_rules_set.find(node_label) == label_rules_set.end()) {
                    label_rules_set.insert(node_label);
                    auto node = readNodeFromFiles(basePath_, node_label);
                    if (node.is_just()) {
                        result.push_back(node.unsafe_get_just());
                    }
                }
            }
            // Add to subdirs_to_recurse for later recursion
            subdirs_to_recurse.push_back(base_directory + entry->d_name + "/");
        }
    }
    closedir(dir);
    // Now recurse into subdirectories after closing the current directory
    for (const auto& subdir : subdirs_to_recurse) {
        auto matching_children = queryNodesFromPath(subdir, label_rule);
        result.insert(result.end(), matching_children.begin(), matching_children.end());
    }
    return result;
}

// Query nodes matching a label rule.
std::vector<TreeNode> FileBackend::queryNodes(const std::string& label_rule) const
{    
    // TODO:  This whole concept is currently half baked.  I'm waiting to see what the use cases are before designing it further.
    // For now, just return all nodes that match the label rule.
    // So recurse from the base path and find all directories that match the label rule.
    return queryNodesFromPath(basePath_, label_rule);
}

std::vector<TreeNode> FileBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const
{
    // Get the full label rule by concatenating the node's label rule with the provided label rule
    std::string full_label_rule = node.getLabelRule() + "/" + label_rule;
    return queryNodes(full_label_rule);
}

bool FileBackend::openTransactionLayer(const TreeNode&) {
    throw std::runtime_error("FileBackend does not support opening transaction layers");
    return false;
}

bool FileBackend::closeTransactionLayers(void) {
    throw std::runtime_error("FileBackend does not support closing transaction layers");
    return false;
}

bool FileBackend::applyTransaction(const Transaction& transaction) {
    // First check all the sub transactions to verify them.
    for (const auto& sub_transaction : transaction) {
        if (!canPerformSubTransaction(sub_transaction)) {
            // runtime error includes the label rule of the node that failed, and the number of children changes
            throw std::runtime_error("Transaction is not valid: " + sub_transaction.first.second.first + " with " +
                std::to_string(sub_transaction.second.size()) + " modified children.");
        }
    }
    // Then perform the sub transactions
    for (const auto& sub_transaction : transaction) {
        if (!performSubTransaction(sub_transaction)) {
            // runtime error includes the label rule of the node that failed, and the number of children changes
            throw std::runtime_error("Transaction failed to apply: " + sub_transaction.first.second.first + " with " +
                std::to_string(sub_transaction.second.size()) + " modified children.");
        }
    }
    auto success = true;

    if (success) {
        // Notify listeners for each operation in the transaction
        for (const auto& sub_transaction : transaction) {
            auto parent_node_update = sub_transaction.first;
            auto label_rule = parent_node_update.second.first;
            notifyListeners(label_rule, parent_node_update.second.second);
            for (const auto& child_node_update : sub_transaction.second) {
                auto child_label_rule = child_node_update.second.first;
                notifyListeners(child_label_rule, child_node_update.second.second);
            }
        }
    }
    return success;
}

// Check if a SubTransaction can be performed without error.
bool FileBackend::canPerformSubTransaction(const SubTransaction& sub_transaction) const
{
    // Check if the parent node exists
    const auto& parent_node_label = sub_transaction.first.second.first;
    auto node_file = getNodeFileName(basePath_, parent_node_label);
    // If the node file does not exist, we cannot perform the transaction
    if (!std::filesystem::exists(node_file)) {
        if (sub_transaction.first.first.is_nothing()) {
            return true; // The transaction expected there was no prior node at all, and it is missing, so good.
        }
        return false; // Parent node does not exist, but was expected by the transaction
    }
    auto old_node = readNodeFromFiles(basePath_, parent_node_label);
    // Then check if the parent node sequence number matches the old version
    if (sub_transaction.first.first.is_nothing()) {
        // The transaction expected there was no prior node at all, so it is OK if the old node is missing.
        if (old_node.is_nothing()) {
            return true; // No prior node, so this is OK
        }
        return false; // Parent node exists, but the transaction expected it to be missing
    }
    else {
        if (old_node.is_nothing()) {
            return false; // Parent node does not exist, but the transaction expected it to
        }
        if (old_node.unsafe_get_just().getVersion().version_number != sub_transaction.first.first.unsafe_get_just()) {
            return false; // Parent node version does not match
        }
    }
    // Check that the new version number is 1 greater than the old version number, (or wraps to 0 if at max)
    maybe<uint16_t> next_version((old_node.unsafe_get_just().getVersion().version_number + 1) % (old_node.unsafe_get_just().getVersion().max_version_sequence));
    if (next_version != sub_transaction.first.second.second.and_then([](const auto& node) { return maybe(node.getVersion().version_number); })) {
        return false; // New version number is not an increment
    }

    // Check each of the child nodes to make sure some other modification has not occurred
    for (auto child : sub_transaction.second) {
        string old_child_label = child.second.first;
        auto old_child_file = getNodeFileName(basePath_, old_child_label);
        auto old_child = readNodeFromFiles(basePath_, old_child_label);
        if (old_child.is_just()) {
            if (child.first.is_nothing()) { // No version supplied so
                // The attempted transaction was expecting no child in the tree
                // This is OK if and only if the child is being deleted
                if (child.second.second.is_just()) {
                    return false; // Child node is not being deleted, but the transaction was expecting it to exist
                }
            } else {
                if (maybe(old_child.unsafe_get_just().getVersion().version_number) != child.first) {
                    return false; // Child node version does not match that expected by the transaction
                }
            }
        } else { // old_child == nothing
            if (child.first.is_just()) {
                return false; // Child node does not exist, but the transaction was expecting it to
            }
        }
    }

    return true;
}

// ASSUMING THAT THE TRANSACTION IS VALID, perform the transaction.
bool FileBackend::performSubTransaction(const SubTransaction& sub_transaction)
{
    // Update the parent node
    auto& parent_node = sub_transaction.first.second.second;
    if (parent_node.is_just()) {
        writeNodeToFiles(basePath_, parent_node.unsafe_get_just()); // Update existing parent node
    } else {
        deleteNodeFiles(basePath_, sub_transaction.first.second.first, true); // Delete parent node
    }

    // Update or delete child nodes
    for (const auto& child : sub_transaction.second) {
        const auto& child_change = child.second;
        if (child_change.second.is_just()) {
            writeNodeToFiles(basePath_, child_change.second.unsafe_get_just()); // Create or update child node
        } else {
            deleteNodeFiles(basePath_, child_change.first, true); // Delete child node
        }
    }

    return true;
}

// Retrieve the entire tree structure (for debugging or full sync purposes).
std::vector<TreeNode> FileBackend::getFullTree() const
{
    return readNodesRecursively(basePath_, "");
}

vector<FileBackend::WatchChain> node_watch_chain(const std::string& base_path, const std::string& label_rule) {
    vector<FileBackend::WatchChain> watch_chains;
    FileBackend::WatchChain directory_watch_chain;
    auto node_dir = getNodeParentPath(base_path, label_rule);
    while(node_dir.length() >= base_path.length()) {
        // Add the directory to the watch chain
        directory_watch_chain.push_front({IN_CREATE | IN_DELETE | IN_IGNORED, node_dir});
        if (node_dir == base_path) {
            // If we reach the base path, we stop adding directories
            break;
        }
        // Move up to the parent directory
        node_dir = getNodeParentPath(base_path, getLabelRuleFromFileName(base_path, node_dir));
    }
    watch_chains.push_back(directory_watch_chain);
    watch_chains.back().push_back({IN_MODIFY | IN_IGNORED, getNodeFileName(base_path, label_rule)});
    auto content_files = readContentFileNames(base_path, label_rule);
    for (const auto& content_file : content_files) {
        watch_chains.push_back(directory_watch_chain);
        watch_chains.back().push_back({IN_MODIFY | IN_IGNORED, content_file});
    }
    return watch_chains;
}

vector<FileBackend::WatchChain> collect_watch_paths(const std::string& base_path, const std::string& label_rule, bool child_notify) {
    vector<FileBackend::WatchChain> watchChains = node_watch_chain(base_path, label_rule);
    // If child_notify is true, also watch the directory for changes
    if (child_notify) {
        auto node_dir = getNodeDirectoryPath(base_path, label_rule);
        // For each directory in the node_dir, get the label_rule, and collect_watch_paths with child_notify = true on them
        for (const auto& entry : std::filesystem::directory_iterator(node_dir)) {
            if (entry.is_directory()) {
                auto child_label_rule = getLabelRuleFromFileName(base_path, node_dir + entry.path().filename().string());
                auto child_watch_paths = collect_watch_paths(base_path, child_label_rule, true);
                watchChains.insert(watchChains.end(), child_watch_paths.begin(), child_watch_paths.end());
            }
        }
    }
    return watchChains;
}

void FileBackend::setupWatch(WatchChainSpecifier watch_chain_specifier) 
{
    auto notification_rule = get<0>(watch_chain_specifier);
    auto listener_name = get<1>(watch_chain_specifier);
    auto child_notify = get<2>(watch_chain_specifier);
    auto watch_chains = collect_watch_paths(basePath_, notification_rule, child_notify);
    watch_chains_[watch_chain_specifier] = watch_chains;
    list<int> wds;
    int watch_chain_vector_index = 0;
    for (const auto& chain : watch_chains) {
        WatchChainIndex watch_chain_index(watch_chain_specifier, watch_chain_vector_index);
        for (const auto& desired : chain) {
            if (!std::filesystem::exists(desired.second)) {
                continue; // Skip this path if it does not exist
            }
            int wd = inotify_add_watch(inotify_fd_, desired.second.c_str(), desired.first);
            if (wd < 0) {
                perror("inotify_add_watch");
                throw std::runtime_error("Failed to add inotify watch for path: " + desired.second);
            }
            FullWatchSpecifier full_watch_specifier(desired, watch_chain_index);
            if (wd_to_watchchain_.find(wd) != wd_to_watchchain_.end()) {
                wd_to_watchchain_[wd].insert(full_watch_specifier);
            } else {
                wd_to_watchchain_[wd] = {full_watch_specifier};
            }
            watchchain_to_wd_[full_watch_specifier] = wd;
        }
        watch_chain_vector_index++;
    }
}

void FileBackend::teardownWatch(WatchChainSpecifier watch_chain_specifier)
{
    auto findit = watch_chains_.find(watch_chain_specifier);
    if (findit != watch_chains_.end()) {
        // Remove all watches for this watch chain specifier
        for (const auto& chain : findit->second) {
            for (const auto& desired : chain) {
                auto full_watch_specifier = FullWatchSpecifier(desired, WatchChainIndex(watch_chain_specifier, 0));
                auto wd_it = watchchain_to_wd_.find(full_watch_specifier);
                if (wd_it != watchchain_to_wd_.end()) {
                    int wd = wd_it->second;
                    if (inotify_rm_watch(inotify_fd_, wd) < 0) {
                        perror("inotify_rm_watch");
                    }
                    wd_to_watchchain_.erase(wd);
                    watchchain_to_wd_.erase(full_watch_specifier);
                }
            }
        }
        watch_chains_.erase(findit);
    }
}

void FileBackend::freeWatch(WatchChainSpecifier watch_chain_specifier)
{
    auto findit = watch_chains_.find(watch_chain_specifier);
    if (findit != watch_chains_.end()) {
        // Remove all watches for this watch chain specifier
        for (const auto& chain : findit->second) {
            for (const auto& desired : chain) {
                for (size_t watch_chain_vector_index = 0; watch_chain_vector_index < findit->second.size(); ++watch_chain_vector_index) {
                    auto watch_chain_index = WatchChainIndex(watch_chain_specifier, watch_chain_vector_index);
                    auto full_watch_specifier = FullWatchSpecifier(desired, watch_chain_index);
                    auto wd_it = watchchain_to_wd_.find(full_watch_specifier);
                    if (wd_it != watchchain_to_wd_.end()) {
                        int wd = wd_it->second;
                        wd_to_watchchain_[wd].erase(full_watch_specifier);
                        if (wd_to_watchchain_[wd].empty()) {
                            if (inotify_rm_watch(inotify_fd_, wd) < 0) {
                                perror("inotify_rm_watch");
                            }
                            wd_to_watchchain_.erase(wd);
                        }
                    }
                    watchchain_to_wd_.erase(full_watch_specifier);
                }
            }
        }
        watch_chains_.erase(findit);
    }
}

void FileBackend::onWatchModifyEvent(int wd)
{
    auto findit = wd_to_watchchain_.find(wd);
    if (findit != wd_to_watchchain_.end()) {
        auto full_watch_specifiers = findit->second;
        for (auto full_watch_specifier : full_watch_specifiers) {
            auto watch_chain_index = full_watch_specifier.second;
            auto watch_chain_specifier = watch_chain_index.first;
            auto label_rule = get<0>(watch_chain_specifier);
            // Read the node from the files
            auto node = readNodeFromFiles(basePath_, label_rule);
            // Notify listeners for this label rule
            notifyListeners(label_rule, node);
        }
    } else {
        std::cerr << "[inotify] modify event: No watch found for wd: " << wd << std::endl;
    }
}

void FileBackend::onWatchCreateEvent(int wd, const std::string& notification_path)
{
    // This only occurs for directories, so depending on the exact path that the watcher is watching, it
    // will imply creating new watchers for the new directory and/or file.
    auto findit = wd_to_watchchain_.find(wd);
    if (findit != wd_to_watchchain_.end()) {
        auto full_watch_specifiers = findit->second;
        for (auto full_watch_specifier : full_watch_specifiers) {
            auto desired_watch = full_watch_specifier.first;
            auto watch_chain_index = full_watch_specifier.second;
            auto watch_chain_specifier = watch_chain_index.first;
            auto watch_chain_vector_index = watch_chain_index.second;
            string createdPath = desired_watch.second + notification_path;
            auto label_rule = get<0>(watch_chain_specifier);
            // Either the notification_path has a '.' in it, and it is the ultimate watch being created,
            // OR the notification_path is yet another directory in the watch chain.
            if (notification_path.find('.') != std::string::npos) {
                // Read the node from the files
                auto node = readNodeFromFiles(basePath_, label_rule);
                // Notify listeners for this label rule
                notifyListeners(label_rule, node);
                // And then completely rebuild the watches for this watch_chain_specifier
                freeWatch(watch_chain_specifier);
                setupWatch(watch_chain_specifier);
            }
            else
            {
                if (createdPath.back() != '/') {
                    createdPath += '/'; // Ensure the path ends with a '/'
                }
                // Find the NEXT desired_watch in the watch chain
                auto watch_chain_it = watch_chains_.find(watch_chain_specifier);
                if (watch_chain_it != watch_chains_.end() && watch_chain_it->second.size() > watch_chain_vector_index) {
                    // We have a valid watch chain, so we can add the next watch
                    WatchChain watch_chain = watch_chain_it->second[watch_chain_vector_index];
                    // Find the index of the desired_watch in the watch chain
                    auto cur_watch_it = std::find(watch_chain.begin(), watch_chain.end(), desired_watch);
                    auto next_watch_it = std::next(cur_watch_it);
                    // If the next_watch matches the notification_path, we can add a new watch for it
                    if (next_watch_it != watch_chain.end() && next_watch_it->second == createdPath) {
                        while(next_watch_it != watch_chain.end()) {
                            // Add a new watch for the next desired path
                            int new_wd = inotify_add_watch(inotify_fd_, next_watch_it->second.c_str(), next_watch_it->first);
                            if (new_wd < 0) {
                                int err = errno;
                                std::cerr << "[inotify_add_watch] Failed to add inotify watch for path: " << notification_path
                                        << ", errno=" << err << ": " << strerror(err) << std::endl;
                                break; // If we fail to add the watch, it is probably because the object hasn't been created yet, so this is the best we can do so far.
                            }
                            FullWatchSpecifier next_full_watch_specifier(*next_watch_it, watch_chain_index);
                            if (wd_to_watchchain_.find(new_wd) != wd_to_watchchain_.end()) {
                                wd_to_watchchain_[new_wd].insert(next_full_watch_specifier);
                            } else {
                                wd_to_watchchain_[new_wd] = {next_full_watch_specifier};
                            }
                            watchchain_to_wd_[next_full_watch_specifier] = new_wd;
                            next_watch_it = std::next(next_watch_it);
                            if (next_watch_it == watch_chain.end())
                            {
                                // Read the node from the files
                                auto node = readNodeFromFiles(basePath_, label_rule);
                                // Notify listeners for this label rule
                                notifyListeners(label_rule, node);
                                // And then completely rebuild the watches for this watch_chain_specifier
                                freeWatch(watch_chain_specifier);
                                setupWatch(watch_chain_specifier);
                                break;
                            }
                        }
                    }
                }
            }
        }
    } else {
        //std::cerr << "[inotify] create event: No watch found for wd: " << wd << std::endl;
    }
}

void FileBackend::onWatchDeleteEvent(int wd, const std::string& notification_path) 
{
    // This closely echos the onWatchCreateEvent, but we need to remove the watch from the maps.
    // This only occurs for directories, so depending on the exact path that the watcher is watching, it
    // will imply creating new watchers for the new directory and/or file.
    auto findit = wd_to_watchchain_.find(wd);
    if (findit != wd_to_watchchain_.end()) {
        auto full_watch_specifiers = findit->second;
        for (auto full_watch_specifier : full_watch_specifiers) {
            auto desired_watch = full_watch_specifier.first;
            auto watch_chain_index = full_watch_specifier.second;
            auto watch_chain_specifier = watch_chain_index.first;
            auto watch_chain_vector_index = watch_chain_index.second;
            string createdPath = desired_watch.second + notification_path;
            auto label_rule = get<0>(watch_chain_specifier);
            // Either the notification_path has a '.' in it, and it is the ultimate watch being destroyed,
            // OR the notification_path is yet another directory in the watch chain.
            if (notification_path.find('.') != std::string::npos) {
                // Need to delete an old watch for the last desired_watch in the chain
                auto watch_chain_it = watch_chains_.find(watch_chain_specifier);
                if (watch_chain_it != watch_chains_.end())
                {
                    auto file_watch = watch_chain_it->second[watch_chain_vector_index].back();
                    if (file_watch.second == createdPath) {
                        FullWatchSpecifier old_full_watch_specifier(file_watch, watch_chain_index);
                        if (watchchain_to_wd_.find(old_full_watch_specifier) != watchchain_to_wd_.end()) {
                            if (file_watch.second.substr(file_watch.second.size() - 5) == ".node")
                            {
                                auto node = maybe<TreeNode>();
                                notifyListeners(label_rule, node); // Notify listeners that the file is deleted
                            }
                            else {  // IF it is not the .node file, then it is considered a modification not a deletion from the backend point of view.
                                auto node = readNodeFromFiles(basePath_, label_rule);
                                if (node.is_just()) {  // The actual delete notification will be sent with the .node file notification.
                                    notifyListeners(label_rule, node); // Notify listeners that the _node_ is modified
                                }
                            }
                            int old_wd = watchchain_to_wd_[old_full_watch_specifier];
                            watchchain_to_wd_.erase(old_full_watch_specifier);
                            auto find_wd = wd_to_watchchain_.find(old_wd);
                            if (find_wd != wd_to_watchchain_.end()) {
                                // Remove the watch from the wd_to_watchchain map
                                find_wd->second.erase(old_full_watch_specifier);
                                // If the wd_to_watchchain entry is empty, remove it
                                if (find_wd->second.empty()) {
                                    wd_to_watchchain_.erase(find_wd); // Remove the watch descriptor if no more watches are associated with it
                                    inotify_rm_watch(inotify_fd_, old_wd);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (createdPath.back() != '/') {
                    createdPath += '/'; // Ensure the path ends with a '/'
                }
                // Find the NEXT desired_watch in the watch chain
                auto watch_chain_it = watch_chains_.find(watch_chain_specifier);
                if (watch_chain_it != watch_chains_.end() && watch_chain_it->second.size() > watch_chain_vector_index) {
                    // We have a valid watch chain, so we can add the next watch
                    WatchChain watch_chain = watch_chain_it->second[watch_chain_vector_index];
                    // Find the index of the desired_watch in the watch chain
                    auto cur_watch_it = std::find(watch_chain.begin(), watch_chain.end(), desired_watch);
                    auto next_watch_it = std::next(cur_watch_it);
                    // If the next_watch matches the notification_path, we should remove the old watch for it
                    if (next_watch_it != watch_chain.end() && next_watch_it->second == createdPath) {
                        auto dir_watch = *next_watch_it;
                        FullWatchSpecifier next_full_watch_specifier(dir_watch, watch_chain_index);
                        if (watchchain_to_wd_.find(next_full_watch_specifier) != watchchain_to_wd_.end()) {
                            int old_wd = watchchain_to_wd_[next_full_watch_specifier];
                            watchchain_to_wd_.erase(next_full_watch_specifier);
                            auto find_wd = wd_to_watchchain_.find(old_wd);
                            if (find_wd != wd_to_watchchain_.end()) {
                                // Remove the watch from the wd_to_watchchain map
                                find_wd->second.erase(next_full_watch_specifier);
                                // If the wd_to_watchchain entry is empty, remove it
                                if (find_wd->second.empty()) {
                                    wd_to_watchchain_.erase(find_wd); // Remove the watch descriptor if no more watches are associated with it
                                    inotify_rm_watch(inotify_fd_, old_wd);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        std::cerr << "[inotify] delete event: No watch found for wd: " << wd << std::endl;
    }
}

void FileBackend::registerNodeListener(const std::string listener_name, const std::string notification_rule, bool child_notify, NodeListenerCallback callback) {
    deregisterNodeListener(listener_name, notification_rule);

    WatchChainSpecifier wc_specifier(notification_rule, listener_name, child_notify);
    setupWatch(wc_specifier);
    auto findit = node_listeners_.find(notification_rule);
    ListenerInfo a_listener = {wc_specifier, callback};
    if (findit != node_listeners_.end()) {
        // If the label_rule already exists, add the listener to the existing list
        auto& listeners = findit->second;
        listeners.push_back(a_listener);
    } else {
        // If the label_rule does not exist, create a new entry
        node_listeners_.emplace(notification_rule, std::list<ListenerInfo>{a_listener});
    }
}

void FileBackend::deregisterNodeListener(const std::string listener_name, const std::string notification_rule) {
    // Deregister a listener for a specific label rule
    auto found = node_listeners_.find(notification_rule);
    if (found != node_listeners_.end()) {
        auto& listeners = found->second;
        // First, clean up inotify watches for all listeners to be removed
        for (auto it = listeners.begin(); it != listeners.end(); ++it) {
            WatchChainSpecifier wc_specifier = std::get<0>(*it);
            if (get<1>(wc_specifier) == listener_name) {
                teardownWatch(wc_specifier); // Remove the watch for this listener
            }
        }
        // Now erase all matching listeners
        listeners.erase(std::remove_if(listeners.begin(), listeners.end(), [&](const ListenerInfo& listener) {
            return std::get<1>(std::get<0>(listener)) == listener_name;
        }), listeners.end());
        // If the list of listeners is empty, remove the label rule from the map
        if (listeners.empty()) {
            node_listeners_.erase(found);
        }
    }
}

void FileBackend::notifyListeners(const std::string& label_rule, const maybe<TreeNode>& node) {
    if (node_listeners_.empty()) {
        return; // No listeners registered
    }
    auto found = node_listeners_.upper_bound(label_rule);
    if (found != node_listeners_.begin()) {
        // Since found is not begin(), it proves there is at least one element
        --found; // Move to the last element that is less than or equal to label_rule
    } else {
        return; // No elements to check
    }
    while (partialLabelRuleMatch(found->first, label_rule)) {
        auto listeners = found->second;
        // Notify all listeners for this label_rule
        for (auto listener : listeners) {
            if (found->first == label_rule) {
                // The parent == label and callback is unconditionally met
                std::get<1>(listener)(*this, label_rule, node);
            } else if (std::get<1>(listener) && checkLabelRuleOverlap(found->first, label_rule)) {
                // The label_rule contains the listener's label_rule, and the callback matches children
                std::get<1>(listener)(*this, label_rule, node);
            } // Otherwise, the label_rule contains the listener's label_rule, but the callback does not match children
        }
        if (found != node_listeners_.begin()) {
            --found; // Move to the last element that is less than or equal to label_rule
        } else {
            break; // No more elements to check
        }
    }
}

void FileBackend::processNotifications()
{
    ssize_t length = read(inotify_fd_, buffer_, sizeof(buffer_));
    if (length < 0) {
        int err = errno;
        if (err == EAGAIN) {
            // No events available, not an error in non-blocking mode
            return;
        }
        std::cerr << "[inotify] read() returned " << length << ", errno=" << err << ": " << strerror(err) << std::endl << flush;
        return;
    }

    ssize_t offset = 0;
    while (offset < length) {
        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(&buffer_[offset]);
        int wd = event->wd;
        int mask = event->mask;
        if ((mask & IN_MODIFY) == IN_MODIFY) {
            onWatchModifyEvent(wd);
        }
        if (event->len > 0) {
            string inotify_path = event->name;
            if ((mask & IN_CREATE) == IN_CREATE) {
                onWatchCreateEvent(wd, inotify_path);
            }
            if ((mask & IN_DELETE) == IN_DELETE) {
                onWatchDeleteEvent(wd, inotify_path);
            }
        }
        offset += sizeof(struct inotify_event) + event->len;
    }
}
