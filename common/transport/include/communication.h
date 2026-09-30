#pragma once
#include <string>
#include <cstring> // Add this line to include the memcmp function
#include <vector>
#include <functional>
#include <cstdint>
#include <iostream>
#include <map>
#include <span>
#include <utility>

#include "shared_chunk.h"
#include "request.h"

using namespace std;
// Server and Client Connection IDs:
//
// The server primarily uses the dcid (Destination Connection ID) to identify incoming connections.
// The client primarily uses the scid (Source Connection ID) to identify itself to the server.
//
// Handler Objects:
//
// There is one Handler object per dcid. The Handler object manages the overall connection associated with that dcid.
//
// Stream Objects:
//
// There is a Stream object for each logical_id within a connection. Each Handler object manages multiple Stream objects, one for each active stream within the connection.
//
// Here is a summary of how these components interact:
//
// When a new connection is initiated, a Handler object is created and associated with the dcid.
// The Handler object manages the connection and handles incoming packets, delegating stream-specific tasks to Stream objects.
// Each Stream object is responsible for handling the data and state associated with a specific stream within the connection.
//
// This design allows the server to efficiently manage multiple connections and streams, with each Handler object managing a single connection and each Stream object managing a single stream within that connection.

const size_t NGTCP2_MAX_CIDLEN = 8; 

struct ngtcp2_cid {
    size_t datalen = 0;
    uint8_t data[NGTCP2_MAX_CIDLEN] = {0,0,0,0,0,0,0,0};
};

ostream &operator<<(ostream &os, const ngtcp2_cid &cid);
bool operator<=(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B);
bool operator>=(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B);
bool operator==(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B);
bool operator<(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B);
bool operator>(ngtcp2_cid const &cid_A, ngtcp2_cid const &cid_B);

class StreamIdentifier {
    public:
    StreamIdentifier(ngtcp2_cid cid, uint16_t logical_id) : cid(cid), logical_id(logical_id) {};
    StreamIdentifier(ngtcp2_cid cid, int64_t stream_id) = delete;
    StreamIdentifier(StreamIdentifier const &other) : cid(other.cid), logical_id(other.logical_id) {};

    StreamIdentifier& operator=(const StreamIdentifier& other) {
        if (this != &other) {
            cid = other.cid;
            logical_id = other.logical_id;
        }
        return *this;
    }        
    // cid is not needed for client side, since the client will know the scid already, 
    // but is important for server side to identify the handler object associated with the connection.
    ngtcp2_cid cid; 
    // logical_id identifies which stream in the connection defines this request;
    uint16_t logical_id;


    bool operator<(const StreamIdentifier& other) const {
        if (cid < other.cid) {
            return true;
        }
        if (other.cid < cid) {
            return false;
        }
        return logical_id < other.logical_id;
    }

    bool operator==(const StreamIdentifier& other) const {
        return cid == other.cid && logical_id == other.logical_id;
    }

    friend std::ostream& operator<<(std::ostream& os, const StreamIdentifier& si) {
        os << "StreamIdentifier: cid=" << si.cid << ", logical_id=" << si.logical_id;
        return os;
    }
};

// The stream callback function is generally used by both sides to process incoming and outgoing data for bidirectional streams.
using stream_callback_fn = std::function<chunks(const StreamIdentifier&, chunks&)>;

// On Client side, the initial outgoing request is first logged into the requestResolutionQueue (because it has no response callbacks yet)
// And because the StreamIdentifier is not yet known.
typedef std::pair<Request, stream_callback_fn> RequestCallback;
// RequestCallback Vector needs to sort by the Request object, so redirect < operator on RequestCallback to Request:
inline bool operator<(RequestCallback& lhs, RequestCallback& rhs) {
    return lhs.first < rhs.first;
}
typedef std::vector<RequestCallback> request_resolutions;

// And for processing of the stream data, the StreamIdentifier is attached to a callback function
typedef std::pair<StreamIdentifier, stream_callback_fn> stream_callback;
struct uri_response_info {
    bool can_handle;
    bool is_live_stream;
    bool is_file;
    size_t dyn_length;
};

typedef std::pair<uri_response_info, stream_callback_fn> prepared_stream_callback;

// While a round robin queue for load balancing is done (for now) with a simple vector.
typedef std::vector<stream_callback> stream_callbacks;

// Also, it is necessary to map StreamIdentifiers to actually received/sent data chunks to the stream for it.
// And prepared but not yet sent-on-the-wire data chunks can use the same mapping.
typedef std::map<StreamIdentifier, chunks> stream_data_chunks;

// Define a callback function type for preparing stream callbacks
using prepare_stream_callback_fn = std::function<prepared_stream_callback(const Request &)>;
typedef std::pair<std::string, prepare_stream_callback_fn> named_prepare_fn;
typedef std::vector<named_prepare_fn> named_prepare_fns;

// Track the return path for a StreamIdentifier
typedef std::map<StreamIdentifier, Request> stream_return_paths;

