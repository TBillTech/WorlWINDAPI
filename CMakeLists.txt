cmake_minimum_required(VERSION 3.10)

# Project name
project(WWATP)

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
foreach(WWATP_HAVE_VAR
    HAVE_ARPA_INET_H HAVE_SYS_SOCKET_H HAVE_SYS_UN_H HAVE_NETINET_IN_H
    HAVE_NETINET_IP_H HAVE_UNISTD_H HAVE_SYS_ENDIAN_H HAVE_ENDIAN_H
    HAVE_BYTESWAP_H HAVE_ASM_TYPES_H HAVE_LINUX_NETLINK_H HAVE_LINUX_RTNETLINK_H)
  if(${WWATP_HAVE_VAR})
    add_compile_definitions(${WWATP_HAVE_VAR})
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
    ${PROJECT_SOURCE_DIR}/libraries/boringssl/include
    # Vendored, version-pinned headers -- must take precedence over any system-installed
    # ngtcp2/nghttp3 packages (e.g. MSYS2's mingw-w64-clang-x86_64-ngtcp2/nghttp3), which
    # are a different version than the static libs built from libraries/ and would
    # otherwise be silently picked up via the compiler's default include search path.
    ${PROJECT_SOURCE_DIR}/libraries/ngtcp2/lib/includes
    ${PROJECT_SOURCE_DIR}/libraries/ngtcp2/crypto/includes
    ${PROJECT_SOURCE_DIR}/libraries/nghttp3/lib/includes
    ${PROJECT_SOURCE_DIR}/test_instances/catch2_unit_tests
)

# Add common source files
set(COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/common/transport/src/memory_pool.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/shared_chunk.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/tcp_communication.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/quic_listener.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/quic_connector.cpp
    ${PROJECT_SOURCE_DIR}/common/transport/src/server_base.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/client_base.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/config_base.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_server_session_boringssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_server_context_boringssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_client_session_boringssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_client_context_boringssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/util.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/http.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/util_openssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_session_base_quictls.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/tls_shared_boringssl.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/urlparse.c
    ${PROJECT_SOURCE_DIR}/common/transport/src/debug.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/shared.cc
    ${PROJECT_SOURCE_DIR}/common/transport/src/siphash.cc
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
    ${PROJECT_SOURCE_DIR}/common/frontend/wwatp_service_cpp/wwatp_service.cpp
)

if(MINGW)
  # Winsock-backed msghdr/recvmsg/sendmsg shim used by quic_listener.cpp/quic_connector.cpp.
  list(APPEND COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/transport/src/win_socket_compat.cc)
  # NOCRYPT/NOGDI keep <windows.h> (pulled in transitively via iphlpapi.h/winsock2.h) from
  # defining wincrypt.h macros like X509_NAME/PKCS7_SIGNER_INFO, which otherwise collide
  # with BoringSSL's identically-named OpenSSL-compatible types. Must be project-wide (not
  # just in win_socket_compat.h) since header include order varies by translation unit.
  add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX NOCRYPT NOGDI)
endif()

# Add server executable
add_executable(wwatp_server ${COMMON_SOURCES} server/main.cpp)

# Add test executables
add_executable(library_tester ${COMMON_SOURCES} test_instances/library_tester/library_tester.cpp)
add_executable(communicate_tester ${COMMON_SOURCES} test_instances/communicate_tester/communicate_tester.cpp)
add_executable(memory_pool_tester ${COMMON_SOURCES} test_instances/memory_pool_tester/memory_pool_tester.cpp)
add_executable(http3_client_backend_tester
    ${COMMON_SOURCES}
    test_instances/http3_client_backend_tester/http3_client_backend_tester.cpp
    test_instances/catch2_unit_tests/backend_testbed.cpp
)
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

target_compile_definitions(library_tester PRIVATE
    WWATP_USE_QUIC_TRANSPORT=$<BOOL:${BUILD_WWATP_QUIC_C}>
)
target_compile_definitions(communicate_tester PRIVATE
    WWATP_USE_QUIC_TRANSPORT=$<BOOL:${BUILD_WWATP_QUIC_C}>
)

