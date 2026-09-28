#include "http3_client_backend.h"
#include "http3_tree_message.h"
#include <thread>

using namespace std;

void Http3ClientBackend::awaitResponse()
{
    std::unique_lock<std::mutex> lock(blockingMutex_);
    responseReady = false; // Reset before waiting for the next response
    responseCondition.wait(lock, [&]{ return responseReady; }); // blocks until ready is true
}

bool Http3ClientBackend::awaitBoolResponse()
{
    awaitResponse();
    std::lock_guard<std::mutex> lock(handlerMutex_);
    bool result = boolResponse;
    boolResponse = false; // Reset for next response
    return result;
}

fplus::maybe<TreeNode> Http3ClientBackend::awaitTreeResponse()
{
    awaitResponse();
    std::lock_guard<std::mutex> lock(handlerMutex_);
    fplus::maybe<TreeNode> result = maybeTreeResponse_;
    maybeTreeResponse_ = fplus::nothing<TreeNode>(); // Reset for next response
    return result;
}

std::vector<TreeNode> Http3ClientBackend::awaitVectorTreeResponse()
{
    awaitResponse();
    std::lock_guard<std::mutex> lock(handlerMutex_);
    std::vector<TreeNode> result = vectorTreeResponse_;
    vectorTreeResponse_.clear(); // Reset for next response
    return result;
}

chunks Http3ClientBackend::awaitChunksResponse()
{
    awaitResponse();
    std::lock_guard<std::mutex> lock(handlerMutex_);
    chunks result = chunksResponse;
    chunksResponse.clear(); // Reset for next response
    return result;
}

void Http3ClientBackend::responseSignal()
{
    std::lock_guard<std::mutex> lock(blockingMutex_);
    responseReady = true;
    responseCondition.notify_one();
}

fplus::maybe<TreeNode>& Http3ClientBackend::getStaticNode() {
    // If the static node is not set, then we need to request it from the server.
    if (blockingMode_) {
        requestStaticNodeData();
    }
    return staticNode_;
}

fplus::maybe<TreeNode> Http3ClientBackend::getNode(const std::string& label_rule) const {
    // Use the caching localBackend_ to get the node
    std::lock_guard<std::mutex> lock(backendMutex_);
    return localBackend_.getNode(label_rule);
}

bool Http3ClientBackend::upsertNode(const std::vector<TreeNode>& nodes) {
    // Use the caching localBackend_ to upsert the node
    bool result(false);
    {
        std::lock_guard<std::mutex> lock(backendMutex_);
        result = localBackend_.upsertNode(nodes);
    }
    // But upsertNode also needs to send to the server.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (!serverSyncOn_)
        {
            return result;
        }
        HTTP3TreeMessage request(0);
        request.encode_upsertNodeRequest(nodes);
        pendingRequests_.push_back(std::move(request));
    }
    if (!blockingMode_) {
        return result;
    }
    return awaitBoolResponse();
}

bool Http3ClientBackend::deleteNode(const std::string& label_rule) {
    // Use the caching localBackend_ to delete the node
    bool result(false);
    {
        std::lock_guard<std::mutex> lock(backendMutex_);
        result = localBackend_.deleteNode(label_rule);
    }
    // But deleteNode also needs to send to the server.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (!serverSyncOn_)
        {
            return result;
        }
        HTTP3TreeMessage request(0);
        request.encode_deleteNodeRequest(label_rule);
        pendingRequests_.push_back(std::move(request));
    }
    if (!blockingMode_) {
        return result;
    }
    return awaitBoolResponse();
}

std::vector<TreeNode> Http3ClientBackend::getPageTree(const std::string& page_node_label_rule) const {
    // Use the caching localBackend_ to get the page tree
    std::lock_guard<std::mutex> lock(backendMutex_);
    return localBackend_.getPageTree(page_node_label_rule);
}

std::vector<TreeNode> Http3ClientBackend::relativeGetPageTree(const TreeNode& node, const std::string& page_node_label_rule) const {
    // create the relative_page_label_rule
    std::string relative_page_label_rule = node.getLabelRule() + "/" + page_node_label_rule;
    return getPageTree(relative_page_label_rule);
}

std::vector<TreeNode> Http3ClientBackend::queryNodes(const std::string& label_rule) const {
    // Use the caching localBackend_ to query the nodes
    std::lock_guard<std::mutex> lock(backendMutex_);
    return localBackend_.queryNodes(label_rule);
}

