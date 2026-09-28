#include <vector>

#include "http3_tree_message.h"

void HTTP3TreeMessage::encode_getNodeRequest(const std::string& label_rule) {
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_NODE_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, label_rule);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

std::string HTTP3TreeMessage::decode_getNodeRequest(){
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_getNodeResponse(const fplus::maybe<TreeNode>& node) {
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_NODE_RESPONSE;
    chunkList encoded = encode_MaybeTreeNode(request_id_, signal_, node);
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

fplus::maybe<TreeNode> HTTP3TreeMessage::decode_getNodeResponse() {
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_MaybeTreeNode(responseChunks);
    processingFinished = true;
    return decoded.second;
}

void HTTP3TreeMessage::encode_upsertNodeRequest(const std::vector<TreeNode>& nodes)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_REQUEST;
    chunkList encoded = encode_VectorTreeNode(request_id_, signal_, nodes);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

std::vector<TreeNode> HTTP3TreeMessage::decode_upsertNodeRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_VectorTreeNode(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_upsertNodeResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_upsertNodeResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_deleteNodeRequest(const std::string& label_rule)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, label_rule);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

std::string HTTP3TreeMessage::decode_deleteNodeRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_deleteNodeResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_deleteNodeResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_getPageTreeRequest(const std::string& page_node_label_rule)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, page_node_label_rule);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

std::string HTTP3TreeMessage::decode_getPageTreeRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_getPageTreeResponse(const std::vector<TreeNode>& nodes)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_RESPONSE;
    chunkList encoded = encode_VectorTreeNode(request_id_, signal_, nodes);
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

std::vector<TreeNode> HTTP3TreeMessage::decode_getPageTreeResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_VectorTreeNode(responseChunks);
    processingFinished = true;
    return decoded.second;
}

void HTTP3TreeMessage::encode_getQueryNodesRequest(const std::string& label_rule)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, label_rule);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

std::string HTTP3TreeMessage::decode_getQueryNodesRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_getQueryNodesResponse(const std::vector<TreeNode>& nodes)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_RESPONSE;
    chunkList encoded = encode_VectorTreeNode(request_id_, signal_, nodes);
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

std::vector<TreeNode> HTTP3TreeMessage::decode_getQueryNodesResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_VectorTreeNode(responseChunks);
    processingFinished = true;
    return decoded.second;
}

void HTTP3TreeMessage::encode_openTransactionLayerRequest(const TreeNode& node)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_REQUEST;
    chunkList encoded = encode_MaybeTreeNode(request_id_, signal_, fplus::maybe<TreeNode>(node));
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

TreeNode HTTP3TreeMessage::decode_openTransactionLayerRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_MaybeTreeNode(requestChunks);
    if (decoded.second.is_nothing()) {
        throw invalid_argument("TreeNode is nothing");
    }
    return decoded.second.unsafe_get_just();
}

void HTTP3TreeMessage::encode_openTransactionLayerResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_openTransactionLayerResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_closeTransactionLayersRequest()
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, "");
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

void HTTP3TreeMessage::decode_closeTransactionLayersRequest()
{
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    // No data to decode, just mark the request as complete
    requestComplete = true;
}

void HTTP3TreeMessage::encode_closeTransactionLayersResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_closeTransactionLayersResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_applyTransactionRequest(const Transaction& transaction)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_REQUEST;
    chunkList encoded = encode_Transaction(request_id_, signal_, transaction);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

Transaction HTTP3TreeMessage::decode_applyTransactionRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_Transaction(requestChunks);
    return decoded.second;
}

void HTTP3TreeMessage::encode_applyTransactionResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_applyTransactionResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_getFullTreeRequest()
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, "");
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

void HTTP3TreeMessage::decode_getFullTreeRequest()
{
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    // No data to decode, just mark the request as complete
    requestComplete = true;
}

void HTTP3TreeMessage::encode_getFullTreeResponse(const std::vector<TreeNode>& nodes)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_RESPONSE;
    chunkList encoded = encode_VectorTreeNode(request_id_, signal_, nodes);
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

std::vector<TreeNode> HTTP3TreeMessage::decode_getFullTreeResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_VectorTreeNode(responseChunks);
    processingFinished = true;
    return decoded.second;
}

void HTTP3TreeMessage::encode_registerNodeListenerRequest(const std::string& listener_name, const std::string& label_rule, bool child_notify)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, listener_name + " " + label_rule + " " + to_string(child_notify));
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