# Define preprocessor macros
#add_definitions(-DENABLE_EXAMPLE_BORINGSSL -DWITH_EXAMPLE_BORINGSSL -DHAVE_ARPA_INET_H)

# Include ExternalProject module
include(ExternalProject)
include(ProcessorCount)
ProcessorCount(WWATP_PROCESSOR_COUNT)
if(NOT WWATP_PROCESSOR_COUNT)
  set(WWATP_PROCESSOR_COUNT 1)
endif()

set(WWATP_DEP_MAKE_PROGRAM "${CMAKE_MAKE_PROGRAM}")
if(NOT WWATP_DEP_MAKE_PROGRAM)
  message(FATAL_ERROR
    "CMAKE_MAKE_PROGRAM is not set. This project's nghttp3/ngtcp2 external builds require a make-compatible tool.")
endif()
get_filename_component(WWATP_MAKE_PROGRAM_NAME "${WWATP_DEP_MAKE_PROGRAM}" NAME)
string(TOLOWER "${WWATP_MAKE_PROGRAM_NAME}" WWATP_MAKE_PROGRAM_NAME_LOWER)
if(NOT WWATP_MAKE_PROGRAM_NAME_LOWER MATCHES "^(make|gmake|mingw32-make)(\\.exe)?$")
  message(FATAL_ERROR
    "This build requires a make-compatible tool for nghttp3/ngtcp2 external builds. "
    "Detected make tool '${WWATP_DEP_MAKE_PROGRAM}'. "
    "Use a make-compatible generator/tool (e.g., Unix Makefiles on Linux, MinGW Makefiles under msys64).")
endif()

# On MinGW, invoking mingw32-make.exe directly as a Windows process (rather than from an
# MSYS2 shell) breaks the autotools/libtool recipes for nghttp3/ngtcp2 (their generated
# shell functions like `fail`/`eval` are misinterpreted), and parallel jobs (-j) have also
# been observed to corrupt recipe shells under this make. Route the build through MSYS2's
# bash (via scripts/mingw-dep-build.sh, which also normalizes generated-file timestamps to
# avoid automake's auto-remake rule) and build serially there.
if(MINGW)
  find_program(WWATP_MSYS2_BASH NAMES bash REQUIRED)
  set(WWATP_DEP_MAKE_JOBS_FLAG "")
  set(WWATP_MINGW_DEP_BUILD_SCRIPT "${PROJECT_SOURCE_DIR}/scripts/mingw-dep-build.sh")
  set(WWATP_NGHTTP3_BUILD_COMMAND ${WWATP_MSYS2_BASH} ${WWATP_MINGW_DEP_BUILD_SCRIPT} ${WWATP_DEP_MAKE_PROGRAM})
  set(WWATP_NGTCP2_BUILD_COMMAND ${WWATP_MSYS2_BASH} ${WWATP_MINGW_DEP_BUILD_SCRIPT} ${WWATP_DEP_MAKE_PROGRAM})
else()
  set(WWATP_DEP_MAKE_JOBS_FLAG "-j${WWATP_PROCESSOR_COUNT}")
  set(WWATP_NGHTTP3_BUILD_COMMAND ${WWATP_DEP_MAKE_PROGRAM} ${WWATP_DEP_MAKE_JOBS_FLAG})
  set(WWATP_NGTCP2_BUILD_COMMAND ${WWATP_DEP_MAKE_PROGRAM} ${WWATP_DEP_MAKE_JOBS_FLAG})
endif()

