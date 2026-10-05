#include <cstring>
#include <iostream>
#include <unistd.h>

#include "socket_communication.h"

TypeLength toTL(TLVType type, uint64_t length)
{
    TypeLength result{};
    result.type = type;
    for (size_t i = 0; i < 7; ++i) {
        result.length[i] = static_cast<uint8_t>(length >> ((6 - i) * 8));
    }
    return result;
}

uint64_t asLength(TypeLength const &tl)
{
    uint64_t length = 0;
    for (size_t i = 0; i < 7; ++i) {
        length = (length << 8) | tl.length[i];
    }
    return length;
}

inline string toString(TLVType t) {
    switch (t) {
        case TLVType::Scheme:     return "scheme";
        case TLVType::Authority:  return "authority";
        case TLVType::Path:       return "path";
        case TLVType::Method:     return "method";
        case TLVType::Priority:   return "priority";
        case TLVType::CID:        return "cid";
        case TLVType::LogicalID:  return "logical_id";
        case TLVType::Chunk:      return "chunk";
        case TLVType::Data:       return "data";
        default: {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned>(t) & 0xFF);
            return hex;
        }
    }
}

StreamIdentifier SocketCommunication::getNewRequestStreamIdentifier(Request const &req) 
{
    if (!req.isWWATP()) {
        lock_guard<std::mutex> lock(returnPathsMutex);
        // This is not a WWATP request, so check if it is already in the staticRequests
        auto findit = staticRequests.find(req);
        if (findit != staticRequests.end()) {
            return findit->second;
        }
    }
    // Since this is simply socket communication, there is no QUIC client to read the cid from. 
    // However, in QUIC on the client, the cid is in effect just tracking one upstream logical connection.
    // Simple socket communication, in contrast, already keeps track of client to server socket connections,
    // and even if there are multiple clients connected to the same server socket, the protcol routes 
    // messages to the correct client.
    // Thus, for a simple socket, cid can be any default value.
    ngtcp2_cid cid = {1, {1}};
    // Use the atomic stream_id_counter to simultaneously increment and return the _logical_ stream_id
    auto stream_id = stream_id_counter.fetch_add(2);
    auto newStreamIdentifier = StreamIdentifier(cid, stream_id);
    lock_guard<std::mutex> lock(returnPathsMutex);
    while(returnPaths.find(newStreamIdentifier) != returnPaths.end()) {
        // If the newStreamIdentifier is already in the returnPaths, we need to increment the stream_id
        stream_id = stream_id_counter.fetch_add(2);
        newStreamIdentifier = StreamIdentifier(cid, stream_id);
    }
    if (!req.isWWATP()) {
        // This is not a WWATP request, so add it to the staticRequests
        staticRequests.insert(make_pair(req, newStreamIdentifier));
    }
    returnPaths.insert(make_pair(newStreamIdentifier, req));
    return newStreamIdentifier;
}

void SocketCommunication::registerResponseHandler(StreamIdentifier sid, stream_callback_fn cb)
{
    lock_guard<std::mutex> lock(requestorQueueMutex);
    requestorQueue.push_back(std::make_pair(sid, cb));
}

bool SocketCommunication::hasResponseHandler(StreamIdentifier sid)
{
    lock_guard<std::mutex> lock(requestorQueueMutex);
    auto it = std::find_if(requestorQueue.begin(), requestorQueue.end(),
        [&sid](const stream_callback& stream_cb) {
            return stream_cb.first == sid;
        });
    if (it != requestorQueue.end()) {
        // The stream callback is already in the queue
        return true;
    }
    return false;
}

void SocketCommunication::deregisterResponseHandler(StreamIdentifier sid)
{
    lock_guard<std::mutex> lock(requestorQueueMutex);
    auto it = std::remove_if(requestorQueue.begin(), requestorQueue.end(),
        [&sid](const stream_callback& stream_cb) {
            return stream_cb.first == sid;
        });
    requestorQueue.erase(it, requestorQueue.end());
}

