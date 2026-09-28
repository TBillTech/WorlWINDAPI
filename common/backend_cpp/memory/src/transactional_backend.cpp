#include "memory_tree.h"
#include "simple_backend.h"
#include "transactional_backend.h"
#include <regex>

using namespace std;
using namespace fplus;

fplus::maybe<TreeNode> TransactionalBackend::getNode(const std::string& label_rule) const {
    // If the label_rule overlaps with one of the nodes in the transaction stack,
    // then one of twp conditions should hold:
    // 1. The label_rule matches a parent node in the transaction stack
    // 2. The label_rule matches a child node in the transaction stack
    // So, test each subtransaction in the stack to see if the label_rule overlaps with any of them.
    for (const auto& sub_transaction : transaction_stack_) {
        maybe<string> m_parent_node_label(sub_transaction.first.second.second.lift(&TreeNode::getLabelRule));
        if (m_parent_node_label.lift_def(false, [label_rule](auto parent_label) 
                { return checkLabelRuleOverlap(label_rule, parent_label); })) {
            if (m_parent_node_label.lift_def(false, [label_rule](auto parent_label) { return label_rule == parent_label; })) {
                // If the label_rule matches the parent node in the transaction stack, then return the parent node
                return sub_transaction.first.second.second;
            }
            // Not the parent node, so check each child node in the transaction stack
            for (const auto& child : sub_transaction.second) {
                // It is expected to either match a child exactly, or the label_rule is independent of the transaction so far
                if (child.second.second.lift_def(false, [label_rule](auto child_node) 
                    { return label_rule == child_node.getLabelRule(); })) {
                    // If the label_rule matches a child node in the transaction stack, then return the child node
                    return child.second.second;
                }
            }
        }
    }
    return tree_.getNode(label_rule);
}

