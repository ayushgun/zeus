cmake_minimum_required(VERSION 3.5)
project(zeus)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEV_WARNINGS "-Wall -Wextra -Werror -pedantic")
    set(SANITIZERS "-fsanitize=undefined -fsanitize=thread")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEV_WARNINGS} ${SANITIZERS}")
endif()

add_library(zeus SHARED src/queue.cpp)
target_include_directories(zeus PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
