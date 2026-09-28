#include "http3_tree_message.h"
#include "http3_server.h"

chunks Http3ServerRoute::processResponseStream(const StreamIdentifier& stream_id, chunks& request) {
    chunks response;
    // Process the request and return the response
    auto it = ongoingResponses_.find(stream_id);
    if (it == ongoingResponses_.end()) {
        if( request.size() == 0) {
            // If there is heretofore no request information, we don't know how to respond yet.
            return response;
        }
        HTTP3TreeMessage state = intializeResponseMessage(stream_id, request);
        ongoingResponses_.emplace(stream_id, std::move(state));
        it = ongoingResponses_.find(stream_id);
    }
    if (it->second.isProcessingFinished()) {
        if (request.size() == 1 && request[0].get_signal_type() == signal_chunk_header::GLOBAL_SIGNAL_TYPE) {
            auto signal = request[0].get_signal<signal_chunk_header>();
            if (signal.signal == signal_chunk_header::SIGNAL_CLOSE_STREAM) {
                ongoingResponses_.erase(it);
            }
        }
        // return signal_chunk_header::SIGNAL_CLOSE_STREAM
        signal_chunk_header signal(stream_id.logical_id, signal_chunk_header::SIGNAL_CLOSE_STREAM);
        response.push_back(shared_span<>(signal, true));
        return response;
    }
    for (auto& chunk : request) {
        if (chunk.get_signal<payload_chunk_header>().signal == payload_chunk_header::SIGNAL_HEARTBEAT) {
            continue;
        }
        it->second.pushRequestChunk(chunk);
    }
    // Check if the request is complete
    if (it->second.isRequestComplete()) {
        if (!it->second.isResponseComplete()) {
            buildResponseChunks(it->second);
        }
        fplus::maybe<shared_span<>> response_chunk = it->second.popResponseChunk();
        while (response_chunk.is_just()) {
            response.push_back(response_chunk.unsafe_get_just());
            response_chunk = it->second.popResponseChunk();
        }
    } 

    return response;
}

