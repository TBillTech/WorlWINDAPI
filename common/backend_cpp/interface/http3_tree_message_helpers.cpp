#include "http3_tree_message_helpers.h"
#include "shared_chunk.h"
#include "tree_node.h"
#include <vector>
#include <algorithm>
#include <stdexcept>

using namespace std;

// All Pseudo codes below are for encode functions (decode and can_decode are not shown)
// Pseudo codes focus on two issues:
//   * whether data is written as clear text or binary
//   * when and how many chunks are appended to the encoded list

// Pseudo code for encode_label
// 1. chunkList encoded starts empty
// 2. Create a chunk with clear text version of string with length
// 3. Append chunk to encoded
chunkList encode_label(uint16_t request_id, uint8_t signal, const string& str) {
    chunkList encoded;
    if (str.size() > shared_span<>::chunk_size - sizeof(payload_chunk_header)) {
        throw invalid_argument("String size exceeds chunk size");
    }
    auto header = payload_chunk_header(request_id, signal, str.size());
    encoded.emplace_back(header, span<const char>(str.c_str(), str.size()));
    return encoded;
}

pair<size_t, string> decode_label(chunkList encoded) {
    auto& chunk = encoded.front();
    string str(chunk.begin<const char>(), chunk.end<const char>());
    return {1, str};
}

fplus::maybe<size_t> can_decode_label(size_t start_chunk, chunkList encoded) {
    if (encoded.size() <= start_chunk) {
        return fplus::nothing<size_t>();
    }
    auto& chunk = *std::next(encoded.begin(), start_chunk);
    if (chunk.get_wire_size() < sizeof(payload_chunk_header)) {
        throw invalid_argument("Chunk size is less than header size");
    }
    auto header = chunk.get_signal<payload_chunk_header>();
    size_t data_size = header.data_length;
    if (chunk.size() < data_size) {
        throw invalid_argument("Chunk size is less than data size");
    }
    return fplus::maybe<size_t>(start_chunk + 1);
}

// Pseudo code for encode_long_string
// 1. chunkList encoded starts empty
// 2. Create a chunk with binary size_t string length at beginning, filled with string data
// 3. Append chunk to encoded
// 4. Continue adding chunks for the remainder of the long string
chunkList encode_long_string(uint16_t request_id, uint8_t signal, const string& str) {
    chunkList encoded;
    auto str_start = str.c_str();
    size_t cur_str_position = 0;
    size_t max_chunk_payload_size = shared_span<>::chunk_size - sizeof(payload_chunk_header);
    size_t first_chunk_payload_size = min(max_chunk_payload_size, str.size() + sizeof(size_t));
    auto first_header = payload_chunk_header(request_id, signal, first_chunk_payload_size);
    auto chunk = shared_span<>(first_header, true);
    auto position = chunk.copy_type(str.size());
    auto first_chunk_str_payload = first_chunk_payload_size - sizeof(size_t);
    auto upto = chunk.copy_span(span<const char>(str_start, first_chunk_str_payload), {true, position});
    encoded.emplace_back(chunk.restrict_upto(upto));
    cur_str_position += first_chunk_str_payload;
    while(cur_str_position < str.size()) {
        auto remaining_str_size = str.size() - cur_str_position;
        auto chunk_payload_size = min(max_chunk_payload_size, remaining_str_size);
        auto header = payload_chunk_header(request_id, signal, chunk_payload_size);
        encoded.emplace_back(header, span<const char>(str_start + cur_str_position, chunk_payload_size));
        cur_str_position += chunk_payload_size;
    }
    return encoded;
}

