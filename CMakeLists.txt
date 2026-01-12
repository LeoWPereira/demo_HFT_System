cmake_minimum_required(VERSION 3.20)
project(HFT_Trading_System CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Extreme optimization flags for low latency
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -ffast-math")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Performance and debugging flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined")

# Find threads library
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(COMMON_SOURCES
    src/common/timestamp.cpp
    src/common/logger.cpp
    src/common/config.cpp
)

set(MARKET_DATA_SOURCES
    src/market_data/order_book.cpp
    src/market_data/market_data_handler.cpp
)

set(TRADING_SOURCES
    src/trading/strategy.cpp
    src/trading/order_manager.cpp
)

set(NETWORK_SOURCES
    src/network/udp_receiver.cpp
    src/network/tcp_sender.cpp
)

# Main executable
add_executable(hft_trading
    src/main.cpp
    ${COMMON_SOURCES}
    ${MARKET_DATA_SOURCES}
    ${TRADING_SOURCES}
    ${NETWORK_SOURCES}
)

target_link_libraries(hft_trading PRIVATE Threads::Threads)

# Benchmarking executable
add_executable(benchmark
    benchmarks/benchmark_main.cpp
    ${COMMON_SOURCES}
    ${MARKET_DATA_SOURCES}
)

target_link_libraries(benchmark PRIVATE Threads::Threads)

# Test executables (separate since each has main())
add_executable(test_order_book
    tests/test_order_book.cpp
    ${COMMON_SOURCES}
    ${MARKET_DATA_SOURCES}
)

add_executable(test_lockfree
    tests/test_lockfree.cpp
)

add_executable(test_advanced_ds
    tests/test_advanced_ds.cpp
)

target_link_libraries(test_order_book PRIVATE Threads::Threads)
target_link_libraries(test_lockfree PRIVATE Threads::Threads)
target_link_libraries(test_advanced_ds PRIVATE Threads::Threads)
