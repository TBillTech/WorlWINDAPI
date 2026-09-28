#include "backend_testbed.h"
#include <catch2/catch_test_macros.hpp>
#include <cassert>

// Ensure assert checks are honored even when built with NDEBUG (e.g., Release builds).
// In that case, map assert to throwing a runtime_error so http3_client_backend_tester still enforces conditions.
#ifdef NDEBUG
#undef assert
#define assert(expr) do { if(!(expr)) throw std::runtime_error("Assertion failed: " #expr); } while(0)
#endif

// Global flag useCatch2 switches Catch2 on or off.
bool useCatch2 = true;

void disableCatch2() {
    useCatch2 = false;
}

TreeNode createNoContentTreeNode(string label_rule, string description, vector<TreeNode::PropertyInfo> property_infos, 
    TreeNodeVersion version, vector<string> child_names, 
    maybe<string> query_how_to, maybe<string> qa_sequence) {
    shared_span<> no_content(global_no_chunk_header, false);
    return TreeNode(label_rule, description, property_infos, version, child_names, std::move(no_content), std::move(query_how_to), std::move(qa_sequence));
}

TreeNode createAnimalNode(string animal, string description, vector<TreeNode::PropertyInfo> property_infos, 
    TreeNodeVersion version, vector<string> child_names, 
    vector<string> property_data, string query_how_to, string qa_sequence) {
    payload_chunk_header header(0, payload_chunk_header::SIGNAL_WWATP_UPSERT_NODE_REQUEST, 0);
    shared_span<> animal_data(header, true);
    pair<bool, pair<size_t, size_t>> next = {false, {0, 0}};
    uint16_t index = 0;
    for (auto& content : property_data) {
        auto info = property_infos[index];
        if(info.first == "int64") {
            stringstream ss(content);
            int64_t value;
            ss >> value;
            next = {true, animal_data.copy_type<int64_t>(value, next)};
        }
        if(info.first == "uint64") {
            stringstream ss(content);
            uint64_t value;
            ss >> value;
            next = {true, animal_data.copy_type<uint64_t>(value, next)};
        }
        if(info.first == "double") {
            stringstream ss(content);
            double value;
            ss >> value;
            next = {true, animal_data.copy_type<double>(value, next)};
        }
        if(info.first == "float") {
            stringstream ss(content);
            float value;
            ss >> value;
            next = {true, animal_data.copy_type<float>(value, next)};
        
        }
        if(info.first == "bool") {
            stringstream ss(content);
            bool value;
            ss >> std::boolalpha >> value;
            next = {true, animal_data.copy_type<bool>(value, next)};
        }
        if (fixed_size_types.find(info.first) == fixed_size_types.end()) {
            next.second = animal_data.copy_type<uint64_t>(content.size(), next);
            next.second = animal_data.copy_span<const char>(std::span<const char>(content.c_str(), content.size()), next);
        }
        index++;
    }
    auto only_animal_data = animal_data.restrict_upto(next.second);
    auto data_size = only_animal_data.size();
    only_animal_data.get_signal<payload_chunk_header>().data_length = static_cast<uint16_t>(data_size);
    if (property_data.empty()) {
        only_animal_data = shared_span<>(global_no_chunk_header, false);
    }
    auto m_query_how_to = maybe<string>(query_how_to);
    if (useCatch2) {
        REQUIRE(m_query_how_to.get_with_default("") == query_how_to);
    }
    else {
        assert(m_query_how_to.get_with_default("") == query_how_to);
    }
    auto m_qa_sequence = maybe<string>(qa_sequence);
    if (useCatch2) {
        REQUIRE(m_qa_sequence.get_with_default("") == qa_sequence);
    }
    else {
        assert(m_qa_sequence.get_with_default("") == qa_sequence);
    }
    return TreeNode(animal, description, property_infos, version, child_names, std::move(only_animal_data), m_query_how_to, m_qa_sequence);
}

vector<TreeNode> createAnimalDossiers(TreeNode &animal_node) {
    vector<TreeNode> animal_dossiers;
    for (const auto& child_name : animal_node.getChildNames()) {
        string child_label_rule = animal_node.getLabelRule() + "/" + child_name;
        TreeNodeVersion child_version = {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)};
        TreeNode child_node = createNoContentTreeNode(child_label_rule, "Animal Dossier", {}, child_version, {}, maybe<string>(), maybe<string>());
        vector<string> note_names;
        for (int i = 0; i < 3; ++i) {
            string note_label_rule = child_label_rule + "/note" + std::to_string(i);
            note_names.push_back("note" + std::to_string(i));
            TreeNodeVersion note_version = {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)};
            TreeNode note_node = createNoContentTreeNode(note_label_rule, child_name + " note " + std::to_string(i), {}, note_version, {}, maybe<string>(), maybe<string>());
            animal_dossiers.push_back(std::move(note_node));
        }
        child_node.setChildNames(note_names);
        animal_dossiers.push_back(std::move(child_node));
    }
    return animal_dossiers;
}