std::vector<TreeNode> Http3ClientBackend::relativeQueryNodes(const TreeNode& node, const std::string& label_rule) const {
    // create the relative_label_rule
    std::string relative_label_rule = node.getLabelRule() + "/" + label_rule;
    return queryNodes(relative_label_rule);
}

bool Http3ClientBackend::openTransactionLayer(const TreeNode& node) {
    // Use the caching localBackend_ to open the transaction layer
    bool result(false);
    {
        std::lock_guard<std::mutex> lock(backendMutex_);
        result = localBackend_.openTransactionLayer(node);
        if(!result) {
            return false;
        }
    }
    // But openTransactionLayer also needs to send to the server.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (!serverSyncOn_)
        {
            return result;
        }
        HTTP3TreeMessage request(0);
        request.encode_openTransactionLayerRequest(node);
        pendingRequests_.push_back(std::move(request));
    }
    if (!blockingMode_) {
        return result;
    }
    result = awaitBoolResponse();
    if (!result) {
        std::lock_guard<std::mutex> lock(backendMutex_);
        localBackend_.closeTransactionLayers();
    }
    return result;
}

bool Http3ClientBackend::closeTransactionLayers(void) {
    // Use the caching localBackend_ to close the transaction layers
    bool result(false);
    {
        std::lock_guard<std::mutex> lock(backendMutex_);
        result = localBackend_.closeTransactionLayers();
    }
    // But closeTransactionLayers also needs to send to the server.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (!serverSyncOn_)
        {
            return result;
        }
        HTTP3TreeMessage request(0);
        request.encode_closeTransactionLayersRequest();
        pendingRequests_.push_back(std::move(request));
    }
    if (!blockingMode_) {
        return result;
    }
    return awaitBoolResponse();
}

bool Http3ClientBackend::applyTransaction(const Transaction& transaction) {
    // ApplyTransaction _surely_ needs to send to the server.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (serverSyncOn_)
        {
            HTTP3TreeMessage request(0);
            request.encode_applyTransactionRequest(transaction);
            pendingRequests_.push_back(std::move(request));
        }
    }
    if (!blockingMode_ || !serverSyncOn_) {
        std::lock_guard<std::mutex> lock(backendMutex_);
        return localBackend_.applyTransaction(transaction);
    } 
    if (awaitBoolResponse()) {
        std::lock_guard<std::mutex> lock(backendMutex_);
        return localBackend_.applyTransaction(transaction);
    }
    return false;
}

std::vector<TreeNode> Http3ClientBackend::getFullTree() const {
    // Use the caching localBackend_ to get the full tree
    std::lock_guard<std::mutex> lock(backendMutex_);
    return localBackend_.getFullTree();
}

void Http3ClientBackend::registerNodeListener(const std::string listener_name, const std::string label_rule, bool child_notify, NodeListenerCallback callback) {
    // Since the user of the http3 client backend is asking to listen for changes, 
    // in fact, the listeners need to be registered both with the local backend and the server.
    // In cases where the local backend sees a change, then the server will eventually duplicate the notification.
    // Therefore, we need to track local notifications and deduplicate them versus server notifications.
    std::lock_guard<std::mutex> lock(backendMutex_);
    NodeListenerCallback local_callback = [this, callback](Backend&, const std::string label_rule, const fplus::maybe<TreeNode> node) {
        // Notify the local backend
        callback(*this, label_rule, node);
    };
    localBackend_.registerNodeListener(listener_name, label_rule, child_notify, local_callback);
    // Register the listener with the server
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (serverSyncOn_)
        {
            HTTP3TreeMessage request(0);
            request.encode_registerNodeListenerRequest(listener_name, label_rule, child_notify);
            pendingRequests_.push_back(std::move(request));
        }
    }
    // But registerNodeListner is not the kind of method that blocks, so done here.
}

void Http3ClientBackend::deregisterNodeListener(const std::string listener_name, const std::string label_rule) {
    std::lock_guard<std::mutex> lock(backendMutex_);
    localBackend_.deregisterNodeListener(listener_name, label_rule);
    // Deregister the listener with the server
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (serverSyncOn_) {
            HTTP3TreeMessage request(0);
            request.encode_deregisterNodeListenerRequest(listener_name, label_rule);
            pendingRequests_.push_back(std::move(request));
        }
    }
}