tuple<std::string, std::string, bool> HTTP3TreeMessage::decode_registerNodeListenerRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    stringstream iss(decoded.second);
    std::string listener_name;
    std::string label_rule;
    bool child_notify;
    // Check if the listener name is empty by checking if peek() is whitespace
    if(!isspace(iss.peek()) ) {
        iss >> listener_name;
    }
    iss.get(); // Consume the whitespace
    if (!isspace(iss.peek()) ) {
        iss >> label_rule;
    }
    iss.get(); // Consume the whitespace
    iss >> child_notify;
    return {listener_name, label_rule, child_notify};
}

void HTTP3TreeMessage::encode_registerNodeListenerResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_registerNodeListenerResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_deregisterNodeListenerRequest(const std::string& listener_name, const std::string& label_rule)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, listener_name + " " + label_rule);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

pair<std::string, std::string> HTTP3TreeMessage::decode_deregisterNodeListenerRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    stringstream iss(decoded.second);
    std::string listener_name;
    std::string label_rule;
    if (!isspace(iss.peek())) {
        iss >> listener_name;
    }
    iss.get(); // Consume the whitespace
    iss >> label_rule;
    return {listener_name, label_rule};
}

void HTTP3TreeMessage::encode_deregisterNodeListenerResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_deregisterNodeListenerResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_notifyListenersRequest(const std::string& label_rule, const fplus::maybe<TreeNode>& node)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, label_rule);
    auto node_chunk = encode_MaybeTreeNode(request_id_, signal_, node);
    encoded.insert(encoded.end(), std::make_move_iterator(node_chunk.begin()), std::make_move_iterator(node_chunk.end()));
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

pair<std::string, fplus::maybe<TreeNode> > HTTP3TreeMessage::decode_notifyListenersRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto decoded = decode_label(requestChunks);
    stringstream iss(decoded.second);
    std::string label_rule;
    iss >> label_rule;
    auto node = decode_MaybeTreeNode(chunkList(std::next(requestChunks.begin(), decoded.first), requestChunks.end()));
    if (node.first == 0) {
        throw invalid_argument("Cannot decode TreeNode");
    }
    return {label_rule, node.second};
}

void HTTP3TreeMessage::encode_notifyListenersResponse(bool success)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, to_string(success));
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

bool HTTP3TreeMessage::decode_notifyListenersResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_label(responseChunks);
    stringstream iss(decoded.second);
    bool success;
    iss >> success;
    processingFinished = true;
    return success;
}

void HTTP3TreeMessage::encode_processNotificationRequest()
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_REQUEST;
    chunkList encoded = encode_label(request_id_, signal_, "");
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
}

void HTTP3TreeMessage::decode_processNotificationRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestComplete = true;
}

void HTTP3TreeMessage::encode_processNotificationResponse()
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_RESPONSE;
    chunkList encoded = encode_label(request_id_, signal_, "");
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
}

void HTTP3TreeMessage::decode_processNotificationResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    processingFinished = true;
}

