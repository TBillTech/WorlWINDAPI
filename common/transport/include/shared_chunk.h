#pragma once

#include <memory>
#include <span>
#include <vector>
#include <stdexcept>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/core/demangle.hpp>
#include <type_traits>
#include <iomanip>
#include <sstream>

#include "memory_pool.h"

// This file defines the shared_span class, which keeps shared_ptrs to contiguous memory chunks in order, and the start and end of the span.
// The underlying chunks are usually 1200, but can technically be any size if an alternative chunk type is provided.
// Since the shared span class contains shared_ptrs to chunks, when the last shared_span goes out of scope, any referenced chunks will be deallocated.
// There is also here defined a shared_chunk class, which is a utility class helping to allocate the chunk, and technically defines a span for the whole chunk.

using namespace std;

template<const int size>
class SizedChunk {
public:
    static constexpr size_t chunk_size = static_cast<size_t>(size);
    uint8_t data[chunk_size];
};

using UDPChunk = SizedChunk<1200>;

template <typename ChunkType, typename PODType>
PODType pod_from_chunk(typename std::remove_const<PODType>::type &result, vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks
        , pair<bool, pair<size_t, size_t>> start = {false, {0, 0}}) {
    size_t byte_offset = chunks.begin()->second.first;
    size_t current_chunk = 0;
    if (start.first) {
        byte_offset = start.second.first;
        current_chunk = start.second.second;
    }
    for (size_t pod_offset = 0; pod_offset < sizeof(PODType);) {
        if (current_chunk >= chunks.size()) {
            throw invalid_argument("Not enough chunks to copy the data type " + boost::core::demangle(typeid(PODType).name()));
        }
        auto& chunk = chunks.at(current_chunk);
        size_t chunk_space = chunk.second.second;
        size_t to_copy = min(sizeof(PODType) - pod_offset, chunk_space + chunk.second.first - byte_offset);
        memcpy(reinterpret_cast<uint8_t*>(&result) + pod_offset, chunk.first->data + byte_offset, to_copy);
        pod_offset += to_copy;
        if (pod_offset == sizeof(PODType)) {
            if (byte_offset + to_copy <= chunk_space + chunk.second.first) {
                return result;
            }
            current_chunk++;
            if (current_chunk <= chunks.size()) {
                byte_offset = chunks.at(current_chunk).second.first;
            }
            return result;
        }
        current_chunk++;
        if (current_chunk <= chunks.size()) {
            byte_offset = chunks.at(current_chunk).second.first;
        }
    }
    return result;
}

