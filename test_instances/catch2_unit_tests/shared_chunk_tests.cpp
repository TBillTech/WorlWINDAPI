#include <catch2/catch_test_macros.hpp>
#include "shared_chunk.h"
#include <span>
#include <cstring>
#include <sstream>

const size_t memoryBlockSize = 2400;
uint8_t memoryBlock0[memoryBlockSize];
uint8_t memoryBlockA[memoryBlockSize];

void init_sizes() {
    // Also wish to test strings
    memory_pool.setPoolSize<shared_span<>>(1000);
}

void init_MemoryBlock() {
    for (size_t i = 0; i < memoryBlockSize; i++) {
        memoryBlock0[i] = i % 256;
    }
    for (size_t i = 0; i < memoryBlockSize; i++) {
        memoryBlockA[i] = (i + 'A') % 256;
    }
}

bool check_span_data(const shared_span<UDPChunk>& span, std::span<const uint8_t> data) {
    using const_span_iterator_type = shared_span<UDPChunk>::const_iterator<uint8_t>;
    const_span_iterator_type iterator = span.begin<const uint8_t>();
    for (size_t i = 0; i < data.size(); i++) {
        if (*iterator != data[i]) {
            return false;
        }
        ++iterator;
    }
    return true;
}

bool copy_data_into_span(shared_span<>& span, std::span<const uint8_t> data) {
    span.copy_from_span(data);
    return check_span_data(span, data);
}

TEST_CASE("shared_span constructor", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> span(global_no_chunk_header, true);
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    REQUIRE(span.size() == ChunkDataSize);
    REQUIRE(copy_data_into_span(span, std::span<const uint8_t>(memoryBlock0, ChunkDataSize)));
}

