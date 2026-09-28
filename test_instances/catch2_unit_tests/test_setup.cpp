#include "memory_pool.h"
#include "shared_chunk.h"

// Global initialization for Catch2 unit tests
// This class constructor runs once before any tests start
class TestSetup {
public:
    TestSetup() {
        // Set memory pool size for UDPChunk to 4 GB
        // This needs to be done once per executable, not per test
        memory_pool.setPoolSize<UDPChunk>(static_cast<uint64_t>(4) * 1024 * 1024 * 1024 / UDPChunk::chunk_size);
    }
};

// Global instance - constructor runs at program startup
static TestSetup test_setup;