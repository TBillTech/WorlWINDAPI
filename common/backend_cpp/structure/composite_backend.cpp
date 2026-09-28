#include "composite_backend.h"
#include "simple_backend.h"

fplus::maybe<TreeNode> CompositeBackend::getNode(const std::string& label_rule) const {
    auto index_backend = getRelevantBackend(label_rule);
    fplus::maybe<TreeNode> result = index_backend.second.getNode(label_rule.substr(index_backend.first));
    if (result.is_nothing() || index_backend.first == 0) {
        return result;
    } else {
        result.unsafe_get_just().prefixLabels(label_rule.substr(0, index_backend.first));
    }
    return result;
}

bool CompositeBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    // Since we have to duplicate the nodes to modify the relative label rules, do them one by one.
    for (const auto& node : nodes) {
        auto index_backend = getRelevantBackend(node.getLabelRule());
        auto new_node = node;
        new_node.shortenLabels(node.getLabelRule().substr(0, index_backend.first));
        index_backend.second.upsertNode({new_node});
    }
    return true;
}

bool CompositeBackend::deleteNode(const std::string& label_rule) {
    auto index_backend = getRelevantBackend(label_rule);
    return index_backend.second.deleteNode(label_rule.substr(index_backend.first));
}

std::vector<TreeNode> CompositeBackend::getPageTree(const std::string& page_node_label_rule) const {
    auto index_backend = getRelevantBackend(page_node_label_rule);
    auto result = index_backend.second.getPageTree(page_node_label_rule.substr(index_backend.first));
    if (index_backend.first == 0) {
        return result;
    }
    // We need to modify the label rules of the nodes in the result to match the label rule of the backend.
    for (auto& node : result) {
        node.prefixLabels(page_node_label_rule.substr(0, index_backend.first));
    } 
    return result;
}

std::vector<TreeNode> CompositeBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    auto absolute_label_rule = node.getLabelRule() + '/' + page_node_label_rule;
    return getPageTree(page_node_label_rule);
}

std::vector<TreeNode> CompositeBackend::queryNodes(const std::string& label_rule) const {
    auto index_backend = getRelevantBackend(label_rule);
    auto result = index_backend.second.queryNodes(label_rule.substr(index_backend.first));
    if (index_backend.first == 0) {
        return result;
    }
    for (auto& node : result) {
        node.prefixLabels(label_rule.substr(0, index_backend.first));
    }
    return result;
}

std::vector<TreeNode> CompositeBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    auto absolute_label_rule = node.getLabelRule() + '/' + label_rule;
    return queryNodes(absolute_label_rule);
}

bool CompositeBackend::openTransactionLayer(const TreeNode& node) {
    auto index_backend = getRelevantBackend(node.getLabelRule());
    auto new_node = node;
    new_node.shortenLabels(node.getLabelRule().substr(0, index_backend.first));
    return index_backend.second.openTransactionLayer(new_node);
}

bool CompositeBackend::closeTransactionLayers(void) {
    // Close all transaction layers in the mounted backends
    for (const auto& [label_rule, backend] : mounted_backends_) {
        backend.closeTransactionLayers();
    }
    return root_backend_.closeTransactionLayers();
}

bool CompositeBackend::applyTransaction(const Transaction& transaction) {
    // First check if all the SubTransactions are for the same backend.
    // If they are not, throw an exception.
    auto index_backend = getRelevantBackend(transaction.begin()->first.second.first);
    auto prefix = transaction.begin()->first.second.first.substr(0, index_backend.first);
    Transaction mounted_transaction;
    for (auto sub_transaction : transaction) {
        auto sub_index_backend = getRelevantBackend(sub_transaction.first.second.first);
        auto sub_prefix = sub_transaction.first.second.first.substr(0, sub_index_backend.first);
        if (sub_prefix != prefix) {
            throw std::runtime_error("Transaction contains SubTransactions for different backends");
        }
        auto sub_transaction_copy = sub_transaction;
        shortenSubTransactionLabels(sub_prefix, sub_transaction_copy);
        mounted_transaction.push_back(sub_transaction_copy);
    }

    auto result = index_backend.second.applyTransaction(mounted_transaction);
    return result;
}