vector<TreeNode> createLionNodes() {
    TreeNode lion = createAnimalNode(
        "lion",
        "King of the jungle", 
        {{"uint64", "popularity"}, {"string", "diet"}},
        {1, 256, "public", maybe<string>(), maybe<string>("tester"), maybe<string>("tester"), maybe<int>(2)}, 
        {"Simba", "Nala"},
        {"10", "carnivore"},
        "url duh!", 
        "Zookeeper1: Lions are majestic.\nZookeeper2: Indeed, they are the kings of the jungle."
    );
    vector<TreeNode> lion_nodes = createAnimalDossiers(lion);
    lion_nodes.insert(lion_nodes.begin(), lion);
    return lion_nodes;
}

vector<TreeNode> createElephantNodes() {
    TreeNode elephant = createAnimalNode(
        "elephant", 
        "Largest land animal", 
        {{"uint64", "popularity"}, {"string", "diet"}},
        {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)}, 
        {"Dumbo", "Babar"}, 
        {"8", "herbivore"},
        "url duh!", 
        "Zookeeper1: Elephants are so strong.\nZookeeper2: And they have great memory!"
    );
    vector<TreeNode> elephant_nodes = createAnimalDossiers(elephant);
    elephant_nodes.insert(elephant_nodes.begin(), elephant);
    if (elephant.getQueryHowTo().get_with_default("") != "url duh!") {
        std::cerr << "Elephant node has incorrect qa_sequence" << std::endl;
        throw std::runtime_error("Elephant node has incorrect qa_sequence");
    }
    return elephant_nodes;
}

vector<TreeNode> createParrotNodes() {
    TreeNode parrot = createAnimalNode(
        "parrot", 
        "Colorful bird", 
        {{"uint64", "popularity"}, {"string", "diet"}},
        {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)}, 
        {"Polly", "Jerome"}, 
        {"7", "omnivore"},
        "url duh!", 
        "Zookeeper1: Parrots can mimic sounds.\nZookeeper2: Yes, they are very intelligent birds."
    );
    vector<TreeNode> parrot_nodes = createAnimalDossiers(parrot);
    parrot_nodes.insert(parrot_nodes.begin(), parrot);
    return parrot_nodes;
}

void checkGetNode(Backend const &backend, const string& label_rule, TreeNode const &expected_node) {
    maybe<TreeNode> node = backend.getNode(label_rule);
    if (node.is_just()) {
        auto found_node = node.get_with_default(TreeNode());
        if (useCatch2) {
            REQUIRE(found_node.getLabelRule() == expected_node.getLabelRule());
            REQUIRE(found_node == expected_node);
        }
        else {
            assert(found_node.getLabelRule() == expected_node.getLabelRule());
            assert(found_node == expected_node);
        }
    } else {
        if (useCatch2)
        {
            FAIL("Node not found: " + label_rule);
        }
        else
        {
            throw std::runtime_error("Node not found: " + label_rule);
        }
    }
}

void checkMultipleGetNode(Backend const &backend, const vector<TreeNode>& expected_nodes) {
    for (auto& expected_node : expected_nodes) {
        checkGetNode(backend, expected_node.getLabelRule(), expected_node);
    }
}

void checkDeletedNode(Backend const &backend, const string& label_rule) {
    auto node = backend.getNode(label_rule);
    if (useCatch2) {
        REQUIRE(node.is_nothing());
    }
    else {
        assert(node.is_nothing());
    }
}

void checkMultipleDeletedNode(Backend const &backend, const vector<TreeNode>& expected_nodes) {
    for (auto& expected_node : expected_nodes) {
        checkDeletedNode(backend, expected_node.getLabelRule());
    }
}

vector<TreeNode> collectAllNotes() {
    auto lion_nodes = createLionNodes();
    auto elephant_nodes = createElephantNodes();
    auto parrot_nodes = createParrotNodes();
    vector<TreeNode> all_notes;
    for (auto& lion_node : lion_nodes) {
        if (lion_node.getLabelRule().find("note") != string::npos) {
            all_notes.push_back(lion_node);
        }
    }
    for (auto& elephant_node : elephant_nodes) {
        if (elephant_node.getLabelRule().find("note") != string::npos) {
            all_notes.push_back(elephant_node);
        }
    }
    for (auto& parrot_node : parrot_nodes) {
        if (parrot_node.getLabelRule().find("note") != string::npos) {
            all_notes.push_back(parrot_node);
        }
    }
    return all_notes;
}