#pragma pack(push, 1)
struct payload_chunk_header {
    payload_chunk_header() :
        signal(0), request_id(0), data_length(0) {}
    payload_chunk_header(uint16_t request_id, uint8_t signal, uint16_t data_length)
        : signal(signal), request_id(request_id), data_length(data_length) 
        {
        }
    payload_chunk_header(payload_chunk_header const &other)
        : signal(other.signal), request_id(other.request_id), data_length(other.data_length) {}
    payload_chunk_header const &operator=(payload_chunk_header const &other) {
        signal = other.signal;
        request_id = other.request_id;
        data_length = other.data_length;
        return *this;
    }
    void copy_partial_data(const uint8_t* data, size_t length) {
        memcpy((void*)this, data, min(length, sizeof(payload_chunk_header)));
    }
    size_t get_wire_size() const {
        return sizeof(payload_chunk_header) + data_length;
    }
    const uint8_t signal_type = GLOBAL_SIGNAL_TYPE;
    uint8_t signal;
    uint16_t request_id;
    uint16_t data_length;
    static constexpr uint8_t GLOBAL_SIGNAL_TYPE = 2;
    static constexpr uint8_t SIGNAL_OTHER_CHUNK = 0x00;
    static constexpr uint8_t SIGNAL_CLOSE_STREAM = 0x01;
    static constexpr uint8_t SIGNAL_HEARTBEAT = 0x02;
    static constexpr uint8_t SIGNAL_PLACEHOLDER = 0xFF;
    //Since the entirety of the transport logic exists only to support the http3 server and client for WWATP,
    //the various signal codes for used by the http3_tree_messages can be defined here. These track the backend.h interface,
    //but also have the journaling api.
    static constexpr uint8_t SIGNAL_WWATP_REQUEST_CONTINUE = 0x03;
    static constexpr uint8_t SIGNAL_WWATP_RESPONSE_CONTINUE = 0x04;
    static constexpr uint8_t SIGNAL_WWATP_REQUEST_FINAL = 0x05;
    static constexpr uint8_t SIGNAL_WWATP_RESPONSE_FINAL = 0x06;
    static constexpr uint8_t SIGNAL_WWATP_GET_NODE_REQUEST = 0x07;
    static constexpr uint8_t SIGNAL_WWATP_GET_NODE_RESPONSE = 0x08;
    static constexpr uint8_t SIGNAL_WWATP_UPSERT_NODE_REQUEST = 0x09;
    static constexpr uint8_t SIGNAL_WWATP_UPSERT_NODE_RESPONSE = 0x0A;
    static constexpr uint8_t SIGNAL_WWATP_DELETE_NODE_REQUEST = 0x0B;
    static constexpr uint8_t SIGNAL_WWATP_DELETE_NODE_RESPONSE = 0x0C;
    static constexpr uint8_t SIGNAL_WWATP_GET_PAGE_TREE_REQUEST = 0x0D;
    static constexpr uint8_t SIGNAL_WWATP_GET_PAGE_TREE_RESPONSE = 0x0E;
    static constexpr uint8_t SIGNAL_WWATP_QUERY_NODES_REQUEST = 0x0F;
    static constexpr uint8_t SIGNAL_WWATP_QUERY_NODES_RESPONSE = 0x10;
    static constexpr uint8_t SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_REQUEST = 0x11;
    static constexpr uint8_t SIGNAL_WWATP_OPEN_TRANSACTION_LAYER_RESPONSE = 0x12;
    static constexpr uint8_t SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_REQUEST = 0x13;
    static constexpr uint8_t SIGNAL_WWATP_CLOSE_TRANSACTION_LAYERS_RESPONSE = 0x14;
    static constexpr uint8_t SIGNAL_WWATP_APPLY_TRANSACTION_REQUEST = 0x15;
    static constexpr uint8_t SIGNAL_WWATP_APPLY_TRANSACTION_RESPONSE = 0x16;
    static constexpr uint8_t SIGNAL_WWATP_GET_FULL_TREE_REQUEST = 0x17;
    static constexpr uint8_t SIGNAL_WWATP_GET_FULL_TREE_RESPONSE = 0x18;
    static constexpr uint8_t SIGNAL_WWATP_REGISTER_LISTENER_REQUEST = 0x19;
    static constexpr uint8_t SIGNAL_WWATP_REGISTER_LISTENER_RESPONSE = 0x1A;
    static constexpr uint8_t SIGNAL_WWATP_DEREGISTER_LISTENER_REQUEST = 0x1B;
    static constexpr uint8_t SIGNAL_WWATP_DEREGISTER_LISTENER_RESPONSE = 0x1C;
    static constexpr uint8_t SIGNAL_WWATP_NOTIFY_LISTENERS_REQUEST = 0x1D;
    static constexpr uint8_t SIGNAL_WWATP_NOTIFY_LISTENERS_RESPONSE = 0x1E;
    static constexpr uint8_t SIGNAL_WWATP_PROCESS_NOTIFICATION_REQUEST = 0x1F;
    static constexpr uint8_t SIGNAL_WWATP_PROCESS_NOTIFICATION_RESPONSE = 0x20;
    static constexpr uint8_t SIGNAL_WWATP_GET_JOURNAL_REQUEST = 0x21;
    static constexpr uint8_t SIGNAL_WWATP_GET_JOURNAL_RESPONSE = 0x22;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct signal_chunk_header {
    signal_chunk_header() : signal(0), request_id(0) {}
    signal_chunk_header(uint16_t request_id, uint8_t signal) 
        : signal(signal), request_id(request_id)  {}
    signal_chunk_header(signal_chunk_header const &other) 
        : signal(other.signal), request_id(other.request_id)   {}
    signal_chunk_header const &operator=(signal_chunk_header const &other) {
        signal = other.signal;
        request_id = other.request_id;
        return *this;
    }
    void copy_partial_data(const uint8_t* data, size_t length) {
        memcpy((void*)this, data, min(length, sizeof(signal_chunk_header)));
    }
    constexpr size_t get_wire_size() const {
        return sizeof(signal_chunk_header);
    }
    const uint8_t signal_type = GLOBAL_SIGNAL_TYPE;
    uint8_t signal;
    uint16_t request_id;
    static constexpr uint8_t GLOBAL_SIGNAL_TYPE = 1; 
    static constexpr uint8_t SIGNAL_CLOSE_STREAM = 0x00000001;
    static constexpr uint8_t SIGNAL_HEARTBEAT = 0x00000002;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct no_chunk_header {
    static constexpr uint8_t GLOBAL_SIGNAL_TYPE = 0;
    const uint8_t signal_type = GLOBAL_SIGNAL_TYPE;
    constexpr size_t get_wire_size() const {
        return 1;
    }
};
#pragma pack(pop)

extern no_chunk_header global_no_chunk_header;

template<class ChunkType = UDPChunk>
class shared_span {
public:
    typedef ChunkType chunk_type;
    static constexpr size_t chunk_size = ChunkType::chunk_size;

    shared_span(const shared_span &other) : chunks(other.chunks), signal_type(other.signal_type) {
    }
    template <typename InputIt>
    shared_span(InputIt begin, InputIt end) : signal_type(no_chunk_header::GLOBAL_SIGNAL_TYPE) {
        if (begin == end) {
            return;
        }
        signal_type = begin->get_signal_type();
        for (auto it = begin; it != end; ++it) {
            chunks.insert(chunks.end(), it->chunks.begin(), it->chunks.end());
        }
    }
    // the default constructor uses the memory pool to allocate a new chunk (the memory pool is parameterized by the chunk type)
    template <typename SignalType = no_chunk_header>
    shared_span(const SignalType signal, bool allocate) : signal_type(signal.signal_type) {
        if ((signal_type != 0) && (allocate == false)) {
            throw invalid_argument("Cannot create a shared_span with a meaningful signal_type and no allocation");
        }
        if (allocate) {
            append_chunk_with_signal(signal);
            // The point of using this constructor is to allocate the maximum possible amount of space (int the chunk), 
            // and to mark the chunk as ranging from the end of the signal to the end of the chunk.
            // However, append_chunk_with_signal will set the chunk range to just cover the signal only.
            chunks.back().second.second = ChunkType::chunk_size - chunks.back().second.first;
        }
    }
    // Move constructor
    shared_span(shared_span &&other) : chunks(std::move(other.chunks)), signal_type(other.signal_type) {
        // No need to clear other.chunks here, because the object moved should shortly go out of scope
    }
    // Constructor that takes a span of bytes and copies them into the shared_span
    template <typename PODType, typename SignalType = no_chunk_header>
    shared_span(const SignalType signal, const std::span<PODType> data) : signal_type(signal.signal_type) {
        size_t byte_offset = 0;
        append_chunk_with_signal(signal);
        while (byte_offset < data.size()*sizeof(PODType)) {
            if ((chunks.back().second.first + chunks.back().second.second) == ChunkType::chunk_size) {
                append_chunk_with_no_signal();
            }
            auto& last_chunk = chunks.back();
            size_t chunk_space = ChunkType::chunk_size - last_chunk.second.first - last_chunk.second.second;
            size_t copy_size = min(chunk_space, (data.size() * sizeof(PODType) - byte_offset));
            memcpy(last_chunk.first->data + last_chunk.second.first + last_chunk.second.second, reinterpret_cast<const uint8_t*>(data.data()) + byte_offset, copy_size);
            last_chunk.second.second += copy_size;
            byte_offset += copy_size;
        }
    }
    shared_span(const span<const uint8_t> data)
    : shared_span(std::move(create_from_data(data))) {}

    void compress() {        
        // Remove empty chunks
        chunks.erase(remove_if(chunks.begin(), chunks.end(),
            [](const auto& chunk) {
                return chunk.second.second == 0;
            }), chunks.end());
    }

    static shared_span create_from_data(const span<const uint8_t> data) {
        uint8_t signal_type = data[0];
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE: {
                // By definition create_from_data will be called with at least one byte, and therefore the
                // no_chunk_header is always complete.
                auto remaining_data = data.subspan(sizeof(no_chunk_header));
                shared_span span(*reinterpret_cast<const no_chunk_header*>(data.data()), remaining_data);
                return std::move(span);
            }
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE: {
                size_t header_data_length = min(sizeof(signal_chunk_header), data.size());
                signal_chunk_header header;
                header.copy_partial_data(data.data(), header_data_length);
                if (header_data_length == sizeof(signal_chunk_header))
                {
                    auto remaining_data = data.subspan(sizeof(signal_chunk_header));
                    shared_span span(header, remaining_data);
                    return std::move(span);
                }
                shared_span partial_span(header, std::span<uint8_t>{});
                // Doctor the the span start so that expand_use will accept the remaining header data later:
                partial_span.chunks.back().second.first = header_data_length;
                return std::move(partial_span);
            }
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE: {
                size_t header_data_length = min(sizeof(payload_chunk_header), data.size());
                payload_chunk_header header;
                header.copy_partial_data(data.data(), header_data_length);
                if (header_data_length == sizeof(payload_chunk_header))
                {
                    auto remaining_data = data.subspan(sizeof(payload_chunk_header));
                    shared_span span(header, remaining_data);
                    return std::move(span);
                }
                shared_span partial_span(header, std::span<uint8_t>{});
                // Doctor the the span start so that expand_use will accept the remaining header data later:
                partial_span.chunks.back().second.first = header_data_length;
                return std::move(partial_span);
                // Now there are a couple cases where things appear dangerous with the wire size, which is data_length.
                // * The condition where data_length is really 0, but less than the whole header has arrived would
                //       be alarming, but this cannot happen, because payload_chunk_header is invalid with 0 payload chunks. 
                //       (should use signal_chunk_header in that case).
                // * The condition where data_length is non-zero, non-mod 256, but the header is incomplete such that data_length is 0, then:
                //       size = 0
                //       signal_size = 6
                //       wire_size = data_recieved < 6
                //       stored_size = data_recieved
                //       but then wire_size < signal_size, and the quic_listener knows to wait for more data.
                // * The condition where data_length % 256 is really 0, but the header is lacking the last byte.  
                //       size = 0
                //       signal_size = 6
                //       wire_size = 256*n + 6, and by definition, n > 0
                //       stored_size = data_recieved
                // * The condition where data_length // 256 == 0, but the header is lacking the last byte.
                //       size = 0
                //       signal_size = 6
                //       wire_size = 6
                //       This could _also_ be fixed if wire_size were limited to chunk.second.first, because then wire_size < 6.
                // In other words, it is absolutely crucial that wire_size _never_ returns 6, and thus never equals the header size.
            }
            default:
                throw invalid_argument("Unknown signal type");
        }
    }

    constexpr shared_span<ChunkType>& operator=(const shared_span<ChunkType>&& other) {
        chunks.clear();
        chunks = std::move(other.chunks);
        signal_type = other.signal_type;
        return *this;
    }

    template <typename SignalType = no_chunk_header>
    void append_chunk_with_signal(SignalType signal) {
        auto chunk = memory_pool.allocate<ChunkType>();
        // The signal is not considered to be part of the chunk range, but chunks always have at minimum a no_chunk_header embedded in them.
        chunks.emplace_back(shared_ptr<ChunkType>(chunk, [](ChunkType* ptr) { memory_pool.deallocate(ptr); }), make_pair(sizeof(SignalType), 0));
        memcpy(chunks.back().first->data, &signal, sizeof(SignalType));
        chunks.back().second.first = sizeof(SignalType);
    }

    void append_chunk_with_no_signal() {
        append_chunk_with_signal(global_no_chunk_header);
    }

    shared_span restrict(pair<size_t, size_t> range) const {
        shared_span new_span(*this);
        size_t restricted_start = range.first;
        size_t restricted_length = range.second;
        size_t current_chunk = 0;
        while(current_chunk < new_span.chunks.size()) {
            auto& chunk = new_span.chunks.at(current_chunk);
            if (restricted_start + restricted_length <= chunk.second.second) {
                // The whole restricted range is in the current chunk
                chunk.second.first = chunk.second.first + restricted_start;
                chunk.second.second = restricted_length;
                new_span.chunks.resize(current_chunk + 1);
                return std::move(new_span);
            }
            if (restricted_start >= chunk.second.second) {
                // The restricted range is not in this chunk
                restricted_start -= chunk.second.second;
                new_span.chunks.erase(new_span.chunks.begin());
                continue;
            }
            // The restricted range starts in this chunk, but extends into the next chunk
            chunk.second.first = chunk.second.first + restricted_start;
            chunk.second.second = chunk.second.second - restricted_start;
            restricted_length -= chunk.second.second;
            restricted_start = 0;
            current_chunk++;
        }
        assert(new_span.chunks.size() == 0 || new_span.chunks.back().second.second < ChunkType::chunk_size);
        // The loop ran out of chunks, and so the resulting span will be shorter than the requested range
        return std::move(new_span);
    }

    // Method to copy a span of bytes into the shared_span.  Will overwrite data, but NEVER exceed the bounds of the span.
    // Will return the number of bytes written to the span.
    // This should only be used for testing (for example in shared_chunk_tester).  This will not alter the signals in the chunks.
    size_t copy_from_span(std::span<const uint8_t> data) const {
        size_t data_offset = 0;
        size_t current_chunk = 0;
        while (data_offset < data.size()) {
            if (current_chunk >= chunks.size()) {
                return data_offset;
            }
            auto& chunk = chunks.at(current_chunk);
            size_t chunk_space = chunk.second.second;
            size_t copy_size = min(chunk_space, data.size() - data_offset);
            memcpy(chunk.first->data + chunk.second.first, data.data() + data_offset, copy_size);
            data_offset += copy_size;
        }
        return data_offset;
    }

    shared_span restrict_upto(pair<size_t, size_t> end_point) const {
        pair<size_t, size_t> range = {0, end_point.first - get_signal_size()
            + end_point.second*sizeof(ChunkType) - get_signal_size()*end_point.second};
        return restrict(range);
    }

    template <typename SignalType>
    SignalType& get_signal() {
        if (chunks.empty()) {
            throw invalid_argument("No signal in the shared_span");
        }
        if (SignalType::GLOBAL_SIGNAL_TYPE != signal_type) {
            throw invalid_argument("Cannot decode signal of type " + boost::core::demangle(typeid(SignalType).name()) + " from shared_span with signal type " + to_string(signal_type));
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                return *reinterpret_cast<SignalType*>(chunks.front().first->data);
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                return *reinterpret_cast<SignalType*>(chunks.front().first->data);
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                return *reinterpret_cast<SignalType*>(chunks.front().first->data);
            default:
                throw invalid_argument("Unknown signal type");
        }
    }

    uint8_t get_signal_type() const {
        return signal_type;
    }
    size_t get_signal_size() const {
        if (chunks.empty()) {
            return 0;
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                return sizeof(no_chunk_header);
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                return sizeof(signal_chunk_header);
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                return sizeof(payload_chunk_header);
            default:
                throw invalid_argument("Unknown signal type");
        }
    }
    size_t get_wire_size() const {
        if (chunks.empty()) {
            return 0;
        }
        size_t read_size = chunks.front().second.first;
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                throw invalid_argument("No chunk header should not exist on the wire.");
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                if (read_size < sizeof(signal_chunk_header)) {
                    return read_size;
                }
                return reinterpret_cast<signal_chunk_header*>(chunks.front().first->data)->get_wire_size();
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                if(read_size < sizeof(payload_chunk_header)) {
                    return read_size;;
                }
                return reinterpret_cast<payload_chunk_header*>(chunks.front().first->data)->get_wire_size();
            default:
                throw invalid_argument("Wire size of signal not implemented.");
        }
    }
    uint8_t get_signal_signal() const {
        if (chunks.empty()) {
            throw invalid_argument("No signal in the shared_span");
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                throw invalid_argument("Global signal does not have a signal value");
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                return reinterpret_cast<signal_chunk_header*>(chunks.front().first->data)->signal;
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                return reinterpret_cast<payload_chunk_header*>(chunks.front().first->data)->signal;
            default:
                throw invalid_argument("Unknown signal type");
        }
    }
    void set_signal_signal(uint8_t signal) {
        if (chunks.empty()) {
            throw invalid_argument("No signal in the shared_span");
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                throw invalid_argument("No chunk header should not exist on the wire.");
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                get_signal<signal_chunk_header>().signal = signal;
                break;
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                get_signal<payload_chunk_header>().signal = signal;
                break;
            default:
                throw invalid_argument("Unknown signal type");
        }
    }
    uint16_t get_request_id() const {
        if (chunks.empty()) {
            throw invalid_argument("No request id in empty shared_span");
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                throw invalid_argument("Empty chunk header does not have request id.");
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                return reinterpret_cast<signal_chunk_header*>(chunks.front().first->data)->request_id;
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                return reinterpret_cast<payload_chunk_header*>(chunks.front().first->data)->request_id;
            default:
                throw invalid_argument("Could not decode request id, signal_type unknown.");
        }
    }
    void set_request_id(uint16_t request_id) {
        if (chunks.empty()) {
            throw invalid_argument("No request id in empty shared_span");
        }
        switch (signal_type) {
            case no_chunk_header::GLOBAL_SIGNAL_TYPE:
                throw invalid_argument("Empty chunk header does not have request id.");
            case signal_chunk_header::GLOBAL_SIGNAL_TYPE:
                get_signal<signal_chunk_header>().request_id = request_id;
                break;
            case payload_chunk_header::GLOBAL_SIGNAL_TYPE:
                get_signal<payload_chunk_header>().request_id = request_id;
                break;
            default:
                throw invalid_argument("Could not decode request id, signal_type unknown.");
        }
    }