class Communication {
public:
    // Here are the 1 to N relationships between URL/Requests, StreamIdentifiers, and Streams:
    // Each URL/Request may map to a single Stream object, but this is a _temporary_ relationship.  
    //    Thus, it is the connection (ngtcp2_cid) Handler objects that track the URL/Request to Stream object. 
    // Each ngtcp2_cid maps to a connection (ngtcp2_cid) Handler object.
    // Each StreamIdentifier is "owned" by the connection (ngtcp2_cid) Handler that matches its cid
    // Each StreamIdentifier also maps to one of the URL/Request
    // Thus, either the StreamIdentifier maps to a single open Stream, 
    //    OR the StreamIdentifier maps to a single URL/Request that can be used to open a new Stream.
    
    // NOTE: This means that sometimes the client will need to send a flush message to the server, which can just be a heartbeat signal. 

    // ## "client" side communication model: worker threads + HTTP3 IO thread
    // worker thread gets a StreamIdentifier from the Communication object by requesting a new StreamIdentifier for a URL/Request
    // worker thread calls setupRequestCallback, which adds a mapping from the StreamIdentifier to the callback.
    // worker thread calls processRequestStream, which handles incoming and outgoing chunks as arguments and returns to the callback.
    // HTTP3 IO thread checks if there are any outgoing chunks to send for a URL/Request and no open stream for it,
    //    and if so, HTTP3 IO thread creates a new open stream.
    // HTTP3 IO thread sends any outgoing chunks over the now opened stream.  IF all outgoing chunks have been sent, then also send FIN.
    // HTTP3 IO thread receives bytes from open streams, and puts them on the incoming chunk queue.
    // Thus the following queues and maps are needed:
    // 1. stream_callbacks requestorQueue: For mapping StreamIdentifiers to callbacks
    // 2. stream_data_chunks incomingChunks: For mapping incoming chunks to StreamIdentifiers
    // 3. stream_data_chunks outgoingChunks: For mapping outgoing chunks to StreamIdentifiers
    virtual StreamIdentifier getNewRequestStreamIdentifier(Request const &req) = 0;
    virtual void registerResponseHandler(StreamIdentifier sid, stream_callback_fn cb) = 0;
    virtual bool hasResponseHandler(StreamIdentifier sid) = 0;
    virtual void deregisterResponseHandler(StreamIdentifier sid) = 0;
    // In order to separate the nominally client side handler threads from the Communication IO thread, a handler thread can call 
    // this function to make the callback from a worker thread and take as much time as necessary without interrupting the 
    // Communication IO.
    virtual bool processRequestStream() = 0;

    // ## "server" side communication model
    // worker thread calls registerRequestHandler to support associated the right callback with a new stream object;
    //     This in effect maps an incoming URL/Request to a callback function.
    // HTTP3 IO thread receives request bytes from the wire, and creates a new Stream object to handle the request.  
    //     Note that since this ULR/Request is in the headers, the Stream object can immediately be associated with a callback.
    // For open streams, HTTP3 IO thread receives request chunks for an open stream, and puts them on the incoming chunk queue (until client FIN).
    //     Note that only after FIN can the StreamIdentifier be read from the request stream, and so after FIN is when the responderQueue is populated.
    //     This further means that the communication (ngtcp2_cid) handler needs to keep track of the which StreamIdentifiers
    //     map to which URL/Request.
    // For open streams, HTTP3 IO thread checks if the FIN has arrived,
    //     and if so sends outgoing chunks to the stream.  If all outgoing chunks have been sent, then also send FIN.
    // worker thread calls processResponseStream, which pops the requestHandler from the responderQueue, 
    //     and processed incoming to outgoing chunks through the callback for the requestHandler.
    // Thus the following queues and maps are needed:
    // 1. stream_callbacks responderQueue: For mapping StreamIdentifiers to callbacks
    // 2. stream_data_chunks incomingChunks: For mapping incoming chunks to StreamIdentifiers
    // 3. stream_data_chunks outgoingChunks: For mapping outgoing chunks to StreamIdentifiers
    // 4. named_prepare_fns preparersStack: For mapping URL/Request to callbacks
    // 5. stream_return_paths returnPaths: For mapping StreamIdentifiers to URL/Requests. 
    virtual void registerRequestHandler(named_prepare_fn preparer) = 0;
    virtual void deregisterRequestHandler(std::string preparer_name) = 0;
    // In order to separate the nominally server side handler threads from the Communication IO thread, a handler thread can call 
    // this function to make the callback from a worker thread and take as much time as necessary without interrupting the 
    // Communication IO.
    virtual bool processResponseStream() = 0;

    // send and receive deal with data coming in and going out on the streams
    // virtual void send() = 0;
    // virtual void receive() = 0;

    // Listen is for servicing the protocol server side.
    virtual void listen(const std::string &local_name, const std::string& local_ip_addr, int local_port) = 0;
    // Close is for shutting down the protocol thread.
    virtual void close() = 0;
    // Connect is for servicing the protocol client side.
    virtual void connect(const std::string &peer_name, const std::string& peer_ip_addr, int peer_port) = 0;
    virtual ~Communication() = default;
};