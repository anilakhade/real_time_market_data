cmake_minimum_required(VERSION 3.16)
project(alpha LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)

# library
add_library(alpha_lib
    src/config.cpp
    src/logger.cpp
    src/http_client.cpp
)
target_include_directories(alpha_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(alpha_lib
    PRIVATE
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# tests
add_executable(cfg_test tests/cfg_test.cpp)
target_link_libraries(cfg_test PRIVATE alpha_lib nlohmann_json::nlohmann_json)
target_include_directories(cfg_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(logger_test tests/logger_test.cpp)
target_link_libraries(logger_test PRIVATE alpha_lib nlohmann_json::nlohmann_json)
target_include_directories(logger_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(http_client_test tests/http_client_test.cpp)
target_link_libraries(http_client_test PRIVATE alpha_lib nlohmann_json::nlohmann_json)
target_include_directories(http_client_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

