// The memory pool tester is a test program for the memory pool class, which is a class that manages a pool of memory for a given type.
// The memory pool tester contains several classes that are stored in vectors and lists, and then uses the memory pool to allocate them.

// This program is careful to use new and delete, and the memory pool to allocate and deallocate memory, and verify that
// no memory leaks or page faults occur due to mistakes in allocated pointer life patterns.

// One crucial aspect of the memory pool is that before any type can be used with the memory pool, the pool size must be set for that type.
// This is done by calling the setPoolSize function on the memory pool object, with the type and the number of elements that should be able to be allocated.

#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <cstdint>
#include <cstring>
#include <fstream>
#include "memory_pool.h"

// Function to print memory usage
void print_memory_usage() {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.find("VmRSS:") == 0 || line.find("VmSize:") == 0) {
            std::cout << line << std::endl;
        }
    }
}

// Here is the test class, which has a prime number size in bytes, and is allocated with new and deleted with delete.

template<int classSize>
class PrimeSizedClass {
    public:
    PrimeSizedClass() : size(classSize) {
        memset(data, 0, classSize);
    }
    ~PrimeSizedClass() {
    }
    uint8_t data[classSize];
    size_t const size;

    // operator overload for new and delete should use the memory pool
    void* operator new(size_t size) {
        return memory_pool.allocate<PrimeSizedClass<classSize>>()->data;
    }
    void operator delete(void* ptr) {
        memory_pool.deallocate<PrimeSizedClass<classSize>>(reinterpret_cast<PrimeSizedClass<classSize>*>(ptr));
    }
};

using PrimeSizedClass1 = PrimeSizedClass<101>;
using PrimeSizedClass2 = PrimeSizedClass<1003>;
constexpr size_t primeSize2Count = 1000000;
using PrimeSizedClassListLike = PrimeSizedClass<33>;

void init_sizes() {
    print_memory_usage();
    memory_pool.setPoolSize<PrimeSizedClass2>(primeSize2Count);
    // Test that the PrimeSizedClass2 pool size is really set to primeSize2Count objects
    auto pool_sizes = memory_pool.getAllocationSizes<PrimeSizedClass2>();
    // pool_sizes should be <primeSize2Count, 0, primeSize2Count>
    if (std::get<0>(pool_sizes) != primeSize2Count || std::get<1>(pool_sizes) != 0) {
        cerr << "Failed to set pool size for PrimeSizedClass2" << endl;
        exit(1);
    }
    // Print out the current memory used by this program
    print_memory_usage();
    
    // Also wish to test strings
    memory_pool.setPoolSize<string>(10000);
}

bool test_single_allocations() {
    // Test that the memory pool can allocate and deallocate a single object of each type
    auto psc1 = new PrimeSizedClass1();
    auto psc2 = new PrimeSizedClass2();
    auto str = new string("Hello, World!");
    delete psc1;
    delete psc2;
    delete str;
    return true;
}

bool test_dozen_allocations() {
    // Test that the memory pool can allocate and deallocate a dozen objects of each type in a mixed up order
    vector<PrimeSizedClass1*> psc1s;
    vector<PrimeSizedClass2*> psc2s;
    vector<string*> strs;
    for (int i = 0; i < 12; i++) {
        psc1s.push_back(new PrimeSizedClass1());
        psc2s.push_back(new PrimeSizedClass2());
        strs.push_back(new string("Hello, World: " + to_string(i)));
    }
    for (int i = 0; i < 6; i++) {
        delete psc1s.back();
        psc1s.pop_back();
        delete psc2s.back();
        psc2s.pop_back();
        delete strs.back();
        strs.pop_back();
    }
    for (int i = 0; i < 6; i++) {
        psc1s.push_back(new PrimeSizedClass1());
        psc2s.push_back(new PrimeSizedClass2());
        strs.push_back(new string("Hello, World: " + to_string(i)));
    }
    while (!psc1s.empty()) {
        delete psc1s.back();
        psc1s.pop_back();
    }
    while (!psc2s.empty()) {
        delete psc2s.back();
        psc2s.pop_back();
    }
    while (!strs.empty()) {
        delete strs.back();
        strs.pop_back();
    }
    return true;
}

bool test_max_pool_allocations() {
    // Test that the memory pool can allocate and deallocate the maximum number of objects of each type
    vector<PrimeSizedClass2*> psc2s;
    for (int i = 0; i < primeSize2Count; i++) {
        psc2s.push_back(new PrimeSizedClass2());
    }
    // Verify that one more allocation throws an exception
    try {
        auto psc2 = new PrimeSizedClass2();
        delete psc2;
        cerr << "Failed to throw exception on overallocation" << endl;
        return false;
    } catch (runtime_error& e) {
        // Expected
        cout << "Caught exception: " << e.what() << endl;
    }
    for (int i = 0; i < primeSize2Count; i++) {
        delete psc2s.back();
        psc2s.pop_back();
    }
    return true;
}

int main() {
    init_sizes();

    if (!test_single_allocations()) {
        cerr << "Failed single allocations test" << endl;
        return 1;
    }

    if (!test_dozen_allocations()) {
        cerr << "Failed dozen allocations test" << endl;
        return 1;
    }

    if (!test_max_pool_allocations()) {
        cerr << "Failed max pool allocations test" << endl;
        return 1;
    }

    cout << "Memory pool test passed" << endl;
    return 0;
}

