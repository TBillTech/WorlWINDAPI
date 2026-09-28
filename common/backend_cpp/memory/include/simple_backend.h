#pragma once

#include "backend.h"
#include "memory_tree.h"
#include <map>

bool checkLabelRuleOverlap(const string& label_rule_1, const string& label_rule_2);
bool partialLabelRuleMatch(const std::string& label_rule, const std::string& notification_rule);

// A simple in-memory implementation of the Backend interface.
class SimpleBackend : public Backend {
public:
    SimpleBackend(shared_ptr<MemoryTree> memory_tree) : memory_tree_(memory_tree) {};
    ~SimpleBackend() override = default;

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

    void processNotifications() override {};

private:
    shared_ptr<MemoryTree> memory_tree_;
    using ListenerInfo = std::pair<std::string, std::pair<bool, NodeListenerCallback>>;
    std::map<std::string, std::vector<ListenerInfo> > node_listeners_;
};
