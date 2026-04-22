find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${GTEST_INCLUDE_DIRS}
)

# Модульные тесты
add_executable(unit_tests
    file_manager_test.cpp
    request_handler_test.cpp
    authenticator_test.cpp
    authorizer_test.cpp
    authorization_header_parser_test.cpp
    ../src/file_manager.cpp
    ../src/request_handler.cpp
    ../src/authenticator.cpp
    ../src/authorizer.cpp
    ../src/path_utils.cpp
    ../src/authorization_header_parser.cpp
    ../src/compression.cpp
)

target_link_libraries(unit_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    glog::glog
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(ZLIB_FOUND)
    target_link_libraries(unit_tests PRIVATE ZLIB::ZLIB)
    target_compile_definitions(unit_tests PRIVATE HAVE_ZLIB=1)
endif()

# Prevent linker from discarding test registration symbols
target_link_options(unit_tests PRIVATE -Wl,--no-gc-sections)

# Проверка поддержки filesystem (повторяем логику из корневого CMakeLists.txt)
include(CheckIncludeFileCXX)
check_include_file_cxx(filesystem HAVE_FILESYSTEM)
if(HAVE_FILESYSTEM)
    target_compile_definitions(unit_tests PRIVATE HAVE_FILESYSTEM=1)
else()
    target_link_libraries(unit_tests PRIVATE stdc++fs)
endif()

# Интеграционные тесты
add_executable(integration_tests
    server_integration_test.cpp
    ../src/server.cpp
    ../src/file_manager.cpp
    ../src/request_handler.cpp
    ../src/authenticator.cpp
    ../src/authorizer.cpp
    ../src/rate_limiter.cpp
    ../src/path_utils.cpp
    ../src/authorization_header_parser.cpp
    ../src/compression.cpp
)

target_link_libraries(integration_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    glog::glog
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(ZLIB_FOUND)
    target_link_libraries(integration_tests PRIVATE ZLIB::ZLIB)
    target_compile_definitions(integration_tests PRIVATE HAVE_ZLIB=1)
endif()

# Проверка поддержки filesystem (повторяем логику из корневого CMakeLists.txt)
include(CheckIncludeFileCXX)
check_include_file_cxx(filesystem HAVE_FILESYSTEM)
if(HAVE_FILESYSTEM)
    target_compile_definitions(integration_tests PRIVATE HAVE_FILESYSTEM=1)
else()
    target_link_libraries(integration_tests PRIVATE stdc++fs)
endif()