std::vector<TreeNode> CompositeBackend::getFullTree() const {
    std::vector<TreeNode> full_tree = root_backend_.getFullTree();
    for (const auto& [label_rule, backend] : mounted_backends_) {
        auto relative_tree = backend.getFullTree();
        auto label_rule_slash = label_rule + '/';
        for (auto& node : relative_tree) {
            node.prefixLabels(label_rule_slash);
        }
        full_tree.insert(full_tree.end(), relative_tree.begin(), relative_tree.end());
    }
    return full_tree;
}

void CompositeBackend::registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) {
    auto index_backend = getRelevantBackend(label_rule);
    auto relative_label_rule = label_rule.substr(index_backend.first);
    auto relative_callback = [this, index_backend, callback, label_rule](Backend&, const std::string& relative_label_rule, const fplus::maybe<TreeNode>& node) {
        auto composite_label_rule = label_rule.substr(0, index_backend.first) + relative_label_rule;
        if (node.is_just()) {
            auto node_copy = node.unsafe_get_just();
            node_copy.prefixLabels(label_rule.substr(0, index_backend.first));
            callback(*this, composite_label_rule, node_copy);
        } else {
            callback(*this, composite_label_rule, node);
        }
    };
    index_backend.second.registerNodeListener(listener_name, relative_label_rule, child_notify, relative_callback);
}

void CompositeBackend::deregisterNodeListener(const std::string listener_name, const std::string label_rule) {
    auto index_backend = getRelevantBackend(label_rule);
    auto relative_label_rule = label_rule.substr(index_backend.first);
    index_backend.second.deregisterNodeListener(listener_name, relative_label_rule);
}

void CompositeBackend::notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
    auto index_backend = getRelevantBackend(label_rule);
    auto relative_label_rule = label_rule.substr(index_backend.first);
    if (node.is_nothing()) {
        index_backend.second.notifyListeners(relative_label_rule, node);
        return;
    }
    auto node_copy = node.unsafe_get_just();
    node_copy.shortenLabels(node_copy.getLabelRule().substr(0, index_backend.first));
    index_backend.second.notifyListeners(relative_label_rule, node_copy);
}

void CompositeBackend::processNotifications() {
    root_backend_.processNotifications();
    for (const auto& [label_rule, backend] : mounted_backends_) {
        backend.processNotifications();
    }
}

std::pair<std::map<std::string, Backend &>::iterator, bool> CompositeBackend::mountBackend(const std::string& label_rule, Backend& backend) {
    if (mounted_backends_.find(label_rule) != mounted_backends_.end()) {
        throw std::runtime_error("Label rule already exists in the mounted backends");
    }
    if (root_backend_.getNode(label_rule).is_just()) {
        throw std::runtime_error("Label rule already exists in the root backend");
    }
    // Check for overlapping label rules
    for (const auto& [mounted_label_rule, mounted_backend] : mounted_backends_) {
        if (checkLabelRuleOverlap(label_rule, mounted_label_rule)) {
            throw std::runtime_error("Label rule overlaps with an existing mounted backend");
        }
    }
    return mounted_backends_.insert(std::pair<std::string, Backend&>(label_rule, backend));
}

pair<int, Backend&> CompositeBackend::getRelevantBackend(const std::string& label_rule) const {
    // Assume that no two backends can have overlapping label rules.
    // Therefore it is safe to assume that the label_rule can only overlap one or none of the mounted backends.
    // This also means that the relevant backend is one that matches the label_rule up to the size of the mounted_label_rule.
    // Furthermore, since the map is sorted by string, we can look for the upper_bound of the label_rule in the map, and 
    // then decrement once to find the _only_ possible relevant backend.
    auto it = mounted_backends_.upper_bound(label_rule);
    if (it == mounted_backends_.begin()) {
        return {0, root_backend_};
    }
    --it;
    if (label_rule.compare(0, it->first.size(), it->first) == 0) {
        // So far so good, but make sure that the label_rule either has no more characters or a '/' at the it->first.size() position.
        // For example, if the mounted_backend is "zoo", but the label_rule is "zoo1", then we don't want to return the mounted_backend.
        if (label_rule.size() == it->first.size()) {
            return {it->first.size(), it->second};
        }
        if (label_rule[it->first.size()] == '/') {
            return {it->first.size()+1, it->second};  // +1 to skip the '/' character
        }
    }
    return {0, root_backend_};
}