bool TransactionalBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    if (transaction_stack_.empty()) {
        // If there is no transaction stack, then just apply the nodes to the tree
        tree_.upsertNode(nodes);
        return true;
    }
    // If there is an ongoing transaction, then all the nodes had better overlap with the transaction stack
    // Moreover, the changes will be logged to the transcation, not directly applied to the tree (yet).
    for (const auto& node : nodes) {
        // Check if the node overlaps with any of the nodes in the transaction stack
        for (auto& sub_transaction : transaction_stack_) {
            auto m_parent_node = sub_transaction.first.second.second;
            if (m_parent_node.lift_def(false, [node](auto parent_node) 
                { return checkLabelRuleOverlap(node.getLabelRule(), parent_node.getLabelRule()); })) {
                // If the node overlaps with a parent node in the transaction stack, then update the transaction
                if (m_parent_node.lift_def(false, [node](auto parent_node) { return node.getLabelRule() == parent_node.getLabelRule(); })) {
                    // If the node matches the parent node in the transaction stack, then update the parent node
                    sub_transaction.first.second.second = node;
                    return true;
                } else {
                    // Otherwise, look for an exact match to the child node
                    for (auto& child : sub_transaction.second) {
                        if (child.second.second.lift_def(false, [node](auto child_node) 
                            { return node.getLabelRule() == child_node.getLabelRule(); })) {
                                // If the node matches a child node in the transaction stack, then update the child node
                            child.second.second = node;
                            return true;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool TransactionalBackend::deleteNode(const std::string& label_rule) {
    if (transaction_stack_.empty()) {
        // If there is no transaction stack, then just apply the delete to the tree
        tree_.deleteNode(label_rule);
        return true;
    }
    // If there is an ongoing transaction, then all the nodes had better overlap with the transaction stack
    // Moreover, the changes will be logged to the transcation, not directly applied to the tree (yet).
    for (auto& sub_transaction : transaction_stack_) {
        const auto& m_parent_node = sub_transaction.first.second.second;
        if (m_parent_node.lift_def(false, [label_rule](auto parent_node) 
            { return checkLabelRuleOverlap(label_rule, parent_node.getLabelRule()); })) {
            // If the label_rule overlaps with a parent node in the transaction stack, then delete something
            // DANGER: deleting things in a transaction is super complex, because tracking the versions of things
            // that evaporate during the transaction is difficult.  So, we take a lazy approach for now, and assume
            // that if the user has the authority to delete, and they really want to, wiping out changes to that
            // which gets deleted is not a problem.  In other words, that deleting can delete the future accidentally.
            // In other, other words, if you want versions checked on deleted children, they need to explicitly be in the
            // transaction stack.
            if (m_parent_node.lift_def(false, [label_rule](auto parent_node) { return label_rule == parent_node.getLabelRule(); })) {
                // If the label_rule matches the parent node in the transaction stack, then delete the parent node
                sub_transaction.first.second.second = maybe<TreeNode>();
                return true;
            } else {
                // Otherwise, look for an exact match to the child node
                for (auto& child : sub_transaction.second) {
                    if (child.second.second.lift_def(false, [label_rule](auto child_node) 
                            { return label_rule == child_node.getLabelRule(); })) {
                        // If the label_rule matches a child node in the transaction stack, then delete the child node
                        child.second.second = maybe<TreeNode>();
                        return true;
                    }
                }
            }
            // Still didn't find it, so create a deletion for a child node in the transaction stack
            // It needs to be added to the sub_transaction.second vector
            // as a new child node with the label_rule.  
            // nullopt for the version is carved out as a special case in the transaction validity check for deletion.
            NewNodeVersion child_node = std::make_pair(maybe<uint16_t>(), std::make_pair(label_rule, maybe<TreeNode>()));
            sub_transaction.second.push_back(child_node);
            return true;
        }
    }
    throw std::runtime_error("Trying to delete a node in a transaction that is not tracked in the transaction stack");
}

std::vector<TreeNode> TransactionalBackend::getPageTree(const std::string& page_node_label_rule) const {
    if (transaction_stack_.empty()) {
        // If there is no transaction stack, then just apply the query to the tree
        return tree_.getPageTree(page_node_label_rule);
    }
    // Otherwise, delegate to getNode
    auto page_node = getNode(page_node_label_rule);
    if (page_node.is_nothing()) {
        throw std::runtime_error("Page node not found: " + page_node_label_rule);
    }
    // Now collect the children label rules
    auto children_label_rules = page_node.lift_def(std::vector<std::string>{}, [](const TreeNode& node) {
        return node.getChildNames();
    });
    // Now get the nodes for the children
    std::vector<TreeNode> child_nodes;
    for (const auto& child_label_rule : children_label_rules) {
        auto child_node = getNode(child_label_rule);
        if (child_node.is_just()) {
            child_nodes.push_back(child_node.unsafe_get_just());
        }
    } 
    return child_nodes;
}

std::vector<TreeNode> TransactionalBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    // concatenate the label rule of the node with the label rule of the page node
    std::string full_label_rule = node.getLabelRule() + "/" + page_node_label_rule;
    // Check if the page node exists
    return getPageTree(full_label_rule);
}

std::vector<TreeNode> TransactionalBackend::queryNodes(const std::string& label_rule) const {
    if (transaction_stack_.empty()) {
        // If there is no transaction stack, then just apply the query to the tree
        return tree_.queryNodes(label_rule);
    }
    // Ideally, the query would use the transaction intelligently and in a sophisticated way.
    // However, that is super complex, since the job of interpreting the query is not the responsibility
    // of this class.  Therefore, something that should still work well is to let the backend resolve the
    // query, and then apply the transaction to the found nodes instead.
    auto nodes = tree_.queryNodes(label_rule);
    vector<TreeNode> results;
    for (const auto& node : nodes) {
        bool foundit = false;
        // Check if the node overlaps with any of the nodes in the transaction stack
        for (auto& sub_transaction : transaction_stack_) {
            const auto& m_parent_node = sub_transaction.first.second.second;
            if (m_parent_node.lift_def(false, [label_rule](auto parent_node) 
                    { return checkLabelRuleOverlap(label_rule, parent_node.getLabelRule()); })) {
                // If the node overlaps with a parent node in the transaction stack, push that onto the results instead
                if (m_parent_node.lift_def(false, [label_rule](auto parent_node) { return label_rule == parent_node.getLabelRule(); })) {
                    // If the node matches the parent node in the transaction stack, then return that instead
                    results.push_back(sub_transaction.first.second.second.unsafe_get_just());
                    foundit = true;
                    break;
                } else {
                    // Otherwise, look for an exact match to the child node
                    for (auto& child : sub_transaction.second) {
                        if (child.second.second.lift_def(false, [label_rule](auto child_node) 
                            { return label_rule == child_node.getLabelRule(); })) {
                        // If the node matches a child node in the transaction stack, then update the child node
                            results.push_back(child.second.second.unsafe_get_just());
                            foundit = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!foundit) {
            // If the node was not found in the transaction stack, then add it to the results
            results.push_back(node);
        }
    }
    return results;
}

std::vector<TreeNode> TransactionalBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    // concatenate the label rule of the node with the label rule of the query
    std::string full_label_rule = node.getLabelRule() + "/" + label_rule;
    return queryNodes(full_label_rule);
}

bool TransactionalBackend::applyTransaction(const Transaction& transaction) {
    return tree_.applyTransaction(transaction);
}

std::vector<TreeNode> TransactionalBackend::getFullTree() const {
    if (transaction_stack_.empty()) {
        // If there is no transaction stack, then just apply the query to the tree
        return tree_.getFullTree();
    }
    throw std::runtime_error("getFullTree() is not supported in transaction mode");
}


bool TransactionalBackend::openTransactionLayer(const TreeNode& node) {
    // throw an exception if this node has a label rule overlap with any other node in the transaction stack
    for (const auto& sub_transaction : transaction_stack_) {
        const auto& m_parent_node = sub_transaction.first.second.second;
        if (m_parent_node.lift_def(false, [node](auto parent_node) 
                { return checkLabelRuleOverlap(node.getLabelRule(), parent_node.getLabelRule()); })) {
            throw std::runtime_error("Label rule overlap detected in transaction stack");
        }
    }
    // Add the new transaction to the stack
    // The initial set of descendants is empty, because the "user" will continue to add changes to the transaction
    // until the transaction is closed.
    // The old version of the node should be read from the tree, and the new version should be set to the next version
    // number of the parent node.
    auto m_parent_node = tree_.getNode(node.getLabelRule());
    if (m_parent_node.is_nothing()) {
        // If the parent node does not exist, then the last version is nullopt
        NewNodeVersion parent_newnode_version = std::make_pair(maybe<uint16_t>(), std::make_pair(node.getLabelRule(), maybe(node)));
        transaction_stack_.emplace_back(std::make_pair(parent_newnode_version, std::vector<NewNodeVersion>()));
    } else {
        // If the parent node exists, then use its version number as the last version
        auto version = m_parent_node.unsafe_get_just().getVersion();
        uint16_t next_version = (version.version_number + 1) % (version.max_version_sequence);
        NewNodeVersion parent_newnode_version = std::make_pair(maybe(next_version), std::make_pair(node.getLabelRule(), maybe(node)));
        transaction_stack_.emplace_back(std::make_pair(parent_newnode_version, std::vector<NewNodeVersion>()));
    }
    return true;
}

bool TransactionalBackend::closeTransactionLayers(void) {
    // If the transaction stack is empty, then there is nothing to do
    if (transaction_stack_.empty()) {
        return true;
    }
    // Otherwise, delegate applying the transaction to the memory tree
    tree_.applyTransaction(transaction_stack_);
    // Clear the transaction stack
    transaction_stack_.clear();
    return true;
}

void TransactionalBackend::registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) {
    auto wrapped_callback = [this, callback](Backend& /*backend*/, const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
        callback(*this, label_rule, node);
    };
    tree_.registerNodeListener(listener_name, label_rule, child_notify, wrapped_callback);
}

void TransactionalBackend::deregisterNodeListener(const std::string listener_name, const std::string label_rule) {
    tree_.deregisterNodeListener(listener_name, label_rule);
}

void TransactionalBackend::notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
    tree_.notifyListeners(label_rule, node);
}