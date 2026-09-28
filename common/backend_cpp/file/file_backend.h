#pragma once

#include <map>
#ifndef _WIN32
#include <sys/inotify.h>
bool is_dir_dirent(struct dirent* ent) { ent->d_type == DT_DIR; }
#else
// Includes for ReadDirectoryChangesW
// But for now, stubs
struct inotify_event {
    int wd;
    int mask;
    int len;
    std::string name;
};
const int IN_NONBLOCK = 0;
const int IN_CLOEXEC = 0;
const int IN_MODIFY = 0;
const int IN_CREATE = 0;
const int IN_DELETE = 0;
const int IN_IGNORED = 0;
#endif
#include <unistd.h>
#include <fcntl.h>

#include "backend.h"

// The FileBackend class implements the Backend interface using a file system as the storage medium.
// It stores nodes in a structured directory format, allowing for easy retrieval and management of tree nodes via the OS file system.
// It does NOT cache any information in memory, because we have a memory backend for that purpose.  Instead it is intended to be synchronous with the file system.
// Therefore, the various notifications must be implemented using inode watchers or similar mechanisms.

// Each node is a set of files and one directory like this:
// 	  Lion/
// 	  Lion.node
// 	  Lion.<order>.<fileformat> (for example Lion.0.triangles.txt, Lion.1.motivations.yaml)
//
// Child nodes then are contained in the parent node's directory, such as:
// 	  Lion/Simba/
//    Lion/Simba.node
//    Lion/Simba.<order>.<fileformat> (for example Lion/Simba.0.dossier.txt, Lion/Simba.1.history.yaml) 
//
// The .node extension file contains the node metadata, such as label rule, description, version, child names, property_data, query how-to, and QA sequence.
// Note that the property_infos field is derived from the property_data fileformats, so it is not stored in the .node file.
// The additional files with the <order>.<fileformat> extension contain data which is loaded into the node's property_data member.
// First, the <order> part so that the files can be loaded into the property_data in the correct order (sort of like the linux boot loader does with initrd files).
// If property_data are seen as attributes of the node, then it is idiomatic to have the first content be a list of names, for example, "Lion.0.names".
// The names are used to identify the category of content, such as "triangles", "motivations", etc, which is a more or less an open category name.
// If a simple category name does not seem specific enough, for example if we want to give it a proper name, it should probably be a child node instead.
// We envision, when schemas are necessary to fully identify the type of content, that there would be in addition a content section with a schema name,
// such as name="triangles_schema".  Finally, and somewhat obviously, the <fileformat> is a file format extension, such as "txt", "yaml", "json", etc.,
// which indicates to the framework the format the data takes on disk.
//
// Thus, there are some clear specifications for the parts of the file name:
// <order> is a zero-padded integer, such as "0000", "0001", etc.  It is used to order the files in the property_data vector (and conveniently in file listings).
// <fileformat> is a string that identifies the file format, such as:
// "int64", "uint64", "string", "double", "py", "css", "html", "txt", "yaml", "json", and ought to be from a relatively small set of file formats that the framework supports.


// These are helper functions to derive file path and names from TreeNode objects, producing the Directory path, the .node file name, and the content file names.
// get functions will use the arguments to derive the file paths.  The read functions will at minimum partially read the file system to supply information.
std::string getNodeDirectoryPath(const std::string& base_path, const std::string& label_rule);
std::string getNodeName(const std::string& base_path, const std::string& label_rule);
std::string getNodeParentPath(const std::string& base_path, const std::string& label_rule);
std::string getNodeFileName(const std::string& base_path, const std::string& label_rule);
std::string getContentFileName(const std::string& base_path, const std::string& label_rule, int order, const std::string& literal_type);
std::vector<std::string> getContentFileNames(const std::string& base_path, const std::string& label_rule, const std::vector<std::string>& property_infos);
std::string getLabelRuleFromFileName(const std::string& base_path, const std::string& file_name);
vector<std::string> readContentFileNames(const std::string& base_path, const std::string& label_rule);
vector<TreeNode::PropertyInfo> parsePropertyInfos(vector<std::string> content_file_names);
shared_span<> readContentFiles(vector<std::string> content_file_names);
fplus::maybe<TreeNode> readNodeFile(const std::string& base_path, const std::string& label_rule);

