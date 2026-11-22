cmake_minimum_required(VERSION 3.16)
project(HostelCareAPI LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.2.1.2
    GIT_SHALLOW TRUE   # download only this version, not the whole history
)
FetchContent_MakeAvailable(Crow)

# Everything except main(): shared by the server and the test program.
add_library(hostelcare_core STATIC
    src/core/util.cpp
    src/core/crypto.cpp
    src/core/store_sqlite.cpp
    src/core/routes_client.cpp
    src/core/sessions.cpp
    src/core/domain.cpp
    src/http/api_core.cpp
    src/http/api_complaints.cpp
    src/http/api_admin.cpp
    src/http/api_worker.cpp
    # Reused legacy C++ modules (Dijkstra, dispatch, keyword priority)
    legacy/campusgraph.cpp
    legacy/dispatchmanager.cpp
    legacy/worker.cpp
    legacy/priorityc.cpp
)
target_include_directories(hostelcare_core PUBLIC src legacy)
target_link_libraries(hostelcare_core PUBLIC Crow::Crow)

# SQLite (database) and libcurl (Google Routes API calls)
#   Ubuntu/WSL/Docker: sudo apt install libsqlite3-dev libcurl4-openssl-dev
#   MSYS2 UCRT64:      pacman -S mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-curl
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
target_link_libraries(hostelcare_core PUBLIC SQLite::SQLite3 CURL::libcurl)

if(WIN32)
    target_link_libraries(hostelcare_core PUBLIC ws2_32 mswsock bcrypt)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(hostelcare_core PUBLIC OpenSSL::Crypto pthread)
endif()

add_executable(hostelcare_api src/api_main.cpp)
target_link_libraries(hostelcare_api PRIVATE hostelcare_core)

# In-process API tests: ./build-ucrt/hostelcare_tests.exe data
add_executable(hostelcare_tests tests/api_tests.cpp)
target_link_libraries(hostelcare_tests PRIVATE hostelcare_core)