void Http3ClientBackend::notifyListeners(const std::string& label_rule, const fplus::maybe<TreeNode>& node) {
    // Notify the local backend
    std::lock_guard<std::mutex> lock(backendMutex_);
    localBackend_.notifyListeners(label_rule, node);
    // Notify the server
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (serverSyncOn_) {
            HTTP3TreeMessage request(0);
            request.encode_notifyListenersRequest(label_rule, node);
            pendingRequests_.push_back(std::move(request));
        }
    }
}

void Http3ClientBackend::processNotifications() {
    // This blocks until a journal request and response has occurred.
    // Since this is the ONLY use of notificationBlock_, it doesn't hurt to store true
    // even when not in blocking mode.
    notificationBlock_.store(true);
    // Meanwhile, tell the backend to process notifications.
    {
        std::lock_guard<std::mutex> lock(backendMutex_);
        localBackend_.processNotifications();
    }
    // And then also tell the server to process notifications.
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (!serverSyncOn_)
        {
            return;
        }
        HTTP3TreeMessage request(0);
        request.encode_processNotificationRequest();
        pendingRequests_.push_back(std::move(request));
    }
    if (!blockingMode_) {
        return;
    }
    awaitResponse();
        // Sleep time is computed as 1/10 th of the journalRequestsPerMinute/60.
    auto sleepTime = std::chrono::milliseconds(1000 * (journalRequestsPerMinute_ / 600));
    auto waitForNotification = [&sleepTime](atomic<bool>& notificationBlock) {
        // Wait for the notification to be processed to make sure the journaling
        // system change from the server has been flushed through.
        while (notificationBlock.load()) {
            std::this_thread::sleep_for(sleepTime);
        }
    };
    if (journalRequestWaiting_.load()) {
        // If we are currently waiting for a journal request, there is a chance this journal request will
        // return stale information.  So wait it out:
        waitForNotification(notificationBlock_);
        // Then wait for the next notification to be processed.
        notificationBlock_.store(true);
        // And set lastJournalRequestTime_ to 2.0 to immediately trigger a new journal request.
        // (less than or equal to 1.0 will suppress the journal request).
        lastJournalRequestTime_.store(2.0);
    }
    waitForNotification(notificationBlock_);
}

