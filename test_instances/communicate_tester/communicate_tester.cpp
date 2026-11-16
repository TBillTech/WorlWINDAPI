#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "communication.h"
#include "shared_chunk.h"
#include "socket_communication.h"

using namespace std;

namespace {

constexpr int tcp_port = 12347;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

void runContractTest(int port) {
    cout << "Testing Communication contract on socket transport" << endl;
    boost::asio::io_context io;
    auto server = make_unique<SocketCommunication>(io);
    auto client = make_unique<SocketCommunication>(io);

    Request first_request{.scheme = "https", .authority = "localhost", .path = "/worlwind/first", .method = "POST", .pri = {0, 0}};
    Request second_request{.scheme = "https", .authority = "localhost", .path = "/worlwind/second", .method = "POST", .pri = {0, 0}};

    bool server_received = false;
    bool client_received = false;
    bool server_handler_called = false;
    const string expected = string("binary\n", 7) + string("\0\x01\x7f", 3);

    auto server_handler = [&server_received, &server_handler_called, &expected](const StreamIdentifier& sid, chunks& request) {
        if (request.empty()) {
            return chunks{};
        }
        server_handler_called = true;
        chunks response;
        for (auto& chunk : request) {
            require(chunk.get_signal_type() == payload_chunk_header::GLOBAL_SIGNAL_TYPE,
                    "server callback lost the payload chunk header");
            auto signal = chunk.get_signal<payload_chunk_header>().signal;
            if (signal == payload_chunk_header::SIGNAL_HEARTBEAT) {
                continue;
            }
            require(signal == payload_chunk_header::SIGNAL_WORLWIND_REQUEST_CONTINUE,
                    "server callback received the wrong payload signal");
            require(string(chunk.begin<const char>(), chunk.end<const char>()) == expected,
                    "server callback received corrupted binary payload");
            server_received = true;
            response.emplace_back(payload_chunk_header(sid.logical_id, payload_chunk_header::SIGNAL_WORLWIND_RESPONSE_CONTINUE, 3),
                                  span<const char>("ack", 3));
        }
        return response;
    };

    prepare_stream_callback_fn prepare = [&server_handler](const Request&) {
        return make_pair(uri_response_info{true, true, false, 0}, server_handler);
    };
    server->registerRequestHandler(make_pair("contract", prepare));
    // SocketCommunication::listen blocks in accept until a client connects.
    exception_ptr listen_error;
    thread listen_thread([&]() {
        try {
            server->listen("localhost", "127.0.0.1", port);
        } catch (...) {
            listen_error = current_exception();
        }
    });
    this_thread::sleep_for(chrono::milliseconds(100));
    try {
        client->connect("localhost", "127.0.0.1", port);
    } catch (...) {
        listen_thread.detach();
        throw;
    }
    struct CloseConnections {
        SocketCommunication& client;
        SocketCommunication& server;
        ~CloseConnections() {
            client.close();
            server.close();
        }
    } close_connections{*client, *server};
    listen_thread.join();
    if (listen_error) {
        rethrow_exception(listen_error);
    }
    this_thread::sleep_for(chrono::milliseconds(100));
    auto first_id = client->getNewRequestStreamIdentifier(first_request);
    cout << "First Id is: " << first_id << endl << flush;
    auto second_id = client->getNewRequestStreamIdentifier(second_request);
    cout << "Second Id is: " << second_id << endl << flush;
    require(!(first_id == second_id), "getNewRequestStreamIdentifier returned a duplicate stream identifier");

    bool request_sent = false;
    bool heartbeat_sent = false;
    auto client_handler = [&](const StreamIdentifier& sid, chunks& response) {
        for (auto& chunk : response) {
            require(chunk.get_signal_type() == payload_chunk_header::GLOBAL_SIGNAL_TYPE,
                    "client callback lost the response payload header");
            require(chunk.get_signal<payload_chunk_header>().signal == payload_chunk_header::SIGNAL_WORLWIND_RESPONSE_CONTINUE,
                    "client callback received the wrong response signal");
            require(string(chunk.begin<const char>(), chunk.end<const char>()) == "ack",
                    "client callback received a corrupted response");
            client_received = true;
        }
        if (!request_sent) {
            request_sent = true;
            chunks request;
            request.emplace_back(payload_chunk_header(sid.logical_id, payload_chunk_header::SIGNAL_WORLWIND_REQUEST_CONTINUE,
                                                       static_cast<uint16_t>(expected.size())),
                                 span<const char>(expected.data(), expected.size()));
            return request;
        }
        if (server_received && !client_received && !heartbeat_sent) {
            heartbeat_sent = true;
            chunks heartbeat;
            heartbeat.emplace_back(payload_chunk_header(sid.logical_id, payload_chunk_header::SIGNAL_HEARTBEAT, 0),
                                   span<const char>("", 0));
            return heartbeat;
        }
        return chunks{};
    };
    client->registerResponseHandler(first_id, client_handler);
    require(client->hasResponseHandler(first_id), "registered response handler was not discoverable");
    client->deregisterResponseHandler(first_id);
    require(!client->hasResponseHandler(first_id), "deregistered response handler remained discoverable");
    client->registerResponseHandler(first_id, client_handler);

    for (int attempt = 0; attempt < 100 && !(server_received && client_received); ++attempt) {
        try {
            client->processRequestStream();
            server->processResponseStream();
        } catch (const exception& error) {
            throw runtime_error(string("Socket contract failure: ") + error.what());
        }
        this_thread::sleep_for(chrono::milliseconds(20));
    }
    require(server_handler_called, "server request handler was never called");
    require(server_received, "server did not receive the request");
    require(client_received, "client did not receive the response");
}

} // namespace

int main() {
    int result = 0;
    auto run = [&result](int port) {
        try {
            runContractTest(port);
        } catch (const exception& error) {
            cerr << "Socket: " << error.what() << endl;
            result = 1;
        }
    };
    run(tcp_port);
    if (result == 0) {
        cout << "Communication contract tests passed" << endl;
    }
    return result;
}