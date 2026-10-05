cmake_minimum_required(VERSION 3.10)

# Project name
project(WorlWINDAPI VERSION 0.9)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable verbose output
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Debug)

# Find Boost library
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()


find_package(Boost REQUIRED COMPONENTS system iostreams)
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

# Checks for header files.
include(CheckIncludeFile)
check_include_file("arpa/inet.h"   HAVE_ARPA_INET_H)
check_include_file("sys/socket.h"  HAVE_SYS_SOCKET_H)
check_include_file("sys/un.h"      HAVE_SYS_UN_H)
check_include_file("netinet/in.h"  HAVE_NETINET_IN_H)
check_include_file("netinet/ip.h"  HAVE_NETINET_IP_H)
check_include_file("unistd.h"      HAVE_UNISTD_H)
check_include_file("sys/endian.h"  HAVE_SYS_ENDIAN_H)
check_include_file("endian.h"      HAVE_ENDIAN_H)
check_include_file("byteswap.h"    HAVE_BYTESWAP_H)
check_include_file("asm/types.h"   HAVE_ASM_TYPES_H)
if(NOT WIN32)
  check_include_file("linux/netlink.h"   HAVE_LINUX_NETLINK_H)
  check_include_file("linux/rtnetlink.h" HAVE_LINUX_RTNETLINK_H)
endif()

# Print the results of the checks
message(STATUS "HAVE_ARPA_INET_H: ${HAVE_ARPA_INET_H}")
message(STATUS "HAVE_SYS_SOCKET_H: ${HAVE_SYS_SOCKET_H}")
message(STATUS "HAVE_SYS_UN_H: ${HAVE_SYS_UN_H}")
message(STATUS "HAVE_NETINET_IN_H: ${HAVE_NETINET_IN_H}")
message(STATUS "HAVE_NETINET_IP_H: ${HAVE_NETINET_IP_H}")
message(STATUS "HAVE_UNISTD_H: ${HAVE_UNISTD_H}")
message(STATUS "HAVE_SYS_ENDIAN_H: ${HAVE_SYS_ENDIAN_H}")
message(STATUS "HAVE_ENDIAN_H: ${HAVE_ENDIAN_H}")
message(STATUS "HAVE_BYTESWAP_H: ${HAVE_BYTESWAP_H}")
message(STATUS "HAVE_ASM_TYPES_H: ${HAVE_ASM_TYPES_H}")
message(STATUS "HAVE_LINUX_NETLINK_H: ${HAVE_LINUX_NETLINK_H}")
message(STATUS "HAVE_LINUX_RTNETLINK_H: ${HAVE_LINUX_RTNETLINK_H}")

# The check_include_file() results above are only usable by the build if actually passed to
# the compiler as defines; wire up each detected header as a compile definition so the
# corresponding #ifdef HAVE_..._H guards in the source (e.g. network.h) take effect.
foreach(WORLWIND_HAVE_VAR
    HAVE_ARPA_INET_H HAVE_SYS_SOCKET_H HAVE_SYS_UN_H HAVE_NETINET_IN_H
    HAVE_NETINET_IP_H HAVE_UNISTD_H HAVE_SYS_ENDIAN_H HAVE_ENDIAN_H
    HAVE_BYTESWAP_H HAVE_ASM_TYPES_H HAVE_LINUX_NETLINK_H HAVE_LINUX_RTNETLINK_H)
  if(${WORLWIND_HAVE_VAR})
    add_compile_definitions(${WORLWIND_HAVE_VAR})
  endif()
endforeach()

# pthread_rwlock_t on Linux requires a feature flag. We limit this to Linux
# because, on Apple platforms, it instead disables APIs we use. See compat(5)
# and sys/cdefs.h. Reportedly, FreeBSD also breaks when this is set. See
# https://crbug.com/boringssl/471.
#if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_XOPEN_SOURCE=700")
#endif()

# cd libraries
# git clone https://github.com/Dobiasd/FunctionalPlus
# cmake -S FunctionalPlus -B FunctionalPlus/build
# cmake --build FunctionalPlus/build
# sudo cmake --install FunctionalPlus/build
find_package(FunctionalPlus REQUIRED)

# Include directories
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/interface
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/include
    ${PROJECT_SOURCE_DIR}/common/transport/include 
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/structure
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/http3_client
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/file
    ${PROJECT_SOURCE_DIR}/common/frontend/http3_server_cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/backend_tester_cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/mediator_cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/wwatp_service_cpp
    ${PROJECT_SOURCE_DIR}/common/frontend
    ${PROJECT_SOURCE_DIR}/test_instances/catch2_unit_tests
)

# Add common source files
set(COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/common/transport/src/memory_pool.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/shared_chunk.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/urlparse.c
    ${PROJECT_SOURCE_DIR}/common/transport/src/communication.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/socket_communication.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/interface/tree_node.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/src/memory_tree.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/src/simple_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/src/transactional_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/src/threadsafe_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/memory/src/transactional_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/structure/composite_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/structure/redirected_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/http3_client/http3_client_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/interface/http3_tree_message.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/interface/http3_tree_message_helpers.cpp
    ${PROJECT_SOURCE_DIR}/common/backend_cpp/file/file_backend.cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/http3_server_cpp/http3_server.cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/mediator_cpp/cloning_mediator.cpp
    ${PROJECT_SOURCE_DIR}/common/frontend/mediator_cpp/yaml_mediator.cpp
)