    // Warning, this should ONLY be used if the shared_span is kept alive, as the underlying chunk will be deallocated when the last shared_span goes out of scope.
    // This is useful for passing to compatible libraries.
    ChunkType* use_chunk() const {
        if (chunks.size() != 1) {
            throw invalid_argument("use_chunk can only be called on a shared_span with one chunk");
        }
        return chunks[0].first.get();
    }

    uint64_t use_chunk_size() const {
        if (chunks.size() != 1) {
            throw invalid_argument("use_chunk_size can only be called on a shared_span with one chunk");
        }
        return chunks[0].second.second + get_signal_size();
    }

    template <typename SignalType = no_chunk_header>
    vector<shared_span<ChunkType>> flatten_with_signal(SignalType signal) const {
        vector<shared_span<ChunkType>> result;
        for (auto& chunk : chunks) {
            span<uint8_t> chunk_data(chunk.first->data + chunk.second.first, chunk.second.second);
            signal.data_length = chunk_data.size();
            result.emplace_back(signal, chunk_data);
        }
        return result;
    }

    shared_span append(shared_span &other) const {
        shared_span new_span(*this);
        new_span.chunks.insert(new_span.chunks.end(), other.chunks.begin(), other.chunks.end());
        return std::move(new_span);
    }

    template <typename PODType>
    pair<size_t, size_t> copy_type(PODType data, pair<bool, pair<size_t, size_t>> start = {false, {0, 0}}) const {
        size_t byte_offset = chunks.begin()->second.first;
        size_t current_chunk = 0;
        if (start.first) {
            byte_offset = start.second.first;
            current_chunk = start.second.second;
        }
        for (size_t pod_offset = 0; pod_offset < sizeof(PODType);) {
            if (current_chunk >= chunks.size()) {
                throw invalid_argument("Not enough chunks to copy the data type " + boost::core::demangle(typeid(PODType).name()));
            }
            auto& chunk = chunks.at(current_chunk);
            size_t chunk_space = chunk.second.second;
            size_t to_copy = min(sizeof(PODType) - pod_offset, chunk_space + chunk.second.first - byte_offset);
            memcpy(chunk.first->data + byte_offset, reinterpret_cast<uint8_t*>(&data) + pod_offset, to_copy);
            pod_offset += to_copy;
            if (pod_offset == sizeof(PODType)) {
                if (byte_offset + to_copy < ChunkType::chunk_size) {
                    return {byte_offset + to_copy, current_chunk};
                }
                current_chunk++;
                if (current_chunk == chunks.size()) {
                    return {get_signal_size(), current_chunk};
                }
                if (current_chunk < chunks.size()) {
                    byte_offset = chunks.at(current_chunk).second.first;
                }
                return {byte_offset + to_copy, current_chunk};
            }
            current_chunk++;
            if (current_chunk < chunks.size()) {
                byte_offset = chunks.at(current_chunk).second.first;
            }
        }
        return {byte_offset, current_chunk};
    }