TreeNode createNotesPageTree() {
    auto noteNodes = collectAllNotes();
    vector<string> note_labels;
    for (auto& note : noteNodes) {
        note_labels.push_back(note.getLabelRule());
    }
    TreeNode notes_page_tree = createNoContentTreeNode("notes", "Animal Notes Page", {}, 
        {1, 256, "public", maybe<string>(), maybe<string>(), maybe<string>(), maybe<int>(2)}, 
        note_labels, maybe<string>(), maybe<string>());
    return notes_page_tree;
}

vector<TreeNode> prefixNodeLabels(string label_prefix, vector<TreeNode> nodes) {
    for (auto& node : nodes) {
        node.setLabelRule(label_prefix + node.getLabelRule());
        auto child_names = node.getChildNames();
        for (auto& child_name : child_names) {
            child_name = label_prefix + child_name;
        }
        node.setChildNames(child_names);
    }
    return nodes;
}

BackendTestbed::BackendTestbed(Backend& backend, bool should_test_notifications, bool should_test_changes)
    : backend_(backend), should_test_notifications_(should_test_notifications), 
      should_test_changes_(should_test_changes) {}

void BackendTestbed::stressTestConstructions(size_t count) {
    for(size_t i = 0; i < count; ++i) {
        addAnimalsToBackend();
        addNotesPageTree();
        string label_rule = "elephant";
        backend_.deleteNode(label_rule);
        checkMultipleDeletedNode(backend_, createElephantNodes());
        checkMultipleGetNode(backend_, createLionNodes());
        checkMultipleGetNode(backend_, createParrotNodes());
    }
}

void BackendTestbed::addAnimalsToBackend() {
    vector<TreeNode> lion_nodes = createLionNodes();
    backend_.upsertNode(lion_nodes);
    vector<TreeNode> elephant_nodes = createElephantNodes();
    backend_.upsertNode(elephant_nodes);
    vector<TreeNode> parrot_nodes = createParrotNodes();
    backend_.upsertNode(parrot_nodes);
}

void BackendTestbed::addNotesPageTree() {
    vector<TreeNode> notes = {createNotesPageTree()};
    backend_.upsertNode(notes);
}

void BackendTestbed::testAnimalNodesNoElephant(string label_prefix) {
    checkMultipleDeletedNode(backend_, prefixNodeLabels(label_prefix, createElephantNodes()));
    checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createLionNodes()));
    checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createParrotNodes()));
}

void BackendTestbed::testBackendLogically(string label_prefix) {
    checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createLionNodes()));
    checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createElephantNodes()));
    checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createParrotNodes()));

    auto notes_pages = backend_.getPageTree(label_prefix + "notes");
    if (useCatch2) {
        REQUIRE(notes_pages == prefixNodeLabels(label_prefix, collectAllNotes()));
    }
    else {
        assert(notes_pages == prefixNodeLabels(label_prefix, collectAllNotes()));
    }

    {
        auto temp_tree = make_shared<MemoryTree>();
        SimpleBackend temp_backend(temp_tree);
        auto full_tree = backend_.getFullTree();
        temp_backend.upsertNode(full_tree);
        checkMultipleGetNode(temp_backend, prefixNodeLabels(label_prefix, createElephantNodes()));
        checkMultipleGetNode(temp_backend, prefixNodeLabels(label_prefix, createLionNodes()));
        checkMultipleGetNode(temp_backend, prefixNodeLabels(label_prefix, createParrotNodes()));
        auto notes_pages = temp_backend.getPageTree(label_prefix + "notes");
        if (useCatch2) {
            REQUIRE(notes_pages == prefixNodeLabels(label_prefix, collectAllNotes()));
        }
        else {
            assert(notes_pages == prefixNodeLabels(label_prefix, collectAllNotes()));
        }
    }
    
    if (should_test_changes_) {
        auto label_rule = label_prefix + "elephant";
        backend_.deleteNode(label_rule);
        checkMultipleDeletedNode(backend_, prefixNodeLabels(label_prefix, createElephantNodes()));
        checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createLionNodes()));
        checkMultipleGetNode(backend_, prefixNodeLabels(label_prefix, createParrotNodes()));
    }

    if (!should_test_notifications_ || !should_test_changes_) {
        return;
    }

    bool lion_node_created = false;
    bool lion_node_deleted = false;
    string lion_label = label_prefix + "lion";
    auto backend_address = &backend_;
    backend_.registerNodeListener("lion_listener", lion_label, false, 
        [lion_label, &lion_node_created, &lion_node_deleted, backend_address](Backend& notified_backend, const string label_rule, const fplus::maybe<TreeNode> node) {
        if (useCatch2) {
            REQUIRE(lion_label == label_rule);
            REQUIRE(backend_address == &notified_backend);
        }
        else {
            assert(lion_label == label_rule);
            assert(backend_address == &notified_backend);
        }
        if (node.is_just()) {
            auto found_node = node.get_with_default(TreeNode());
            if (found_node.getLabelRule() == lion_label) {
                lion_node_created = true;
            }
        } else {
            lion_node_deleted = true;
        }
    });
    auto prefixed_lion_nodes = prefixNodeLabels(label_prefix, createLionNodes());
    backend_.upsertNode(prefixed_lion_nodes);
    backend_.processNotifications();
    if (useCatch2) {
        REQUIRE(lion_node_created);
    }
    else {
        assert(lion_node_created);
    }
    backend_.deleteNode(lion_label);
    backend_.processNotifications();
    if (useCatch2) {
        REQUIRE(lion_node_deleted);
    }
    else {
        assert(lion_node_deleted);
    }
    backend_.deregisterNodeListener("lion_listener", lion_label);
    backend_.processNotifications();
    lion_node_created = false;
    lion_node_deleted = false;
    backend_.upsertNode(prefixed_lion_nodes);
    backend_.processNotifications();
    if (useCatch2) {
        REQUIRE(!lion_node_created);
    }
    else {
        assert(!lion_node_created);
    }
    backend_.deleteNode(lion_label);
    backend_.processNotifications();
    if (useCatch2) {
        REQUIRE(!lion_node_deleted);
    }
    else {
        assert(!lion_node_deleted);
    }
}