TEST_CASE("shared_span segment", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> span(global_no_chunk_header, true);
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    copy_data_into_span(span, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
    auto restricted_span = span.restrict(std::make_pair(7, 5));
    auto remaining_span = span.restrict(std::make_pair(7+5, ChunkDataSize-5-7));
    REQUIRE(check_span_data(restricted_span, std::span<const uint8_t>(memoryBlock0 + 7, 5)));
    REQUIRE(check_span_data(remaining_span, std::span<const uint8_t>(memoryBlock0 + 7 + 5, ChunkDataSize-5-7)));
    copy_data_into_span(restricted_span, std::span<const uint8_t>(memoryBlockA+7, 5));
    auto restricted_span2 = span.restrict(std::make_pair(7, 5));
    REQUIRE(check_span_data(restricted_span2, std::span<const uint8_t>(memoryBlockA + 7, 5)));
    REQUIRE(check_span_data(remaining_span, std::span<const uint8_t>(memoryBlock0 + 7 + 5, ChunkDataSize-5-7)));
    copy_data_into_span(restricted_span2, std::span<const uint8_t>(memoryBlock0+7, 5));
    REQUIRE(check_span_data(span, std::span<const uint8_t>(memoryBlock0, ChunkDataSize)));
}

TEST_CASE("shared_span restrict", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> span(global_no_chunk_header, true);
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    copy_data_into_span(span, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
    auto restricted_span = span.restrict(std::make_pair(7, 5));
    auto remaining_span = span.restrict(std::make_pair(7+5, ChunkDataSize-5-7));
    REQUIRE(check_span_data(restricted_span, std::span<const uint8_t>(memoryBlock0 + 7, 5)));
    REQUIRE(check_span_data(remaining_span, std::span<const uint8_t>(memoryBlock0 + 7 + 5, ChunkDataSize-5-7)));
}

TEST_CASE("shared_span copy from span", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> span(global_no_chunk_header, true);
    REQUIRE(copy_data_into_span(span, std::span<const uint8_t>(memoryBlock0, span.size())));
}

TEST_CASE("shared_span use_chunk", "[shared_span]") {
    init_sizes();
    shared_span<> span(global_no_chunk_header, true);
    span.copy_from_span(std::span<const uint8_t>((uint8_t*)"Hello, World!", 13));
    auto chunk = span.use_chunk();
    REQUIRE(memcmp(chunk->data + span.get_signal_size(), "Hello, World!", 13) == 0);
    memcpy(chunk->data + span.get_signal_size(), "Goodbye, World!", 14);
    REQUIRE(check_span_data(span, std::span<const uint8_t>((uint8_t*)"Goodbye, World!", 14)));
}

TEST_CASE("shared_span append", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> sspan(global_no_chunk_header, true);
    shared_span<> span2(global_no_chunk_header, true);
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    copy_data_into_span(sspan, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
    copy_data_into_span(span2, std::span<const uint8_t>(memoryBlockA, ChunkDataSize));
    auto appended_span = sspan.append(span2);
    auto restricted_span = appended_span.restrict(std::make_pair(200, ChunkDataSize-200));
    auto remaining_span = appended_span.restrict(std::make_pair(ChunkDataSize, ChunkDataSize-200));
    REQUIRE(check_span_data(restricted_span, std::span<const uint8_t>(memoryBlock0+200, ChunkDataSize-200)));
    REQUIRE(check_span_data(remaining_span, std::span<const uint8_t>(memoryBlockA, ChunkDataSize-200)));
}

TEST_CASE("shared_span copy_more", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> sspan(global_no_chunk_header, std::span<const uint8_t>(memoryBlock0, 2400));
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    auto restricted_span = sspan.restrict(std::make_pair(0, 1200));
    auto remaining_span = sspan.restrict(std::make_pair(1200, 2400));
    REQUIRE(check_span_data(restricted_span, std::span<const uint8_t>(memoryBlock0, 1200)));
    REQUIRE(check_span_data(remaining_span, std::span<const uint8_t>(memoryBlock0+1200, 1200)));
    auto more_restricted_span = restricted_span.restrict(std::make_pair(600, 600));
    REQUIRE(check_span_data(more_restricted_span, std::span<const uint8_t>(memoryBlock0+600, 600)));
    auto more_remaining_span = remaining_span.restrict(std::make_pair(0, 600));
    REQUIRE(check_span_data(more_remaining_span, std::span<const uint8_t>(memoryBlock0+1200, 600)));
    auto combined_span = more_restricted_span.append(more_remaining_span);
    REQUIRE(check_span_data(combined_span, std::span<const uint8_t>(memoryBlock0+600, 1200)));
    auto middle_span = combined_span.restrict(std::make_pair(300, 600));
    REQUIRE(check_span_data(middle_span, std::span<const uint8_t>(memoryBlock0+900, 600)));
    auto trailing_span = middle_span.restrict(std::make_pair(301, 100));
    REQUIRE(check_span_data(trailing_span, std::span<const uint8_t>(memoryBlock0+1201, 100)));
}

TEST_CASE("shared_span POD iterator", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    // First create two shared_spans will allocation using an odd size
    shared_span<> sspan(global_no_chunk_header, true);
    shared_span<> span2(global_no_chunk_header, true);
    // Make a POD type that is an odd size compared to 1200
    struct OddSizedPOD {
        uint8_t data[307];
        OddSizedPOD() {
            for (size_t i = 0; i < 307; i++) {
                data[i] = i % 256;
            }
        }
    };
    // Create an appended span from the two:
    shared_span<> appended_span = sspan.append(span2);
    // Now copy 7 of the OddSizedPOD into the appended span
    OddSizedPOD odd_pod[7];
    appended_span.copy_span(std::span<OddSizedPOD>(odd_pod, 7));
    // Create an iterator to stride through the OddSizedPODs in the appended span
    shared_span<>::iterator<OddSizedPOD> odd_iter = appended_span.begin<OddSizedPOD>();
    for (size_t i = 0; i < 7; i++) {
        try {
            OddSizedPOD pod = *odd_iter;
            for (size_t j = 0; j < 307; j++) {
                REQUIRE(pod.data[j] == j % 256);
            }
        } catch (std::exception& e) {
            if ((string(e.what()) == "PODType extends past the end of the span") && (i == 3)) {
                auto next_span = odd_iter.to_span();
                OddSizedPOD next_pod;
                next_span.at<OddSizedPOD>(next_pod);
                for (size_t j = 0; j < 307; j++) {
                    REQUIRE(next_pod.data[j] == j % 256);
                }
            } else {
                FAIL("Unexpected exception: " << e.what());
            }
        }
        ++odd_iter;
    }
}

TEST_CASE("shared_span string iteration", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    shared_span<> sspan(global_no_chunk_header, true);
    shared_span<> span2(global_no_chunk_header, true);
    const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
    copy_data_into_span(sspan, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
    copy_data_into_span(span2, std::span<const uint8_t>(memoryBlockA, ChunkDataSize));
    auto appended_span = sspan.append(span2);
    auto restricted_span = appended_span.restrict(std::make_pair(200, ChunkDataSize));
    std::string appended_string(restricted_span.begin<const char>(), restricted_span.end<const char>());
    std::string original_string(reinterpret_cast<const char*>(memoryBlock0)+200, ChunkDataSize-200);
    original_string.append(reinterpret_cast<const char*>(memoryBlockA), 200);
    REQUIRE(appended_string == original_string);
}

TEST_CASE("shared_span iostreams", "[shared_span]") {
    init_sizes();
    init_MemoryBlock();
    {
        shared_span<> sspan(global_no_chunk_header, true);
        const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
        copy_data_into_span(sspan, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
        std::ostringstream oss;
        oss << sspan;
        std::string output = oss.str();
        std::istringstream iss(output);
        shared_span<> new_sspan(global_no_chunk_header, false);
        iss >> new_sspan;
        REQUIRE(check_span_data(new_sspan, std::span<const uint8_t>(memoryBlock0, ChunkDataSize)));
    }
    {
        shared_span<> sspan(global_no_chunk_header, true);
        const size_t ChunkDataSize = shared_span<>::chunk_size - sizeof(no_chunk_header);
        copy_data_into_span(sspan, std::span<const uint8_t>(memoryBlock0, ChunkDataSize));
        auto restricted_span = sspan.restrict(std::make_pair(7, 50));
        std::ostringstream oss;
        oss << restricted_span;
        std::string output = oss.str();
        std::istringstream iss(output);
        shared_span<> new_sspan(global_no_chunk_header, false);
        iss >> new_sspan;
        REQUIRE(check_span_data(new_sspan, std::span<const uint8_t>(memoryBlock0 + 7, 50)));
    }
}

TEST_CASE("shared_span construction from non-empty vector with end iterators", "[shared_span]") {
    // Create a non-empty vector of shared_span<>
    std::vector<shared_span<>> vec;
    shared_span<> sspan(global_no_chunk_header, true);
    vec.push_back(sspan);
    vec.push_back(sspan);
    vec.push_back(sspan);

    // Both iterators point to end()
    shared_span<> empty_span(std::next(vec.begin(), vec.size()), std::next(vec.begin(), vec.size()));

    REQUIRE(empty_span.size() == 0);
    REQUIRE(empty_span.begin<uint8_t>() == empty_span.end<uint8_t>());
}