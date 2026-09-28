#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <yaml-cpp/yaml.h>
#include "communication.h"
#include "shared_chunk.h"
#include "tcp_communication.h"

#if WWATP_USE_QUIC_TRANSPORT
#include "quic_connector.h"
#include "quic_listener.h"
#endif

using namespace std;

namespace {

constexpr int tcp_port = 12347;
constexpr int quic_port = 12348;

void require(bool condition, const string& message) {
    if (!condition) {
        throw runtime_error(message);
    }
}

YAML::Node quicConfig() {
    YAML::Node config;
    config["private_key_file"] = "../test_instances/data/private_key.pem";
    config["cert_file"] = "../test_instances/data/cert.pem";
    config["quiet"] = true;
    config["send_trailers"] = false;
    config["log_path"] = "../test_instances/sandbox/";
    return config;
}

unique_ptr<Communication> makeServer(const string& protocol, boost::asio::io_context& io) {
#if WWATP_USE_QUIC_TRANSPORT
    if (protocol == "QUIC") {
        return make_unique<QuicListener>(io, quicConfig());
    }
#endif
    require(protocol == "TCP", "TCP transport is unavailable");
    return make_unique<TcpCommunication>(io);
}

unique_ptr<Communication> makeClient(const string& protocol, boost::asio::io_context& io) {
#if WWATP_USE_QUIC_TRANSPORT
    if (protocol == "QUIC") {
        return make_unique<QuicConnector>(io, quicConfig());
    }
#endif
    require(protocol == "TCP", "TCP transport is unavailable");
    return make_unique<TcpCommunication>(io);
}

void runContractTest(const string& protocol, int port) {
    cout << "Testing Communication contract: " << protocol << endl;
    boost::asio::io_context io;
    auto server = makeServer(protocol, io);
    auto client = makeClient(protocol, io);

    Request first_request{.scheme = "https", .authority = "localhost", .path = "/first", .method = "POST", .pri = {0, 0}};
    Request second_request{.scheme = "https", .authority = "localhost", .path = "/second", .method = "POST", .pri = {0, 0}};
    auto first_id = client->getNewRequestStreamIdentifier(first_request);
    auto second_id = client->getNewRequestStreamIdentifier(second_request);
    require(!(first_id == second_id), "getNewRequestStreamIdentifier returned a duplicate stream identifier");

    bool server_received = false;
    bool client_received = false;
    bool server_handler_called = false;
    const string expected = string("binary\n", 7) + string("\0\x01\x7f", 3);

    auto server_handler = [&server_received, &server_handler_called, &expected](const StreamIdentifier& sid, chunks& request) {
        server_handler_called = true;
        require(!request.empty(), "server callback received no chunks");
        require(request.front().get_signal_type() == payload_chunk_header::GLOBAL_SIGNAL_TYPE,
                "server callback lost the payload chunk header");
        require(request.front().get_signal<payload_chunk_header>().signal == payload_chunk_header::SIGNAL_WWATP_REQUEST_CONTINUE,
                "server callback received the wrong payload signal");
        require(string(request.front().begin<const char>(), request.front().end<const char>()) == expected,
                "server callback received corrupted binary payload");
        server_received = true;
        chunks response;
        response.emplace_back(payload_chunk_header(sid.logical_id, payload_chunk_header::SIGNAL_WWATP_RESPONSE_CONTINUE, 0),
                              span<const char>("ack", 3));
        response.front().get_signal<payload_chunk_header>().data_length = 3;
        return response;
    };

    prepare_stream_callback_fn prepare = [&server_handler](const Request&) {
        return make_pair(uri_response_info{true, true, false, 0}, server_handler);
    };
    server->registerRequestHandler(make_pair("contract", prepare));
    server->listen("localhost", "127.0.0.1", port);
    this_thread::sleep_for(chrono::milliseconds(100));
    client->connect("localhost", "127.0.0.1", port);
    this_thread::sleep_for(chrono::milliseconds(100));

    auto client_handler = [&client_received](const StreamIdentifier&, chunks& response) {
        require(!response.empty(), "client callback received no response chunks");
        require(response.front().get_signal_type() == payload_chunk_header::GLOBAL_SIGNAL_TYPE,
                "client callback lost the response payload header");
        require(response.front().get_signal<payload_chunk_header>().signal == payload_chunk_header::SIGNAL_WWATP_RESPONSE_CONTINUE,
                "client callback received the wrong response signal");
        require(string(response.front().begin<const char>(), response.front().end<const char>()) == "ack",
                "client callback received a corrupted response");
        client_received = true;
        return chunks{};
    };
    client->registerResponseHandler(first_id, client_handler);
    require(client->hasResponseHandler(first_id), "registered response handler was not discoverable");
    client->deregisterResponseHandler(first_id);
    require(!client->hasResponseHandler(first_id), "deregistered response handler remained discoverable");
    client->registerResponseHandler(first_id, client_handler);

    chunks request;
    request.emplace_back(payload_chunk_header(first_id.logical_id, payload_chunk_header::SIGNAL_WWATP_REQUEST_CONTINUE,
                                               static_cast<uint16_t>(expected.size())),
                         span<const char>(expected.data(), expected.size()));
    // The callback is the contract's producer; the first invocation creates the request.
    bool request_sent = false;
    client->deregisterResponseHandler(first_id);
    client->registerResponseHandler(first_id, [&request_sent, &request](const StreamIdentifier&, chunks&) {
        if (!request_sent) {
            request_sent = true;
            return request;
        }
        return chunks{};
    });

    for (int attempt = 0; attempt < 100 && !(server_received && client_received); ++attempt) {
        try {
            client->processRequestStream();
            server->processResponseStream();
        } catch (const exception& error) {
            client->close();
            server->close();
            throw runtime_error(protocol + " contract failure: " + error.what());
        }
        this_thread::sleep_for(chrono::milliseconds(20));
    }
    client->close();
    server->close();
    require(server_handler_called, "server request handler was never called");
    require(server_received && client_received, "request/response exchange did not complete");
}

} // namespace

int main() {
    int result = 0;
    auto run = [&result](const string& protocol, int port) {
        try {
            runContractTest(protocol, port);
        } catch (const exception& error) {
            cerr << protocol << ": " << error.what() << endl;
            result = 1;
        }
    };
    run("TCP", tcp_port);
#if WWATP_USE_QUIC_TRANSPORT
    run("QUIC", quic_port);
#endif
    if (result == 0) {
        cout << "Communication contract tests passed" << endl;
    }
    return result;
}