void BackendTestbed::testPeerNotification(Backend &to_be_modified, function<void(void)> background_service, string label_prefix)
{
    if(!should_test_notifications_ || !should_test_changes_) {
        if (useCatch2) {
            WARN("Skipping testPeerNotification because notifications or changes are disabled.");
        }
        else {
            std::cerr << "Skipping testPeerNotification because notifications or changes are disabled." << std::endl;
        }
        assert(false); // Running in this mode would be completely senseless
        return;
    }
    bool lion_node_created = false;
    bool lion_node_deleted = false;
    string lion_label = label_prefix + "lion";
    to_be_modified.deleteNode(lion_label);
    background_service();
    auto backend_address = &backend_;
    backend_.registerNodeListener("lion_listener", lion_label, false, 
        [lion_label, &lion_node_created, &lion_node_deleted, backend_address](Backend& notified_backend, const string label_rule, const fplus::maybe<TreeNode> node) {
        if (useCatch2) {
            REQUIRE(lion_label == label_rule);
            REQUIRE(backend_address == &notified_backend);
        }
        else {
            assert(lion_label == label_rule);
            assert(backend_address == &notified_backend);
        }
        if (node.is_just()) {
            auto found_node = node.get_with_default(TreeNode());
            if (found_node.getLabelRule() == lion_label) {
                lion_node_created = true;
            }
        } else {
            lion_node_deleted = true;
        }
    });
    background_service();
    auto prefixed_lion_nodes = prefixNodeLabels(label_prefix, createLionNodes());
    to_be_modified.upsertNode(prefixed_lion_nodes);
    to_be_modified.processNotifications();
    background_service();
    backend_.processNotifications();
    background_service();
    if (useCatch2) {
        REQUIRE(lion_node_created);
    }
    else {
        assert(lion_node_created);
    }
    to_be_modified.deleteNode(lion_label);
    to_be_modified.processNotifications();
    background_service();
    backend_.processNotifications();
    background_service();
    if (useCatch2) {
        REQUIRE(lion_node_deleted);
    }
    else {
        assert(lion_node_deleted);
    }
    backend_.deregisterNodeListener("lion_listener", lion_label);
    to_be_modified.processNotifications();
    background_service();
    backend_.processNotifications();
    lion_node_created = false;
    lion_node_deleted = false;
    to_be_modified.upsertNode(prefixed_lion_nodes);
    to_be_modified.processNotifications();
    background_service();
    backend_.processNotifications();
    background_service();
    if (useCatch2) {
        REQUIRE(!lion_node_created);
    }
    else {
        assert(!lion_node_created);
    }
    to_be_modified.deleteNode(lion_label);
    to_be_modified.processNotifications();
    background_service();
    backend_.processNotifications();
    background_service();
    if (useCatch2) {
        REQUIRE(!lion_node_deleted);
    }
    else {
        assert(!lion_node_deleted);
    }

}