    template <typename PODType>
    pair<size_t, size_t> copy_span(span<PODType> data, pair<bool, pair<size_t, size_t>> start = {false, {0, 0}}) const {
        for (size_t i = 0; i < data.size(); i++) {
            auto next_start = copy_type(data[i], start);
            start = {true, next_start};
        }
        return start.second;
    }

    pair<size_t, size_t> expand_use(size_t size) {
        if (chunks.size() == 0) {
            throw invalid_argument("Cannot expand use on empty shared_span");
        }
        auto& last_chunk = chunks.back();
        size_t chunk_space = ChunkType::chunk_size - last_chunk.second.first - last_chunk.second.second;
        if (chunk_space < size) {
            throw invalid_argument("Not enough space in the last chunk to expand use");
        }
        if (last_chunk.second.first < get_signal_size()) {
            pair<size_t, size_t> next_start = {last_chunk.second.first, chunks.size()-1};
            // The last chunk is not fully used, so we can expand use in the last chunk
            size_t more_header = min(get_signal_size() - last_chunk.second.first, size);
            last_chunk.second.first += more_header;
            size -= more_header;
            last_chunk.second.second += size;
            return next_start;
        }
        pair<size_t, size_t> next_start = {last_chunk.second.second + last_chunk.second.first, chunks.size()-1};
        // The end of the used data, second, is updated AFTER the computation of next_start, because expand use has not actually
        // written data yet, but is preparing for it and returning the location where the data will be written.
        last_chunk.second.second += size;
        return next_start;
    }

