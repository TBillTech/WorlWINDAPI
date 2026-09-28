#include "memory_tree.h"
#include "simple_backend.h"
#include <regex>

using namespace std;
using namespace fplus;

// This function will check two label rules to determine if they overlap.
// label rules overlap if the label rule of one node is a prefix of the other.
// For example, "A" and "A/B" overlap, but "A/C" and "A/CB" do not.
bool checkLabelRuleOverlap(const string& label_rule_1, const string& label_rule_2) {
    auto stripQuery = [](const std::string& label_rule) {
        size_t query_pos = label_rule.find('?');
        return query_pos == std::string::npos ? label_rule : label_rule.substr(0, query_pos);
    };

    std::string stripped_label1 = stripQuery(label_rule_1);
    std::string stripped_label2 = stripQuery(label_rule_2);

    if (stripped_label1.empty() || stripped_label2.empty()) {
        return true; // By definition, empty label rules overlap with everything
    }
    if (stripped_label1 == stripped_label2) {
        return true;
    }
    

    auto last_slash1 = stripped_label1.find_last_of('/');
    auto last_slash2 = stripped_label2.find_last_of('/');
    auto min_last_slash = std::min(last_slash1, last_slash2);
    // Now, if the min_last_slash is not a slash in both, then this proves that, at minimum, the
    // last label of the shorter rule has diverged from the longer rule.
    if (stripped_label1[min_last_slash] != '/' || stripped_label2[min_last_slash] != '/') {
        return false;
    }
    // On the other hand, if the strings leading up to the min_last_slash are NOT the same, then they do not overlap
    // at all, so we can return false.
    if (stripped_label1.substr(0, min_last_slash) != stripped_label2.substr(0, min_last_slash)) {
        return false;
    }
    // AT this point, it is proved that up to and including min_last_slash the rules are identical, 
    // so we consider the extensions past that point:
    auto extension1 = stripped_label1.substr(min_last_slash + 1);
    auto extension2 = stripped_label2.substr(min_last_slash + 1);
    // Now, extension1 and extension2 have the properties that the shortest label rule extension is a single
    // string not including any slash, and the other extension might have more slashes.
    // The condition where the rules overlap is also the condition that the shorter extension matches 
    // exactly with the longer extension up to that point, AND that the longer extension has a slash _after_ that.
    auto slash_pos_1 = extension1.find('/');
    auto slash_pos_2 = extension2.find('/');
    if (slash_pos_1 == std::string::npos && slash_pos_2 == std::string::npos) {
        // Since they are provably not the same at this line of code, they cannot overlap if neither has a slash at all
        return false; // No overlap
    }
    if (slash_pos_1 == std::string::npos) {
        // extension2 is longer, and it overlaps if and only if it has a slash just past the shorter extension
        return (slash_pos_2 == extension1.length());
    }
    if (slash_pos_2 == std::string::npos) {
        // extension1 is longer, and it overlaps if and only if it has a slash just past the shorter extension
        return (slash_pos_1 == extension2.length());
    }

    return false;
}

maybe<TreeNode> SimpleBackend::getNode(const std::string& label_rule) const {
    return memory_tree_->getNode(label_rule);
}

bool SimpleBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    memory_tree_->upsertNode(nodes);
    // Notify listeners for each node
    for (const auto& node : nodes) {
        auto label_rule = node.getLabelRule();
        notifyListeners(label_rule, node);
    }
    return true;
}

bool SimpleBackend::deleteNode(const std::string& label_rule) {
    memory_tree_->deleteNode(label_rule);
    // Notify listeners for each node
    notifyListeners(label_rule, maybe<TreeNode>());
    return true;
}

std::vector<TreeNode> SimpleBackend::getPageTree(const std::string& page_node_label_rule) const {
    // Check if the page node exists
    auto page_node = memory_tree_->getNode(page_node_label_rule);
    if (page_node.is_nothing()) {
        return {};
    }
    // Now collect the children label rules
    auto children_label_rules = page_node.lift_def(std::vector<std::string>{}, [](const TreeNode& node) {
        return node.getChildNames();
    });
    return memory_tree_->getNodes(children_label_rules);
}

std::vector<TreeNode> SimpleBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    // concatenate the label rule of the node with the label rule of the page node
    std::string full_label_rule = node.getLabelRule() + "/" + page_node_label_rule;
    // Check if the page node exists
    auto page_node = memory_tree_->getNode(full_label_rule);
    if (page_node.is_nothing()) {
        throw std::runtime_error("Page node not found: " + full_label_rule);
    }
    return getPageTree(full_label_rule);
}

