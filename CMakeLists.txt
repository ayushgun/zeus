cmake_minimum_required(VERSION 3.27)
project(zeus)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEV_WARNINGS "-Wall -Wextra -Werror -pedantic")
    set(SANITIZERS "-fsanitize=undefined -fsanitize=thread")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEV_WARNINGS} ${SANITIZERS}")
endif()

add_library(zeus INTERFACE)
target_include_directories(zeus INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(GTest REQUIRED)
if(GTEST_FOUND)
    enable_testing()
    add_executable(zeus_tests test/test_queue.cpp)
    target_link_libraries(zeus_tests GTest::GTest GTest::Main zeus)
    target_include_directories(zeus_tests PRIVATE ${GTEST_INCLUDE_DIRS})

    include(GoogleTest)
    gtest_discover_tests(zeus_tests)
endif()