    template <typename PODType>
    inline PODType& at(typename std::remove_const<PODType>::type& result, pair<bool, pair<size_t, size_t>> start = {false, {0, 0}}) const {
        pod_from_chunk<ChunkType, typename std::remove_const<PODType>::type>(result, chunks, start);
        return result;
    }

    template <typename PODType>
    class const_iterator : public boost::iterator_facade<
        const_iterator<const PODType>, const PODType, boost::random_access_traversal_tag, const PODType&> {
    public:
        template <typename PODType2>
        friend class const_iterator;
        friend class boost::iterator_core_access;
        using ModifyablePODType = typename std::remove_const<PODType>::type;

        const_iterator(vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks) : chunks(chunks), chunk_index(0), span_index(0) {
            if (chunks.size() > 0) {
                while (chunks[chunk_index].second.second == 0) {
                    chunk_index++;
                    if (chunk_index == chunks.size()) {
                        return;
                    }
                }
                span_index = chunks[chunk_index].second.first;
            }
        }
        const_iterator(vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks, size_t chunk_index, size_t span_index) : chunks(chunks), chunk_index(chunk_index), span_index(span_index) {
        }
        template <typename PODType2>
        const_iterator(const_iterator<const PODType2> const &other) : chunks(other.chunks), chunk_index(other.chunk_index), span_index(other.span_index) {
        }
        const_iterator& operator++() {
            if (chunk_index == chunks.size()) {
                span_index = 0;
                return *this;
            }
            size_t bytes_to_skip = sizeof(PODType);
            while (bytes_to_skip > 0) {
                size_t chunk_space = chunks[chunk_index].second.first + chunks[chunk_index].second.second - span_index;
                if (bytes_to_skip <= chunk_space) {
                    span_index += bytes_to_skip;
                    if (span_index == chunks[chunk_index].second.first + chunks[chunk_index].second.second) {
                        chunk_index++;
                        if (chunk_index == chunks.size()) {
                            span_index = 0;
                            return *this;
                        }
                        while(chunks[chunk_index].second.second == 0) {
                            chunk_index++;
                            if (chunk_index == chunks.size()) {
                                span_index = 0;
                                return *this;
                            }
                        }
                        span_index = chunks[chunk_index].second.first;
                    }
                    return *this;
                }
                bytes_to_skip -= chunk_space;
                chunk_index++;
                if (chunk_index == chunks.size()) {
                    // Partial PODType at the end of span should throw exception
                    throw invalid_argument("PODType extends past the end of the span, but not enough bytes to complete the PODType");
                }
                while(chunks[chunk_index].second.second == 0) {
                    chunk_index++;
                    if (chunk_index == chunks.size()) {
                        // Partial PODType at the end of span should throw exception
                        throw invalid_argument("PODType extends past the end of the span, but not enough bytes to complete the PODType");
                    }
                }
                span_index = chunks[chunk_index].second.first;
            }
            return *this;
        }

        void increment() { ++(*this); } // Required for forward iterators

        bool equal(const const_iterator& other) const { return !(*this != other); }
    
        uint8_t& dereference() const { return *(*this); }        

        bool operator!=(const_iterator const &other) const {
            return &chunks != &other.chunks || chunk_index != other.chunk_index || span_index != other.span_index;
        }

        const PODType& operator*() {
            // If the length of the PODType extends past the end of the chunk, then throw an exception
            if (span_index + sizeof(PODType) > chunks[chunk_index].second.first + chunks[chunk_index].second.second) {
                if (chunk_index + 1 > chunk_stitches.size()) {
                    // Then need to create a PODType which correctly stiches the two parts from the consequitive chunks together, and
                    // allows to be returned from this operator
                    if (chunk_index > chunk_stitches.size()){
                        throw invalid_argument("When dereferencing chunks, must dereference spanning chunks in order.");
                    }
                    chunk_stitches.resize(chunk_stitches.size() + 1);
                    pair<bool, pair<size_t, size_t>> start_loc = {true, {span_index, chunk_index}};
                    vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks_ref = chunks;
                    pod_from_chunk<ChunkType, ModifyablePODType>(chunk_stitches[chunk_index], chunks_ref, start_loc);
                }
                return chunk_stitches[chunk_index];
            // Otherwise, check alignment before returning the reference to the PODType
            } else {
                // Check if the address is properly aligned for PODType
                uintptr_t addr = reinterpret_cast<uintptr_t>(chunks[chunk_index].first->data + span_index);
                if (addr % alignof(PODType) == 0) {
                    // Properly aligned, safe to return direct reference
                    return *reinterpret_cast<const PODType*>(chunks[chunk_index].first->data + span_index);
                } else {
                    // Not aligned, use safe copy through chunk_stitches
                    if (chunk_index >= chunk_stitches.size()) {
                        chunk_stitches.resize(chunk_index + 1);
                    }
                    pair<bool, pair<size_t, size_t>> start_loc = {true, {span_index, chunk_index}};
                    pod_from_chunk<ChunkType, ModifyablePODType>(chunk_stitches[chunk_index], chunks, start_loc);
                    return chunk_stitches[chunk_index];
                }
            }
        }
        shared_span<ChunkType> to_span() const {
            // Create a new span, starting with the chunk_index and update the first chunk with via the span_index
            shared_span new_span(global_no_chunk_header, false);
            new_span.chunks.insert(new_span.chunks.end(), chunks.begin() + chunk_index, chunks.end());
            new_span.signal_type = reinterpret_cast<uint8_t*>(new_span.chunks.begin()->first->data)[0];
            // Then restrict it to the span_index
            size_t signal_size = new_span.get_signal_size();
            shared_span restricted_span = new_span.restrict(make_pair(span_index-signal_size, new_span.size()));
            return move(restricted_span);
        }
        std::ptrdiff_t distance_to(const_iterator const &other) const {
            if (&chunks != &other.chunks) {
                throw invalid_argument("Cannot compare iterators from different shared_spans");
            }
            // If the other iterator is before this one, then return a negative distance
            if ((chunk_index < other.chunk_index) || ((chunk_index == other.chunk_index) && (span_index < other.span_index))) {
                return -1 * other.distance_to(*this);
            }
            if (chunk_index == other.chunk_index) {
                return (other.span_index - span_index);
            }
            std::ptrdiff_t distance = 0;
            // Distance within this last chunk
            if (chunk_index < chunks.size()) {
                distance -= span_index - chunks[chunk_index].second.first;
            }
            // Distance for the full chunks in between
            for (size_t i = other.chunk_index + 1; i < chunk_index; ++i) {
                distance -= chunks[i].second.second;
            }
            // Distance within the first chunk
            auto& other_chunk = chunks[other.chunk_index];
            distance -= (other_chunk.second.first + other_chunk.second.second - other.span_index);
            return distance / sizeof(PODType);
        }
    private:
        vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks;
        size_t chunk_index;
        size_t span_index;
        vector<ModifyablePODType> chunk_stitches;
    };
    // The const version of the begin function uses a const iterator
    template <typename PODType>
    const_iterator<const PODType> cbegin() const {
        return const_iterator<const PODType>(chunks);
    }
    template <typename PODType>
    const_iterator<const PODType> cend() const {
        return const_iterator<const PODType>(chunks, chunks.size(), 0);
    }
    template <typename PODType>
    auto crange() const {
        struct const_range {
            shared_span const &span;
            const_range(shared_span const &span) : span(span) {}
            decltype(auto) begin() { return span.begin<PODType>(); }
            decltype(auto) end() { return span.end<PODType>(); }
        };
        return const_range(*this);
    }