pair<size_t, string> decode_long_string(chunkList encoded) {
    auto chunk_count = can_decode_long_string(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode long string");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    shared_span<> concatenated(encoded.begin(), std::next(encoded.begin(), chunk_count_value));
    auto just_payload = concatenated.restrict({sizeof(size_t), concatenated.size()});
    string decoded_str(just_payload.begin<const char>(), just_payload.end<const char>());
    return {chunk_count_value, decoded_str};
}

fplus::maybe<size_t> can_decode_long_string(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto& chunk = *std::next(encoded.begin(), start_chunk);
    if (chunk.size() < sizeof(payload_chunk_header)) {
        throw invalid_argument("Chunk size is less than header size");
    }
    size_t long_string_length = 0;
    chunk.at<size_t>(long_string_length);
    auto expected_chunk = start_chunk+1;
    int32_t string_offset = long_string_length;
    string_offset -= shared_span<>::chunk_size - sizeof(payload_chunk_header) - sizeof(size_t);
    while(string_offset > 0){
        expected_chunk++;
        string_offset -= shared_span<>::chunk_size - sizeof(payload_chunk_header);
    }
    if (expected_chunk > encoded.size()) {
        return fplus::nothing<size_t>();
    }
    return fplus::maybe<size_t>(expected_chunk);
}

// Pseudo code for encode_MaybeTreeNode
// 1. If mnode is nothing, encode_long_string("Nothing") and return
// 2. If mnode is just, oss = a string "Just " followed by the TreeNode (via ostream operator, not including the property data)
// 3. Append 8 * 2 hex bytes to oss for the number of property data chunks.
// 4. Start with encoded ChunkList = encode_long_string(oss);
// 5. Append property data chunks to encoded
chunkList encode_MaybeTreeNode(uint16_t request_id, uint8_t signal, const fplus::maybe<TreeNode>& mnode) {
    if (mnode.is_nothing()) {
        return encode_long_string(request_id, signal, "Nothing");
    }
    TreeNode node = mnode.unsafe_get_just();
    stringstream oss;
    oss << "Just ";
    oss << hide_contents << node;
    oss << "\n";

    payload_chunk_header header(request_id, signal, 0);
    auto contents_vec = node.getPropertyData().flatten_with_signal(header);
    // Output node.getPropertyData().size() as hex, padded to fixed width
    // However, worrying about a 32 bit system, use sizeof(size_t) == 8
    oss << std::setw(8 * 2) << std::setfill('0') << std::hex << contents_vec.size();
    oss << std::dec; // Reset to decimal for any further output

    chunkList encoded = encode_long_string(request_id, signal, oss.str());
    chunkList property_data(std::make_move_iterator(contents_vec.begin()), std::make_move_iterator(contents_vec.end()));
    encoded.insert(encoded.end(), std::make_move_iterator(property_data.begin()), std::make_move_iterator(property_data.end()));
    return encoded;
}

pair<size_t, fplus::maybe<TreeNode>> decode_MaybeTreeNode(chunkList encoded) {
    auto treenode_chunks = can_decode_long_string(0, encoded);
    if (treenode_chunks.is_nothing()) {
        throw invalid_argument("Cannot decode MaybeTreeNode");
    }
    auto chunk_start = treenode_chunks.unsafe_get_just();
    auto decoded = decode_long_string(encoded);
    auto decoded_str = decoded.second;
    if (decoded_str == "Nothing") {
        return {chunk_start, fplus::nothing<TreeNode>()};
    }
    TreeNode node;
    stringstream iss(decoded_str);
    std::string label;
    iss >> label; // Consume "Just" to get to the TreeNode
    iss >> node;
    size_t contents_count;
    iss >> std::hex >> contents_count;
    chunkList::iterator start_it = std::next(encoded.begin(), chunk_start);
    chunkList::iterator end_it = std::next(encoded.begin(), chunk_start + contents_count);
    shared_span<> property_data(start_it, end_it);
    node.setPropertyData(std::move(property_data));
    return {chunk_start+contents_count, fplus::maybe<TreeNode>(node)};
}

fplus::maybe<size_t> can_decode_MaybeTreeNode(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto& chunk = *std::next(encoded.begin(), start_chunk);
    if (chunk.size() < sizeof(payload_chunk_header)) {
        throw invalid_argument("Chunk size is less than header size");
    }
    auto can_decode_structure = can_decode_long_string(start_chunk, encoded);
    if (can_decode_structure.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    if (can_decode_structure.unsafe_get_just() > encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count = can_decode_structure.unsafe_get_just();
    // now grab the _last_ 8 * 2 characters of the _last_ chunk that can be decoded:
    auto& last_chunk = *std::next(encoded.begin(), chunk_count - 1);
    auto last_chunk_size = last_chunk.size();
    if (last_chunk_size < 8 * 2) {
        auto decoded = decode_long_string({last_chunk});
        if (decoded.second == "Nothing") {
            return can_decode_structure;
        }
        return fplus::nothing<size_t>();
    }
    auto chunk_count_start = sizeof(payload_chunk_header) + last_chunk_size - 8 * 2;
    struct sixteen_bytes_t {
        char property_data[8*2];
    };
    sixteen_bytes_t contents_size_str = {0};
    last_chunk.at<sixteen_bytes_t>(contents_size_str, 
        {true, {chunk_count_start, 0}});
    // Use string_view to avoid null-termination issues
    std::string_view contents_view(contents_size_str.property_data, sizeof(contents_size_str.property_data));
    std::stringstream iss;
    iss.write(contents_view.data(), contents_view.size());
    size_t contents_size;
    iss >> std::hex >> contents_size;
    // Finally, check if chunk_count + contents_size + start_chunk is within the bounds of the encoded chunkList
    if (chunk_count + contents_size > encoded.size()) {
        return fplus::nothing<size_t>();
    }
    return fplus::maybe<size_t>(chunk_count + contents_size);
}

// Pseudo code for encode_SequentialNotification
// 1. Create a string oss = notification sequence number + " " + node label notification is for;
// 2. ChunkList encoded = encode_long_string(oss);
// 3. Append encode_MaybeTreeNode(the node as modified when originating the notification)
chunkList encode_SequentialNotification(uint16_t request_id, uint8_t signal, const SequentialNotification& notifications) {
    // The sequential nofitication is a pair of a uint64_t and a Notification,
    // and the Notification is a pair of a string and a maybe<TreeNode>.
    // We want to leverage the encode and decode methods for the string and maybe<TreeNode> types,
    // So Pack up the uint64_t and the string into a chunk, and then contatenate 
    // the chunkList from delegating to encode_MaybeTreeNode.
 
    // We can leverage encode_label by prefixing the nofication label with the sequence number
    // and then encoding the label as a string.
    stringstream oss;
    oss << notifications.first << " " << notifications.second.first;
    chunkList encoded = encode_long_string(request_id, signal, oss.str());
    chunkList maybe_tree = encode_MaybeTreeNode(request_id, signal, notifications.second.second);
    encoded.insert(encoded.end(), std::make_move_iterator(maybe_tree.begin()), std::make_move_iterator(maybe_tree.end()));
    return encoded;
}

pair<size_t, SequentialNotification> decode_SequentialNotification(chunkList encoded) {
    auto chunk_count = can_decode_long_string(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode SequentialNotification");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    auto decoded = decode_long_string(encoded);
    // unpack the uint64_t and the string
    stringstream iss(decoded.second);
    uint64_t sequence_number;
    iss >> sequence_number;
    // use the current offset of the iss to get the start of the notification label
    auto notification_label_start = static_cast<std::streamoff>(iss.tellg()) + 1; // +1 to skip the space after the sequence number
    string notification_label = decoded.second.substr(notification_label_start);
    // Then, decode the maybe TreeNode beginning at encoded[1]
    auto maybe_tree = decode_MaybeTreeNode(chunkList(std::next(encoded.begin(), decoded.first), encoded.end()));
    if (maybe_tree.first == 0) {
        throw invalid_argument("Cannot decode MaybeTreeNode");
    }
    auto maybe_tree_value = maybe_tree.second;
    Notification notification(notification_label, maybe_tree_value);
    SequentialNotification seq_notification(sequence_number, notification);
    return {chunk_count_value + maybe_tree.first, seq_notification};
}

fplus::maybe<size_t> can_decode_SequentialNotification(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto& chunk = *std::next(encoded.begin(), start_chunk);
    if (chunk.size() < sizeof(payload_chunk_header)) {
        throw invalid_argument("Chunk size is less than header size");
    }
    auto can_decode_structure = can_decode_long_string(start_chunk, encoded);
    if (can_decode_structure.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    if (can_decode_structure.unsafe_get_just() >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    // Then simply delegate to can_decode_MaybeTreeNode
    return can_decode_MaybeTreeNode(can_decode_structure.unsafe_get_just(), encoded);
}

// Pseudo code for encode_VectorSequentialNotification
// 1. chunkList encoded = encode_label(notifications.size())
// 2. for each notification in notifications, append encode_SequentialNotification(notification) to encoded
chunkList encode_VectorSequentialNotification(uint16_t request_id, uint8_t signal, const vector<SequentialNotification>& notifications) {
    chunkList encoded = encode_label(request_id, signal, to_string(notifications.size()));
    for (const auto& notification : notifications) {
        auto notification_chunk = encode_SequentialNotification(request_id, signal, notification);
        encoded.insert(encoded.end(), std::make_move_iterator(notification_chunk.begin()), std::make_move_iterator(notification_chunk.end()));
    }
    return encoded;
}

pair<size_t, vector<SequentialNotification>> decode_VectorSequentialNotification(chunkList encoded) {
    auto decoded = decode_label(encoded);
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    vector<SequentialNotification> notifications;
    for (size_t i = 0; i < vector_size; ++i) {
        auto notification = decode_SequentialNotification(chunkList(std::next(encoded.begin(), decoded.first), encoded.end()));
        notifications.push_back(notification.second);
        decoded.first += notification.first;
    }
    return {decoded.first, notifications};
}

fplus::maybe<size_t> can_decode_VectorSequentialNotification(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto can_decode_count = can_decode_label(start_chunk, encoded);
    if (can_decode_count.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    size_t can_decode_count_value = can_decode_count.unsafe_get_just();
    auto vector_size_decode = decode_label(chunkList(std::next(encoded.begin(), can_decode_count_value-1), encoded.end()));
    string vector_size_str = vector_size_decode.second;
    stringstream iss(vector_size_str);
    size_t vector_size;
    iss >> vector_size;
    size_t current_offset = 1 + start_chunk;
    for (size_t i = 0; i < vector_size; ++i) {
        auto notification = can_decode_SequentialNotification(current_offset, encoded);
        if (notification.is_nothing()) {
            return fplus::nothing<size_t>();
        }
        current_offset = notification.unsafe_get_just();
    }
    return fplus::maybe<size_t>(current_offset);
}

// Pseudo code for encode_NewNodeVersion
// 1. Create a string oss = "Nothing" if new_node_version optional version_number is nothing
// 2. Otherwise oss = "Just" + " " + new_node_version version_number
// 3. ChunkList encoded = encode_long_string(oss)
// 4. Append encode_MaybeTreeNode() to encoded
chunkList encode_NewNodeVersion(uint16_t request_id, uint8_t signal, const NewNodeVersion& new_node_version) {
    // The new node version is a decorated Notificaton similiar to the SequentialNotification.
    // It is a maybe uint16_t with a nofication label and a maybe<TreeNode>.
    // We want to leverage the encode and decode methods for the string and maybe<TreeNode> types,
    // So Pack up the string into a chunk, and then contatenate 
    // the chunkList from delegating to encode_MaybeTreeNode.
    stringstream oss;
    if (new_node_version.first.is_nothing()) {
        oss << "Nothing";
    } else {
        oss << "Just " << new_node_version.first.unsafe_get_just();
    }
    oss << " " << new_node_version.second.first;
    chunkList encoded = encode_long_string(request_id, signal, oss.str());
    chunkList maybe_tree = encode_MaybeTreeNode(request_id, signal, new_node_version.second.second);
    encoded.insert(encoded.end(), std::make_move_iterator(maybe_tree.begin()), std::make_move_iterator(maybe_tree.end()));
    return encoded;    
}

pair<size_t, NewNodeVersion> decode_NewNodeVersion(chunkList encoded) {
    auto chunk_count = can_decode_long_string(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode NewNodeVersion");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    auto decoded = decode_long_string(encoded);
    // unpack the uint16_t and the string
    stringstream iss(decoded.second);
    fplus::maybe<uint16_t> version;
    string version_str;
    iss >> version_str;
    if (version_str == "Nothing") {
        version = fplus::nothing<uint16_t>();
    } else {
        uint16_t version_value;
        iss >> version_value;
        version = fplus::maybe<uint16_t>(version_value);
    }
    // use the current offset of the iss to get the start of the notification label
    auto notification_label_start = static_cast<std::streamoff>(iss.tellg()) + 1; // +1 to skip the space after the version number
    string notification_label = decoded.second.substr(notification_label_start);
    // Then, decode the maybe TreeNode beginning at encoded[1]
    auto maybe_tree = decode_MaybeTreeNode(chunkList(std::next(encoded.begin(), decoded.first), encoded.end()));
    if (maybe_tree.first == 0) {
        throw invalid_argument("Cannot decode MaybeTreeNode");
    }
    auto maybe_tree_value = maybe_tree.second;
    Notification notification(notification_label, maybe_tree_value);
    NewNodeVersion new_node_version(version, notification);
    return {chunk_count_value + maybe_tree.first, new_node_version};
}

fplus::maybe<size_t> can_decode_NewNodeVersion(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto& chunk = *std::next(encoded.begin(), start_chunk);
    if (chunk.size() < sizeof(payload_chunk_header)) {
        throw invalid_argument("Chunk size is less than header size");
    }
    auto can_decode_structure = can_decode_label(start_chunk, encoded);
    if (can_decode_structure.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    // Then simply delegate to can_decode_MaybeTreeNode
    return can_decode_MaybeTreeNode(can_decode_structure.unsafe_get_just(), encoded);
}

// Pseudo code for encode_SubTransaction
// 1. ChunkList encoded = encode_label(number of descendant notifications in the sub transaction)
// 2. Append encode_NewNodeVersion(main notification in the sub transaction) to encoded
// 3. For each descendant notification in the sub transaction, append encode_NewNodeVersion(notification) to encoded
chunkList encode_SubTransaction(uint16_t request_id, uint8_t signal, const SubTransaction& sub_transaction) {
    // The sub transaction is one NewNodeVerison plus a vector of one or more NewNodeVersions.
    // So the only thing to track beyond simply delegating to N NewNodeVersion encoders is 
    // the count of the vector.  The count can leverage the encode_label method.
    chunkList encoded = encode_label(request_id, signal, to_string(sub_transaction.second.size()));
    auto first_new_node_version = encode_NewNodeVersion(request_id, signal, sub_transaction.first);
    encoded.insert(encoded.end(), std::make_move_iterator(first_new_node_version.begin()), std::make_move_iterator(first_new_node_version.end()));
    for (const auto& new_node_version : sub_transaction.second) {
        auto new_node_version_chunk = encode_NewNodeVersion(request_id, signal, new_node_version);
        encoded.insert(encoded.end(), std::make_move_iterator(new_node_version_chunk.begin()), std::make_move_iterator(new_node_version_chunk.end()));
    }
    return encoded;    
}

pair<size_t, SubTransaction> decode_SubTransaction(chunkList encoded) {
    auto chunk_count = can_decode_label(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode SubTransaction");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    auto decoded = decode_label(encoded);
    // unpack the uint16_t and the string
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    // Then, decode the NewNodeVersion beginning at encoded[1]
    auto base_newnode = decode_NewNodeVersion(chunkList(std::next(encoded.begin(), decoded.first), encoded.end()));
    if (base_newnode.first == 0) {
        throw invalid_argument("Cannot decode NewNodeVersion");
    }
    auto base_newnode_value = base_newnode.second;
    size_t newnode_version_offset = base_newnode.first+1;
    vector<NewNodeVersion> new_node_versions;
    for (size_t i = 0; i < vector_size; ++i) {
        auto new_node_version = decode_NewNodeVersion(chunkList(std::next(encoded.begin(), newnode_version_offset), encoded.end()));
        if (new_node_version.first == 0) {
            throw invalid_argument("Cannot decode NewNodeVersion");
        }
        new_node_versions.push_back(new_node_version.second);
        newnode_version_offset += new_node_version.first;
    }
    SubTransaction sub_transaction(base_newnode_value, std::move(new_node_versions));
    return {newnode_version_offset, sub_transaction};
}

fplus::maybe<size_t> can_decode_SubTransaction(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count = can_decode_label(start_chunk, encoded);
    if (chunk_count.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value - start_chunk != 1) {
        throw invalid_argument("Chunk count for vector length is not 1");
    }
    chunkList just_count_chunk;
    just_count_chunk.push_back(*std::next(encoded.begin(), start_chunk));
    auto decoded = decode_label(just_count_chunk);
    // read the length of the vector
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    // Check the base NewNodeVersion can be decoded
    size_t current_chunk = start_chunk + 1;
    auto base_newnode = can_decode_NewNodeVersion(current_chunk, encoded);
    if (base_newnode.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    current_chunk = base_newnode.unsafe_get_just();
    for (size_t i = 0; i < vector_size; ++i) {
        auto new_node_version = can_decode_NewNodeVersion(current_chunk, encoded);
        if (new_node_version.is_nothing()) {
            return fplus::nothing<size_t>();
        }
        current_chunk = new_node_version.unsafe_get_just();
    }
    // Then simply delegate to can_decode_NewNodeVersion
    return current_chunk;
}

// Pseudo code for encode_Transaction
// 1. chunkList encoded = encode_label(transaction.size())
// 2. for each sub_transaction in transaction, append encode_SubTransaction(sub_transaction) to encoded
chunkList encode_Transaction(uint16_t request_id, uint8_t signal, const Transaction& transaction) {
    // The transaction is a vector of SubTransactions.
    // So the only thing to track beyond simply delegating to N SubTransaction encoders is 
    // the count of the vector.  The count can leverage the encode_label method.
    chunkList encoded = encode_label(request_id, signal, to_string(transaction.size()));
    for (const auto& sub_transaction : transaction) {
        auto sub_transaction_chunk = encode_SubTransaction(request_id, signal, sub_transaction);
        encoded.insert(encoded.end(), std::make_move_iterator(sub_transaction_chunk.begin()), std::make_move_iterator(sub_transaction_chunk.end()));
    }
    return encoded;    
}

pair<size_t, Transaction> decode_Transaction(chunkList encoded) {
    auto chunk_count = can_decode_label(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode Transaction");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    auto decoded = decode_label(encoded);
    // read the length of the vector
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    // Then, decode the SubTransaction beginning at encoded[1]
    size_t sub_transaction_offset = decoded.first;
    Transaction transaction;
    for (size_t i = 0; i < vector_size; ++i) {
        auto sub_transaction = decode_SubTransaction(chunkList(std::next(encoded.begin(), sub_transaction_offset), encoded.end()));
        if (sub_transaction.first == 0) {
            throw invalid_argument("Cannot decode SubTransaction");
        }
        transaction.push_back(sub_transaction.second);
        sub_transaction_offset += sub_transaction.first;
    }
    return {sub_transaction_offset, transaction};
}

fplus::maybe<size_t> can_decode_Transaction(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count = can_decode_label(start_chunk, encoded);
    if (chunk_count.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value - start_chunk != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    chunkList just_count_chunk;
    just_count_chunk.push_back(*std::next(encoded.begin(), start_chunk));
    auto decoded = decode_label(just_count_chunk);
    // read the length of the vector
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    size_t current_chunk = start_chunk + 1;
    for (size_t i = 0; i < vector_size; ++i) {
        auto sub_transaction = can_decode_SubTransaction(current_chunk, encoded);
        if (sub_transaction.is_nothing()) {
            return fplus::nothing<size_t>();
        }
        current_chunk = sub_transaction.unsafe_get_just();
    }
    return current_chunk;
}

// Pseudo code for encode_VectorTreeNode
// 1. ChunkList encoded = encode_label(vector_size)
// 2. for each node in the vector, append encode_MaybeTreeNode(node) to encoded
chunkList encode_VectorTreeNode(uint16_t request_id, uint8_t signal, const std::vector<TreeNode>& nodes) {
    // The vector of tree nodes is a vector of TreeNodes.
    // So the only thing to track beyond simply delegating to N TreeNode encoders is 
    // the count of the vector.  The count can leverage the encode_label method.
    chunkList encoded = encode_label(request_id, signal, to_string(nodes.size()));
    for (const auto& node : nodes) {
        auto node_chunk = encode_MaybeTreeNode(request_id, signal, fplus::maybe<TreeNode>(node));
        encoded.insert(encoded.end(), std::make_move_iterator(node_chunk.begin()), std::make_move_iterator(node_chunk.end()));
    }
    return encoded;    
}

pair<size_t, std::vector<TreeNode>> decode_VectorTreeNode(chunkList encoded) {
    auto chunk_count = can_decode_label(0, encoded);
    if (chunk_count.is_nothing()) {
        throw invalid_argument("Cannot decode VectorTreeNode");
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    auto decoded = decode_label(encoded);
    // read the length of the vector
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    // Then, decode the TreeNode beginning at encoded[1]
    size_t tree_node_offset = decoded.first;
    std::vector<TreeNode> nodes;
    for (size_t i = 0; i < vector_size; ++i) {
        auto node = decode_MaybeTreeNode(chunkList(std::next(encoded.begin(), tree_node_offset), encoded.end()));
        if (node.first == 0) {
            throw invalid_argument("Cannot decode TreeNode");
        }
        if (node.second.is_nothing()) {
            throw invalid_argument("TreeNode is nothing");
        }
        nodes.push_back(node.second.unsafe_get_just());
        tree_node_offset += node.first;
    }
    return {tree_node_offset, nodes};
}

fplus::maybe<size_t> can_decode_VectorTreeNode(size_t start_chunk, chunkList encoded) {
    if (start_chunk >= encoded.size()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count = can_decode_label(start_chunk, encoded);
    if (chunk_count.is_nothing()) {
        return fplus::nothing<size_t>();
    }
    auto chunk_count_value = chunk_count.unsafe_get_just();
    // if chunk_count_values is not 1, then something went wrong
    if (chunk_count_value - start_chunk != 1) {
        throw invalid_argument("Chunk count is not 1");
    }
    chunkList just_count_chunk;
    just_count_chunk.push_back(*std::next(encoded.begin(), start_chunk));
    auto decoded = decode_label(just_count_chunk);
    // read the length of the vector
    stringstream iss(decoded.second);
    size_t vector_size;
    iss >> vector_size;
    size_t current_chunk = start_chunk + 1;
    for (size_t i = 0; i < vector_size; ++i) {
        auto node = can_decode_MaybeTreeNode(current_chunk, encoded);
        if (node.is_nothing()) {
            return fplus::nothing<size_t>();
        }
        current_chunk = node.unsafe_get_just();
    }
    return current_chunk;
}

