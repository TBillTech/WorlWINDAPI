#include "threadsafe_backend.h"
#include <utility>

fplus::maybe<TreeNode> ThreadsafeBackend::getNode(const std::string& label_rule) const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.getNode(label_rule);
}

bool ThreadsafeBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.upsertNode(nodes);
}

bool ThreadsafeBackend::deleteNode(const std::string& label_rule) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.deleteNode(label_rule);
}

std::vector<TreeNode> ThreadsafeBackend::getPageTree(const std::string& page_node_label_rule) const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.getPageTree(page_node_label_rule);
}

std::vector<TreeNode> ThreadsafeBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.relativeGetPageTree(node, page_node_label_rule);
}

std::vector<TreeNode> ThreadsafeBackend::queryNodes(const std::string& label_rule) const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.queryNodes(label_rule);
}

std::vector<TreeNode> ThreadsafeBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.relativeQueryNodes(node, label_rule);
}

bool ThreadsafeBackend::openTransactionLayer(const TreeNode& node) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.openTransactionLayer(node);
}

bool ThreadsafeBackend::closeTransactionLayers(void) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.closeTransactionLayers();
}

bool ThreadsafeBackend::applyTransaction(const Transaction& transaction) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.applyTransaction(transaction);
}

std::vector<TreeNode> ThreadsafeBackend::getFullTree() const {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    return tree_.getFullTree();
}

void ThreadsafeBackend::registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) {
    std::lock_guard<std::mutex> lock(tree_mutex_);

    // Create a wrapper callback that captures the original callback and this pointer
    NodeListenerCallback wrapped_callback = [this, callback](Backend&, const std::string listener, const fplus::maybe<TreeNode> node) {
        std::lock_guard<std::mutex> notification_lock(notification_mutex_);
        NodeListenerCallbackArgs args = std::make_tuple(listener, node);
        notification_stack_.emplace_back(std::make_pair(std::move(args), callback));
    };

    // Register the wrapped callback with the tree_
    tree_.registerNodeListener(listener_name, label_rule, child_notify, wrapped_callback);
}

void ThreadsafeBackend::deregisterNodeListener(const std::string listener_name, const std::string label_rule) {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    tree_.deregisterNodeListener(listener_name, label_rule);
}

void ThreadsafeBackend::notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
    std::lock_guard<std::mutex> lock(notification_mutex_);
    tree_.notifyListeners(label_rule, node);
}

void ThreadsafeBackend::processNotifications() {
    std::unique_lock<std::mutex> lock(notification_mutex_);
    while (!notification_stack_.empty()) {
        auto [args, callback] = notification_stack_.back();
        notification_stack_.pop_back();
        lock.unlock(); // Unlock the mutex before calling the callback
        callback(*this, std::get<0>(args), std::get<1>(args));
        lock.lock();
    }
    lock.unlock();
}