    // Begin returns an iterator class that knows how to increment from the end of the span of one chunk to the start of the span of the next chunk.
    template <typename PODType>
    class iterator : public boost::iterator_facade<
        iterator<PODType>, PODType, boost::random_access_traversal_tag, const PODType&>{
    public:
        template <typename PODType2>
        friend class const_iterator;
        template <typename PODType2>
        friend class iterator;
        friend class boost::iterator_core_access;
        iterator(vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks) : chunks(chunks), chunk_index(0), span_index(0) {
            if (chunks.size() > 0) {
                while (chunks[chunk_index].second.second == 0) {
                    chunk_index++;
                    if (chunk_index == chunks.size()) {
                        return;
                    }
                }
                span_index = chunks[chunk_index].second.first;
            }
        }
        iterator(vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks, size_t chunk_index, size_t span_index) : chunks(chunks), chunk_index(chunk_index), span_index(span_index) {
        }
        template <typename PODType2>
        iterator(iterator<PODType2> const &other) : chunks(other.chunks), chunk_index(other.chunk_index), span_index(other.span_index) {
        }
        iterator& operator++() {
            if (chunk_index == chunks.size()) {
                span_index = 0;
                return *this;
            }
            size_t bytes_to_skip = sizeof(PODType);
            while (bytes_to_skip > 0) {
                size_t chunk_space = chunks[chunk_index].second.first + chunks[chunk_index].second.second - span_index;
                if (bytes_to_skip <= chunk_space) {
                    span_index += bytes_to_skip;
                    if (span_index == chunks[chunk_index].second.first + chunks[chunk_index].second.second) {
                        chunk_index++;
                        if (chunk_index == chunks.size()) {
                            span_index = 0;
                            return *this;
                        }
                        while(chunks[chunk_index].second.second == 0) {
                            chunk_index++;
                            if (chunk_index == chunks.size()) {
                                span_index = 0;
                                return *this;
                            }
                        }
                        span_index = chunks[chunk_index].second.first;
                    }
                    return *this;
                }
                bytes_to_skip -= chunk_space;
                chunk_index++;
                if (chunk_index == chunks.size()) {
                    // Partial PODType at the end of span should throw exception
                    throw invalid_argument("PODType extends past the end of the span, but not enough bytes to complete the PODType");
                }
                while(chunks[chunk_index].second.second == 0) {
                    chunk_index++;
                    if (chunk_index == chunks.size()) {
                        // Partial PODType at the end of span should throw exception
                        throw invalid_argument("PODType extends past the end of the span, but not enough bytes to complete the PODType");
                    }
                }
                span_index = chunks[chunk_index].second.first;
            }
            return *this;
        }

        void increment() { ++(*this); } // Required for forward iterators

        bool equal(const iterator& other) const { return !(*this != other); }
    
        uint8_t& dereference() const { return *(*this); }        

        bool operator!=(iterator const &other) const {
            return &chunks != &other.chunks || chunk_index != other.chunk_index || span_index != other.span_index;
        }
        const PODType& operator*() const {
            // If the length of the PODType extends past the end of the chunk, then throw an exception
            if (span_index + sizeof(PODType) > chunks[chunk_index].second.first + chunks[chunk_index].second.second) {
                // non const iterator dereferencing of PODTypes that cross between chunks is not allowed, 
                // because it would allow writing past the end of the chunk into other memory 
                throw invalid_argument("PODType extends past the end of the span");
            // Otherwise, use safe copying to handle potential alignment issues
            } else {
                // Check if the address is properly aligned for PODType
                uintptr_t addr = reinterpret_cast<uintptr_t>(chunks[chunk_index].first->data + span_index);
                if (addr % alignof(PODType) == 0) {
                    // Properly aligned, safe to return direct reference
                    return *reinterpret_cast<PODType*>(chunks[chunk_index].first->data + span_index);
                } else {
                    // Not aligned, use safe copy through chunk_stitch
                    pair<bool, pair<size_t, size_t>> start_loc = {true, {span_index, chunk_index}};
                    pod_from_chunk<ChunkType, PODType>(chunk_stitch, chunks, start_loc);
                    return chunk_stitch;
                }
            }
        }

        void set(const PODType& value) {
            // Use the shared_span's copy_type method to safely write the value
            // Create a temporary shared_span from current chunks to call copy_type
            shared_span new_span(global_no_chunk_header, false);
            new_span.chunks.insert(new_span.chunks.end(), chunks.begin() + chunk_index, chunks.end());
            new_span.signal_type = reinterpret_cast<uint8_t*>(new_span.chunks.begin()->first->data)[0];
            
            pair<bool, pair<size_t, size_t>> start_loc = {true, {span_index, chunk_index}};
            new_span.copy_type(value, start_loc);
        }
        shared_span<ChunkType> to_span() const {
            // Create a new span, starting with the chunk_index and update the first chunk with via the span_index
            shared_span new_span(global_no_chunk_header, false);
            new_span.chunks.insert(new_span.chunks.end(), chunks.begin() + chunk_index, chunks.end());
            new_span.signal_type = reinterpret_cast<uint8_t*>(new_span.chunks.begin()->first->data)[0];
            // Then restrict it to the span_index, which is measured from the absolute start of the chunk
            // so in fact it needs to skip over the signal size for the chunk
            size_t signal_size = new_span.get_signal_size();
            shared_span restricted_span = new_span.restrict(make_pair(span_index-signal_size, new_span.size()));
            return std::move(restricted_span);
        }
        // Conversion operator to const_iterator
        template <typename PODType2>
        operator const_iterator<PODType2>() const {
            return const_iterator(chunks, chunk_index, span_index);
        }
        std::ptrdiff_t distance_to(iterator const &other) const {
            if (&chunks != &other.chunks) {
                throw invalid_argument("Cannot compare iterators from different shared_spans");
            }
            // If the other iterator is before this one, then return a negative distance
            if ((chunk_index < other.chunk_index) || ((chunk_index == other.chunk_index) && (span_index < other.span_index))) {
                return -1 * other.distance_to(*this);
            }
            if (chunk_index == other.chunk_index) {
                return (other.span_index - span_index);
            }
            std::ptrdiff_t distance = 0;
            // Distance within this last chunk
            if (chunk_index < chunks.size()) {
                distance -= span_index - chunks[chunk_index].second.first;
            }
            // Distance for the full chunks in between
            for (size_t i = other.chunk_index + 1; i < chunk_index; ++i) {
                distance -= chunks[i].second.second;
            }
            // Distance within the first chunk
            auto& other_chunk = chunks[other.chunk_index];
            distance -= (other_chunk.second.first + other_chunk.second.second - other.span_index);
            return distance / sizeof(PODType);
        }
    private:
        vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> const &chunks;
        size_t chunk_index;
        size_t span_index;
        mutable PODType chunk_stitch;
    };

