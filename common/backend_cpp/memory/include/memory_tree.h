#pragma once

#include "backend.h"
#include <unordered_map>
#include <mutex>

// A thread-safe in-memory tree structure for storing TreeNode objects.
class MemoryTree {
public:
    MemoryTree() = default;
    ~MemoryTree() = default;

    // Retrieve a node by its label rule.
    fplus::maybe<TreeNode> getNode(const std::string& label_rule) const;

    // Retrieve multiple nodes by their label rules.
    std::vector<TreeNode> getNodes(const std::vector<std::string>& label_rules) const;

    // Add or update a parent node and its children in the tree.
    bool upsertNode(const std::vector<TreeNode>& nodes);

    // Delete a node and its children from the tree.
    bool deleteNode(const std::string& label_rule);

    // Query nodes matching a label rule.
    std::vector<TreeNode> queryNodes(const std::string& label_rule) const;

    // Retrieve the entire tree structure.
    std::vector<TreeNode> getFullTree() const;

    // Try to apply a subtransaction to the tree.
    // This will throw an exception if canPerformSubTransaction() fails.
    bool applyTransaction(const Transaction& transaction);

    // stream operators for reading and writing the tree
    friend std::ostream& operator<<(std::ostream& os, const MemoryTree& tree);
    friend std::istream& operator>>(std::istream& is, MemoryTree& tree);

    bool operator==(const MemoryTree& other) const;

private:
    // Delete a node and its children from the tree without locking.
    bool unsafeDeleteNode(const std::string& label_rule);
    
    // Check if a SubTransaction can be performed without error.
    bool canPerformSubTransaction(const SubTransaction& sub_transaction) const;

    // ASSUMING THAT THE TRANSACTION IS VALID, perform the transaction.
    bool performSubTransaction(const SubTransaction& sub_transaction);

    std::map<std::string, TreeNode> tree_; // In-memory storage for the tree
};
