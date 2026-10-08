# Taiwanese Enterprises LLC backend build.
# Needs a C++17 compiler, CMake, and the SQLite dev headers.

cmake_minimum_required(VERSION 3.16)
project(te_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(te_server
    src/main.cpp
    src/quote_store.cpp
)

# Crow is a single header in third_party, and it needs the standalone
# Asio headers, also vendored under third_party/asio. No library to find.
target_include_directories(te_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# sqlite3 for quote storage, pthread for the Crow server threads.
target_link_libraries(te_server PRIVATE sqlite3 pthread)
