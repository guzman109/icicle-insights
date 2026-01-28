cmake_minimum_required(VERSION 3.25)
project(icicle-insights VERSION 0.1.0 LANGUAGES CXX)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Glaze Dev Build (FetchContent for testing new features)
include(FetchContent)

fetchcontent_declare(
  glaze
  GIT_REPOSITORY https://github.com/stephenberry/glaze.git
  GIT_TAG tls_http_client
  GIT_SHALLOW TRUE
)

fetchcontent_makeavailable(glaze)

# Find Conan-provided dependencies
find_package(asio REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(libpqxx REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)

file(GLOB git_SRC src/git/*.cpp)
file(GLOB server_SRC src/server/*.cpp)

# Executable
add_executable(${PROJECT_NAME}
    src/insights.cpp
    ${git_SRC}
    ${server_SRC}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    asio::asio
    glaze::glaze
    libpq::libpq
    libpqxx::pqxx
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLZ_ENABLE_SSL
)