bool SocketCommunication::processRequestStream()
{
    std::unique_lock<std::mutex> requestorLock(requestorQueueMutex);
    if (!requestorQueue.empty()) {
        stream_callback stream_cb = requestorQueue.front();
        requestorQueue.erase(requestorQueue.begin());
        requestorLock.unlock();
        StreamIdentifier stream_id = stream_cb.first;
        // Now the stream callback is not on the queue, so no other worker will try to service it while this is operational.
        // Call the callback function OUTSIDE the lock
        stream_callback_fn fn = stream_cb.second;
        chunks to_process;
        bool signal_closed = false;
        {
            lock_guard<std::mutex> lock(incomingChunksMutex);
            auto incoming = incomingChunks.find(stream_id);
            if (incoming != incomingChunks.end()) {
                to_process.swap(incoming->second);
            }
        }
        chunks processed = fn(stream_cb.first, to_process);
        size_t chunk_count = processed.size();
        if(!processed.empty())
        {   
            if (processed.size() == 1 && processed[0].get_signal_type() == signal_chunk_header::GLOBAL_SIGNAL_TYPE) {
                // If the processed chunk is a signal chunk, we need to handle it differently
                auto signal = processed[0].get_signal<signal_chunk_header>();
                if (signal.signal == signal_chunk_header::SIGNAL_CLOSE_STREAM) {
                    // If the signal is to close the stream, we need to set the signal_closed flag
                    signal_closed = true;
                }
            }
            else {
                lock_guard<std::mutex> lock(outgoingChunksMutex);
                auto outgoing = outgoingChunks.find(stream_cb.first);
                if (outgoing == outgoingChunks.end()) {
                    auto inserted = outgoingChunks.insert(make_pair(stream_cb.first, chunks()));
                    inserted.first->second.swap(processed);
                }
                else {
                    for (auto &chunk : processed) {
                        outgoing->second.emplace_back(chunk);
                    }
                }
            }
        }
        if (!signal_closed) {
            // If the stream is not closed, we need to push it back to the requestorQueue
            // This is done by locking the requestorQueueMutex again
            requestorLock.lock();
            requestorQueue.push_back(stream_cb);
            if (chunk_count > 0) {
                // If there are chunks to process, we need to start the write event
                outgoingMessageCV.notify_one();
            }
        }
        else {
            // If the stream is closed, we need to also remove any remainders from the incomingChunks and outgoingChunks
            {
                lock_guard<std::mutex> lock(incomingChunksMutex);
                incomingChunks.erase(stream_cb.first);
            }
            {
                lock_guard<std::mutex> lock(outgoingChunksMutex);
                outgoingChunks.erase(stream_cb.first);
            }
            {
                // TODO: Garbage collect old unsused returnPaths
                //lock_guard<std::mutex> lock(returnPathsMutex);
                //returnPaths.erase(stream_cb.first);
                // Don't erase from the staticRequests though
            }
            // And by definition, it is not in the requestorQueue anymore
        }
    } else {
        return false;
    }
    return true;
}

void SocketCommunication::check_deadline() {
    if (timer.expiry() <= std::chrono::steady_clock::now()) {
        timed_out = true;
        socket.cancel();
        timer.expires_at(std::chrono::steady_clock::time_point::max());
    }
    timer.async_wait([this](const boost::system::error_code&) { check_deadline(); });
}

void SocketCommunication::registerRequestHandler(named_prepare_fn preparer)
{
    lock_guard<std::mutex> lock(preparerStackMutex);
    preparersStack.push_back(preparer);
}

void SocketCommunication::deregisterRequestHandler(std::string preparer_name)
{
    lock_guard<std::mutex> lock(preparerStackMutex);
    auto it = std::remove_if(preparersStack.begin(), preparersStack.end(),
        [&preparer_name](const named_prepare_fn& preparer) {
            return preparer.first == preparer_name;
        });
    preparersStack.erase(it, preparersStack.end());
}