set(WWATP_LIB_PREFIX "${CMAKE_STATIC_LIBRARY_PREFIX}")
set(WWATP_LIB_SUFFIX "${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(BORINGSSL_SSL_LIB "${PROJECT_SOURCE_DIR}/libraries/boringssl/build/${WWATP_LIB_PREFIX}ssl${WWATP_LIB_SUFFIX}")
set(BORINGSSL_CRYPTO_LIB "${PROJECT_SOURCE_DIR}/libraries/boringssl/build/${WWATP_LIB_PREFIX}crypto${WWATP_LIB_SUFFIX}")
set(NGHTTP3_STATIC_LIB "${PROJECT_SOURCE_DIR}/libraries/nghttp3/lib/.libs/${WWATP_LIB_PREFIX}nghttp3${WWATP_LIB_SUFFIX}")
set(NGTCP2_STATIC_LIB "${PROJECT_SOURCE_DIR}/libraries/ngtcp2/lib/.libs/${WWATP_LIB_PREFIX}ngtcp2${WWATP_LIB_SUFFIX}")
set(NGTCP2_CRYPTO_STATIC_LIB "${PROJECT_SOURCE_DIR}/libraries/ngtcp2/crypto/boringssl/${WWATP_LIB_PREFIX}ngtcp2_crypto_boringssl${WWATP_LIB_SUFFIX}")
set(WWATP_SUPPORTS_QUIC_C_SHARED_DEPS FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(WWATP_SUPPORTS_QUIC_C_SHARED_DEPS TRUE)
  set(NGTCP2_CRYPTO_SHARED_LIB "${PROJECT_SOURCE_DIR}/libraries/ngtcp2/crypto/boringssl/.build/libngtcp2_crypto_boringssl.so")
endif()

if(MINGW)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/libraries/boringssl/CMakeLists.txt")
    message(FATAL_ERROR
      "MinGW build requires vendored BoringSSL at libraries/boringssl. "
      "Initialize/populate the libraries directory before configuring.")
  endif()
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/libraries/nghttp3/Makefile")
    message(FATAL_ERROR
      "MinGW build requires libraries/nghttp3/Makefile for the external nghttp3 build. "
      "Generate it under msys64 first (for example: autoreconf -i && ./configure in libraries/nghttp3), then rerun CMake.")
  endif()
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/libraries/ngtcp2/Makefile")
    message(FATAL_ERROR
      "MinGW build requires libraries/ngtcp2/Makefile for the external ngtcp2 build. "
      "Generate it under msys64 first (for example: autoreconf -i && ./configure ... --with-boringssl in libraries/ngtcp2), then rerun CMake.")
  endif()
endif()

# Clone and build BoringSSL
# git clone https://boringssl.googlesource.com/boringssl
# cd boringssl
# git checkout 294ab9730c570213b496cfc2fc14b3c0bfcd4bcc
# cmake -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON
# make -j$(nproc) -C build
ExternalProject_Add(
    boringssl
    PREFIX ${PROJECT_SOURCE_DIR}/libraries/boringssl
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/libraries/boringssl
    BINARY_DIR ${PROJECT_SOURCE_DIR}/libraries/boringssl
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -S ${PROJECT_SOURCE_DIR}/libraries/boringssl -B ${PROJECT_SOURCE_DIR}/libraries/boringssl/build -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    BUILD_COMMAND ${CMAKE_COMMAND} --build ${PROJECT_SOURCE_DIR}/libraries/boringssl/build --parallel ${WWATP_PROCESSOR_COUNT}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${BORINGSSL_SSL_LIB} ${BORINGSSL_CRYPTO_LIB}
)
# Create an imported target for the BoringSSL library
add_library(boringssl_lib STATIC IMPORTED)
set_target_properties(boringssl_lib PROPERTIES
    IMPORTED_LOCATION ${BORINGSSL_SSL_LIB}
)
add_library(boringcrypto_lib STATIC IMPORTED)
set_target_properties(boringcrypto_lib PROPERTIES
    IMPORTED_LOCATION ${BORINGSSL_CRYPTO_LIB}
)


# nghttp3 library
# cd libraries
# git clone https://github.com/ngtcp2/nghttp3
# cd nghttp3
# git submodule update --init
# autoreconf -i
# ./configure
# make
# sudo make install
ExternalProject_Add(
    nghttp3
    PREFIX ${PROJECT_SOURCE_DIR}/libraries/nghttp3
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/libraries/nghttp3
    BINARY_DIR ${PROJECT_SOURCE_DIR}/libraries/nghttp3
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${WWATP_NGHTTP3_BUILD_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${NGHTTP3_STATIC_LIB}
)
# Create an imported target for the nghttp3 library
add_library(nghttp3_lib STATIC IMPORTED)
set_target_properties(nghttp3_lib PROPERTIES
    IMPORTED_LOCATION ${NGHTTP3_STATIC_LIB}
)

# Clone and build ngtcp2
# git clone --recursive  https://github.com/ngtcp2/ngtcp2
# cd ngtcp2
# autoreconf -i
# ./configure PKG_CONFIG_PATH=$PWD/../nghttp3/lib \
#     BORINGSSL_LIBS="-L$PWD/../boringssl/build/ssl -lssl -L$PWD/../boringssl/build/crypto -lcrypto" \
#     BORINGSSL_CFLAGS="-I$PWD/../boringssl/include"\
#     --with-boringssl
# make
# And for the shared library libngtcp2_crypto_boringssl.so:
# from repo root (or cd libraries/ngtcp2/crypto/boringssl first)
# mkdir -p libraries/ngtcp2/crypto/boringssl/build
# cd libraries/ngtcp2/crypto/boringssl/build

# cmake .. \
#   -DENABLE_SHARED_LIB=ON \
#   -DENABLE_STATIC_LIB=OFF \
#   -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
#   -DBORINGSSL_INCLUDE_DIRS=/mnt/d/code/wwatp/libraries/boringssl/include \
#   -DBORINGSSL_LIBRARIES="/mnt/d/code/wwatp/libraries/boringssl/build/libssl.a;/mnt/d/code/wwatp/libraries/boringssl/build/libcrypto.a"

# # build the shared target (name is ngtcp2_crypto_boringssl)
# make -j ngtcp2_crypto_boringssl

# # optional: see the output
# ls -l libngtcp2_crypto_boringssl.so* || ls -l .
ExternalProject_Add(
    ngtcp2
    PREFIX ${PROJECT_SOURCE_DIR}/libraries/ngtcp2
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/libraries/ngtcp2
    BINARY_DIR ${PROJECT_SOURCE_DIR}/libraries/ngtcp2
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${WWATP_NGTCP2_BUILD_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${NGTCP2_STATIC_LIB}
)
# Create an imported target for the ngtcp2 library
add_library(ngtcp2_lib STATIC IMPORTED)
set_target_properties(ngtcp2_lib PROPERTIES
    IMPORTED_LOCATION ${NGTCP2_STATIC_LIB}
)
# Create an imported target for the ngtcp2 library
add_library(ngtcp2_crypto_lib STATIC IMPORTED)
set_target_properties(ngtcp2_crypto_lib PROPERTIES
    IMPORTED_LOCATION ${NGTCP2_CRYPTO_STATIC_LIB}
)

# Shared variant of ngtcp2 crypto glue (built in subdir with CMake)
if(WWATP_SUPPORTS_QUIC_C_SHARED_DEPS)
    add_library(ngtcp2_crypto_boringssl_so SHARED IMPORTED)
    set_target_properties(ngtcp2_crypto_boringssl_so PROPERTIES
        IMPORTED_LOCATION ${NGTCP2_CRYPTO_SHARED_LIB}
    )
endif()

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

# Ensure executables depend on external libraries
add_dependencies(wwatp_server ngtcp2 nghttp3 boringssl)
add_dependencies(library_tester ngtcp2 nghttp3 boringssl)
add_dependencies(communicate_tester ngtcp2 nghttp3 boringssl)
add_dependencies(memory_pool_tester ngtcp2 nghttp3 boringssl)
add_dependencies(http3_client_backend_tester ngtcp2 nghttp3 boringssl Catch2)
add_dependencies(catch2_unit_tests ngtcp2 nghttp3 boringssl Catch2)

# Link libraries (if any)
set(WWATP_COMMON_LINK_LIBS
    ngtcp2_lib
    nghttp3_lib
    boringssl_lib
    boringcrypto_lib
    ngtcp2_crypto_lib
    ${Boost_LIBRARIES}
    FunctionalPlus::fplus
    yaml-cpp::yaml-cpp
)

if(MINGW)
    find_library(WWATP_LIBEV_LIBRARY NAMES ev)
    if(NOT WWATP_LIBEV_LIBRARY)
        message(FATAL_ERROR
            "MinGW build requires libev, but it was not found. "
            "Install mingw-w64-x86_64-libev (msys64) or provide libev via CMAKE_PREFIX_PATH/CMAKE_LIBRARY_PATH.")
    endif()
    set(WWATP_PLATFORM_LINK_LIBS ws2_32 iphlpapi "${WWATP_LIBEV_LIBRARY}")
elseif(WIN32)
    message(FATAL_ERROR "Native Windows builds are supported only with MinGW in msys64 (MINGW=TRUE).")
else()
    set(WWATP_PLATFORM_LINK_LIBS Threads::Threads dl m ev)
endif()

target_link_libraries(wwatp_server ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS})
target_link_libraries(library_tester ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS})
target_link_libraries(communicate_tester ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS})
target_link_libraries(memory_pool_tester ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS})
target_link_libraries(http3_client_backend_tester PRIVATE ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS} Catch2::Catch2)
target_link_libraries(catch2_unit_tests PRIVATE ${WWATP_COMMON_LINK_LIBS} ${WWATP_PLATFORM_LINK_LIBS} Catch2::Catch2WithMain)
target_compile_options(http3_client_backend_tester PRIVATE -g -O0 -fsanitize=address,undefined -Wall -Wextra -Wpedantic)
target_link_options(http3_client_backend_tester PRIVATE -fsanitize=address,undefined)
# Enable verbose linker output
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--verbose")

