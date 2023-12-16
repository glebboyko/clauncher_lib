cmake_minimum_required(VERSION 3.20)
project(clauncher)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)

get_filename_component(DIR_INCLUDE include ABSOLUTE DIRECTORY)
set(${PROJECT_NAME}_DIR ${DIR_INCLUDE} PARENT_SCOPE)

set(CTCP_LIB "c_tcp_lib")
file(GLOB LIB_CONT "${CMAKE_SOURCE_DIR}/source/${CTCP_LIB}/*")
list(LENGTH LIB_CONT LIB_LENGTH)

set(MY_GIT_LINK "https://github.com/glebboyko")
if (LIB_LENGTH EQUAL 0)
    execute_process(COMMAND git clone "${MY_GIT_LINK}/${CTCP_LIB}.git" "${CMAKE_SOURCE_DIR}/source/${CTCP_LIB}")
else ()
    execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}/source/${CTCP_LIB}" pull)
endif ()

add_subdirectory("${CMAKE_SOURCE_DIR}/source/${CTCP_LIB}/cpp")
include_directories(${c_tcp_DIR})

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/source)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_SOURCE_DIR}/source/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*/${CTCP_LIB}/*")
add_library(${PROJECT_NAME}
        SHARED
        ${SOURCE_FILES})
add_library(${PROJECT_NAME}_lib ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE ${CTCP_LIB})