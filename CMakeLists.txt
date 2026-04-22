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

# Compression libraries
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    message(STATUS "Found ZLIB: ${ZLIB_VERSION}")
    add_definitions(-DHAVE_ZLIB=1)
else()
    message(WARNING "ZLIB not found - gzip compression will be disabled")
endif()

find_package(Brotli QUIET)
if(Brotli_FOUND)
    message(STATUS "Found Brotli")
    add_definitions(-DHAVE_BROTLI=1)
else()
    # Try alternative names
    find_library(BROTLIENC_LIB NAMES brotlienc brotlienc-static)
    find_library(BROTLIDEC_LIB NAMES brotlidec brotlidec-static)
    find_path(BROTLI_INCLUDE_DIR NAMES brotli/encode.h brotli/decode.h)
    if(BROTLIENC_LIB AND BROTLIDEC_LIB AND BROTLI_INCLUDE_DIR)
        message(STATUS "Found Brotli via manual search")
        set(Brotli_FOUND TRUE)
        set(BROTLI_LIBRARIES ${BROTLIENC_LIB} ${BROTLIDEC_LIB})
        set(BROTLI_INCLUDE_DIRS ${BROTLI_INCLUDE_DIR})
        add_definitions(-DHAVE_BROTLI=1)
    else()
        message(WARNING "Brotli not found - brotli compression will be disabled")
    endif()
endif()


# Исходники
set(sources
    backend/src/main.cpp
    backend/src/server.cpp
    backend/src/file_manager.cpp
    backend/src/request_handler.cpp
    backend/src/authenticator.cpp
    backend/src/authorizer.cpp
    backend/src/rate_limiter.cpp
    backend/src/path_utils.cpp
    backend/src/config.cpp
    backend/src/authorization_header_parser.cpp
    backend/src/compression.cpp
)

# Заголовки
set(headers
    backend/include/server.hpp
    backend/include/file_manager.hpp
    backend/include/request_handler.hpp
    backend/include/authenticator.hpp
    backend/include/authorizer.hpp
    backend/include/rate_limiter.hpp
    backend/include/config.hpp
    backend/include/authorization_header_parser.hpp
    backend/include/compression.hpp
)

# Executable
add_executable(s3_server ${sources} ${headers} backend/src/compression.cpp)

# Включаем директории
target_include_directories(s3_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/include
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

# Compression libraries
if(ZLIB_FOUND)
    target_link_libraries(s3_server PRIVATE ZLIB::ZLIB)
    target_compile_definitions(s3_server PRIVATE HAVE_ZLIB=1)
endif()

if(Brotli_FOUND)
    if(TARGET Brotli::brotlienc AND TARGET Brotli::brotlidec)
        target_link_libraries(s3_server PRIVATE Brotli::brotlienc Brotli::brotlidec)
    elseif(BROTLI_LIBRARIES)
        target_link_libraries(s3_server PRIVATE ${BROTLI_LIBRARIES})
        target_include_directories(s3_server PRIVATE ${BROTLI_INCLUDE_DIRS})
    endif()
    target_compile_definitions(s3_server PRIVATE HAVE_BROTLI=1)
endif()

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
    add_subdirectory(backend/tests)
endif()