void Http3ClientBackend::processHTTP3Response(HTTP3TreeMessage& response) {
    // Process the response based on the signal
    switch (response.getSignal()) {
        case payload_chunk_header::SIGNAL_OTHER_CHUNK: {
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    auto aChunk = response.popResponseChunk();
                    while (aChunk.is_just()) {
                        chunksResponse.push_back(aChunk.unsafe_get_just());
                        aChunk = response.popResponseChunk();
                    }
                }
                if (blockingMode_) {
                    responseSignal();
                } else {
                    disableServerSync();
                    setNodeChunks(chunksResponse);
                    enableServerSync();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_NODE_RESPONSE: {
                fplus::maybe<TreeNode> node = response.decode_getNodeResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    maybeTreeResponse_ = node;
                }
                if (blockingMode_) {
                    responseSignal();
                } else {
                    disableServerSync();
                    if (node.is_just()) {
                        upsertNode({node.unsafe_get_just()});
                    } else {
                        deleteNode(response.decode_deleteNodeRequest());
                    }
                    enableServerSync();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_RESPONSE: {
                bool success = response.decode_upsertNodeResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = success;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_RESPONSE: {
                bool success = response.decode_deleteNodeResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = success;
                }
                if (blockingMode_) {
                    responseSignal();
                } 
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_RESPONSE: {
                std::vector<TreeNode> nodes = response.decode_getPageTreeResponse();
                // Alternatively, if the request was actually a sync-up from the Journal Request, 
                // then this should never do the response signal, but _should_ clear the notification block.
                if (response.isJournalRequest()) {
                    disableServerSync();
                    upsertNode(nodes);
                    enableServerSync();
                    notificationBlock_.store(false); // Clear the notification block
                } else {
                    {
                        std::lock_guard<std::mutex> lock(handlerMutex_);
                        vectorTreeResponse_ = nodes;
                    }
                    if (blockingMode_) {
                        responseSignal();
                    } else {
                        disableServerSync();
                        upsertNode(nodes);
                        enableServerSync();
                    }
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_RESPONSE: {
                std::vector<TreeNode> nodes = response.decode_getQueryNodesResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    vectorTreeResponse_ = nodes;
                }
                if (blockingMode_) {
                    responseSignal();
                } else {
                    disableServerSync();
                    upsertNode(nodes);
                    enableServerSync();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_RESPONSE: {
                bool success = response.decode_openTransactionLayerResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = success;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_RESPONSE: {
                bool success = response.decode_closeTransactionLayersResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = success;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_RESPONSE: {
                bool success = response.decode_applyTransactionResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = success;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_RESPONSE: {
                std::vector<TreeNode> nodes = response.decode_getFullTreeResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    vectorTreeResponse_ = nodes;
                }
                if (blockingMode_) {
                    responseSignal();
                } else {
                    disableServerSync();
                    deleteNode("");
                    upsertNode(nodes);
                    enableServerSync();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_RESPONSE: {
                bool result = response.decode_registerNodeListenerResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = result;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_RESPONSE: {
                bool result = response.decode_deregisterNodeListenerResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = result;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_RESPONSE: {
                bool result = response.decode_notifyListenersResponse();
                {
                    std::lock_guard<std::mutex> lock(handlerMutex_);
                    boolResponse = result;
                }
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_RESPONSE: {
                response.decode_processNotificationResponse();
                if (blockingMode_) {
                    responseSignal();
                }
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_RESPONSE: {
                std::vector<SequentialNotification> notifications = response.decode_getJournalResponse();
                // loop backwards through the notifications for the last solicited notification
                // (unsolicited notifications are (uint64_t)-1, indicating usually reverted/rejected changes)
                bool notificationGap = lastNotification_.first < notifications.front().first;
                uint64_t curNotificationIndex = notifications.front().first;
                if(!notificationGap) {
                    for (auto it = notifications.begin(); it != notifications.end(); ++it) {
                        if (it->first == (uint64_t)-1) {
                            continue;
                        }
                        if (it->first > curNotificationIndex + 1) {
                            notificationGap = true;
                            break;
                        }
                        curNotificationIndex = it->first;
                    }
                }
                SequentialNotification lastValidNotification = notifications.front();
                for (auto it = notifications.begin(); it != notifications.end(); ++it) {
                    if (!notificationGap && it->first > lastNotification_.first) {
                        processOneNotification(*it);
                    }
                    if (it->first == (uint64_t)-1) {
                        continue;
                    }
                    lastValidNotification = *it;
                }
                if (notificationGap) {
                    getMutablePageTreeInMode(false, true);
                }
                else {
                    // If there is no gap, then immediately clear any notification block
                    notificationBlock_.store(false);
                }
                updateLastNotification(lastValidNotification);
            }
            break;
        default:
            // Handle unknown signal
            std::cerr << "Unknown signal: 0x" << hex << static_cast<int>(response.getSignal()) << std::endl;
            break;
        }
}

HTTP3TreeMessage Http3ClientBackend::popNextRequest() {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    if (pendingRequests_.empty()) {
        throw std::runtime_error("No pending requests");
    }
    HTTP3TreeMessage request(std::move(pendingRequests_.front()));
    pendingRequests_.pop_front();
    return request;
}

HTTP3TreeMessage Http3ClientBackend::solicitJournalRequest() const {
    HTTP3TreeMessage request(0);
    std::lock_guard<std::mutex> lock(handlerMutex_);
    request.encode_getJournalRequest(lastNotification_);
    return request;
}

void Http3ClientBackend::requestFullTreeSync() {
    if (staticNode_.is_just()) {
        requestStaticNodeData();
        return;
    }
    // requestFullTreeSync explicitly from the server.
    {
        HTTP3TreeMessage request(0);
        request.encode_getFullTreeRequest();
        std::lock_guard<std::mutex> lock(handlerMutex_);
        pendingRequests_.push_back(std::move(request));
    }
    if(!blockingMode_) {
        return;
    }
    // Wait for the response
    auto nodes = awaitVectorTreeResponse();
    disableServerSync();
    deleteNode("");
    upsertNode(nodes);
    enableServerSync();
    std::lock_guard<std::mutex> lock(backendMutex_);
}

void Http3ClientBackend::requestStaticNodeData() {
    // requestStaticNodeData explicitly from the server.
    {
        HTTP3TreeMessage request(0);
        request.setup_staticNodeDataRequest();
        std::lock_guard<std::mutex> lock(handlerMutex_);
        pendingRequests_.push_back(std::move(request));
    }
    if(!blockingMode_) {
        return;
    }
    // Wait for the response
    auto chunks = awaitChunksResponse();

    disableServerSync();
    setNodeChunks(chunks);
    enableServerSync();
}

void Http3ClientBackend::setNodeChunks(chunks& chunks) {
    std::lock_guard<std::mutex> lock(backendMutex_);
    if (staticNode_.is_just()) {
        shared_span<> concatted(chunks.begin(), chunks.end());
        staticNode_.unsafe_get_just().setPropertyData(std::move(concatted));
        localBackend_.upsertNode({staticNode_.unsafe_get_just()});
    } else {
        std::cerr << "Error: Static Node for storing property_data is not defined" << std::endl;
    }
}

void Http3ClientBackend::setMutablePageTreeLabelRule(std::string label_rule) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    mutablePageTreeLabelRule_ = label_rule;
}

void Http3ClientBackend::getMutablePageTree() {
    getMutablePageTreeInMode(blockingMode_, false);
}

void Http3ClientBackend::getMutablePageTreeInMode(bool blocking_mode, bool isJournalRequest) {
    // Use the caching localBackend_ to get the mutable page tree
    string label_rule = "MutableNodes";
    {
        std::lock_guard<std::mutex> lock(handlerMutex_);
        auto label_rule = mutablePageTreeLabelRule_;
    }
    {
        HTTP3TreeMessage request(0);
        request.encode_getPageTreeRequest(label_rule);
        request.setIsJournalRequest(isJournalRequest);
        std::lock_guard<std::mutex> lock(handlerMutex_);
        pendingRequests_.push_back(std::move(request));
    }
    if(!blocking_mode) {
        return;
    }
    // Wait for the response
    auto nodes = awaitVectorTreeResponse();
    disableServerSync();
    upsertNode(nodes);
    enableServerSync();
    std::lock_guard<std::mutex> lock(backendMutex_);
    needMutablePageTreeSync_ = false;
}

void Http3ClientBackend::processOneNotification(SequentialNotification const& notification) {
    //uint64_t sequence_num = notification.first;
    Notification const& notification_data = notification.second;
    std::string label_rule = notification_data.first;
    if (label_rule == "") { // For tracking purposes, 
        // for example, the client does not have premission to view the node.
        return;
    }
    fplus::maybe<TreeNode> const& node = notification_data.second;
    if (node.is_just()) {
        disableServerSync();
        upsertNode({node.unsafe_get_just()});
        enableServerSync();
    } else {
        disableServerSync();
        deleteNode(label_rule);
        enableServerSync();
    }
}

void Http3ClientBackend::updateLastNotification(SequentialNotification const& notification) {
    if (notification.first == (uint64_t)-1) {
        return; // Unsolicited notifications cannot be used for tracking purposes.
    }
    notificationBlock_.store(false);
    std::lock_guard<std::mutex> lock(backendMutex_);
    lastNotification_ = notification;
}

Http3ClientBackend& Http3ClientBackendUpdater::addBackend(Backend& local_backend, bool blocking_mode, Request request,
        size_t journal_requests_per_minute, fplus::maybe<TreeNode> static_node) {
    return backends_.emplace_back(local_backend, blocking_mode, request, journal_requests_per_minute, static_node);
}

Http3ClientBackend& Http3ClientBackendUpdater::getBackend(const std::string& url) {
    for (auto& backend : backends_) {
        if (url.find(backend.getRequestUrlInfo().path) != std::string::npos) {
            return backend;
        }
    }
    throw std::runtime_error("Backend not found");
}

bool Http3ClientBackend::needToSendJournalRequest(double time) {
    if (journalRequestWaiting_.load()) {
        return false; // If we are already waiting for a journal request, then don't need to send one.
    }
    if ((journalRequestsPerMinute_ == 0) || (lastJournalRequestTime_.load() <= 1.0)) {
        lastJournalRequestTime_.store(time);
        return false;
    }
    double timeDelta = time - lastJournalRequestTime_.load();
    bool needToSend = (timeDelta*journalRequestsPerMinute_ > 60.0);
    if (needToSend) {
        journalRequestWaiting_.store(true); // Set the flag to indicate we are waiting for a journal request.
        lastJournalRequestTime_.store(time);
    }
    return needToSend;
}

void Http3ClientBackend::setJournalRequestComplete() {
    journalRequestWaiting_.store(false); // Reset the flag to indicate we are no longer waiting for a journal request.
}

void Http3ClientBackendUpdater::maintainRequestHandlers(Communication& connector, double time) {
    for (auto& backend : backends_) {
        if (backend.hasNextRequest()) {
            auto req = backend.getRequestUrlInfo();
            StreamIdentifier stream_id = connector.getNewRequestStreamIdentifier(req);
            auto theRequest = ongoingRequests_.emplace(stream_id, backend.popNextRequest());
            theRequest.first->second.setRequestId(stream_id.logical_id);
            HTTP3TreeMessage& theMessage = theRequest.first->second;
            stream_callback_fn messageHandler = [&backend, &theMessage, &connector](StreamIdentifier const& stream_identifier, chunks& response) {
                chunks request;
                auto requestChunk = theMessage.popRequestChunk();
                while(requestChunk.is_just()) {
                    auto chunk = requestChunk.unsafe_get_just();
                    request.push_back(chunk);
                    requestChunk = theMessage.popRequestChunk();
                }
                for (auto& chunk : response) {
                    theMessage.pushResponseChunk(chunk);
                }
                if (response.empty() && request.empty() && !theMessage.isResponseComplete() && backend.getRequestUrlInfo().isWWATP()) {
                    chunks response_chunks;
                    auto tag = payload_chunk_header(stream_identifier.logical_id, payload_chunk_header::SIGNAL_HEARTBEAT, 0);
                    response_chunks.emplace_back(tag, span<const char>("", 0));
                    return response_chunks;
                }
                if (theMessage.isResponseComplete()) {
                    backend.processHTTP3Response(theMessage);
                }
                if (theMessage.isProcessingFinished()) {
                    // return signal_chunk_header::SIGNAL_CLOSE_STREAM
                    signal_chunk_header signal(stream_identifier.logical_id, signal_chunk_header::SIGNAL_CLOSE_STREAM);
                    request.push_back(shared_span<>(signal, true));
                    connector.deregisterResponseHandler(stream_identifier);
                }
                return request;
            };
            connector.registerResponseHandler(stream_id, messageHandler);
        }
        if (backend.needToSendJournalRequest(time)) {
            auto req = backend.getRequestUrlInfo();
            StreamIdentifier stream_id = connector.getNewRequestStreamIdentifier(req);
            auto theRequest = ongoingRequests_.emplace(stream_id, backend.solicitJournalRequest());
            theRequest.first->second.setRequestId(stream_id.logical_id);
            HTTP3TreeMessage& theMessage = theRequest.first->second;
            stream_callback_fn messageHandler = [&backend, &theMessage, &connector](StreamIdentifier const& stream_identifier, chunks& response) {
                chunks request;
                auto requestChunk = theMessage.popRequestChunk();
                while(requestChunk.is_just()) {
                    auto chunk = requestChunk.unsafe_get_just();
                    request.push_back(chunk);
                    requestChunk = theMessage.popRequestChunk();
                }
                for (auto& chunk : response) {
                    theMessage.pushResponseChunk(chunk);
                }
                if (response.empty() && request.empty() && !theMessage.isResponseComplete() && backend.getRequestUrlInfo().isWWATP()) {
                    chunks response_chunks;
                    auto tag = payload_chunk_header(stream_identifier.logical_id, payload_chunk_header::SIGNAL_HEARTBEAT, 0);
                    response_chunks.emplace_back(tag, span<const char>("", 0));
                    return response_chunks;
                }
                if (theMessage.isResponseComplete()) {
                    backend.processHTTP3Response(theMessage);
                }
                if (theMessage.isProcessingFinished()) {
                    // return signal_chunk_header::SIGNAL_CLOSE_STREAM
                    signal_chunk_header signal(stream_identifier.logical_id, signal_chunk_header::SIGNAL_CLOSE_STREAM);
                    request.push_back(shared_span<>(signal, true));
                    connector.deregisterResponseHandler(stream_identifier);
                    backend.setJournalRequestComplete();
                }
                return request;
            };
            connector.registerResponseHandler(stream_id, messageHandler);
        }
    }
    lastTime_ = time;
}