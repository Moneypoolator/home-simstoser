cmake_minimum_required(VERSION 3.15)
project(s3_prototype VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Найти зависимости
find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(glog REQUIRED)
find_package(Threads REQUIRED)
find_package(GTest QUIET)


# Исходники
set(sources
    src/main.cpp
    src/server.cpp
    src/file_manager.cpp
    src/request_handler.cpp
    src/authenticator.cpp
    src/authorizer.cpp
)

# Заголовки
set(headers
    include/server.hpp
    include/file_manager.hpp
    include/request_handler.hpp
    include/authenticator.hpp
    include/authorizer.hpp
)

# Executable
add_executable(s3_server ${sources} ${headers})

# Включаем директории
target_include_directories(s3_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${GLOG_INCLUDE_DIRS}
)

# Линковка
target_link_libraries(s3_server PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    glog::glog
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Проверка поддержки filesystem
include(CheckIncludeFileCXX)
check_include_file_cxx(filesystem HAVE_FILESYSTEM)
if(HAVE_FILESYSTEM)
    target_compile_definitions(s3_server PRIVATE HAVE_FILESYSTEM=1)
else()
    target_link_libraries(s3_server PRIVATE stdc++fs)
endif()

# Устанавливаем флаги компилятора
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(s3_server PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )
endif()

if(GTest_FOUND)
    enable_testing()
    add_subdirectory(tests)
endif()
