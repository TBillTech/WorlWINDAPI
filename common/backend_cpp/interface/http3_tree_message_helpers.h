#pragma once

#include <fplus/fplus.hpp>
#include "shared_chunk.h"
#include "tree_node.h"
#include "backend.h"

// These helper methods handle the tree main data types used by the class.
// The decoders return a maybe pair of the number of chunks consumed and the decoded data.
// If there are insufficient chunks, the decoder will return a maybe with no value.
// The can decode tests look forward by start_chunk chunks, 
// and check if there are enough chunks to decode the data at that chunk onward.

using chunkList = std::list<shared_span<>>;

chunkList encode_label(uint16_t request_id, uint8_t signal, const string& str);
pair<size_t, string> decode_label(chunkList encoded);
fplus::maybe<size_t> can_decode_label(size_t start_chunk, chunkList encoded);

chunkList encode_long_string(uint16_t request_id, uint8_t signal, const string& str);
pair<size_t, string> decode_long_string(chunkList encoded);
fplus::maybe<size_t> can_decode_long_string(size_t start_chunk, chunkList encoded);

chunkList encode_MaybeTreeNode(uint16_t request_id, uint8_t signal, const fplus::maybe<TreeNode>& node);
pair<size_t, fplus::maybe<TreeNode> > decode_MaybeTreeNode(chunkList encoded);
fplus::maybe<size_t> can_decode_MaybeTreeNode(size_t start_chunk, chunkList encoded);

chunkList encode_SequentialNotification(uint16_t request_id, uint8_t signal, const SequentialNotification& notifications);
pair<size_t, SequentialNotification > decode_SequentialNotification(chunkList encoded);
fplus::maybe<size_t> can_decode_SequentialNotification(size_t start_chunk, chunkList encoded);

chunkList encode_VectorSequentialNotification(uint16_t request_id, uint8_t signal, const vector<SequentialNotification>& notifications);
pair<size_t, vector<SequentialNotification> > decode_VectorSequentialNotification(chunkList encoded);
fplus::maybe<size_t> can_decode_VectorSequentialNotification(size_t start_chunk, chunkList encoded);

chunkList encode_NewNodeVersion(uint16_t request_id, uint8_t signal, const NewNodeVersion& version);
pair<size_t, NewNodeVersion> decode_NewNodeVersion(chunkList encoded);
fplus::maybe<size_t> can_decode_NewNodeVersion(size_t start_chunk, chunkList encoded);

chunkList encode_SubTransaction(uint16_t request_id, uint8_t signal, const SubTransaction& transaction);
pair<size_t, SubTransaction> decode_SubTransaction(chunkList encoded);
fplus::maybe<size_t> can_decode_SubTransaction(size_t start_chunk, chunkList encoded);

chunkList encode_Transaction(uint16_t request_id, uint8_t signal, const Transaction& transaction);
pair<size_t, Transaction> decode_Transaction(chunkList encoded);
fplus::maybe<size_t> can_decode_Transaction(size_t start_chunk, chunkList encoded);

chunkList encode_VectorTreeNode(uint16_t request_id, uint8_t signal, const std::vector<TreeNode>& nodes);
pair<size_t, std::vector<TreeNode> > decode_VectorTreeNode(chunkList encoded);
fplus::maybe<size_t> can_decode_VectorTreeNode(size_t start_chunk, chunkList encoded);