std::vector<TreeNode> SimpleBackend::queryNodes(const std::string& label_rule) const {
    return memory_tree_->queryNodes(label_rule);
}

std::vector<TreeNode> SimpleBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    // concatenate the label rule of the node with the label rule of the query
    std::string full_label_rule = node.getLabelRule() + "/" + label_rule;
    return queryNodes(full_label_rule);
}

bool SimpleBackend::openTransactionLayer(const TreeNode&) {
    // throw an exception: SimpleBackend does not support opening Transaction layers
    throw std::runtime_error("SimpleBackend does not support opening transaction layers");
    return false;
}

bool SimpleBackend::closeTransactionLayers(void) {
    // throw an exception: SimpleBackend does not support closing Transaction layers
    throw std::runtime_error("SimpleBackend does not support closing transaction layers");
    return false;
}

bool SimpleBackend::applyTransaction(const Transaction& transaction) {
    auto success = memory_tree_->applyTransaction(transaction);
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

std::vector<TreeNode> SimpleBackend::getFullTree() const {
    return memory_tree_->getFullTree();
}

void SimpleBackend::registerNodeListener(const std::string listener_name, const std::string notification_rule, bool child_notify, NodeListenerCallback callback) {
    deregisterNodeListener(listener_name, notification_rule);
    auto findit = node_listeners_.find(notification_rule);
    ListenerInfo a_listener = {listener_name, {child_notify, callback}};
    if (findit != node_listeners_.end()) {
        // If the label_rule already exists, add the listener to the existing list
        auto& listeners = findit->second;
        listeners.push_back(a_listener);
    } else {
        // If the label_rule does not exist, create a new entry
        node_listeners_.emplace(notification_rule, std::vector<ListenerInfo>{a_listener});
    }
}

void SimpleBackend::deregisterNodeListener(const std::string listener_name, const std::string notification_rule) {
    // Deregister a listener for a specific label rule
    auto found = node_listeners_.find(notification_rule);
    if (found != node_listeners_.end()) {
        auto& listeners = found->second;
        // Remove the listener from the list
        auto it = std::remove_if(listeners.begin(), listeners.end(), [&](const ListenerInfo& listener) {
            return listener.first == listener_name;
        });
        if (it != listeners.end()) {
            listeners.erase(it, listeners.end());
        }
        // If the list of listeners is empty, remove the label rule from the map
        if (listeners.empty()) {
            node_listeners_.erase(found);
        }
    }
}

bool partialLabelRuleMatch(const std::string& label_rule, const std::string& notification_rule) {
    // Strip the query path from the label_rule
    auto stripQuery = [](const std::string& label_rule) {
        size_t query_pos = label_rule.find('?');
        return query_pos == std::string::npos ? label_rule : label_rule.substr(0, query_pos);
    };
    std::string stripped_label_rule = stripQuery(label_rule);
    // If the rules exactly match, return true:
    if (stripped_label_rule == notification_rule) {
        return true;
    }
    if (notification_rule.size() > stripped_label_rule.size()) {
        return false;
    }
    // If the notification_rule is a prefix of the label_rule, return true:
    return stripped_label_rule.find(notification_rule) == 0;
}

void SimpleBackend::notifyListeners(const std::string& label_rule, const maybe<TreeNode>& node) {
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
    // found->first will NEVER be longer than the label_rule, because upper_bound returns the first element that is greater than label_rule lexicographically
    // then decremented once.
    while (partialLabelRuleMatch(label_rule, found->first)) {
        auto listeners = found->second;
        // Notify all listeners for this label_rule
        for (auto listener : listeners) {
            if (found->first == label_rule) {
                // The parent == label and callback is unconditionally met
                listener.second.second(*this, label_rule, node);
            } else if (listener.second.first && checkLabelRuleOverlap(label_rule, found->first)) {
                // The label_rule contains the listener's label_rule, and the callback matches children
                listener.second.second(*this, label_rule, node);
            } // Otherwise, the label_rule contains the listener's label_rule, but the callback does not match children
        }
        if (found != node_listeners_.begin()) {
            --found; // Move to the last element that is less than or equal to label_rule
        } else {
            break; // No more elements to check
        }
    }
}