# Enable testing and have CTest discover Catch2 test cases from our executable
enable_testing()
catch_discover_tests(catch2_unit_tests)
add_test(NAME communicate_tester COMMAND communicate_tester)

# Optional: Build C-compatible QUIC client facade for Node bindings (FFI/N-API)
# option(BUILD_WWATP_QUIC_C "Build C facade shared lib for QuicConnector" ON)
# if(BUILD_WWATP_QUIC_C)
#     # Build C facade shared library, linking to transport/client sources
#     add_library(wwatp_quic_c SHARED ${COMMON_SOURCES}
#         ${PROJECT_SOURCE_DIR}/common/transport/src/quic_connector_c.cc
#     )
#     target_include_directories(wwatp_quic_c PUBLIC ${PROJECT_SOURCE_DIR}/common/transport/include ${Boost_INCLUDE_DIRS})
#     add_dependencies(wwatp_quic_c ngtcp2 nghttp3 boringssl)
#     # Prefer shared objects for ngtcp2/nghttp3 to satisfy -fPIC in shared lib
#     set(NGTCP2_SO ${PROJECT_SOURCE_DIR}/libraries/ngtcp2/lib/.libs/libngtcp2.so)
#     set(NGHTTP3_SO ${PROJECT_SOURCE_DIR}/libraries/nghttp3/lib/.libs/libnghttp3.so)
#     target_link_libraries(wwatp_quic_c PRIVATE ${NGTCP2_SO} ${NGHTTP3_SO} ngtcp2_crypto_boringssl_so boringssl_lib boringcrypto_lib ${Boost_LIBRARIES} ${WWATP_PLATFORM_LINK_LIBS} FunctionalPlus::fplus yaml-cpp::yaml-cpp)
#     # Ensure PIC for any objects we compile into this shared library
#     set_target_properties(wwatp_quic_c PROPERTIES POSITION_INDEPENDENT_CODE ON)
#     # Help the runtime linker find dependent shared libraries during development.
#     set_target_properties(wwatp_quic_c PROPERTIES BUILD_RPATH "${PROJECT_SOURCE_DIR}/libraries/ngtcp2/lib/.libs;${PROJECT_SOURCE_DIR}/libraries/nghttp3/lib/.libs;${PROJECT_SOURCE_DIR}/libraries/ngtcp2/crypto/boringssl/.build")
#     set_target_properties(wwatp_quic_c PROPERTIES OUTPUT_NAME "wwatp_quic")
# endif()