bool SocketCommunication::processResponseStream()
{
    std::unique_lock<std::mutex> responderLock(responderQueueMutex);
    if (!responderQueue.empty()) {
        stream_callback stream_cb = responderQueue.front();
        responderQueue.erase(responderQueue.begin());
        responderLock.unlock();
        // Now the stream callback is not on the queue, so no other worker will try to service it while this is operational.
        // Call the callback function OUTSIDE the lock
        stream_callback_fn fn = stream_cb.second;
        chunks to_process;
        bool signal_closed = false;
        {
            lock_guard<std::mutex> lock(incomingChunksMutex);
            auto incoming = incomingChunks.find(stream_cb.first);
            if (incoming != incomingChunks.end()) {
                to_process.swap(incoming->second);
            }
        }
        chunks processed = fn(stream_cb.first, to_process);
        size_t chunk_count = processed.size();
        if(!processed.empty())
        {   
            unique_lock<std::mutex> lock(outgoingChunksMutex);
            auto outgoing = outgoingChunks.find(stream_cb.first);
            if (processed.size() == 1 && processed[0].get_signal_type() == signal_chunk_header::GLOBAL_SIGNAL_TYPE) {
                // If the processed chunk is a signal chunk, we need to check if it is a close stream signal
                auto signal = processed[0].get_signal<signal_chunk_header>();
                if (signal.signal == signal_chunk_header::SIGNAL_CLOSE_STREAM) {
                    if (outgoing == outgoingChunks.end()) {
                        // No chunks left to send
                        signal_closed = true;
                        lock.unlock();
                        processed = fn(stream_cb.first, processed);  // Feed the signal close back to the callback so it can cleanup too. 
                    }
                }
            }
            else {
                if (outgoing == outgoingChunks.end()) {
                    auto inserted = outgoingChunks.insert(make_pair(stream_cb.first, chunks()));
                    inserted.first->second.swap(processed);
                }
                else {
                    for (auto &chunk : processed) {
                        outgoing->second.emplace_back(chunk);
                    }
                }
            }
        }
        if (!signal_closed) {
            // If the stream is not closed, we need to push it back to the requestorQueue
            // This is done by locking the requestorQueueMutex again
            responderLock.lock();
            responderQueue.push_back(stream_cb);
            if (chunk_count > 0) {
                outgoingMessageCV.notify_one();
            }
        }
        else {
            // If the stream is closed, we need to also remove any remainders from the incomingChunks and outgoingChunks
            {
                lock_guard<std::mutex> lock(incomingChunksMutex);
                incomingChunks.erase(stream_cb.first);
            }
            {
                lock_guard<std::mutex> lock(outgoingChunksMutex);
                outgoingChunks.erase(stream_cb.first);
            }
            {
                // TODO: garbage collect old unused return paths
                //lock_guard<std::mutex> lock(returnPathsMutex);
                //returnPaths.erase(stream_cb.first);
            }
            // And by definition, it is not in the requestorQueue anymore
        }
    } else {
        return false;
    }
    return true;
}