    template <typename PODType>
    auto begin() const {
        if constexpr (std::is_const_v<PODType>) {
            return cbegin<PODType>();
        } else {
            return iterator<PODType>(chunks);
        }
    }

    template <typename PODType>
    auto end() const {
        if constexpr (std::is_const_v<PODType>) {
            return cend<PODType>();
        } else {
            return iterator<PODType>(chunks, chunks.size(), 0);
        }
    }

    template <typename PODType>
    auto range() const {
        struct _range {
            shared_span const &span;
            _range(shared_span const &span) : span(span) {}
            decltype(auto) begin() { return span.begin<PODType>(); }
            decltype(auto) end() { return span.end<PODType>(); }
        };
        return _range(*this);        
    }

    size_t size() const {
        size_t total = 0;
        for (auto& chunk : chunks) {
            total += chunk.second.second;
        }
        return total;
    }

    size_t stored_size() const {
        size_t total = 0;
        for (auto& chunk : chunks) {
            total += chunk.second.second + chunk.second.first;
        }
        return total;
    }

    std::ostream& write(std::ostream& os) const;
    std::istream& read(std::istream& is);

private:
    vector<pair<shared_ptr<ChunkType>, pair<size_t, size_t>>> chunks;
    uint8_t signal_type; // If non-zero, indicates that this shared_span is a signal_type, and cannot be combined with other shared_spans
};

