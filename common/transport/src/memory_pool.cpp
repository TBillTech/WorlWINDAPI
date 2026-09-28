#include "memory_pool.h"

MemoryPool memory_pool;

std::string format_bytes(size_t bytes) {
    const size_t KB = 1024;
    const size_t MB = KB * 1024;
    const size_t GB = MB * 1024;

    if (bytes >= GB) {
        return (boost::format("%.2f GB") % (static_cast<double>(bytes) / GB)).str();
    } else if (bytes >= MB) {
        return (boost::format("%.2f MB") % (static_cast<double>(bytes) / MB)).str();
    } else if (bytes >= KB) {
        return (boost::format("%.2f KB") % (static_cast<double>(bytes) / KB)).str();
    } else {
        return (boost::format("%d bytes") % bytes).str();
    }
}