void HTTP3TreeMessage::encode_getJournalRequest(SequentialNotification const& last_notification)
{
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_REQUEST;
    // Just send the uint64_t sequence number in the request
    chunkList encoded = encode_SequentialNotification(request_id_, signal_, 
        {last_notification.first, {"", fplus::maybe<TreeNode>()}});
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.insert(requestChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    isInitialized_ = true;
    isJournalRequest_ = true;
}

SequentialNotification HTTP3TreeMessage::decode_getJournalRequest()
{
    if (!requestComplete) {
        throw invalid_argument("HTTP3TreeMessage has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    auto notification = decode_SequentialNotification(requestChunks);
    if (notification.first == 0) {
        throw invalid_argument("Cannot decode SequentialNotification");
    }
    requestComplete = true;
    isJournalRequest_ = true;
    return notification.second;
}

void HTTP3TreeMessage::encode_getJournalResponse(const std::vector<SequentialNotification>& notifications)
{
    signal_ = payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_RESPONSE;
    chunkList encoded = encode_VectorSequentialNotification(request_id_, signal_, notifications);
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    responseChunks.insert(responseChunks.end(), std::make_move_iterator(encoded.begin()), std::make_move_iterator(encoded.end()));
    responseComplete = true;
    isJournalRequest_ = true;
}

std::vector<SequentialNotification> HTTP3TreeMessage::decode_getJournalResponse()
{
    if (!responseComplete) {
        throw invalid_argument("HTTP3TreeMessage response has not been fully recieved");
    }
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    auto decoded = decode_VectorSequentialNotification(responseChunks);
    processingFinished = true;
    isJournalRequest_ = true;
    return decoded.second;
}

void HTTP3TreeMessage::setup_staticNodeDataRequest(void) {
    if (isInitialized_) {
        throw invalid_argument("HTTP3TreeMessage is already initialized");
    }
    signal_ = payload_chunk_header::SIGNAL_OTHER_CHUNK;
    isInitialized_ = true;
    auto tag = payload_chunk_header(0, payload_chunk_header::SIGNAL_PLACEHOLDER, 0);
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.emplace_back(tag, true);
}

void HTTP3TreeMessage::setRequestId(uint16_t request_id) {
    request_id_ = request_id;
    for (auto& chunk : requestChunks) {
        if (chunk.get_signal_type() == payload_chunk_header::GLOBAL_SIGNAL_TYPE) {
            chunk.set_request_id(request_id);
        }
    }
    requestComplete = true;
}

fplus::maybe<shared_span<> > HTTP3TreeMessage::popRequestChunk() {
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    if (requestChunks.empty()) {
        return fplus::nothing<shared_span<>>();
    }
    auto chunk = fplus::maybe(requestChunks.front());
    requestChunks.pop_front();
    return chunk;
}


void HTTP3TreeMessage::pushRequestChunk(shared_span<> chunk) {
    uint8_t signal = chunk.get_signal_signal();
    std::lock_guard<std::mutex> lock(requestChunksMutex);
    requestChunks.push_back(chunk);
    if (requestChunks.size() == 1) {
        request_id_ = chunk.get_request_id();
        isInitialized_ = true;
    }
    auto can_decode = fplus::maybe<size_t>();
    switch (signal) {
        case payload_chunk_header::SIGNAL_WWATP_GET_NODE_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_REQUEST:
            can_decode = can_decode_VectorTreeNode(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_REQUEST:
            can_decode = can_decode_MaybeTreeNode(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_REQUEST:
            can_decode = can_decode_Transaction(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            // If decoding the label fails, then MaybeTreeNode will fail too, so attempting 
            // decode at index 0 is OK here.
            can_decode = can_decode_MaybeTreeNode(can_decode.get_with_default(0), requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_REQUEST:
            can_decode = can_decode_label(0, requestChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_REQUEST:
            can_decode = can_decode_SequentialNotification(0, requestChunks);
            break;
        default:
            throw invalid_argument("Unknown signal");
    }
    if (can_decode.is_just()) {
        requestComplete = true;
    }
}

fplus::maybe<shared_span<> > HTTP3TreeMessage::popResponseChunk() {
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    if (responseChunks.empty()) {
        if (responseComplete) {
            processingFinished = true;
        }
        return fplus::nothing<shared_span<>>();
    }
    auto chunk = fplus::maybe(responseChunks.front());
    responseChunks.pop_front();
    return chunk;
}

void HTTP3TreeMessage::pushResponseChunk(shared_span<> chunk) {
    std::lock_guard<std::mutex> lock(responseChunksMutex);
    uint8_t signal_type = chunk.get_signal_type();
    if (signal_type == no_chunk_header::GLOBAL_SIGNAL_TYPE) {
        responseChunks.push_back(chunk);
        return;
    }
    uint8_t signal = chunk.get_signal_signal();
    if (signal_type == signal_chunk_header::GLOBAL_SIGNAL_TYPE) {
        if (signal == signal_chunk_header::SIGNAL_CLOSE_STREAM) {
            responseComplete = true;
            return;
        }
        return;
    }
    responseChunks.push_back(chunk);
    auto can_decode = fplus::maybe<size_t>();
    switch (signal) {
        case payload_chunk_header::SIGNAL_WWATP_GET_NODE_RESPONSE:
            can_decode = can_decode_MaybeTreeNode(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_RESPONSE:
            can_decode = can_decode_VectorTreeNode(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_RESPONSE:
            can_decode = can_decode_VectorTreeNode(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_RESPONSE:
            can_decode = can_decode_VectorTreeNode(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_RESPONSE:
            can_decode = can_decode_label(0, responseChunks);
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_RESPONSE:
            can_decode = can_decode_VectorSequentialNotification(0, responseChunks);
            break;
        default:
            throw invalid_argument("Unknown signal");
    }
    if (can_decode.is_just()) {
        signal_ = signal;  // Rewrite the message signal state to be the response signal
        responseComplete = true;
    }
}

void HTTP3TreeMessage::reset() {
    request_id_ = 0;
    isInitialized_ = false;
    requestComplete = false;
    responseComplete = false;
    requestChunks.clear();
    responseChunks.clear();
}