void SocketCommunication::listen(const std::string &local_name, const std::string& local_ip_addr, int local_port)
{
    is_server = true;
    auto port_str = std::to_string(local_port);
    // Core execution context required for all Asio I/O
    boost::asio::io_context io_context;
    using endpoint = boost::asio::ip::tcp::endpoint;

    // Acceptor listens for incoming IPv4 connections on port 12345
    boost::asio::ip::tcp::acceptor acceptor(io_context, endpoint(boost::asio::ip::tcp::v4(), local_port));
    std::cout << "Server is listening on port " << local_port << " ..." << std::endl << std::flush;
    // Wait and accept incoming connection
    acceptor.accept(socket);
    std::cout << "Client connected from: " << socket.remote_endpoint() << std::endl;

    recv_thread_ = thread([this]()
                            {
        try {
            Request currentRequest;
            ngtcp2_cid init_cid({1, {1}});
            StreamIdentifier currentSid(init_cid, uint16_t{1});
            while (!terminate_.load()) {
                boost::system::error_code ec;
                array<TypeLength, 1> tl_buffer;
                size_t n = read(socket, boost::asio::buffer(tl_buffer), ec);
                uint64_t payloadSize = asLength(tl_buffer[0]);
                TLVType payloadType = tl_buffer[0].type;

                std::vector<uint8_t> payload(payloadSize);

                boost::asio::read(socket, boost::asio::buffer(payload));
                switch(payloadType) {
                    case TLVType::Scheme:
                        currentRequest.scheme = string(payload.begin(), payload.end());
                        break;
                    case TLVType::Authority:
                        currentRequest.authority = string(payload.begin(), payload.end());
                        break;
                    case TLVType::Path: 
                        currentRequest.path = string(payload.begin(), payload.end());
                        break;
                    case TLVType::Method:
                        currentRequest.method = string(payload.begin(), payload.end());
                        break;
                    case TLVType::Priority:
                        currentRequest.pri = static_cast<decltype(currentRequest.pri)>(payload.front());
                        break;
                    case TLVType::CID:
                        std::memcpy(&currentSid.cid, payload.data(), min(sizeof(currentSid.cid), payloadSize));
                        break;
                    case TLVType::LogicalID:
                        std::memcpy(&currentSid.logical_id, payload.data(), min(sizeof(currentSid.logical_id), payloadSize));
                        break;
                    case TLVType::Chunk:
                        {
                            shared_span chunk(std::span<uint8_t>(payload.data(), payloadSize));
                            route_chunk(currentRequest, currentSid, move(chunk));
                        }
                        break;
                    case TLVType::Data:
                        cerr << "Raw Data grams not implemented. Discarding Data gram of size " << payloadSize << "." << endl << flush;
                        break;
                    default:
                        cerr << "Unrecognized payload type " << toString(payloadType) << endl << flush;
                }
            }
        } catch (std::exception& e) {
            std::cerr << "Server Exception: " << e.what() << std::endl;
        }        
        return EXIT_SUCCESS; 
    });
    send_thread_ = thread([this, local_name, local_ip_addr, local_port]()
                            {
        try {
            while (!terminate_.load()) {
                // TODO: send and recv messages, for
                // size_t length = socket.read_some(boost::asio::buffer(data), error);
                // std::string message(data, length);
                // boost::asio::write(socket, boost::asio::buffer(message), error);
                // if (error == boost::asio::error::eof) {
                //     std::cout << "Client disconnected cleanly." << std::endl;
                // } else if (error) {
                //     throw boost::system::system_error(error);
                // }
            }
        } catch (std::exception& e) {
            std::cerr << "Server Exception: " << e.what() << std::endl;
        }        
        return EXIT_SUCCESS; 
    });
}

void SocketCommunication::close()
{
    terminate_.store(true);
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void SocketCommunication::connect(const std::string &peer_name, const std::string& peer_ip_addr, int peer_port)
{
    auto port_str = std::to_string(peer_port);

    // Resolve the server's address and port string into endpoints
    boost::asio::ip::tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(peer_ip_addr, port_str);

    // Create socket and establish the connection
    boost::asio::connect(socket, endpoints);
    std::cout << "SocketCommunicaton::connect: Connected to server successfully!" << std::endl << std::flush;
    recv_thread_ = thread([this, peer_name, peer_ip_addr, peer_port]() 
                            {
        try {

            while (!terminate_.load()) {
                // TODO: send and recv messages, for example
                // boost::asio::write(socket, boost::asio::buffer(message));
                // boost::system::error_code error;
                // size_t reply_length = socket.read_some(boost::asio::buffer(reply), error);
                // if (!error) {
                //     std::cout << "Reply from server: ";
                //     std::cout.write(reply, reply_length);
                //     std::cout << std::endl;
                // } else {
                //     std::cerr << "Read error: " << error.message() << std::endl;
                // }
                
            }
        } catch (std::exception& e) {
            std::cerr << "Client Exception: " << e.what() << std::endl << std::flush;
        }
        return EXIT_SUCCESS;
    });    
    send_thread_ = thread([this, peer_name, peer_ip_addr, peer_port]() 
                            {
        try {
            while (!terminate_.load()) {
                // TODO: send and recv messages, for example
                // boost::asio::write(socket, boost::asio::buffer(message));
                // boost::system::error_code error;
                // size_t reply_length = socket.read_some(boost::asio::buffer(reply), error);
                // if (!error) {
                //     std::cout << "Reply from server: ";
                //     std::cout.write(reply, reply_length);
                //     std::cout << std::endl;
                // } else {
                //     std::cerr << "Read error: " << error.message() << std::endl;
                // }
                
            }
        } catch (std::exception& e) {
            std::cerr << "Client Exception: " << e.what() << std::endl << std::flush;
        }
        return EXIT_SUCCESS;
    });    
}

void SocketCommunication::route_chunk(Request const &r, StreamIdentifier const &sid, shared_span<> &&chunk)
{

}
