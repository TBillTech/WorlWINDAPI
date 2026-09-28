#pragma once

#include <map>

#include "backend.h"

class CompositeBackend : public Backend {
    public:
        CompositeBackend(Backend & root) : root_backend_(root) {};
        ~CompositeBackend() override = default;
    
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
    
        void processNotifications() override;

        // There is a function unique to the CompositeBackend that allows you to mount a backend to a specific label rule.
        std::pair<std::map<std::string, Backend &>::iterator, bool> mountBackend(const std::string& label_rule, Backend& backend);
    
    private:
        pair<int, Backend&> getRelevantBackend(const std::string& label_rule) const;

        Backend &root_backend_;
        std::map<std::string, Backend& > mounted_backends_;
};