bool createNodeDirectory(const std::string& base_path, const std::string& label_rule);
void writeNodeFile(const std::string& base_path, const TreeNode& node);
void writeContentFiles(vector<std::string> content_types, vector<std::string> content_file_names, const shared_span<>& data);

bool writeNodeToFiles(const std::string& base_path, const TreeNode& node);
fplus::maybe<TreeNode> readNodeFromFiles(const std::string& base_path, const std::string& label_rule);
bool deleteNodeFiles(const std::string& base_path, const std::string& label_rule, bool recursive = false);

class FileBackend : public Backend {
    public:
        FileBackend(std::string basePath);
        ~FileBackend() override;
    
        // Retrieve a node by its label rule.
        fplus::maybe<TreeNode> getNode(const std::string& label_rule) const override;
    
        // Add or update a parent node and its children in the tree.
        bool upsertNode(const std::vector<TreeNode>& nodes) override;
    
        // Delete a node and its children from the tree.
        bool deleteNode(const std::string& label_rule) override;
    
        std::vector<TreeNode> getPageTree(const std::string& page_node_label_rule) const override;
        std::vector<TreeNode> relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const override;
    
        // Query nodes matching a label rule.
        std::vector<TreeNode> queryNodes(const std::string& label_rule) const override;
        std::vector<TreeNode> relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const override;
    
        bool openTransactionLayer(const TreeNode& node) override;
        bool closeTransactionLayers(void) override;
        bool applyTransaction(const Transaction& transaction) override;
    
        // Retrieve the entire tree structure (for debugging or full sync purposes).
        std::vector<TreeNode> getFullTree() const override;
    
        void registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) override;
        void deregisterNodeListener(const std::string listener_name, const std::string label_rule) override;
    
        // Notify listeners for a specific label rule.
        void notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) override;
    
        // processNotifications will check on the inotify file descriptor for any events and process them accordingly, without a dedicated thread.
        void processNotifications() override;

        using DesiredWatch = std::pair<uint32_t, std::string>; // (mask, path)
        using WatchChain = std::list<DesiredWatch>; // List of desired watches (for an ultimate label rule), going up the directory heirarchy to the file path itself.
        using WatchChainSpecifier = std::tuple<std::string, std::string, bool>; // (label_rule, listener_name, child_notify)
        using WatchChainIndex = pair<WatchChainSpecifier, size_t>; // (watch_chain_specifier, index into watch chains vector of watch_chains_)
        using FullWatchSpecifier = std::pair<DesiredWatch, WatchChainIndex>; // (desired_watch, watch_chain_index)

    private:
        std::vector<TreeNode> queryNodesFromPath(const std::string& base_directory, const std::string& label_rule) const;
        bool canPerformSubTransaction(const SubTransaction& sub_transaction) const;
        bool performSubTransaction(const SubTransaction& sub_transaction);

        // Create watch chains will create the creation chain, the delete chain, and the modify watch for a given label rule.
        void setupWatch(WatchChainSpecifier watch_chain_specifier);
        void teardownWatch(WatchChainSpecifier watch_chain_specifier);
        void freeWatch(WatchChainSpecifier watch_chain_specifier);
        void onWatchModifyEvent(int wd);
        void onWatchCreateEvent(int wd, const std::string& notification_path);
        void onWatchDeleteEvent(int wd, const std::string& notification_path);
        void onWatchCloseEvent(int wd); // This is called when a watch is closed, such as when the file is deleted or the directory is removed.

        std::string basePath_;

        int inotify_fd_ = -1; // File descriptor for inotify
        char buffer_[4096]
            __attribute__ ((aligned(__alignof__(struct inotify_event))));        
        // A given label rule can have multiple watchers if it is associated with multiple directories or files.
        std::map<FullWatchSpecifier, int> watchchain_to_wd_; // Maps (desired_watch, watch_chain_index) to the watch descriptor.
        std::map<int, std::set<FullWatchSpecifier> > wd_to_watchchain_; // Maps watch descriptor to (desired_watch, watch_chain_index).
        std::list<int> inotify_notifications; // List of wd that have pending notifications.
        std::map<WatchChainSpecifier, vector<WatchChain> > watch_chains_; // Maps (label_rule, listener_name, child_notify) to a watch chain.

        using ListenerInfo = std::tuple<WatchChainSpecifier, NodeListenerCallback>;
        std::map<std::string, std::list<ListenerInfo> > node_listeners_;
};
