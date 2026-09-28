#include "redirected_backend.h"
#include "simple_backend.h"

fplus::maybe<TreeNode> RedirectedBackend::getNode(const std::string& label_rule) const {
    fplus::maybe<TreeNode> result = root_backend_.getNode(prefix_ + label_rule);
    if (result.is_nothing()) {
        return result;
    } else {
        result.unsafe_get_just().shortenLabels(prefix_);
    }
    return result;
}

bool RedirectedBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    // Since we have to duplicate the nodes to modify the relative label rules, do them one by one.
    for (const auto& node : nodes) {
        auto new_node = node;
        new_node.prefixLabels(prefix_);
        root_backend_.upsertNode({new_node});
    }
    return true;
}

bool RedirectedBackend::deleteNode(const std::string& label_rule) {
    return root_backend_.deleteNode(prefix_ + label_rule);
}

std::vector<TreeNode> RedirectedBackend::getPageTree(const std::string& page_node_label_rule) const {
    auto result = root_backend_.getPageTree(prefix_ + page_node_label_rule);
    for (auto& node : result) {
        node.shortenLabels(prefix_);
    } 
    return result;
}

std::vector<TreeNode> RedirectedBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    auto absolute_label_rule = node.getLabelRule() + '/' + page_node_label_rule;
    return getPageTree(page_node_label_rule);
}

std::vector<TreeNode> RedirectedBackend::queryNodes(const std::string& label_rule) const {
    auto result = root_backend_.queryNodes(prefix_ + label_rule);
    for (auto& node : result) {
        node.shortenLabels(prefix_);
    }
    return result;
}

std::vector<TreeNode> RedirectedBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    auto absolute_label_rule = node.getLabelRule() + '/' + label_rule;
    return queryNodes(absolute_label_rule);
}

bool RedirectedBackend::openTransactionLayer(const TreeNode& node) {
    auto new_node = node;
    new_node.prefixLabels(prefix_);
    return root_backend_.openTransactionLayer(new_node);
}

bool RedirectedBackend::closeTransactionLayers(void) {
    return root_backend_.closeTransactionLayers();
}

bool RedirectedBackend::applyTransaction(const Transaction& transaction) {
    Transaction mounted_transaction;
    for (auto sub_transaction : transaction) {
        auto sub_transaction_copy = sub_transaction;
        prefixSubTransactionLabels(prefix_, sub_transaction_copy);
        mounted_transaction.push_back(sub_transaction_copy);
    }

    auto result = root_backend_.applyTransaction(mounted_transaction);
    return result;
}

std::vector<TreeNode> RedirectedBackend::getFullTree() const {
    std::vector<TreeNode> full_tree = root_backend_.getFullTree();
    // Filter out all the nodes that do not start with the prefix
    full_tree.erase(std::remove_if(full_tree.begin(), full_tree.end(), [this](const TreeNode& node) {
        return node.getLabelRule().find(prefix_) != 0;
    }), full_tree.end());
    for (auto& node : full_tree) {
        node.shortenLabels(prefix_);
    }
    return full_tree;
}

void RedirectedBackend::registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) {
    auto relative_label_rule = prefix_ + label_rule;
    auto relative_callback = [this, callback](Backend&, const std::string& relative_label_rule, const fplus::maybe<TreeNode>& node) {
        auto redirected_label_rule = relative_label_rule.substr(prefix_.size());
        if (node.is_just()) {
            auto node_copy = node.unsafe_get_just();
            node_copy.shortenLabels(prefix_);
            callback(*this, redirected_label_rule, node_copy);
        } else {
            callback(*this, redirected_label_rule, node);
        }
    };
    root_backend_.registerNodeListener(listener_name, relative_label_rule, child_notify, relative_callback);
}

void RedirectedBackend::deregisterNodeListener(const std::string listener_name, const std::string label_rule) {
    auto relative_label_rule = prefix_ + label_rule;
    root_backend_.deregisterNodeListener(listener_name, relative_label_rule);
}

void RedirectedBackend::notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
    auto relative_label_rule = prefix_ + label_rule;
    if (node.is_nothing()) {
        root_backend_.notifyListeners(relative_label_rule, node);
        return;
    }
    auto node_copy = node.unsafe_get_just();
    node_copy.prefixLabels(prefix_);
    root_backend_.notifyListeners(relative_label_rule, node_copy);
}

void RedirectedBackend::processNotifications() {
    root_backend_.processNotifications();
}