typedef vector<shared_span<> > chunks;


template<typename ChunkType>
std::ostream& shared_span<ChunkType>::write(std::ostream& os) const {
    os << "shared_span( ";
    if (chunks.empty()) {
        os << "signal_type: 0 ";
        os << "signal_size: 0 ";
        os << "data_size: 0 ";
        os << "chunk_data: [\n";
        os << "])\n";
        return os;
    }
    os << "signal_type: " << (uint16_t)get_signal_type() << " ";
    os << "signal_size: " << get_signal_size() << " ";
    os << "data_size: " << size() << " ";
    os << "chunk_data: [\n";
    uint32_t count = 0;
    // First, write out the header
    uint8_t *header = chunks[0].first.get()->data;
    for (size_t i = 0; i < get_signal_size(); ++i) {
        // Write bytes out contiguously using UUEncoding
        os << std::hex << std::setw(2) << std::setfill('0') << (uint16_t)header[i];
        // Every 16 bytes, add a newline
        if (count % 16 == 15) {
            os << "\n";
        }
        count++;
    }
    for (auto byte: range<uint8_t>()) {
        // Write bytes out contiguously using UUEncoding
        os << std::hex << std::setw(2) << std::setfill('0') << (uint16_t)byte;
        // Every 16 bytes, add a newline
        if (count % 16 == 15) {
            os << "\n";
        }
        count++;
    }
    os << "])\n" << std::dec;
    return os;
}

template <typename ChunkType>
std::istream& shared_span<ChunkType>::read(std::istream& is) {
    std::string label;
    is >> label; // Read and discard "shared_span("

    uint16_t signal_type;
    is >> label >> signal_type; // Read "signal_type:" and the value
    size_t signal_size;
    is >> label >> signal_size; // Read "signal_size:" and the value
    size_t data_size;
    is >> label >> data_size; // Read "data_size:" and the value
    // Construct the header structure
    size_t total_size = signal_size + data_size;

    is >> label; // Read and discard "chunk_data:"
    is >> label; // discard " ["

    std::vector<uint8_t> data(total_size);
    for (size_t i = 0; i < total_size; ++i) {
        char hex_byte[3] = {0}; // Buffer for two hex characters + null terminator
        char c;
        size_t count = 0;
        while (count < 2 && is.get(c)) {
            if (!std::isspace(c)) {
                hex_byte[count++] = c;
            }
        }
        if (count < 2) {
            throw std::runtime_error("Unexpected end of stream while reading UUEncoded bytes");
        }
        int byte;
        std::istringstream hex_stream(hex_byte);
        hex_stream >> std::hex >> byte;
        data[i] = static_cast<uint8_t>(byte);
    }

    is >> label; // Read and discard "])"

    if (total_size == 0) {
        // If the total size is 0, then we have an empty shared_span
        chunks.clear();
        signal_type = 0;
        return is;
    }
    *this = std::move(shared_span<ChunkType>(std::span<const uint8_t>(data.data(), data.size())));
    return is;
}


template <typename ChunkType>
std::ostream& operator<<(std::ostream& os, const shared_span<ChunkType>& span) {
    return span.write(os);
}

template <typename ChunkType>
std::istream& operator>>(std::istream& is, shared_span<ChunkType>& span) {
    return span.read(is);
}
