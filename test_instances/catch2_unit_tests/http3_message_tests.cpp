#include <catch2/catch_all.hpp>
#include "http3_tree_message.h"
#include "backend_testbed.h"

using namespace std;
using namespace fplus;

template <typename encoded_type>
void test_encode_decode_request(uint16_t request_id, const encoded_type& obj,
    HTTP3TreeMessage& client_request,
    HTTP3TreeMessage& server_request,
    function<void(HTTP3TreeMessage&, const encoded_type&)> encode_func,
    function<encoded_type(HTTP3TreeMessage&)> decode_func) {
    encode_func(client_request, obj);
    REQUIRE(client_request.isInitialized());
    client_request.setRequestId(request_id);
    REQUIRE(client_request.isRequestComplete());
    auto m_chunk = client_request.popRequestChunk();
    while(m_chunk.is_just()) {
        REQUIRE(server_request.isRequestComplete() == false);
        server_request.pushRequestChunk(m_chunk.unsafe_get_just());
        m_chunk = client_request.popRequestChunk();
    }
    REQUIRE(server_request.isRequestComplete());

    auto decoded = decode_func(server_request);
    REQUIRE(decoded == obj);
}

template <typename encoded_type>
void test_encode_decode_response(const encoded_type& obj,
    HTTP3TreeMessage& client_response,
    HTTP3TreeMessage& server_response,
    function<void(HTTP3TreeMessage&, const encoded_type&)> encode_func,
    function<encoded_type(HTTP3TreeMessage&)> decode_func) {
    REQUIRE(server_response.isResponseComplete() == false);
    encode_func(server_response, obj);
    REQUIRE(server_response.isResponseComplete());
    auto m_chunk = server_response.popResponseChunk();
    while(m_chunk.is_just()) {
        REQUIRE(client_response.isResponseComplete() == false);
        client_response.pushResponseChunk(m_chunk.unsafe_get_just());
        m_chunk = server_response.popResponseChunk();
    }
    REQUIRE(client_response.isRequestComplete());
    REQUIRE(server_response.isProcessingFinished());

    auto decoded = decode_func(client_response);
    REQUIRE(decoded == obj);
    REQUIRE(client_response.isProcessingFinished());
}

template <typename request_type, typename response_type>
void messageCycleTest(uint16_t request_id, request_type const& request_obj, 
        response_type const& response_obj, 
        function<void(HTTP3TreeMessage&, const request_type&)> encode_request_func,
        function<request_type(HTTP3TreeMessage&)> decode_request_func,
        function<void(HTTP3TreeMessage&, const response_type&)> encode_response_func,
        function<response_type(HTTP3TreeMessage&)> decode_response_func,
        uint8_t request_signal, uint8_t response_signal)
{
    HTTP3TreeMessage client_message;
    HTTP3TreeMessage server_message;

    test_encode_decode_request(request_id, request_obj, client_message, server_message,
        encode_request_func, decode_request_func);

    REQUIRE(client_message.getSignal() == request_signal);
    REQUIRE(server_message.getRequestId() == request_id);
    
    test_encode_decode_response(response_obj, client_message, server_message,
        encode_response_func, decode_response_func);
    REQUIRE(server_message.getSignal() == response_signal);
}