void Http3ServerRoute::buildResponseChunks(HTTP3TreeMessage& requested) {
    switch (requested.getSignal()) {
        case payload_chunk_header::SIGNAL_WWATP_GET_NODE_REQUEST:
            {
                auto node_label = requested.decode_getNodeRequest();
                auto node = backend_.getNode(node_label);
                requested.encode_getNodeResponse(node);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_REQUEST:
            {
                auto nodes = requested.decode_upsertNodeRequest();
                backend_.upsertNode(nodes);
                requested.encode_upsertNodeResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_REQUEST:
            {
                auto node_label = requested.decode_deleteNodeRequest();
                backend_.deleteNode(node_label);
                requested.encode_deleteNodeResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_REQUEST:
            {
                auto page_node_label_rule = requested.decode_getPageTreeRequest();
                auto nodes = backend_.getPageTree(page_node_label_rule);
                requested.encode_getPageTreeResponse(nodes);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_REQUEST:
            {
                auto nodes = requested.decode_getQueryNodesRequest();
                auto nodes_response = backend_.queryNodes(nodes);
                requested.encode_getQueryNodesResponse(nodes_response);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_REQUEST:
            {
                auto node = requested.decode_openTransactionLayerRequest();
                backend_.openTransactionLayer(node);
                requested.encode_openTransactionLayerResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_REQUEST:
            {
                backend_.closeTransactionLayers();
                requested.encode_closeTransactionLayersResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_REQUEST:
            {
                auto transaction = requested.decode_applyTransactionRequest();
                backend_.applyTransaction(transaction);
                requested.encode_applyTransactionResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_REQUEST:
            {
                auto nodes = backend_.getFullTree();
                requested.encode_getFullTreeResponse(nodes);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_REQUEST:
            {
                auto listener_info = requested.decode_registerNodeListenerRequest();
                string node_label = std::get<1>(listener_info);
                bool child_notify = std::get<2>(listener_info);
                auto findit = listeningLabels_.find(node_label);
                if ((findit == listeningLabels_.end()) || (child_notify && !findit->second)) {
                    listeningLabels_[node_label] = child_notify;
                    NodeListenerCallback callback = [this](Backend& /*backend*/, const std::string& label, const fplus::maybe<TreeNode>& node) {
                        SequentialNotification notification = {0, {label, node}};
                        if (journal_.size() > 0) {
                            notification.first = journal_.rbegin()->first + 1;
                        }
                        journal_.push_back(notification);
                        if (journal_.size() > maxJournalSize_) {
                            journal_.erase(journal_.begin());
                        }
                    };
                    backend_.registerNodeListener(url_, node_label, child_notify, callback);
                    // And also check if it is in the mutable page children
                    auto mutable_page = backend_.getNode(mutablePageLabelRule_);
                    if (mutable_page.is_just()) {
                        auto mutable_page_children = mutable_page.unsafe_get_just().getChildNames();
                        // Try to find the child in the mutable page children using binary search
                        auto it = std::lower_bound(mutable_page_children.begin(), mutable_page_children.end(), node_label);
                        if (it == mutable_page_children.end() || *it != node_label) {
                            mutable_page_children.insert(it, node_label);
                            mutable_page.unsafe_get_just().setChildNames(mutable_page_children);
                            backend_.upsertNode({mutable_page.unsafe_get_just()});
                        }
                    } else {
                        // If the mutable page is not found, create a new one
                        TreeNode mutable_page_node;
                        mutable_page_node.setLabelRule(mutablePageLabelRule_);
                        mutable_page_node.setChildNames({node_label});
                        backend_.upsertNode({mutable_page_node});
                    }
                }
                requested.encode_registerNodeListenerResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_REQUEST:
            {
                auto listener_info = requested.decode_deregisterNodeListenerRequest();
                // Deregestering the listener is not supported, since the notifications
                // are the same for all clients.
                requested.encode_deregisterNodeListenerResponse(false);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_REQUEST:
            {
                Notification notification = requested.decode_notifyListenersRequest();
                auto label_rule = notification.first;
                auto node = notification.second;
                backend_.notifyListeners(label_rule, node);
                requested.encode_notifyListenersResponse(true);
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_REQUEST:
            {
                backend_.processNotifications();
                requested.encode_processNotificationResponse();
            }
            break;
        case payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_REQUEST:
            {
                auto last_notification = requested.decode_getJournalRequest();
                auto last_notification_index = last_notification.first;
                bool wrong_journal_sequence = false;
                auto it = std::lower_bound(journal_.begin(), journal_.end(), last_notification_index,
                    [](const SequentialNotification& notification, uint64_t index) {
                        return notification.first < index;
                    });
                if (it == journal_.end() && last_notification_index != journal_.rbegin()->first) {
                    wrong_journal_sequence = true;
                }
                if (last_notification_index < journal_.begin()->first) {
                    wrong_journal_sequence = true;
                }
                if (wrong_journal_sequence) {
                    // A WRONG_JOURNAL_SEQUENCE is a SequentialNotification with number 0, then the number 2, then the latest sequence number.
                    std::vector<SequentialNotification> notifications;
                    notifications.push_back({0, {"", fplus::maybe<TreeNode>()}});
                    notifications.push_back({2, {"", fplus::maybe<TreeNode>()}});
                    notifications.push_back({journal_.rbegin()->first, {"", fplus::maybe<TreeNode>()}});
                    requested.encode_getJournalResponse(notifications);
                } else {
                    std::vector<SequentialNotification> notifications(it, journal_.end());
                    requested.encode_getJournalResponse(notifications);
                    for (const auto& notification : notifications) {
                        // TODO: Remove these logs later
                        if (notification.first == 0)
                            continue;
                        cerr << "Notification: " << notification.first;
                        cerr << ", label = " << notification.second.first;
                        if (notification.second.second.is_just())
                            cerr << ", payload found";
                        cerr << endl << flush;
                    }
                }
            }
            break;
        default:
            throw invalid_argument("Unknown signal");
    }
}

HTTP3TreeMessage Http3ServerRoute::intializeResponseMessage(const StreamIdentifier& stream_id, chunks& request) const {
    if (stream_id.logical_id != request.front().get_request_id()) {
        throw std::invalid_argument("Stream logical ID does not match request ID");
    }
    return HTTP3TreeMessage(stream_id.logical_id, request.front().get_signal_signal());
}


bool HTTP3Server::isChunkStream(std::string const& url) const {
    // Return true if and only if "wwatp/" appears in the URL
    if (url.find("wwatp/") != std::string::npos) {
        return true;
    }
    return false;
}

chunks HTTP3Server::staticAsset(std::string const& url) const {
    auto it = staticAssets_.find(url);
    if (it != staticAssets_.end()) {
        return it->second;
    }
    // If the asset is not found, return an empty chunk
    return chunks();
}

prepared_stream_callback HTTP3Server::getResponseCallback(const Request & req) {
    if (req.isWWATP()) {
        // This is a chunk stream, so we need set up the response for the right backend
        auto it = servers_.find(req.path);
        if (it != servers_.end()) {
            Http3ServerRoute& route = it->second;
            stream_callback_fn callback = [&route](const StreamIdentifier& stream_id, chunks& request) {
                return route.processResponseStream(stream_id, request);
            };
            return std::make_pair(uri_response_info{true, true, false, 0}, callback);
        }
    } else {
        // This is a static asset, so we can return it directly.
        // Note that each outstanding request on the client side will have a different stream id,
        // so returning sum_size will apply to only this one asset.
        // That will control when the server stops sending data.
        // To control the number of times the chunks are sent, prepareStaticHandler will directly
        // call the callback with the stream id and immediately add them to the outgoing queue,
        // while substituting a noop for the responder callback. 
        auto it = staticAssets_.find(req.path);
        if (it != staticAssets_.end()) {
            chunks const& asset = it->second; 
            stream_callback_fn callback = [&asset](const StreamIdentifier&, chunks&) {
                return asset;
            };
            size_t sum_size = 0;
            for (const auto& chunk : asset) {
                sum_size += chunk.size();
            }
            return std::make_pair(uri_response_info{true, false, false, sum_size}, callback);
        }
    }
    return std::make_pair(uri_response_info{false, false, false, 0}, nullptr);
}
