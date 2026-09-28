#pragma once
#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/pool.hpp>
#include <mutex>
#include <typeindex>
#include <utility>
#include <map>
#include <stdexcept>
#include <memory>
#include <boost/core/demangle.hpp>
#include <boost/format.hpp>
#include <string>

using namespace std;

string format_bytes(size_t bytes);

class MemoryPool {
public:
    MemoryPool() = default;
    ~MemoryPool() {
        for (auto& pool : pools) {
            delete pool.second;
        }
    }

    template <typename T>
    T* allocate() {
        lock_guard<mutex> lock(mtx);
        auto type = type_index(typeid(T));
        if (pools.find(type) == pools.end()) {
            // Should the pool not be initialized, then use default parameters for the pool
            pools[type] = new boost::pool<>(sizeof(T));
        }
        auto find_max_allocation = max_allocations.find(type);
        auto find_allocation_count = allocation_counts.find(type);
        auto find_pool = pools.find(type);
        // If either find_pool or find_allocation_count is not found, then make sure a default pool exists, and allocation count = 0
        if (find_pool == pools.end() || find_allocation_count == allocation_counts.end()) {
            if (find_pool == pools.end()) {
                pools[type] = new boost::pool<>(sizeof(T));
                find_pool = pools.find(type);
            }
            allocation_counts[type] = 0;
            find_allocation_count = allocation_counts.find(type);
        }
        // if the allocation count is greater than the max allocation count, then throw an exception
        if (find_max_allocation != max_allocations.end() && find_allocation_count->second >= find_max_allocation->second) {
            throw runtime_error("Exceeded maximum allocation count of " + boost::core::demangle(typeid(T).name()) 
                + " objects limited to " + to_string(find_max_allocation->second) 
                + " or " + format_bytes(find_allocation_count->second * sizeof(T)));
        }
        find_allocation_count->second++;
        return static_cast<T*>(find_pool->second->malloc());
    }

    template <typename T>
    void deallocate(T* ptr) {
        lock_guard<mutex> lock(mtx);
        auto type = type_index(typeid(T));
        pools[type]->free(ptr);
        allocation_counts[type]--;
    }

    template <typename T>
    void setPoolSize(size_t max_allocation) {
        {
            lock_guard<mutex> lock(mtx);
            auto type = type_index(typeid(T));
            if (pools.find(type) != pools.end()) {
                delete pools[type];
            }
            if (max_allocation != 0) {
                size_t requested_size = sizeof(T);
                size_t next_size = max_allocation;
                size_t max_size = max_allocation; // This is intended to allocate a huge block of memory all at once on initialization
                pools[type] = new boost::pool<>(requested_size, next_size, max_size);
                max_allocations[type] = max_allocation;
                allocation_counts[type] = 0;
            }
        }
        // Allocate one, and free one, to make sure that the pool is initialized
        T* ptr = allocate<T>();
        deallocate(ptr);
    }

    template <typename T>
    tuple<size_t, size_t> getAllocationSizes() {
        lock_guard<mutex> lock(mtx);
        auto type = type_index(typeid(T));
        auto find_max_allocation = max_allocations.find(type);
        auto find_allocation_count = allocation_counts.find(type);
        size_t max_allocation = 0;
        if (find_max_allocation != max_allocations.end()) {
            max_allocation = find_max_allocation->second;
        }
        size_t allocation_count = 0;
        if (find_allocation_count != allocation_counts.end()) {
            allocation_count = find_allocation_count->second;
        }
        return make_tuple(max_allocation, allocation_count);
    }

private:
    mutex mtx;
    map<type_index, boost::pool<>*> pools;
    map<type_index, size_t> max_allocations;
    map<type_index, size_t> allocation_counts;
};

extern MemoryPool memory_pool;

