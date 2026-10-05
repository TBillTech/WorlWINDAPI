#pragma once

#include "communication.h"
#include <boost/asio.hpp>

class SocketCommunication : public Communication {
public:
    SocketCommunication(boost::asio::io_context& p_io_context)
        : io_context(p_io_context), socket(io_context), timer(io_context) {
    };
    ~SocketCommunication() override {
        terminate();
    }

    virtual StreamIdentifier getNewRequestStreamIdentifier(Request const &req) override;
    virtual void registerResponseHandler(StreamIdentifier sid, stream_callback_fn cb) override;
    virtual bool hasResponseHandler(StreamIdentifier sid) override;
    virtual void deregisterResponseHandler(StreamIdentifier sid) override;
    virtual bool processRequestStream() override;
    void check_deadline();

    virtual void registerRequestHandler(named_prepare_fn preparer) override;
    virtual void deregisterRequestHandler(std::string preparer_name) override;
    virtual bool processResponseStream() override;

    virtual void listen(const std::string &local_name, const std::string& local_ip_addr, int local_port) override;
    virtual void close() override;
    virtual void connect(const std::string &peer_name, const std::string& peer_ip_addr, int peer_port) override;

    void route_chunk(Request const &r, StreamIdentifier const &sid, shared_span<> &&chunk);

    void receiveSignal(StreamIdentifier const& sid, shared_span<> && signal) {
        lock_guard<mutex> lock(incomingChunksMutex);
        auto outgoing = incomingChunks.find(sid);
        if (outgoing == incomingChunks.end()) {
            auto inserted = incomingChunks.insert(make_pair(sid, chunks()));
            inserted.first->second.emplace_back(std::move(signal)); // Move the signal
        } else {
            outgoing->second.emplace_back(std::move(signal)); // Move the signal
        }
    }

private:
    uint16_t getNextStaticLogicalId() {
        // Increment the static stream ID counter and return the next logical ID
        return static_stream_id_counter.fetch_add(2); // Use odd IDs for static streams
    }

    void terminate() {
        // Set the terminate flag
        terminate_.store(true);

        // Wait for the threads to finish
        if (send_thread_.joinable()) {
            send_thread_.join();
        }
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
    }

    void outgoingMessageWait() {
        std::unique_lock<std::mutex> lock(outgoingChunksMutex);
        outgoingMessageCV.wait(lock, [&] { return hasOutgoingMessages(); });
    }

    bool hasOutgoingMessages() {
        for (auto it = outgoingChunks.begin(); it != outgoingChunks.end(); ++it)
        {
            if (!it->second.empty())
                return true;
        }
        return false;
    }

    std::atomic<uint16_t> static_stream_id_counter = 1; // Start from 1, and use odd IDs for static stream logical Ids

    string server_name;
    boost::asio::io_context &io_context;
    boost::asio::ip::tcp::socket socket;
    boost::asio::ip::tcp::endpoint endpoint;
    boost::asio::steady_timer timer;
    bool timed_out;
    boost::asio::streambuf receive_buffer;

    thread send_thread_;
    thread recv_thread_;
    std::atomic<bool> terminate_ = false;
    string received_so_far;  // This is a buffer for partially read messages
    std::atomic<uint16_t> stream_id_counter = 2; // Start from 2, and use even IDs for client stream logical Ids
    bool is_server = false;

    stream_callbacks requestorQueue;

    named_prepare_fns preparersStack;
    stream_callbacks responderQueue;
    stream_return_paths returnPaths;
    map<Request, StreamIdentifier> staticRequests;

    stream_data_chunks incomingChunks;
    stream_data_chunks outgoingChunks;

    stream_callbacks streamClosingQueue;

    condition_variable outgoingMessageCV;
    mutex requestResolverMutex;
    mutex requestorQueueMutex;
    mutex preparerStackMutex;
    mutex responderQueueMutex;
    mutex returnPathsMutex;
    mutex incomingChunksMutex;
    mutex outgoingChunksMutex;
    mutex streamClosingMutex;

};

enum class TLVType : uint8_t {
    Scheme      = 0x01,
    Authority   = 0x02,
    Path        = 0x03,
    Method      = 0x04,
    Priority    = 0x05, // pri { urgency, inc }
    CID         = 0x06,
    LogicalID   = 0x07,
    Chunk       = 0x08,
    Data        = 0x09,
};

#pragma pack(push, 1)
struct TypeLength {
    TLVType type;
    uint8_t length[7];
};
#pragma pack(pop)

TypeLength toTL(uint8_t type, uint64_t length);

uint64_t asLength(TypeLength const &tl);

inline string toString(TLVType t);