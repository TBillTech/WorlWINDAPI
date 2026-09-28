#include "cloning_mediator.h"
#include "tree_node.h"

void mediatorCallback(Backend& to, bool versioned, atomic<bool>& setProcessing, atomic<bool>& isProcessing, 
        const std::string& label_rule, const fplus::maybe<TreeNode>& m_from) {
    if (isProcessing.load()) {
        return; // Ignore if already processing
    }
    setProcessing.store(true);
    auto m_to = to.getNode(label_rule);
    if (m_from == m_to) {
        // If the node is already the same in both backends, do nothing
        setProcessing.store(false);
        return;
    }
    if (m_from.is_nothing() && m_to.is_just()) {
        // If the node does not exist in 'from', delete it from 'to'
        to.deleteNode(label_rule);
    } else {
        auto from = m_from.unsafe_get_just();
        if (versioned)
        {  
            if (m_to.lift_def(true, [&from](auto to) { return from.getVersion() >= to.getVersion(); })) {
                to.upsertNode({from});
            }
        }
        else {
            // If the node exists and is valid, upsert it to 'to'
            to.upsertNode({from});
        }
    }
    setProcessing.store(false);
}

CloningMediator::CloningMediator(const std::string& name, Backend& a, Backend& b, bool versioned)
    : name_(name), backendA_(a), backendB_(b), versioned_(versioned)
{
    processingA_.store(false);
    processingB_.store(false);
    string root_node_label_rule = ""; // Assuming the root node is empty or defined elsewhere
    // Create a callback for backend A to listen for changes
    backendA_.registerNodeListener("CloningMediatorA", root_node_label_rule, true, [this](Backend&, const std::string& label_rule, const fplus::maybe<TreeNode>& m_nodeA) {
        mediatorCallback(backendB_, versioned_, processingA_, processingB_, label_rule, m_nodeA);
    });

    // Create a callback for backend B to listen for changes
    backendB_.registerNodeListener("CloningMediatorB", root_node_label_rule, true, [this](Backend&, const std::string& label_rule, const fplus::maybe<TreeNode>& m_nodeB) {
        mediatorCallback(backendA_, versioned_, processingB_, processingA_, label_rule, m_nodeB);
    });
}

CloningMediator::~CloningMediator() 
{
    string root_node_label_rule = ""; // Assuming the root node is empty or defined elsewhere
    backendA_.deregisterNodeListener("CloningMediatorA", root_node_label_rule);
    backendB_.deregisterNodeListener("CloningMediatorB", root_node_label_rule);
}