TEST_CASE("HTTP3TreeMessage encode and decode getNodeRequest", "[http3_tree_message]") {
    auto nodeRequestTest = [](uint16_t request_id, const std::string& request_obj, const fplus::maybe<TreeNode>& response_obj) {
        using request_type = std::string;
        using response_type = fplus::maybe<TreeNode>;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_GET_NODE_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_GET_NODE_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_getNodeRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_getNodeRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_getNodeResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_getNodeResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;
    std::string label_rule = "test_label";

    auto anAnimal = createAnimalNode("Seal", "A marine mammal", {{"txt", ""}, {"txt", ""}}, 
    {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
    "How to query seal", "Seal QA sequence");

    nodeRequestTest(request_id++, label_rule, fplus::just(anAnimal));
    nodeRequestTest(request_id++, "", fplus::just(anAnimal));
    nodeRequestTest(request_id++, "/lion/cub_1", fplus::nothing<TreeNode>());
    nodeRequestTest(request_id++, "https://example.com/path/to/resource?query=param#fragment", fplus::just(anAnimal));
}

TEST_CASE("HTTP3TreeMessage encode and decode upsertNodeRequest", "[http3_tree_message]") {
    auto nodeUpsertTest = [](uint16_t request_id, const std::vector<TreeNode>& request_obj, bool response_obj) {
        using request_type = std::vector<TreeNode>;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_upsertNodeRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_upsertNodeRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_upsertNodeResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_upsertNodeResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", {{"txt", ""}, {"txt", ""}}, 
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query seal", "Seal QA sequence");

    nodeUpsertTest(request_id++, {complex_node}, true);
    nodeUpsertTest(request_id++, {simpleAnimal}, true);
    nodeUpsertTest(request_id++, {simpleAnimal, complex_node, anAnimal}, true);
    nodeUpsertTest(request_id++, {}, false);
    nodeUpsertTest(request_id++, {simpleAnimal}, false);
}

TEST_CASE("HTTP3TreeMessage encode and decode deleteNodeRequest", "[http3_tree_message]") {
    auto nodeDeleteTest = [](uint16_t request_id, const std::string& request_obj, bool response_obj) {
        using request_type = std::string;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_DELETE_NODE_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_deleteNodeRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_deleteNodeRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_deleteNodeResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_deleteNodeResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    nodeDeleteTest(request_id++, "test_label", true);
    nodeDeleteTest(request_id++, "", true);
    nodeDeleteTest(request_id++, "/lion/cub_1", false);
    nodeDeleteTest(request_id++, "https://example.com/path/to/resource?query=param#fragment", true);
}

TEST_CASE("HTTP3TreeMessage encode and decode getPageTreeRequest", "[http3_tree_message]") {
    auto pageTreeRequestTest = [](uint16_t request_id, const std::string& request_obj, const std::vector<TreeNode>& response_obj) {
        using request_type = std::string;
        using response_type = std::vector<TreeNode>;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_GET_PAGE_TREE_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_getPageTreeRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_getPageTreeRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_getPageTreeResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_getPageTreeResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}}, 
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", {{"txt", ""}, {"txt", ""}}, 
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query seal", "Seal QA sequence");

    pageTreeRequestTest(request_id++, "https://example.com/path/to/animal_page", {anAnimal});
    pageTreeRequestTest(request_id++, "test_page", {complex_node, simpleAnimal, anAnimal});
    pageTreeRequestTest(request_id++, "", {complex_node, simpleAnimal, anAnimal});
    pageTreeRequestTest(request_id++, "https://example.com/path/to/resource?query=param#fragment", {complex_node, simpleAnimal, anAnimal});
    pageTreeRequestTest(request_id++, "/lion/cub_1", {complex_node, simpleAnimal, anAnimal});
    pageTreeRequestTest(request_id++, "non_existent_page", {});
    pageTreeRequestTest(request_id++, "https://example.com/path/to/non_existent_page", {});
    pageTreeRequestTest(request_id++, "https://example.com/path/to/empty_page", {});
}

TEST_CASE("HTTP3TreeMessage encode and decode getQueryNodesRequest", "[http3_tree_message]") {
    auto queryNodesRequestTest = [](uint16_t request_id, const std::string& request_obj, const std::vector<TreeNode>& response_obj) {
        using request_type = std::string;
        using response_type = std::vector<TreeNode>;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_QUERY_NODES_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_getQueryNodesRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_getQueryNodesRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_getQueryNodesResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_getQueryNodesResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", 
        {{"txt", "dossier_1"}, {"txt", "dossier_2"}}, 
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query seal", "Seal QA sequence");
    queryNodesRequestTest(request_id++, "test_page", {anAnimal, complex_node});
    queryNodesRequestTest(request_id++, "https://example.com/path/to/lion?query=param#fragment", {anAnimal, complex_node, simpleAnimal});
    queryNodesRequestTest(request_id++, "/Lion/Pup_1", {anAnimal, complex_node, simpleAnimal});
    queryNodesRequestTest(request_id++, "Sponge", {anAnimal, complex_node, simpleAnimal});
    queryNodesRequestTest(request_id++, "non_existent_query", {});
    queryNodesRequestTest(request_id++, "https://example.com/path/to/non_existent_query", {});
    queryNodesRequestTest(request_id++, "", {complex_node});
}

TEST_CASE("HTTP3TreeMessage encode and decode openTransactionLayerRequest", "[http3_tree_message]") {
    auto openTransactionLayerTest = [](uint16_t request_id, const TreeNode& request_obj, bool response_obj) {
        using request_type = TreeNode;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_openTransactionLayerRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_openTransactionLayerRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_openTransactionLayerResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_openTransactionLayerResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", 
        {{"txt", "dossier_1"}, {"txt", "dossier_2"}}, 
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query seal", "Seal QA sequence");

    openTransactionLayerTest(request_id++, complex_node, true);
    openTransactionLayerTest(request_id++, simpleAnimal, true);
    openTransactionLayerTest(request_id++, anAnimal, false);
}

TEST_CASE("HTTP3TreeMessage encode and decode closeTransactionLayersRequest", "[http3_tree_message]") {
    auto closeTransactionLayersTest = [](uint16_t request_id, bool request_obj, bool response_obj) {
        using request_type = bool; // bool type just for making the lambda compile
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_closeTransactionLayersRequest(); };
        auto decode_request_func = 
            [&request_obj](HTTP3TreeMessage & server_message) { 
                server_message.decode_closeTransactionLayersRequest();
                return request_obj; // mock up returning the request_obj
            };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_closeTransactionLayersResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_closeTransactionLayersResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    closeTransactionLayersTest(request_id++, true, true);
    closeTransactionLayersTest(request_id++, true, false);
}

TEST_CASE("HTTP3TreeMessage encode and decode applyTransactionRequest", "[http3_tree_message]") {
    auto applyTransactionTest = [](uint16_t request_id, const Transaction& request_obj, bool response_obj) {
        using request_type = Transaction;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_APPLY_TRANSACTION_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_applyTransactionRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_applyTransactionRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_applyTransactionResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_applyTransactionResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;
        
    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    NewNodeVersion emptiest_version({fplus::maybe<uint16_t>(), {"", fplus::maybe<TreeNode>()}});
    NewNodeVersion nolabel_version({fplus::maybe<uint16_t>(10), {"", fplus::maybe<TreeNode>()}});
    NewNodeVersion emptier_version({fplus::maybe<uint16_t>(), {"/lion/cub_1", fplus::maybe<TreeNode>()}});
    NewNodeVersion empty_version({fplus::maybe<uint16_t>(0), {"/lion/cub_1", fplus::maybe<TreeNode>()}});

    NewNodeVersion simple_version({fplus::maybe<uint16_t>(13), {simpleAnimal.getLabelRule(), fplus::maybe<TreeNode>(simpleAnimal)}});
    NewNodeVersion complex_version({fplus::maybe<uint16_t>(12233), {complex_node.getLabelRule(), fplus::maybe<TreeNode>(complex_node)}});

    SubTransaction empty_sub_transaction(emptiest_version, {});

    SubTransaction simple_sub_transaction(empty_version, {emptiest_version});

    SubTransaction basic_sub_transaction(simple_version, {emptier_version, simple_version, empty_version, simple_version});

    SubTransaction complex_sub_transaction(complex_version, {emptier_version, simple_version, empty_version, simple_version, complex_version});

    Transaction empty_transaction({});
    Transaction simple_transaction({empty_sub_transaction});
    Transaction basic_transaction({basic_sub_transaction, simple_sub_transaction});
    Transaction complex_transaction({empty_sub_transaction, simple_sub_transaction, basic_sub_transaction, complex_sub_transaction});

    applyTransactionTest(request_id++, empty_transaction, true);
    applyTransactionTest(request_id++, simple_transaction, true);
    applyTransactionTest(request_id++, basic_transaction, true);
    applyTransactionTest(request_id++, complex_transaction, true);
    applyTransactionTest(request_id++, Transaction(), false);
    applyTransactionTest(request_id++, empty_transaction, false);
    applyTransactionTest(request_id++, simple_transaction, false);
}

TEST_CASE("HTTP3TreeMessage encode and decode getFullTreeRequest", "[http3_tree_message]") {
    auto fullTreeRequestTest = [](uint16_t request_id, bool request_obj, const std::vector<TreeNode>& response_obj) {
        using request_type = bool; // No request data for this operation
        using response_type = std::vector<TreeNode>;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_GET_FULL_TREE_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_getFullTreeRequest(); };
        auto decode_request_func = 
            [&request_obj](HTTP3TreeMessage & server_message) { 
                server_message.decode_getFullTreeRequest();
                return request_obj; // mock up returning the request_obj
            };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_getFullTreeResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_getFullTreeResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", 
        {{"txt", "dossier_1"}, {"txt", "dossier_2"}},
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"},
        "How to query seal", "Seal QA sequence");
    fullTreeRequestTest(request_id++, true, {anAnimal, complex_node, simpleAnimal});
    fullTreeRequestTest(request_id++, true, {complex_node});
    fullTreeRequestTest(request_id++, true, {anAnimal});
    fullTreeRequestTest(request_id++, true, {});
}

TEST_CASE("HTTP3TreeMessage encode and decode registerNodeListenerRequest", "[http3_tree_message]") {
    auto registerNodeListenerTest = [](uint16_t request_id, tuple<string, string, bool> request_obj, bool response_obj) {
        using request_type = tuple<string, string, bool>;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_REGISTER_LISTENER_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_registerNodeListenerRequest(get<0>(request_obj), get<1>(request_obj), get<2>(request_obj)); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_registerNodeListenerRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_registerNodeListenerResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_registerNodeListenerResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    registerNodeListenerTest(request_id++, make_tuple("listener1", "test_label", true), true);
    registerNodeListenerTest(request_id++, make_tuple("listener2", "", false), true);
    registerNodeListenerTest(request_id++, make_tuple("", "test_label", true), false);
    registerNodeListenerTest(request_id++, make_tuple("listener3", "https://example.com/path/to/resource?query=param#fragment", true), true);
}

TEST_CASE("HTTP3TreeMessage encode and decode deregisterNodeListenerRequest", "[http3_tree_message]") {
    auto deregisterNodeListenerTest = [](uint16_t request_id, const pair<string, string>& request_obj, bool response_obj) {
        using request_type = pair<string, string>;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_DEREGISTER_LISTENER_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_deregisterNodeListenerRequest(request_obj.first, request_obj.second); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_deregisterNodeListenerRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_deregisterNodeListenerResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_deregisterNodeListenerResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    deregisterNodeListenerTest(request_id++, make_pair("listener1", "test_label"), true);
    deregisterNodeListenerTest(request_id++, make_pair("listener2", ""), true);
    deregisterNodeListenerTest(request_id++, make_pair("", "test_label"), false);
    deregisterNodeListenerTest(request_id++, make_pair("listener3", "https://example.com/path/to/resource?query=param#fragment"), true);
}

TEST_CASE("HTTP3TreeMessage encode and decode notifyListenersRequest", "[http3_tree_message]") {
    auto notifyListenersTest = [](uint16_t request_id, pair<string, maybe<TreeNode>> const& request_obj, bool response_obj) {
        using request_type = pair<string, maybe<TreeNode>>;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_NOTIFY_LISTENERS_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_notifyListenersRequest(request_obj.first, request_obj.second); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_notifyListenersRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_notifyListenersResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_notifyListenersResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    auto anAnimal = createAnimalNode("Seal", "A marine mammal", 
        {{"txt", "dossier_1"}, {"txt", "dossier_2"}},
        {1, 1}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"},
        "How to query seal", "Seal QA sequence");
    notifyListenersTest(request_id++, {"listener1", maybe<TreeNode>(anAnimal)}, true);
    notifyListenersTest(request_id++, make_pair("listener2", maybe<TreeNode>(complex_node)), true);
    notifyListenersTest(request_id++, make_pair("listener3", maybe<TreeNode>()), false);
    notifyListenersTest(request_id++, make_pair("", maybe<TreeNode>(simpleAnimal)), true);
    notifyListenersTest(request_id++, make_pair("", maybe<TreeNode>()), true);
}

TEST_CASE("HTTP3TreeMessage encode and decode processNotificationRequest", "[http3_tree_message]") {
    auto processNotificationTest = [](uint16_t request_id, bool request_obj, bool response_obj) {
        using request_type = bool;
        using response_type = bool;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_PROCESS_NOTIFICATION_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_processNotificationRequest(); };
        auto decode_request_func = 
            [&request_obj](HTTP3TreeMessage & server_message) { 
                server_message.decode_processNotificationRequest();
                return request_obj; // mock up returning the request_obj
            };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_processNotificationResponse(); };
        auto decode_response_func = 
            [&response_obj](HTTP3TreeMessage & client_message) { 
                client_message.decode_processNotificationResponse(); 
                return response_obj; // mock up returning the response_obj
            };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    SequentialNotification empty(0, Notification("", fplus::maybe<TreeNode>()));
    SequentialNotification single_notification(1, Notification("", fplus::maybe<TreeNode>()));

    processNotificationTest(request_id++, true, true);
}   

TEST_CASE("HTTP3TreeMessage encode and decode getJournalRequest", "[http3_tree_message]") {
    auto getJournalRequestTest = [](uint16_t request_id, const SequentialNotification& request_obj, const vector<SequentialNotification>& response_obj) {
        using request_type = SequentialNotification;
        using response_type = vector<SequentialNotification>;
        uint8_t request_signal = payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_REQUEST;
        uint8_t response_signal = payload_chunk_header::SIGNAL_WWATP_GET_JOURNAL_RESPONSE;
        auto encode_request_func = 
            [](HTTP3TreeMessage & client_message, request_type const& request_obj) { 
                client_message.encode_getJournalRequest(request_obj); };
        auto decode_request_func = 
            [](HTTP3TreeMessage & server_message) { 
                return server_message.decode_getJournalRequest(); };
        auto encode_response_func = 
            [](HTTP3TreeMessage & server_message, response_type const response_obj) { 
                server_message.encode_getJournalResponse(response_obj); };
        auto decode_response_func = 
            [](HTTP3TreeMessage & client_message) { 
                return client_message.decode_getJournalResponse(); };
        messageCycleTest<request_type, response_type>(request_id, request_obj, response_obj, 
            encode_request_func, decode_request_func,
            encode_response_func, decode_response_func,
            request_signal, response_signal);
    };

    uint16_t request_id = 6;

    // getJournalRequest always drops the label and the tree node when sending to server, so
    // only test request objects with empty notifications.
    SequentialNotification empty(0, Notification("", fplus::maybe<TreeNode>()));
    SequentialNotification single_notification(1, Notification("", fplus::maybe<TreeNode>()));

    // More complex vector
    TreeNode complex_node = createAnimalNode("https://example.com/path/to/lion?query=param#fragment", "A complex animal", 
        {{"txt", "pup_1_dossier"}, {"txt", "pup_2_dossier"}},
        {2, 2}, {"pup_1", "pup_2"}, {"pup 1 dossier", "pup 2 dossier"}, 
        "How to query complex animal", "Complex Animal QA sequence");
    TreeNode simpleAnimal = createAnimalNode("Sponge", "Bottom Feeder", {}, 
        {1, 1}, {}, {}, "", "");
    vector<SequentialNotification> complex_vector = {
        SequentialNotification(10, Notification("/lion/pup_1", fplus::maybe<TreeNode>())),
        SequentialNotification(11, Notification(complex_node.getLabelRule(), fplus::maybe<TreeNode>(complex_node))),
        SequentialNotification(12, Notification("/lion/pup_2", fplus::maybe<TreeNode>())),
        SequentialNotification(13, Notification(complex_node.getLabelRule(), fplus::maybe<TreeNode>(simpleAnimal))),
        SequentialNotification(14, Notification(complex_node.getLabelRule(), fplus::maybe<TreeNode>(complex_node))),
        SequentialNotification(12, Notification("/lion/pup_2", fplus::maybe<TreeNode>()))
    };

    getJournalRequestTest(request_id++, empty, {});
    getJournalRequestTest(request_id++, single_notification, {single_notification});
    getJournalRequestTest(request_id++, single_notification, complex_vector);
}