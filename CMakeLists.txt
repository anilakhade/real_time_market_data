cmake_minimum_required(VERSION 3.16)
project(alpha LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)

# Core library
add_library(alpha_lib
    src/config.cpp
    src/logger.cpp
    src/http_client.cpp
    src/auth.cpp
    src/totp.cpp
    src/websocket_client.cpp
    src/subscription_manager.cpp
    src/ingest_queue.cpp
    src/parser.cpp
    src/ltp_store.cpp
    src/consumer.cpp
    src/sharder.cpp
)
target_include_directories(alpha_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(alpha_lib
    PRIVATE
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Tests
add_executable(cfg_test tests/cfg_test.cpp)
target_link_libraries(cfg_test PRIVATE alpha_lib)

add_executable(logger_test tests/logger_test.cpp)
target_link_libraries(logger_test PRIVATE alpha_lib)

add_executable(http_client_test tests/http_client_test.cpp)
target_link_libraries(http_client_test PRIVATE alpha_lib)

add_executable(auth_test tests/auth_test.cpp)
target_link_libraries(auth_test PRIVATE alpha_lib)

add_executable(totp_test tests/totp_test.cpp)
target_link_libraries(totp_test PRIVATE alpha_lib)

add_executable(websocket_client_test tests/websocket_client_test.cpp)
target_link_libraries(websocket_client_test PRIVATE alpha_lib)

add_executable(subscription_manager_test tests/subscription_manager_test.cpp)
target_link_libraries(subscription_manager_test PRIVATE alpha_lib)

add_executable(ingest_queue_test tests/ingest_queue_test.cpp)
target_link_libraries(ingest_queue_test PRIVATE alpha_lib)

add_executable(parser_test tests/parser_test.cpp)
target_link_libraries(parser_test PRIVATE alpha_lib)

add_executable(consumer_test tests/consumer_test.cpp)
target_link_libraries(consumer_test PRIVATE alpha_lib)

add_executable(sharder_test tests/sharder_test.cpp)
target_link_libraries(sharder_test PRIVATE alpha_lib)