if(MINGW)
  add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX NOCRYPT NOGDI)
endif()

# Add test executables
add_executable(communicate_tester ${COMMON_SOURCES} test_instances/communicate_tester/communicate_tester.cpp)
add_executable(memory_pool_tester ${COMMON_SOURCES} test_instances/memory_pool_tester/memory_pool_tester.cpp)
add_executable(catch2_unit_tests
    ${COMMON_SOURCES}
    test_instances/catch2_unit_tests/http3_message_tests.cpp
    test_instances/catch2_unit_tests/shared_chunk_tests.cpp
    test_instances/catch2_unit_tests/backend_cpp_tests.cpp
    test_instances/catch2_unit_tests/backend_testbed.cpp
    test_instances/catch2_unit_tests/http3_message_helper_tests.cpp
    test_instances/catch2_unit_tests/frontend_cpp_tests.cpp
    test_instances/catch2_unit_tests/yaml_tests.cpp
    test_instances/catch2_unit_tests/tree_node_tests.cpp
)

# Define preprocessor macros

include(ProcessorCount)
ProcessorCount(WORLWIND_PROCESSOR_COUNT)
if(NOT WORLWIND_PROCESSOR_COUNT)
  set(WORLWIND_PROCESSOR_COUNT 1)
endif()

set(WORLWIND_DEP_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM}")
get_filename_component(WORLWIND_MAKE_PROGRAM_NAME "${WORLWIND_DEP_MAKE_PROGRAM}" NAME)
string(TOLOWER "${WORLWIND_MAKE_PROGRAM_NAME}" WORLWIND_MAKE_PROGRAM_NAME_LOWER)

# On MinGW, invoking mingw32-make.exe directly as a Windows process (rather than from an
# MSYS2 shell) breaks the autotools/libtool recipes for nghttp3/ngtcp2 (their generated
# shell functions like `fail`/`eval` are misinterpreted), and parallel jobs (-j) have also
# been observed to corrupt recipe shells under this make. Route the build through MSYS2's
# bash (via scripts/mingw-dep-build.sh, which also normalizes generated-file timestamps to
# avoid automake's auto-remake rule) and build serially there.
if(MINGW)
  find_program(WORLWIND_MSYS2_BASH NAMES bash REQUIRED)
  set(WORLWIND_DEP_MAKE_JOBS_FLAG "")
  set(WORLWIND_MINGW_DEP_BUILD_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/mingw-dep-build.sh")
  set(WORLWIND_NGHTTP3_BUILD_COMMAND ${WORLWIND_MSYS2_BASH} ${WORLWIND_MINGW_DEP_BUILD_SCRIPT} ${WORLWIND_DEP_MAKE_PROGRAM})
  set(WORLWIND_NGTCP2_BUILD_COMMAND ${WORLWIND_MSYS2_BASH} ${WORLWIND_MINGW_DEP_BUILD_SCRIPT} ${WORLWIND_DEP_MAKE_PROGRAM})
else()
  set(WORLWIND_DEP_MAKE_JOBS_FLAG "-j${WORLWIND_PROCESSOR_COUNT}")
  set(WORLWIND_NGHTTP3_BUILD_COMMAND ${WORLWIND_DEP_MAKE_PROGRAM} ${WORLWIND_DEP_MAKE_JOBS_FLAG})
  set(WORLWIND_NGTCP2_BUILD_COMMAND ${WORLWIND_DEP_MAKE_PROGRAM} ${WORLWIND_DEP_MAKE_JOBS_FLAG})
endif()

set(WORLWIND_LIB_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
set(WORLWIND_LIB_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4 # or latest stable
)
FetchContent_MakeAvailable(Catch2)

# Make Catch2's CTest integration module available
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Link libraries (if any)
set(WORLWIND_COMMON_LINK_LIBS
    ${Boost_LIBRARIES}
    FunctionalPlus::fplus
    yaml-cpp::yaml-cpp
)

target_link_libraries(communicate_tester ${WORLWIND_COMMON_LINK_LIBS} ${WORLWIND_PLATFORM_LINK_LIBS})
target_link_libraries(memory_pool_tester ${WORLWIND_COMMON_LINK_LIBS} ${WORLWIND_PLATFORM_LINK_LIBS})
target_link_libraries(catch2_unit_tests PRIVATE ${WORLWIND_COMMON_LINK_LIBS} ${WORLWIND_PLATFORM_LINK_LIBS} Catch2::Catch2WithMain)
# Enable verbose linker output
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--verbose")

# Enable testing and have CTest discover Catch2 test cases from our executable
enable_testing()
catch_discover_tests(catch2_unit_tests)
add_test(NAME communicate_tester COMMAND communicate_tester)

